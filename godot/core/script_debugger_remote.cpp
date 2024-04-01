/*************************************************************************/
/*  script_debugger_remote.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "script_debugger_remote.h"
#include "core/io/marshalls.h"
#include "globals.h"
#include "io/ip.h"
#include "os/input.h"
#include "os/os.h"

void ScriptDebuggerRemote::_send_video_memory() {

	List<ResourceUsage> usage;
	if (resource_usage_func)
		resource_usage_func(&usage);

	usage.sort();

	packet_peer_stream->put_var("message:video_mem");
	packet_peer_stream->put_var(usage.size() * 4);

	for (List<ResourceUsage>::Element *E = usage.front(); E; E = E->next()) {

		packet_peer_stream->put_var(E->get().path);
		packet_peer_stream->put_var(E->get().type);
		packet_peer_stream->put_var(E->get().format);
		packet_peer_stream->put_var(E->get().vram);
	}
}

Error ScriptDebuggerRemote::connect_to_host(const String &p_host, uint16_t p_port) {

	IP_Address ip;
	if (p_host.is_valid_ip_address())
		ip = p_host;
	else
		ip = IP::get_singleton()->resolve_hostname(p_host);

	int port = p_port;

	int tries = 3;
	tcp_client->connect(ip, port);

	while (tries--) {

		if (tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
			break;
		} else {

			OS::get_singleton()->delay_usec(1000000);
			print_line("Remote Debugger: Connection failed with status: '" + String::num(tcp_client->get_status()) + "', retrying in 1 sec.");
		};
	};

	if (tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {

		print_line("Remote Debugger: Unable to connect");
		return FAILED;
	};

	//    print_line("Remote Debugger: Connection OK!");
	packet_peer_stream->set_stream_peer(tcp_client);

	return OK;
}

static int _ScriptDebuggerRemote_found_id = 0;
static Object *_ScriptDebuggerRemote_find = NULL;
static void _ScriptDebuggerRemote_debug_func(Object *p_obj) {

	if (_ScriptDebuggerRemote_find == p_obj) {
		_ScriptDebuggerRemote_found_id = p_obj->get_instance_ID();
	}
}

static ObjectID safe_get_instance_id(const Variant &p_v) {

	Object *o = p_v;
	if (o == NULL)
		return 0;
	else {

		REF r = p_v;
		if (r.is_valid()) {

			return r->get_instance_ID();
		} else {

			_ScriptDebuggerRemote_found_id = 0;
			_ScriptDebuggerRemote_find = NULL;
			ObjectDB::debug_objects(_ScriptDebuggerRemote_debug_func);
			return _ScriptDebuggerRemote_found_id;
		}
	}
}

void ScriptDebuggerRemote::debug(ScriptLanguage *p_script, bool p_can_continue) {

	//this function is called when there is a debugger break (bug on script)
	//or when execution is paused from editor

	if (!tcp_client->is_connected()) {
		ERR_EXPLAIN("Script Debugger failed to connect, but being used anyway.");
		ERR_FAIL();
	}

	packet_peer_stream->put_var("debug_enter");
	packet_peer_stream->put_var(2);
	packet_peer_stream->put_var(p_can_continue);
	packet_peer_stream->put_var(p_script->debug_get_error());

	skip_profile_frame = true; // to avoid super long frame time for the frame

	Input::MouseMode mouse_mode = Input::get_singleton()->get_mouse_mode();
	if (mouse_mode != Input::MOUSE_MODE_VISIBLE)
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);

	while (true) {

		_get_output();

		if (packet_peer_stream->get_available_packet_count() > 0) {

			Variant var;
			Error err = packet_peer_stream->get_var(var);
			ERR_CONTINUE(err != OK);
			ERR_CONTINUE(var.get_type() != Variant::ARRAY);

			Array cmd = var;

			ERR_CONTINUE(cmd.size() == 0);
			ERR_CONTINUE(cmd[0].get_type() != Variant::STRING);

			String command = cmd[0];

			if (command == "get_stack_dump") {

				packet_peer_stream->put_var("stack_dump");
				int slc = p_script->debug_get_stack_level_count();
				packet_peer_stream->put_var(slc);

				for (int i = 0; i < slc; i++) {

					Dictionary d;
					d["file"] = p_script->debug_get_stack_level_source(i);
					d["line"] = p_script->debug_get_stack_level_line(i);
					d["function"] = p_script->debug_get_stack_level_function(i);
					//d["id"]=p_script->debug_get_stack_level_
					d["id"] = 0;

					packet_peer_stream->put_var(d);
				}

			} else if (command == "get_stack_frame_vars") {

				cmd.remove(0);
				ERR_CONTINUE(cmd.size() != 1);
				int lv = cmd[0];

				ScriptInstance *self_instance = p_script->debug_get_stack_level_instance(lv);

				List<String> members;
				List<Variant> member_vals;

				p_script->debug_get_stack_level_members(lv, &members, &member_vals);

				ERR_CONTINUE(members.size() != member_vals.size());

				List<String> locals;
				List<Variant> local_vals;

				p_script->debug_get_stack_level_locals(lv, &locals, &local_vals);

				ERR_CONTINUE(locals.size() != local_vals.size());

				List<String> globals;
				List<Variant> global_vals;

				p_script->debug_get_globals(&globals, &global_vals);

				ERR_CONTINUE(globals.size() != global_vals.size());

				packet_peer_stream->put_var("stack_frame_vars");
				packet_peer_stream->put_var(locals.size() + members.size() + globals.size());

				{ //locals
					List<String>::Element *E = locals.front();
					List<Variant>::Element *F = local_vals.front();

					while (E) {

						PropertyInfo pi(var.get_type(), String("locals/") + E->get());
						packet_peer_stream->put_var(_serialize(F->get(), pi));

						E = E->next();
						F = F->next();
					}
				}

				{ //members

					if (self_instance) { // self

						members.push_front("self");
						member_vals.push_front(self_instance->get_owner());
					}

					List<String>::Element *E = members.front();
					List<Variant>::Element *F = member_vals.front();

					while (E) {

						PropertyInfo pi(var.get_type(), String("members/") + E->get());
						packet_peer_stream->put_var(_serialize(F->get(), pi));

						E = E->next();
						F = F->next();
					}
				}

				if (self_instance) { // constants

					Ref<Script> script = self_instance->get_script();
					if (!script.is_null()) {

						const Map<StringName, Variant> &constants = script->get_constants();

						for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {

							PropertyInfo pi(var.get_type(), String("constants/") + E->key());
							packet_peer_stream->put_var(_serialize(E->value(), pi));
						}
					}
				}

				{ //globals
					List<String>::Element *E = globals.front();
					List<Variant>::Element *F = global_vals.front();

					while (E) {

						PropertyInfo pi(var.get_type(), String("globals/") + E->get());
						packet_peer_stream->put_var(_serialize(F->get(), pi));

						E = E->next();
						F = F->next();
					}
				}

			} else if (command == "step") {

				set_depth(-1);
				set_lines_left(1);
				break;
			} else if (command == "next") {

				set_depth(0);
				set_lines_left(1);
				break;

			} else if (command == "continue") {

				set_depth(-1);
				set_lines_left(-1);
				OS::get_singleton()->move_window_to_foreground();
				break;
			} else if (command == "break") {
				ERR_PRINT("Got break when already broke!");
				break;
			} else if (command == "request_scene_tree") {

				if (request_scene_tree)
					request_scene_tree(request_scene_tree_ud);

			} else if (command == "request_video_mem") {

				_send_video_memory();
			} else if (command == "inspect_object") {

				ObjectID id = cmd[1];
				_send_object_id(id);
			} else if (command == "set_object_property") {

				_set_object_property(cmd[1], cmd[2], cmd[3]);

			} else if (command == "reload_scripts") {
				reload_all_scripts = true;
			} else if (command == "breakpoint") {

				bool set = cmd[3];
				if (set)
					insert_breakpoint(cmd[2], cmd[1]);
				else
					remove_breakpoint(cmd[2], cmd[1]);

			} else {
				_parse_live_edit(cmd);
			}

		} else {
			OS::get_singleton()->delay_usec(10000);
		}
	}

	packet_peer_stream->put_var("debug_exit");
	packet_peer_stream->put_var(0);

	if (mouse_mode != Input::MOUSE_MODE_VISIBLE)
		Input::get_singleton()->set_mouse_mode(mouse_mode);
}

void ScriptDebuggerRemote::_get_output() {

	mutex->lock();
	if (output_strings.size()) {

		locking = true;
		packet_peer_stream->put_var("output");
		packet_peer_stream->put_var(output_strings.size());

		while (output_strings.size()) {

			packet_peer_stream->put_var(output_strings.front()->get());
			output_strings.pop_front();
		}
		locking = false;
	}

	while (messages.size()) {
		locking = true;
		packet_peer_stream->put_var("message:" + messages.front()->get().message);
		packet_peer_stream->put_var(messages.front()->get().data.size());
		for (int i = 0; i < messages.front()->get().data.size(); i++) {
			packet_peer_stream->put_var(messages.front()->get().data[i]);
		}
		messages.pop_front();
		locking = false;
	}

	while (errors.size()) {
		locking = true;
		packet_peer_stream->put_var("error");
		OutputError oe = errors.front()->get();

		packet_peer_stream->put_var(oe.callstack.size() + 2);

		Array error_data;

		error_data.push_back(oe.hr);
		error_data.push_back(oe.min);
		error_data.push_back(oe.sec);
		error_data.push_back(oe.msec);
		error_data.push_back(oe.source_func);
		error_data.push_back(oe.source_file);
		error_data.push_back(oe.source_line);
		error_data.push_back(oe.error);
		error_data.push_back(oe.error_descr);
		error_data.push_back(oe.warning);
		packet_peer_stream->put_var(error_data);
		packet_peer_stream->put_var(oe.callstack.size());
		for (int i = 0; i < oe.callstack.size(); i++) {
			packet_peer_stream->put_var(oe.callstack[i]);
		}

		errors.pop_front();
		locking = false;
	}
	mutex->unlock();
}

void ScriptDebuggerRemote::line_poll() {

	//the purpose of this is just processing events every now and then when the script might get too busy
	//otherwise bugs like infinite loops cant be catched
	if (poll_every % 2048 == 0)
		_poll_events();
	poll_every++;
}

void ScriptDebuggerRemote::_err_handler(void *ud, const char *p_func, const char *p_file, int p_line, const char *p_err, const char *p_descr, ErrorHandlerType p_type) {

	if (p_type == ERR_HANDLER_SCRIPT)
		return; //ignore script errors, those go through debugger

	ScriptDebuggerRemote *sdr = (ScriptDebuggerRemote *)ud;

	OutputError oe;
	oe.error = p_err;
	oe.error_descr = p_descr;
	oe.source_file = p_file;
	oe.source_line = p_line;
	oe.source_func = p_func;
	oe.warning = p_type == ERR_HANDLER_WARNING;
	uint64_t time = OS::get_singleton()->get_ticks_msec();
	oe.hr = time / 3600000;
	oe.min = (time / 60000) % 60;
	oe.sec = (time / 1000) % 60;
	oe.msec = time % 1000;
	Array cstack;

	Vector<ScriptLanguage::StackInfo> si;

	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		si = ScriptServer::get_language(i)->debug_get_current_stack_info();
		if (si.size())
			break;
	}

	cstack.resize(si.size() * 2);
	for (int i = 0; i < si.size(); i++) {
		String path;
		int line = 0;
		if (si[i].script.is_valid()) {
			path = si[i].script->get_path();
			line = si[i].line;
		}
		cstack[i * 2 + 0] = path;
		cstack[i * 2 + 1] = line;
	}

	oe.callstack = cstack;

	sdr->mutex->lock();

	if (!sdr->locking && sdr->tcp_client->is_connected()) {

		sdr->errors.push_back(oe);
	}

	sdr->mutex->unlock();
}

bool ScriptDebuggerRemote::_parse_live_edit(const Array &cmd) {

	String cmdstr = cmd[0];
	if (!live_edit_funcs || !cmdstr.begins_with("live_"))
		return false;

	//print_line(Variant(cmd).get_construct_string());
	if (cmdstr == "live_set_root") {

		if (!live_edit_funcs->root_func)
			return true;
		//print_line("root: "+Variant(cmd).get_construct_string());
		live_edit_funcs->root_func(live_edit_funcs->udata, cmd[1], cmd[2]);

	} else if (cmdstr == "live_node_path") {

		if (!live_edit_funcs->node_path_func)
			return true;
		//print_line("path: "+Variant(cmd).get_construct_string());

		live_edit_funcs->node_path_func(live_edit_funcs->udata, cmd[1], cmd[2]);

	} else if (cmdstr == "live_res_path") {

		if (!live_edit_funcs->res_path_func)
			return true;
		live_edit_funcs->res_path_func(live_edit_funcs->udata, cmd[1], cmd[2]);

	} else if (cmdstr == "live_node_prop_res") {
		if (!live_edit_funcs->node_set_res_func)
			return true;

		live_edit_funcs->node_set_res_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_node_prop") {

		if (!live_edit_funcs->node_set_func)
			return true;
		live_edit_funcs->node_set_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_res_prop_res") {

		if (!live_edit_funcs->res_set_res_func)
			return true;
		live_edit_funcs->res_set_res_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_res_prop") {

		if (!live_edit_funcs->res_set_func)
			return true;
		live_edit_funcs->res_set_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_node_call") {

		if (!live_edit_funcs->node_call_func)
			return true;
		live_edit_funcs->node_call_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]);

	} else if (cmdstr == "live_res_call") {

		if (!live_edit_funcs->res_call_func)
			return true;
		live_edit_funcs->res_call_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]);

	} else if (cmdstr == "live_create_node") {

		live_edit_funcs->tree_create_node_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_instance_node") {

		live_edit_funcs->tree_instance_node_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_remove_node") {

		live_edit_funcs->tree_remove_node_func(live_edit_funcs->udata, cmd[1]);

	} else if (cmdstr == "live_remove_and_keep_node") {

		live_edit_funcs->tree_remove_and_keep_node_func(live_edit_funcs->udata, cmd[1], cmd[2]);
	} else if (cmdstr == "live_restore_node") {

		live_edit_funcs->tree_restore_node_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3]);

	} else if (cmdstr == "live_duplicate_node") {

		live_edit_funcs->tree_duplicate_node_func(live_edit_funcs->udata, cmd[1], cmd[2]);
	} else if (cmdstr == "live_reparent_node") {

		live_edit_funcs->tree_reparent_node_func(live_edit_funcs->udata, cmd[1], cmd[2], cmd[3], cmd[4]);

	} else {

		return false;
	}

	return true;
}

void ScriptDebuggerRemote::_send_object_id(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj)
		return;

	Array props;
	const uint32_t SEND_PROPERTIES = 0xFFFFFFFF; // All kind of properties are allowed
	List<PropertyInfo> pinfo;
	Set<StringName> ignored_properties;
	obj->get_property_list(&pinfo, true);
	bool query_props = true;

	if (ScriptInstance *si = obj->get_script_instance()) {
		if (!si->get_script().is_null()) {

			const Map<StringName, Variant> &constants = si->get_script()->get_constants();
			for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {

				PropertyInfo pi(E->value().get_type(), String("constants/") + E->key());
				props.push_back(_serialize(E->value(), pi));
			}

			const Set<StringName> &members = si->get_script()->get_members();
			for (Set<StringName>::Element *E = members.front(); E; E = E->next()) {

				Variant m;
				if (si->get(E->get(), m)) {

					PropertyInfo pi(m.get_type(), String("members/") + E->get());
					props.push_back(_serialize(m, pi));

					ignored_properties.insert(E->get());
				}
			}

			PropertyInfo pi(Variant::OBJECT, String("Resource/script"));
			props.push_back(_serialize(si->get_script(), pi));
		}
	} else if (Resource *res = obj->cast_to<Resource>()) {

		if (res->cast_to<Script>()) {

			const Map<StringName, Variant> &constants = res->cast_to<Script>()->get_constants();
			for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {

				PropertyInfo pi(E->value().get_type(), String("constants/") + E->key());
				props.push_back(_serialize(E->value(), pi));
			}
		}

		PropertyInfo pi(Variant::OBJECT, res->get_type_name(), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CATEGORY);
		props.push_front(_serialize(res, pi));

		PropertyInfo pathpi(Variant::STRING, "Resource/Resource");
		pathpi.hint_string = "REMOTE:RES";
		props.push_back(_serialize(res->get_path(), pathpi));

		query_props = false;
	} else if (obj->is_type("Node")) {

		String path = obj->call("get_path");
		PropertyInfo pi(Variant::OBJECT, String("Node/path"));
		props.push_back(_serialize(path, pi));
	}

	if (query_props) {
		for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {

			if (E->get().usage & SEND_PROPERTIES && !ignored_properties.has(E->get().name)) {

				Variant var = obj->get(E->get().name);
				props.push_back(_serialize(var, E->get()));
			}
		}
	}

	packet_peer_stream->put_var("message:inspect_object");
	packet_peer_stream->put_var(2);
	packet_peer_stream->put_var(p_id);
	packet_peer_stream->put_var(props);
}

void ScriptDebuggerRemote::_set_object_property(ObjectID p_id, const String &p_property, const Variant &p_value) {

	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj)
		return;

	String name = p_property;
	const String member_prefix = String("members/");
	if (name.begins_with(member_prefix) && p_property.length() > member_prefix.length()) {
		name = p_property.substr(member_prefix.length(), p_property.length());
	}

	obj->set(name, p_value);
}

int __put_value_to_buff_at_pos(const Variant &value, DVector<uint8_t> &buff, int pos) {

	int size_required = 0;

	DVector<uint8_t> valuebuf;
	if (value.get_type() == Variant::RAW_ARRAY)
		valuebuf = value;

	String sub_contetn;

	switch (value.get_type()) {
		case Variant::NIL:
		case Variant::INT:
		case Variant::BOOL:
			size_required = sizeof(uint32_t);
			break;
		case Variant::REAL:
			size_required = sizeof(uint32_t);
			break;
		case Variant::STRING: {
			sub_contetn = value;
			const int MAX_STR_LEN = 80; // More than 80 will not be send to the debugger
			if (sub_contetn.length() > MAX_STR_LEN)
				sub_contetn = sub_contetn.substr(0, MAX_STR_LEN) + "...";
			encode_variant(sub_contetn, NULL, size_required);
		} break;
		default:
			encode_variant(value, NULL, size_required);
			break;
	}
	if (buff.size() < pos + size_required)
		buff.resize((pos + size_required) * 2);

	switch (value.get_type()) {
		case Variant::INT:
		case Variant::BOOL:
			encode_uint32(value, &buff.write()[pos]);
			break;
		case Variant::REAL:
			encode_float(value, &buff.write()[pos]);
			break;
		case Variant::STRING:
			encode_variant(sub_contetn, &buff.write()[pos], size_required);
			break;
		default:
			encode_variant(value, &buff.write()[pos], size_required);
			break;
	}

	return size_required;
}

int ScriptDebuggerRemote::_serialize_variant(const Variant &var, const PropertyInfo &p_info, DVector<uint8_t> &buff) {

	int used_size = 0;

	used_size += __put_value_to_buff_at_pos(p_info.name, buff, used_size);
	const int type_index = used_size;
	used_size += __put_value_to_buff_at_pos(var.get_type(), buff, type_index);
	const int hint_index = used_size;
	used_size += __put_value_to_buff_at_pos(p_info.hint, buff, hint_index);
	used_size += __put_value_to_buff_at_pos(p_info.usage, buff, used_size);

	StringName hint_string = p_info.hint_string;

	int value_len = 0;
	switch (var.get_type()) {
		case Variant::OBJECT: {
			__put_value_to_buff_at_pos(Variant::INT, buff, type_index);
			__put_value_to_buff_at_pos(PROPERTY_HINT_OBJECT_ID, buff, hint_index);
			Object *obj = var;
			uint32_t id = obj ? obj->get_instance_ID() : 0;
			hint_string = obj ? obj->get_type_name() : hint_string;
			value_len += __put_value_to_buff_at_pos(id, buff, used_size + value_len);
		} break;
		case Variant::IMAGE:
			value_len += __put_value_to_buff_at_pos(Image(), buff, used_size + value_len);
			break;
		case Variant::ARRAY: {
			Array arr = var;
			value_len += __put_value_to_buff_at_pos(arr.size(), buff, used_size + value_len);

			for (int i = 0; i < arr.size(); i++) {

				const Variant &e = arr[i];
				PropertyInfo pi(e.get_type(), "");
				DVector<uint8_t> ebuff;
				ebuff.resize(256);
				ebuff.resize(_serialize_variant(e, pi, ebuff));

				value_len += __put_value_to_buff_at_pos(ebuff, buff, used_size + value_len);
			}
		} break;
		case Variant::STRING_ARRAY: {

			DVector<String> strarr = var;
			Array arr;
			arr.resize(strarr.size());

			for (int i = 0; i < strarr.size(); i++)
				arr[i] = strarr[i];

			PropertyInfo pi(Variant::ARRAY, "");
			DVector<uint8_t> ebuff;
			ebuff.resize(256);
			ebuff.resize(_serialize_variant(arr, pi, ebuff));

			value_len += __put_value_to_buff_at_pos(ebuff, buff, used_size + value_len);
		} break;
		case Variant::DICTIONARY: {
			Dictionary dict = var;
			value_len += __put_value_to_buff_at_pos(dict.size(), buff, used_size + value_len);

			const Array &keys = dict.keys();
			for (int i = 0; i < keys.size(); i++) {

				PropertyInfo pi;
				const Variant &key = keys[i];

				DVector<uint8_t> tmpbuff;
				tmpbuff.resize(256);
				pi.type = key.get_type();
				tmpbuff.resize(_serialize_variant(key, pi, tmpbuff));
				value_len += __put_value_to_buff_at_pos(tmpbuff, buff, used_size + value_len);

				const Variant &value = dict[key];
				pi.type = value.get_type();
				tmpbuff.resize(_serialize_variant(value, pi, tmpbuff));
				value_len += __put_value_to_buff_at_pos(tmpbuff, buff, used_size + value_len);
			}
		} break;
		default:
			value_len += __put_value_to_buff_at_pos(var, buff, used_size + value_len);
			break;
	}
	used_size += value_len;

	used_size += __put_value_to_buff_at_pos(hint_string, buff, used_size);

	return used_size;
}

DVector<uint8_t> ScriptDebuggerRemote::_serialize(const Variant &var, const PropertyInfo &p_info) {

	DVector<uint8_t> buff;
	buff.resize(256);
	buff.resize(_serialize_variant(var, p_info, buff));

	return buff;
}

void ScriptDebuggerRemote::_poll_events() {

	//this si called from ::idle_poll, happens only when running the game,
	//does not get called while on debug break

	while (packet_peer_stream->get_available_packet_count() > 0) {

		_get_output();

		//send over output_strings

		Variant var;
		Error err = packet_peer_stream->get_var(var);

		ERR_CONTINUE(err != OK);
		ERR_CONTINUE(var.get_type() != Variant::ARRAY);

		Array cmd = var;

		ERR_CONTINUE(cmd.size() == 0);
		ERR_CONTINUE(cmd[0].get_type() != Variant::STRING);

		String command = cmd[0];
		//cmd.remove(0);

		if (command == "break") {

			if (get_break_language())
				debug(get_break_language());
		} else if (command == "request_scene_tree") {

			if (request_scene_tree)
				request_scene_tree(request_scene_tree_ud);
		} else if (command == "request_video_mem") {

			_send_video_memory();
		} else if (command == "inspect_object") {

			ObjectID id = cmd[1];
			_send_object_id(id);
		} else if (command == "set_object_property") {

			_set_object_property(cmd[1], cmd[2], cmd[3]);
		} else if (command == "set_variable_value") {

			// TODO: Implement set the value stack variale

		} else if (command == "start_profiling") {

			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_start();
			}

			max_frame_functions = cmd[1];
			profiler_function_signature_map.clear();
			profiling = true;
			frame_time = 0;
			idle_time = 0;
			fixed_time = 0;
			fixed_frame_time = 0;

			print_line("PROFILING ALRIGHT!");

		} else if (command == "stop_profiling") {

			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_stop();
			}
			profiling = false;
			_send_profiling_data(false);
			print_line("PROFILING END!");
		} else if (command == "reload_scripts") {
			reload_all_scripts = true;
		} else if (command == "breakpoint") {

			bool set = cmd[3];
			if (set)
				insert_breakpoint(cmd[2], cmd[1]);
			else
				remove_breakpoint(cmd[2], cmd[1]);
		} else {
			_parse_live_edit(cmd);
		}
	}
}

void ScriptDebuggerRemote::_send_profiling_data(bool p_for_frame) {

	int ofs = 0;

	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		if (p_for_frame)
			ofs += ScriptServer::get_language(i)->profiling_get_frame_data(&profile_info[ofs], profile_info.size() - ofs);
		else
			ofs += ScriptServer::get_language(i)->profiling_get_accumulated_data(&profile_info[ofs], profile_info.size() - ofs);
	}

	for (int i = 0; i < ofs; i++) {
		profile_info_ptrs[i] = &profile_info[i];
	}

	SortArray<ScriptLanguage::ProfilingInfo *, ProfileInfoSort> sa;
	sa.sort(profile_info_ptrs.ptr(), ofs);

	int to_send = MIN(ofs, max_frame_functions);

	//check signatures first
	uint64_t total_script_time = 0;

	for (int i = 0; i < to_send; i++) {

		if (!profiler_function_signature_map.has(profile_info_ptrs[i]->signature)) {

			int idx = profiler_function_signature_map.size();
			packet_peer_stream->put_var("profile_sig");
			packet_peer_stream->put_var(2);
			packet_peer_stream->put_var(profile_info_ptrs[i]->signature);
			packet_peer_stream->put_var(idx);

			profiler_function_signature_map[profile_info_ptrs[i]->signature] = idx;
		}

		total_script_time += profile_info_ptrs[i]->self_time;
	}

	//send frames then

	if (p_for_frame) {
		packet_peer_stream->put_var("profile_frame");
		packet_peer_stream->put_var(8 + profile_frame_data.size() * 2 + to_send * 4);
	} else {
		packet_peer_stream->put_var("profile_total");
		packet_peer_stream->put_var(8 + to_send * 4);
	}

	packet_peer_stream->put_var(OS::get_singleton()->get_frames_drawn()); //total frame time
	packet_peer_stream->put_var(frame_time); //total frame time
	packet_peer_stream->put_var(idle_time); //idle frame time
	packet_peer_stream->put_var(fixed_time); //fixed frame time
	packet_peer_stream->put_var(fixed_frame_time); //fixed frame time

	packet_peer_stream->put_var(USEC_TO_SEC(total_script_time)); //total script execution time

	if (p_for_frame) {

		packet_peer_stream->put_var(profile_frame_data.size()); //how many profile framedatas to send
		packet_peer_stream->put_var(to_send); //how many script functions to send
		for (int i = 0; i < profile_frame_data.size(); i++) {

			packet_peer_stream->put_var(profile_frame_data[i].name);
			packet_peer_stream->put_var(profile_frame_data[i].data);
		}
	} else {
		packet_peer_stream->put_var(0); //how many script functions to send
		packet_peer_stream->put_var(to_send); //how many script functions to send
	}

	for (int i = 0; i < to_send; i++) {

		int sig_id = -1;

		if (profiler_function_signature_map.has(profile_info_ptrs[i]->signature)) {
			sig_id = profiler_function_signature_map[profile_info_ptrs[i]->signature];
		}

		packet_peer_stream->put_var(sig_id);
		packet_peer_stream->put_var(profile_info_ptrs[i]->call_count);
		packet_peer_stream->put_var(profile_info_ptrs[i]->total_time / 1000000.0);
		packet_peer_stream->put_var(profile_info_ptrs[i]->self_time / 1000000.0);
	}

	if (p_for_frame) {
		profile_frame_data.clear();
	}
}

void ScriptDebuggerRemote::idle_poll() {

	// this function is called every frame, except when there is a debugger break (::debug() in this class)
	// execution stops and remains in the ::debug function

	_get_output();

	if (requested_quit) {

		packet_peer_stream->put_var("kill_me");
		packet_peer_stream->put_var(0);
		requested_quit = false;
	}

	if (performance) {

		uint64_t pt = OS::get_singleton()->get_ticks_msec();
		if (pt - last_perf_time > 1000) {

			last_perf_time = pt;
			int max = performance->get("MONITOR_MAX");
			Array arr;
			arr.resize(max);
			for (int i = 0; i < max; i++) {
				arr[i] = performance->call("get_monitor", i);
			}
			packet_peer_stream->put_var("performance");
			packet_peer_stream->put_var(1);
			packet_peer_stream->put_var(arr);
		}
	}

	if (profiling) {

		if (skip_profile_frame) {
			skip_profile_frame = false;
		} else {
			//send profiling info normally
			_send_profiling_data(true);
		}
	}

	if (reload_all_scripts) {

		for (int i = 0; i < ScriptServer::get_language_count(); i++) {
			ScriptServer::get_language(i)->reload_all_scripts();
		}
		reload_all_scripts = false;
	}

	_poll_events();
}

void ScriptDebuggerRemote::send_message(const String &p_message, const Array &p_args) {

	mutex->lock();
	if (!locking && tcp_client->is_connected()) {

		Message msg;
		msg.message = p_message;
		msg.data = p_args;
		messages.push_back(msg);
	}
	mutex->unlock();
}

void ScriptDebuggerRemote::_print_handler(void *p_this, const String &p_string) {

	ScriptDebuggerRemote *sdr = (ScriptDebuggerRemote *)p_this;

	uint64_t ticks = OS::get_singleton()->get_ticks_usec() / 1000;
	sdr->msec_count += ticks - sdr->last_msec;
	sdr->last_msec = ticks;

	if (sdr->msec_count > 1000) {
		sdr->char_count = 0;
		sdr->msec_count = 0;
	}

	String s = p_string;
	int allowed_chars = MIN(MAX(sdr->max_cps - sdr->char_count, 0), s.length());

	if (allowed_chars == 0)
		return;

	if (allowed_chars < s.length()) {
		s = s.substr(0, allowed_chars);
	}

	sdr->char_count += allowed_chars;
	bool overflowed = sdr->char_count >= sdr->max_cps;

	sdr->mutex->lock();
	if (!sdr->locking && sdr->tcp_client->is_connected()) {

		if (overflowed)
			s += "[...]";

		sdr->output_strings.push_back(s);

		if (overflowed) {
			sdr->output_strings.push_back("[output overflow, print less text!]");
		}
	}
	sdr->mutex->unlock();
}

void ScriptDebuggerRemote::request_quit() {

	requested_quit = true;
}

void ScriptDebuggerRemote::set_request_scene_tree_message_func(RequestSceneTreeMessageFunc p_func, void *p_udata) {

	request_scene_tree = p_func;
	request_scene_tree_ud = p_udata;
}

void ScriptDebuggerRemote::set_live_edit_funcs(LiveEditFuncs *p_funcs) {

	live_edit_funcs = p_funcs;
}

bool ScriptDebuggerRemote::is_profiling() const {

	return profiling;
}
void ScriptDebuggerRemote::add_profiling_frame_data(const StringName &p_name, const Array &p_data) {

	int idx = -1;
	for (int i = 0; i < profile_frame_data.size(); i++) {
		if (profile_frame_data[i].name == p_name) {
			idx = i;
			break;
		}
	}

	FrameData fd;
	fd.name = p_name;
	fd.data = p_data;

	if (idx == -1) {
		profile_frame_data.push_back(fd);
	} else {
		profile_frame_data[idx] = fd;
	}
}

void ScriptDebuggerRemote::profiling_start() {
	//ignores this, uses it via connnection
}

void ScriptDebuggerRemote::profiling_end() {
	//ignores this, uses it via connnection
}

void ScriptDebuggerRemote::profiling_set_frame_times(float p_frame_time, float p_idle_time, float p_fixed_time, float p_fixed_frame_time) {

	frame_time = p_frame_time;
	idle_time = p_idle_time;
	fixed_time = p_fixed_time;
	fixed_frame_time = p_fixed_frame_time;
}

ScriptDebuggerRemote::ResourceUsageFunc ScriptDebuggerRemote::resource_usage_func = NULL;

ScriptDebuggerRemote::ScriptDebuggerRemote() {

	tcp_client = StreamPeerTCP::create_ref();
	packet_peer_stream = Ref<PacketPeerStream>(memnew(PacketPeerStream));
	packet_peer_stream->set_stream_peer(tcp_client);
	packet_peer_stream->set_input_buffer_max_size(1024 * 1024 * 1);
	mutex = Mutex::create();
	locking = false;

	phl.printfunc = _print_handler;
	phl.userdata = this;
	add_print_handler(&phl);
	requested_quit = false;
	performance = Globals::get_singleton()->get_singleton_object("Performance");
	last_perf_time = 0;
	poll_every = 0;
	request_scene_tree = NULL;
	live_edit_funcs = NULL;
	max_cps = GLOBAL_DEF("debug/max_remote_stdout_chars_per_second", 2048);
	char_count = 0;
	msec_count = 0;
	last_msec = 0;
	skip_profile_frame = false;

	eh.errfunc = _err_handler;
	eh.userdata = this;
	add_error_handler(&eh);

	profile_info.resize(CLAMP(int(Globals::get_singleton()->get("debug/profiler_max_functions")), 128, 65535));
	profile_info_ptrs.resize(profile_info.size());
	profiling = false;
	max_frame_functions = 16;
	reload_all_scripts = false;
}

ScriptDebuggerRemote::~ScriptDebuggerRemote() {

	remove_print_handler(&phl);
	remove_error_handler(&eh);
	memdelete(mutex);
}
