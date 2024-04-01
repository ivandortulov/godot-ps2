/*************************************************************************/
/*  globals.cpp                                                          */
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
#include "globals.h"
#include "os/dir_access.h"
#include "os/file_access.h"

#include "bind/core_bind.h"
#include "io/file_access_network.h"
#include "io/file_access_pack.h"
#include "io/marshalls.h"
#include "os/keyboard.h"
#include "os/os.h"

Globals *Globals::singleton = NULL;

Globals *Globals::get_singleton() {

	return singleton;
}

String Globals::get_resource_path() const {

	return resource_path;
};

String Globals::localize_path(const String &p_path) const {

	if (resource_path == "")
		return p_path; //not initialied yet

	if (p_path.begins_with("res://") || p_path.begins_with("user://") ||
			(p_path.is_abs_path() && !p_path.begins_with(resource_path)))
		return p_path.simplify_path();

	DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	String path = p_path.replace("\\", "/").simplify_path();

	if (dir->change_dir(path) == OK) {

		String cwd = dir->get_current_dir();
		cwd = cwd.replace("\\", "/");

		memdelete(dir);

		if (!cwd.begins_with(resource_path)) {
			return p_path;
		};

		return cwd.replace_first(resource_path, "res:/");
	} else {

		memdelete(dir);

		int sep = path.find_last("/");
		if (sep == -1) {
			return "res://" + path;
		};

		String parent = path.substr(0, sep);

		String plocal = localize_path(parent);
		if (plocal == "") {
			return "";
		};
		return plocal + path.substr(sep, path.size() - sep);
	};
}

void Globals::set_persisting(const String &p_name, bool p_persist) {

	ERR_FAIL_COND(!props.has(p_name));
	props[p_name].persist = p_persist;
}

bool Globals::is_persisting(const String &p_name) const {

	ERR_FAIL_COND_V(!props.has(p_name), false);
	return props[p_name].persist;
}

String Globals::globalize_path(const String &p_path) const {

	if (p_path.begins_with("res://")) {

		if (resource_path != "") {

			return p_path.replace("res:/", resource_path);
		};
		return p_path.replace("res://", "");
	};

	return p_path;
}

bool Globals::_set(const StringName &p_name, const Variant &p_value) {

	_THREAD_SAFE_METHOD_

	if (p_value.get_type() == Variant::NIL)
		props.erase(p_name);
	else {
		if (props.has(p_name)) {
			if (!props[p_name].overrided)
				props[p_name].variant = p_value;

			if (props[p_name].order >= NO_ORDER_BASE && registering_order) {
				props[p_name].order = last_order++;
			}
		} else {
			props[p_name] = VariantContainer(p_value, last_order++ + (registering_order ? 0 : NO_ORDER_BASE));
		}
	}

	if (!disable_platform_override) {

		String s = String(p_name);
		int sl = s.find("/");
		int p = s.find(".");
		if (p != -1 && sl != -1 && p < sl) {

			Vector<String> ps = s.substr(0, sl).split(".");
			String prop = s.substr(sl, s.length() - sl);
			for (int i = 1; i < ps.size(); i++) {

				if (ps[i] == OS::get_singleton()->get_name()) {

					String fullprop = ps[0] + prop;

					set(fullprop, p_value);
					props[fullprop].overrided = true;
				}
			}
		}
	}

	return true;
}
bool Globals::_get(const StringName &p_name, Variant &r_ret) const {

	_THREAD_SAFE_METHOD_

	if (!props.has(p_name))
		return false;

	r_ret = props[p_name].variant;
	return true;
}

struct _VCSort {

	String name;
	Variant::Type type;
	int order;
	int flags;

	bool operator<(const _VCSort &p_vcs) const { return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order; }
};

void Globals::_get_property_list(List<PropertyInfo> *p_list) const {

	_THREAD_SAFE_METHOD_

	Set<_VCSort> vclist;

	for (Map<StringName, VariantContainer>::Element *E = props.front(); E; E = E->next()) {

		const VariantContainer *v = &E->get();

		if (v->hide_from_editor)
			continue;

		_VCSort vc;
		vc.name = E->key();
		vc.order = v->order;
		vc.type = v->variant.get_type();
		if (vc.name.begins_with("input/") || vc.name.begins_with("import/") || vc.name.begins_with("export/") || vc.name.begins_with("/remap") || vc.name.begins_with("/locale") || vc.name.begins_with("/autoload"))
			vc.flags = PROPERTY_USAGE_CHECKABLE | PROPERTY_USAGE_STORAGE;
		else
			vc.flags = PROPERTY_USAGE_CHECKABLE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;

		if (v->persist) {
			vc.flags |= PROPERTY_USAGE_CHECKED;
		}

		vclist.insert(vc);
	}

	for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

		if (custom_prop_info.has(E->get().name)) {
			PropertyInfo pi = custom_prop_info[E->get().name];
			pi.name = E->get().name;
			pi.usage = E->get().flags;
			p_list->push_back(pi);
		} else
			p_list->push_back(PropertyInfo(E->get().type, E->get().name, PROPERTY_HINT_NONE, "", E->get().flags));
	}
}

bool Globals::_load_resource_pack(const String &p_pack) {

	if (PackedData::get_singleton()->is_disabled())
		return false;

	bool ok = PackedData::get_singleton()->add_pack(p_pack) == OK;

	if (!ok)
		return false;

	//if data.pck is found, all directory access will be from here
	DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);
	using_datapack = true;

	return true;
}

Error Globals::setup(const String &p_path, const String &p_main_pack) {

	//an absolute mess of a function, must be cleaned up and reorganized somehow at some point

	//_load_settings(p_path+"/override.cfg");

	if (p_main_pack != "") {

		bool ok = _load_resource_pack(p_main_pack);
		ERR_FAIL_COND_V(!ok, ERR_CANT_OPEN);

		if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {

			_load_settings("res://override.cfg");
		}

		return OK;
	}

	if (OS::get_singleton()->get_executable_path() != "") {

		if (_load_resource_pack(OS::get_singleton()->get_executable_path())) {

			if (p_path != "") {
				resource_path = p_path;
			} else {
				DirAccess *d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
				resource_path = d->get_current_dir();
				memdelete(d);
			}
			if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {

				_load_settings("res://override.cfg");
			}

			return OK;
		}
	}

	if (FileAccessNetworkClient::get_singleton()) {

		if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {

			_load_settings("res://override.cfg");
		}

		return OK;
	}

	if (OS::get_singleton()->get_resource_dir() != "") {
		//OS will call Globals->get_resource_path which will be empty if not overriden!
		//if the OS would rather use somewhere else, then it will not be empty.
		resource_path = OS::get_singleton()->get_resource_dir().replace("\\", "/");
		if (resource_path.length() && resource_path[resource_path.length() - 1] == '/')
			resource_path = resource_path.substr(0, resource_path.length() - 1); // chop end

		print_line("has res dir: " + resource_path);
		if (!_load_resource_pack("res://data.pck"))
			_load_resource_pack("res://data.zip");
		// make sure this is load from the resource path
		print_line("exists engine cfg? " + itos(FileAccess::exists("/engine.cfg")));
		if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {
			print_line("loaded engine.cfg");
			_load_settings("res://override.cfg");
		}

		return OK;
	}

	DirAccess *d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (!d) {

		resource_path = p_path;

	} else {

		d->change_dir(p_path);

		String candidate = d->get_current_dir();
		String current_dir = d->get_current_dir();
		String exec_name = OS::get_singleton()->get_executable_path().get_file().basename();
		bool found = false;
		bool first_time = true;

		while (true) {
			//try to load settings in ascending through dirs shape!

			//tries to open pack, but only first time
			if (first_time && (_load_resource_pack(current_dir + "/" + exec_name + ".pck") || _load_resource_pack(current_dir + "/" + exec_name + ".zip"))) {
				if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {

					_load_settings("res://override.cfg");
					found = true;
				}
				break;
			} else if (first_time && (_load_resource_pack(current_dir + "/data.pck") || _load_resource_pack(current_dir + "/data.zip"))) {
				if (_load_settings("res://engine.cfg") == OK || _load_settings_binary("res://engine.cfb") == OK) {

					_load_settings("res://override.cfg");
					found = true;
				}
				break;
			} else if (_load_settings(current_dir + "/engine.cfg") == OK || _load_settings_binary(current_dir + "/engine.cfb") == OK) {

				_load_settings(current_dir + "/override.cfg");
				candidate = current_dir;
				found = true;
				break;
			}

			d->change_dir("..");
			if (d->get_current_dir() == current_dir)
				break; //not doing anything useful
			current_dir = d->get_current_dir();
			first_time = false;
		}

		resource_path = candidate;
		resource_path = resource_path.replace("\\", "/"); // windows path to unix path just in case
		memdelete(d);

		if (!found)
			return ERR_FILE_NOT_FOUND;
	};

	if (resource_path.length() && resource_path[resource_path.length() - 1] == '/')
		resource_path = resource_path.substr(0, resource_path.length() - 1); // chop end

	return OK;
}

bool Globals::has(String p_var) const {

	_THREAD_SAFE_METHOD_

	return props.has(p_var);
}

static Vector<String> _decode_params(const String &p_string) {

	int begin = p_string.find("(");
	ERR_FAIL_COND_V(begin == -1, Vector<String>());
	begin++;
	int end = p_string.find(")");
	ERR_FAIL_COND_V(end < begin, Vector<String>());
	return p_string.substr(begin, end - begin).split(",");
}

static String _get_chunk(const String &str, int &pos, int close_pos) {

	enum {
		MIN_COMMA,
		MIN_COLON,
		MIN_CLOSE,
		MIN_QUOTE,
		MIN_PARENTHESIS,
		MIN_CURLY_OPEN,
		MIN_OPEN
	};

	int min_pos = close_pos;
	int min_what = MIN_CLOSE;

#define TEST_MIN(m_how, m_what)           \
	{                                     \
		int res = str.find(m_how, pos);   \
		if (res != -1 && res < min_pos) { \
			min_pos = res;                \
			min_what = m_what;            \
		}                                 \
	}

	TEST_MIN(",", MIN_COMMA);
	TEST_MIN("[", MIN_OPEN);
	TEST_MIN("{", MIN_CURLY_OPEN);
	TEST_MIN("(", MIN_PARENTHESIS);
	TEST_MIN("\"", MIN_QUOTE);

	int end = min_pos;

	switch (min_what) {

		case MIN_COMMA: {
		} break;
		case MIN_CLOSE: {
			//end because it's done
		} break;
		case MIN_QUOTE: {
			end = str.find("\"", min_pos + 1) + 1;
			ERR_FAIL_COND_V(end == -1, Variant());

		} break;
		case MIN_PARENTHESIS: {

			end = str.find(")", min_pos + 1) + 1;
			ERR_FAIL_COND_V(end == -1, Variant());

		} break;
		case MIN_OPEN: {
			int level = 1;
			end++;
			while (end < close_pos) {

				if (str[end] == '[')
					level++;
				if (str[end] == ']') {
					level--;
					if (level == 0)
						break;
				}
				end++;
			}
			ERR_FAIL_COND_V(level != 0, Variant());
			end++;
		} break;
		case MIN_CURLY_OPEN: {
			int level = 1;
			end++;
			while (end < close_pos) {

				if (str[end] == '{')
					level++;
				if (str[end] == '}') {
					level--;
					if (level == 0)
						break;
				}
				end++;
			}
			ERR_FAIL_COND_V(level != 0, Variant());
			end++;
		} break;
	}

	String ret = str.substr(pos, end - pos);

	pos = end;
	while (pos < close_pos) {
		if (str[pos] != ',' && str[pos] != ' ' && str[pos] != ':')
			break;
		pos++;
	}

	return ret;
}

static Variant _decode_variant(const String &p_string) {

	String str = p_string.strip_edges();

	if (str.nocasecmp_to("true") == 0)
		return Variant(true);
	if (str.nocasecmp_to("false") == 0)
		return Variant(false);
	if (str.nocasecmp_to("nil") == 0)
		return Variant();
	if (str.is_valid_float()) {
		if (str.find(".") == -1)
			return str.to_int();
		else
			return str.to_double();
	}
	if (str.begins_with("#")) { //string
		return Color::html(str);
	}
	if (str.begins_with("\"")) { //string
		int end = str.find_last("\"");
		ERR_FAIL_COND_V(end == 0, Variant());
		return str.substr(1, end - 1).xml_unescape();
	}

	if (str.begins_with("[")) { //array

		int close_pos = str.find_last("]");
		ERR_FAIL_COND_V(close_pos == -1, Variant());
		Array array;

		int pos = 1;

		while (pos < close_pos) {

			String s = _get_chunk(str, pos, close_pos);
			array.push_back(_decode_variant(s));
		}
		return array;
	}

	if (str.begins_with("{")) { //array

		int close_pos = str.find_last("}");
		ERR_FAIL_COND_V(close_pos == -1, Variant());
		Dictionary d;

		int pos = 1;

		while (pos < close_pos) {

			String key = _get_chunk(str, pos, close_pos);
			String data = _get_chunk(str, pos, close_pos);
			d[_decode_variant(key)] = _decode_variant(data);
		}
		return d;
	}
	if (str.begins_with("key")) {
		Vector<String> params = _decode_params(p_string);
		ERR_FAIL_COND_V(params.size() != 1 && params.size() != 2, Variant());
		int scode = 0;

		if (params[0].is_numeric()) {
			scode = params[0].to_int();
			if (scode < 10)
				scode += KEY_0;
		} else
			scode = find_keycode(params[0]);

		InputEvent ie;
		ie.type = InputEvent::KEY;
		ie.key.scancode = scode;

		if (params.size() == 2) {
			String mods = params[1];
			if (mods.findn("C") != -1)
				ie.key.mod.control = true;
			if (mods.findn("A") != -1)
				ie.key.mod.alt = true;
			if (mods.findn("S") != -1)
				ie.key.mod.shift = true;
			if (mods.findn("M") != -1)
				ie.key.mod.meta = true;
		}
		return ie;
	}

	if (str.begins_with("mbutton")) {
		Vector<String> params = _decode_params(p_string);
		ERR_FAIL_COND_V(params.size() != 2, Variant());

		InputEvent ie;
		ie.type = InputEvent::MOUSE_BUTTON;
		ie.device = params[0].to_int();
		ie.mouse_button.button_index = params[1].to_int();

		return ie;
	}

	if (str.begins_with("jbutton")) {
		Vector<String> params = _decode_params(p_string);
		ERR_FAIL_COND_V(params.size() != 2, Variant());

		InputEvent ie;
		ie.type = InputEvent::JOYSTICK_BUTTON;
		ie.device = params[0].to_int();
		ie.joy_button.button_index = params[1].to_int();

		return ie;
	}

	if (str.begins_with("jaxis")) {
		Vector<String> params = _decode_params(p_string);
		ERR_FAIL_COND_V(params.size() != 2, Variant());

		InputEvent ie;
		ie.type = InputEvent::JOYSTICK_MOTION;
		ie.device = params[0].to_int();
		int axis = params[1].to_int();
		;
		ie.joy_motion.axis = axis >> 1;
		ie.joy_motion.axis_value = axis & 1 ? 1 : -1;

		return ie;
	}
	if (str.begins_with("img")) {
		Vector<String> params = _decode_params(p_string);
		if (params.size() == 0) {
			return Image();
		}

		ERR_FAIL_COND_V(params.size() != 5, Image());

		String format = params[0].strip_edges();

		Image::Format imgformat;

		if (format == "grayscale") {
			imgformat = Image::FORMAT_GRAYSCALE;
		} else if (format == "intensity") {
			imgformat = Image::FORMAT_INTENSITY;
		} else if (format == "grayscale_alpha") {
			imgformat = Image::FORMAT_GRAYSCALE_ALPHA;
		} else if (format == "rgb") {
			imgformat = Image::FORMAT_RGB;
		} else if (format == "rgba") {
			imgformat = Image::FORMAT_RGBA;
		} else if (format == "indexed") {
			imgformat = Image::FORMAT_INDEXED;
		} else if (format == "indexed_alpha") {
			imgformat = Image::FORMAT_INDEXED_ALPHA;
		} else if (format == "bc1") {
			imgformat = Image::FORMAT_BC1;
		} else if (format == "bc2") {
			imgformat = Image::FORMAT_BC2;
		} else if (format == "bc3") {
			imgformat = Image::FORMAT_BC3;
		} else if (format == "bc4") {
			imgformat = Image::FORMAT_BC4;
		} else if (format == "bc5") {
			imgformat = Image::FORMAT_BC5;
		} else if (format == "custom") {
			imgformat = Image::FORMAT_CUSTOM;
		} else {

			ERR_FAIL_V(Image());
		}

		int mipmaps = params[1].to_int();
		int w = params[2].to_int();
		int h = params[3].to_int();

		if (w == 0 && h == 0) {
			//r_v = Image(w, h, imgformat);
			return Image();
		};

		String data = params[4];
		int datasize = data.length() / 2;
		DVector<uint8_t> pixels;
		pixels.resize(datasize);
		DVector<uint8_t>::Write wb = pixels.write();
		const CharType *cptr = data.c_str();

		int idx = 0;
		uint8_t byte;
		while (idx < datasize * 2) {

			CharType c = *(cptr++);

			ERR_FAIL_COND_V(c == '<', ERR_FILE_CORRUPT);

			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {

				if (idx & 1) {

					byte |= HEX2CHR(c);
					wb[idx >> 1] = byte;
				} else {

					byte = HEX2CHR(c) << 4;
				}

				idx++;
			}
		}

		wb = DVector<uint8_t>::Write();

		return Image(w, h, mipmaps, imgformat, pixels);
	}

	if (str.find(",") != -1) { //vector2 or vector3
		// Since the data could be stored as Vector2(0, 0)
		// We need to first remove any string from the data.
		// To do that, we split the data using '(' as delimiter
		// and keep the last element only.
		// Then using the "split_floats" function should work like a charm
		Vector<String> sarr = str.split("(", true);
		Vector<float> farr = sarr[sarr.size() - 1].split_floats(",", true);
		if (farr.size() == 2) {
			return Vector2(farr[0], farr[1]);
		}
		if (farr.size() == 3) {
			return Vector3(farr[0], farr[1], farr[2]);
		}
		ERR_FAIL_V(Variant());
	}

	return Variant();
}

void Globals::set_registering_order(bool p_enable) {

	registering_order = p_enable;
}

Error Globals::_load_settings_binary(const String p_path) {

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	uint8_t hdr[4];
	f->get_buffer(hdr, 4);
	if (hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G') {

		memdelete(f);
		ERR_EXPLAIN("Corrupted header in binary engine.cfb (not ECFG)");
		ERR_FAIL_V(ERR_FILE_CORRUPT;)
	}

	set_registering_order(false);

	uint32_t count = f->get_32();

	for (int i = 0; i < count; i++) {

		uint32_t slen = f->get_32();
		CharString cs;
		cs.resize(slen + 1);
		cs[slen] = 0;
		f->get_buffer((uint8_t *)cs.ptr(), slen);
		String key;
		key.parse_utf8(cs.ptr());

		uint32_t vlen = f->get_32();
		Vector<uint8_t> d;
		d.resize(vlen);
		f->get_buffer(d.ptr(), vlen);
		Variant value;
		Error err = decode_variant(value, d.ptr(), d.size());
		ERR_EXPLAIN("Error decoding property: " + key);
		ERR_CONTINUE(err != OK);
		set(key, value);
		set_persisting(key, true);
	}

	set_registering_order(true);

	return OK;
}
Error Globals::_load_settings(const String p_path) {

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (err != OK) {

		return err;
	}

	String line;
	String section;
	String subpath;

	set_registering_order(false);

	int line_count = 0;

	while (!f->eof_reached()) {

		String line = f->get_line().strip_edges();
		line_count++;

		if (line == "")
			continue;

		// find comments

		{

			int pos = 0;
			while (true) {
				int ret = line.find(";", pos);
				if (ret == -1)
					break;

				int qc = 0;
				for (int i = 0; i < ret; i++) {

					if (line[i] == '"')
						qc++;
				}

				if (!(qc & 1)) {
					//not inside string, real comment
					line = line.substr(0, ret);
					break;
				}

				pos = ret + 1;
			}
		}

		if (line.begins_with("[")) {

			int end = line.find_last("]");
			ERR_CONTINUE(end != line.length() - 1);

			String section = line.substr(1, line.length() - 2);

			if (section == "global" || section == "")
				subpath = "";
			else
				subpath = section + "/";

		} else if (line.find("=") != -1) {

			int eqpos = line.find("=");
			String var = line.substr(0, eqpos).strip_edges();
			String value = line.substr(eqpos + 1, line.length()).strip_edges();

			Variant val = _decode_variant(value);

			set(subpath + var, val);
			set_persisting(subpath + var, true);
			//props[subpath+var]=VariantContainer(val,last_order++,true);

		} else {

			if (line.length() > 0) {
				ERR_PRINT(String("Syntax error on line " + itos(line_count) + " of file " + p_path).ascii().get_data());
			};
		};
	}

	memdelete(f);

	set_registering_order(true);

	return OK;
}

static String _encode_variant(const Variant &p_variant) {

	switch (p_variant.get_type()) {

		case Variant::BOOL: {
			bool val = p_variant;
			return (val ? "true" : "false");
		} break;
		case Variant::INT: {
			int val = p_variant;
			return itos(val);
		} break;
		case Variant::REAL: {
			float val = p_variant;
			return rtos(val) + (val == int(val) ? ".0" : "");
		} break;
		case Variant::VECTOR2: {
			Vector2 val = p_variant;
			return String("Vector2(") + rtos(val.x) + String(", ") + rtos(val.y) + String(")");
		} break;
		case Variant::VECTOR3: {
			Vector3 val = p_variant;
			return String("Vector3(") + rtos(val.x) + String(", ") + rtos(val.y) + String(", ") + rtos(val.z) + String(")");
		} break;
		case Variant::STRING: {
			String val = p_variant;
			return "\"" + val.xml_escape() + "\"";
		} break;
		case Variant::COLOR: {

			Color val = p_variant;
			return "#" + val.to_html();
		} break;
		case Variant::STRING_ARRAY:
		case Variant::INT_ARRAY:
		case Variant::REAL_ARRAY:
		case Variant::ARRAY: {
			Array arr = p_variant;
			String str = "[";
			for (int i = 0; i < arr.size(); i++) {

				if (i > 0)
					str += ", ";
				str += _encode_variant(arr[i]);
			}
			str += "]";
			return str;
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			String str = "{";
			List<Variant> keys;
			d.get_key_list(&keys);
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				if (E != keys.front())
					str += ", ";
				str += _encode_variant(E->get());
				str += ":";
				str += _encode_variant(d[E->get()]);
			}
			str += "}";
			return str;
		} break;
		case Variant::IMAGE: {
			String str = "img(";

			Image img = p_variant;
			if (!img.empty()) {

				String format;
				switch (img.get_format()) {

					case Image::FORMAT_GRAYSCALE: format = "grayscale"; break;
					case Image::FORMAT_INTENSITY: format = "intensity"; break;
					case Image::FORMAT_GRAYSCALE_ALPHA: format = "grayscale_alpha"; break;
					case Image::FORMAT_RGB: format = "rgb"; break;
					case Image::FORMAT_RGBA: format = "rgba"; break;
					case Image::FORMAT_INDEXED: format = "indexed"; break;
					case Image::FORMAT_INDEXED_ALPHA: format = "indexed_alpha"; break;
					case Image::FORMAT_BC1: format = "bc1"; break;
					case Image::FORMAT_BC2: format = "bc2"; break;
					case Image::FORMAT_BC3: format = "bc3"; break;
					case Image::FORMAT_BC4: format = "bc4"; break;
					case Image::FORMAT_BC5: format = "bc5"; break;
					case Image::FORMAT_CUSTOM: format = "custom custom_size=" + itos(img.get_data().size()) + ""; break;
					default: {}
				}

				str += format + ", ";
				str += itos(img.get_mipmaps()) + ", ";
				str += itos(img.get_width()) + ", ";
				str += itos(img.get_height()) + ", ";
				DVector<uint8_t> data = img.get_data();
				int ds = data.size();
				DVector<uint8_t>::Read r = data.read();
				for (int i = 0; i < ds; i++) {
					uint8_t byte = r[i];
					const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
					char bstr[3] = { hex[byte >> 4], hex[byte & 0xF], 0 };
					str += bstr;
				}
			}
			str += ")";
			return str;
		} break;
		case Variant::INPUT_EVENT: {

			InputEvent ev = p_variant;

			switch (ev.type) {

				case InputEvent::KEY: {

					String mods;
					if (ev.key.mod.control)
						mods += "C";
					if (ev.key.mod.shift)
						mods += "S";
					if (ev.key.mod.alt)
						mods += "A";
					if (ev.key.mod.meta)
						mods += "M";
					if (mods != "")
						mods = ", " + mods;

					return "key(" + keycode_get_string(ev.key.scancode) + mods + ")";
				} break;
				case InputEvent::MOUSE_BUTTON: {

					return "mbutton(" + itos(ev.device) + ", " + itos(ev.mouse_button.button_index) + ")";
				} break;
				case InputEvent::JOYSTICK_BUTTON: {

					return "jbutton(" + itos(ev.device) + ", " + itos(ev.joy_button.button_index) + ")";
				} break;
				case InputEvent::JOYSTICK_MOTION: {

					return "jaxis(" + itos(ev.device) + ", " + itos(ev.joy_motion.axis * 2 + (ev.joy_motion.axis_value < 0 ? 0 : 1)) + ")";
				} break;
				default: {

					return "nil";
				} break;
			}
		} break;
		default: {}
	}

	return "nil"; //don't know wha to do with this
}

int Globals::get_order(const String &p_name) const {

	ERR_FAIL_COND_V(!props.has(p_name), -1);
	return props[p_name].order;
}

void Globals::set_order(const String &p_name, int p_order) {

	ERR_FAIL_COND(!props.has(p_name));
	props[p_name].order = p_order;
}

void Globals::clear(const String &p_name) {

	ERR_FAIL_COND(!props.has(p_name));
	props.erase(p_name);
}

Error Globals::save() {

	return save_custom(get_resource_path() + "/engine.cfg");
}

Error Globals::_save_settings_binary(const String &p_file, const Map<String, List<String> > &props, const CustomMap &p_custom) {

	Error err;
	FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);
	if (err != OK) {

		ERR_EXPLAIN("Coudln't save engine.cfb at " + p_file);
		ERR_FAIL_COND_V(err, err)
	}

	uint8_t hdr[4] = { 'E', 'C', 'F', 'G' };
	file->store_buffer(hdr, 4);

	int count = 0;

	for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			count++;
		}
	}

	file->store_32(count); //store how many properties are saved

	for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			String key = F->get();
			if (E->key() != "")
				key = E->key() + "/" + key;
			Variant value;
			if (p_custom.has(key))
				value = p_custom[key];
			else
				value = get(key);

			file->store_32(key.length());
			file->store_string(key);

			int len;
			Error err = encode_variant(value, NULL, len);
			if (err != OK)
				memdelete(file);
			ERR_FAIL_COND_V(err != OK, ERR_INVALID_DATA);

			Vector<uint8_t> buff;
			buff.resize(len);

			err = encode_variant(value, &buff[0], len);
			if (err != OK)
				memdelete(file);
			ERR_FAIL_COND_V(err != OK, ERR_INVALID_DATA);
			file->store_32(len);
			file->store_buffer(buff.ptr(), buff.size());
		}
	}

	file->close();
	memdelete(file);

	return OK;
}

Error Globals::_save_settings_text(const String &p_file, const Map<String, List<String> > &props, const CustomMap &p_custom) {

	Error err;
	FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);

	if (err) {
		ERR_EXPLAIN("Coudln't save engine.cfg - " + p_file);
		ERR_FAIL_COND_V(err, err)
	}

	for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

		if (E != props.front())
			file->store_string("\n");

		if (E->key() != "")
			file->store_string("[" + E->key() + "]\n\n");
		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			String key = F->get();
			if (E->key() != "")
				key = E->key() + "/" + key;
			Variant value;
			if (p_custom.has(key))
				value = p_custom[key];
			else
				value = get(key);

			file->store_string(F->get() + "=" + _encode_variant(value) + "\n");
		}
	}

	file->close();
	memdelete(file);

	return OK;
}

Error Globals::_save_custom_bnd(const String &p_file) { // add other params as dictionary and array?

	return save_custom(p_file);
};

Error Globals::save_custom(const String &p_path, const CustomMap &p_custom, const Set<String> &p_ignore_masks) {

	ERR_FAIL_COND_V(p_path == "", ERR_INVALID_PARAMETER);

	Set<_VCSort> vclist;

	for (Map<StringName, VariantContainer>::Element *G = props.front(); G; G = G->next()) {

		const VariantContainer *v = &G->get();

		if (v->hide_from_editor)
			continue;

		if (p_custom.has(G->key()))
			continue;

		bool discard = false;

		for (const Set<String>::Element *E = p_ignore_masks.front(); E; E = E->next()) {

			if (String(G->key()).match(E->get())) {
				discard = true;
				break;
			}
		}

		if (discard)
			continue;

		_VCSort vc;
		vc.name = G->key(); //*k;
		vc.order = v->order;
		vc.type = v->variant.get_type();
		vc.flags = PROPERTY_USAGE_CHECKABLE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		if (!v->persist)
			continue;

		vclist.insert(vc);
	}

	for (const Map<String, Variant>::Element *E = p_custom.front(); E; E = E->next()) {

		_VCSort vc;
		vc.name = E->key();
		vc.order = 0xFFFFFFF;
		vc.type = E->get().get_type();
		vc.flags = PROPERTY_USAGE_STORAGE;
		vclist.insert(vc);
	}

	Map<String, List<String> > props;

	for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

		String category = E->get().name;
		String name = E->get().name;

		int div = category.find("/");

		if (div < 0)
			category = "";
		else {

			category = category.substr(0, div);
			name = name.substr(div + 1, name.size());
		}
		props[category].push_back(name);
	}

	if (p_path.ends_with(".cfg"))
		return _save_settings_text(p_path, props, p_custom);
	else if (p_path.ends_with(".cfb"))
		return _save_settings_binary(p_path, props, p_custom);
	else {

		ERR_EXPLAIN("Unknown config file format: " + p_path);
		ERR_FAIL_V(ERR_FILE_UNRECOGNIZED);
	}

	return OK;

#if 0
	Error err = file->open(dst_file,FileAccess::WRITE);
	if (err) {
		memdelete(file);
		ERR_EXPLAIN("Coudln't save engine.cfg");
		ERR_FAIL_COND_V(err,err)
	}


	for(Map<String,List<String> >::Element *E=props.front();E;E=E->next()) {

		if (E!=props.front())
			file->store_string("\n");

		if (E->key()!="")
			file->store_string("["+E->key()+"]\n\n");
		for(List<String>::Element *F=E->get().front();F;F=F->next()) {

			String key = F->get();
			if (E->key()!="")
				key=E->key()+"/"+key;
			Variant value;

			if (p_custom.has(key))
				value=p_custom[key];
			else
				value = get(key);

			file->store_string(F->get()+"="+_encode_variant(value)+"\n");

		}
	}

	file->close();
	memdelete(file);


	return OK;
#endif
}

Variant _GLOBAL_DEF(const String &p_var, const Variant &p_default) {

	if (Globals::get_singleton()->has(p_var))
		return Globals::get_singleton()->get(p_var);
	Globals::get_singleton()->set(p_var, p_default);
	return p_default;
}

void Globals::add_singleton(const Singleton &p_singleton) {

	singletons.push_back(p_singleton);
}

Object *Globals::get_singleton_object(const String &p_name) const {

	for (const List<Singleton>::Element *E = singletons.front(); E; E = E->next()) {
		if (E->get().name == p_name) {
			return E->get().ptr;
		};
	};

	return NULL;
};

bool Globals::has_singleton(const String &p_name) const {

	return get_singleton_object(p_name) != NULL;
};

void Globals::get_singletons(List<Singleton> *p_singletons) {

	for (List<Singleton>::Element *E = singletons.front(); E; E = E->next())
		p_singletons->push_back(E->get());
}

Vector<String> Globals::get_optimizer_presets() const {

	List<PropertyInfo> pi;
	Globals::get_singleton()->get_property_list(&pi);
	Vector<String> names;

	for (List<PropertyInfo>::Element *E = pi.front(); E; E = E->next()) {

		if (!E->get().name.begins_with("optimizer_presets/"))
			continue;
		names.push_back(E->get().name.get_slicec('/', 1));
	}

	names.sort();

	return names;
}

void Globals::_add_property_info_bind(const Dictionary &p_info) {

	ERR_FAIL_COND(!p_info.has("name"));
	ERR_FAIL_COND(!p_info.has("type"));

	PropertyInfo pinfo;
	pinfo.name = p_info["name"];
	ERR_FAIL_COND(!props.has(pinfo.name));
	pinfo.type = Variant::Type(p_info["type"].operator int());
	ERR_FAIL_INDEX(pinfo.type, Variant::VARIANT_MAX);

	if (p_info.has("hint"))
		pinfo.hint = PropertyHint(p_info["hint"].operator int());
	if (p_info.has("hint_string"))
		pinfo.hint_string = p_info["hint_string"];

	set_custom_property_info(pinfo.name, pinfo);
}

void Globals::set_custom_property_info(const String &p_prop, const PropertyInfo &p_info) {

	ERR_FAIL_COND(!props.has(p_prop));
	custom_prop_info[p_prop] = p_info;
}

void Globals::set_disable_platform_override(bool p_disable) {

	disable_platform_override = p_disable;
}

bool Globals::is_using_datapack() const {

	return using_datapack;
}

void Globals::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("has", "name"), &Globals::has);
	ObjectTypeDB::bind_method(_MD("set_order", "name", "pos"), &Globals::set_order);
	ObjectTypeDB::bind_method(_MD("get_order", "name"), &Globals::get_order);
	ObjectTypeDB::bind_method(_MD("set_persisting", "name", "enable"), &Globals::set_persisting);
	ObjectTypeDB::bind_method(_MD("is_persisting", "name"), &Globals::is_persisting);
	ObjectTypeDB::bind_method(_MD("add_property_info", "hint"), &Globals::_add_property_info_bind);
	ObjectTypeDB::bind_method(_MD("clear", "name"), &Globals::clear);
	ObjectTypeDB::bind_method(_MD("localize_path", "path"), &Globals::localize_path);
	ObjectTypeDB::bind_method(_MD("globalize_path", "path"), &Globals::globalize_path);
	ObjectTypeDB::bind_method(_MD("save"), &Globals::save);
	ObjectTypeDB::bind_method(_MD("has_singleton", "name"), &Globals::has_singleton);
	ObjectTypeDB::bind_method(_MD("get_singleton", "name"), &Globals::get_singleton_object);
	ObjectTypeDB::bind_method(_MD("load_resource_pack", "pack"), &Globals::_load_resource_pack);

	ObjectTypeDB::bind_method(_MD("save_custom", "file"), &Globals::_save_custom_bnd);
}

Globals::Globals() {

	singleton = this;
	last_order = 0;
	disable_platform_override = false;
	registering_order = true;

	Array va;
	InputEvent key;
	key.type = InputEvent::KEY;
	InputEvent joyb;
	joyb.type = InputEvent::JOYSTICK_BUTTON;

	set("application/name", "");
	set("application/main_scene", "");
	custom_prop_info["application/main_scene"] = PropertyInfo(Variant::STRING, "application/main_scene", PROPERTY_HINT_FILE, "tscn,scn,xscn,xml,res");
	set("application/disable_stdout", false);
	set("application/use_shared_user_dir", true);

	key.key.scancode = KEY_RETURN;
	va.push_back(key);
	key.key.scancode = KEY_ENTER;
	va.push_back(key);
	key.key.scancode = KEY_SPACE;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_BUTTON_0;
	va.push_back(joyb);
	set("input/ui_accept", va);
	input_presets.push_back("input/ui_accept");

	va = Array();
	key.key.scancode = KEY_SPACE;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_BUTTON_3;
	va.push_back(joyb);
	set("input/ui_select", va);
	input_presets.push_back("input/ui_select");

	va = Array();
	key.key.scancode = KEY_ESCAPE;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_BUTTON_1;
	va.push_back(joyb);
	set("input/ui_cancel", va);
	input_presets.push_back("input/ui_cancel");

	va = Array();
	key.key.scancode = KEY_TAB;
	va.push_back(key);
	set("input/ui_focus_next", va);
	input_presets.push_back("input/ui_focus_next");

	va = Array();
	key.key.scancode = KEY_TAB;
	key.key.mod.shift = true;
	va.push_back(key);
	set("input/ui_focus_prev", va);
	input_presets.push_back("input/ui_focus_prev");
	key.key.mod.shift = false;

	va = Array();
	key.key.scancode = KEY_LEFT;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_DPAD_LEFT;
	va.push_back(joyb);
	set("input/ui_left", va);
	input_presets.push_back("input/ui_left");

	va = Array();
	key.key.scancode = KEY_RIGHT;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_DPAD_RIGHT;
	va.push_back(joyb);
	set("input/ui_right", va);
	input_presets.push_back("input/ui_right");

	va = Array();
	key.key.scancode = KEY_UP;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_DPAD_UP;
	va.push_back(joyb);
	set("input/ui_up", va);
	input_presets.push_back("input/ui_up");

	va = Array();
	key.key.scancode = KEY_DOWN;
	va.push_back(key);
	joyb.joy_button.button_index = JOY_DPAD_DOWN;
	va.push_back(joyb);
	set("input/ui_down", va);
	input_presets.push_back("input/ui_down");

	va = Array();
	key.key.scancode = KEY_PAGEUP;
	va.push_back(key);
	set("input/ui_page_up", va);
	input_presets.push_back("input/ui_page_up");

	va = Array();
	key.key.scancode = KEY_PAGEDOWN;
	va.push_back(key);
	set("input/ui_page_down", va);
	input_presets.push_back("input/ui_page_down");

	//	set("display/orientation", "landscape");

	custom_prop_info["display/orientation"] = PropertyInfo(Variant::STRING, "display/orientation", PROPERTY_HINT_ENUM, "landscape,portrait,reverse_landscape,reverse_portrait,sensor_landscape,sensor_portrait,sensor");
	custom_prop_info["render/mipmap_policy"] = PropertyInfo(Variant::INT, "render/mipmap_policy", PROPERTY_HINT_ENUM, "Allow,Allow For Po2,Disallow");
	custom_prop_info["render/thread_model"] = PropertyInfo(Variant::INT, "render/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");
	custom_prop_info["physics_2d/thread_model"] = PropertyInfo(Variant::INT, "physics_2d/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");

	set("debug/profiler_max_functions", 16384);
	using_datapack = false;
}

Globals::~Globals() {

	singleton = NULL;
}
