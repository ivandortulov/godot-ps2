/*************************************************************************/
/*  editor_data.cpp                                                      */
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
#include "editor_data.h"
#include "editor_node.h"
#include "editor_settings.h"
#include "globals.h"
#include "io/resource_loader.h"
#include "os/dir_access.h"
#include "os/file_access.h"
#include "scene/resources/packed_scene.h"

void EditorHistory::_cleanup_history() {

	for (int i = 0; i < history.size(); i++) {

		bool fail = false;

		for (int j = 0; j < history[i].path.size(); j++) {
			if (!history[i].path[j].ref.is_null())
				continue;

			if (ObjectDB::get_instance(history[i].path[j].object))
				continue; //has isntance, try next

			if (j <= history[i].level) {
				//before or equal level, complete fail
				fail = true;
			} else {
				//after level, clip
				history[i].path.resize(j);
			}

			break;
		}

		if (fail) {
			history.remove(i);
			i--;
		}
	}

	if (current >= history.size())
		current = history.size() - 1;
}

void EditorHistory::_add_object(ObjectID p_object, const String &p_property, int p_level_change) {

	Object *obj = ObjectDB::get_instance(p_object);
	ERR_FAIL_COND(!obj);
	Reference *r = obj->cast_to<Reference>();
	Obj o;
	if (r)
		o.ref = REF(r);
	o.object = p_object;
	o.property = p_property;

	History h;

	bool has_prev = current >= 0 && current < history.size();

	if (has_prev) {

		if (obj->is_type("ScriptEditorDebuggerInspectedObject")) {
			for (int i = current; i >= 0; i--) {
				Object *pre_obj = ObjectDB::get_instance(get_history_obj(i));
				if (pre_obj != NULL && pre_obj->is_type("ScriptEditorDebuggerInspectedObject")) {
					if (pre_obj->call("get_remote_object_id") == obj->call("get_remote_object_id")) {
						History &pr = history[i];
						pr.path[pr.path.size() - 1] = o;
						current = i;
						return;
					}
				}
			}
		}
		history.resize(current + 1); //clip history to next
	}

	if (p_property != "" && has_prev) {
		//add a sub property
		History &pr = history[current];
		h = pr;
		h.path.resize(h.level + 1);
		h.path.push_back(o);
		h.level++;
	} else if (p_level_change != -1 && has_prev) {
		//add a sub property
		History &pr = history[current];
		h = pr;
		ERR_FAIL_INDEX(p_level_change, h.path.size());
		h.level = p_level_change;
	} else {
		//add a new node
		h.path.push_back(o);
		h.level = 0;
	}

	history.push_back(h);
	current++;
}

void EditorHistory::add_object(ObjectID p_object) {

	_add_object(p_object, "", -1);
}

void EditorHistory::add_object(ObjectID p_object, const String &p_subprop) {

	_add_object(p_object, p_subprop, -1);
}

void EditorHistory::add_object(ObjectID p_object, int p_relevel) {

	_add_object(p_object, "", p_relevel);
}

int EditorHistory::get_history_len() {
	return history.size();
}
int EditorHistory::get_history_pos() {
	return current;
}

ObjectID EditorHistory::get_history_obj(int p_obj) const {
	ERR_FAIL_INDEX_V(p_obj, history.size(), 0);
	ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), 0);
	return history[p_obj].path[history[p_obj].level].object;
}

bool EditorHistory::is_at_begining() const {
	return current <= 0;
}
bool EditorHistory::is_at_end() const {

	return ((current + 1) >= history.size());
}

bool EditorHistory::next() {

	_cleanup_history();

	if ((current + 1) < history.size())
		current++;
	else
		return false;

	return true;
}

bool EditorHistory::previous() {

	_cleanup_history();

	if (current > 0)
		current--;
	else
		return false;

	return true;
}

ObjectID EditorHistory::get_current() {

	if (current < 0 || current >= history.size())
		return 0;

	History &h = history[current];
	Object *obj = ObjectDB::get_instance(h.path[h.level].object);
	if (!obj)
		return 0;

	return obj->get_instance_ID();
}

int EditorHistory::get_path_size() const {

	if (current < 0 || current >= history.size())
		return 0;

	const History &h = history[current];
	return h.path.size();
}

ObjectID EditorHistory::get_path_object(int p_index) const {

	if (current < 0 || current >= history.size())
		return 0;

	const History &h = history[current];

	ERR_FAIL_INDEX_V(p_index, h.path.size(), 0);

	Object *obj = ObjectDB::get_instance(h.path[p_index].object);
	if (!obj)
		return 0;

	return obj->get_instance_ID();
}

String EditorHistory::get_path_property(int p_index) const {

	if (current < 0 || current >= history.size())
		return "";

	const History &h = history[current];

	ERR_FAIL_INDEX_V(p_index, h.path.size(), "");

	return h.path[p_index].property;
}

void EditorHistory::clear() {

	history.clear();
	current = -1;
}

EditorHistory::EditorHistory() {

	current = -1;
}

EditorPlugin *EditorData::get_editor(Object *p_object) {

	for (int i = 0; i < editor_plugins.size(); i++) {

		if (editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object))
			return editor_plugins[i];
	}

	return NULL;
}

EditorPlugin *EditorData::get_subeditor(Object *p_object) {

	for (int i = 0; i < editor_plugins.size(); i++) {

		if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object))
			return editor_plugins[i];
	}

	return NULL;
}

Vector<EditorPlugin *> EditorData::get_subeditors(Object *p_object) {
	Vector<EditorPlugin *> sub_plugins;
	for (int i = 0; i < editor_plugins.size(); i++) {
		if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			sub_plugins.push_back(editor_plugins[i]);
		}
	}
	return sub_plugins;
}

EditorPlugin *EditorData::get_editor(String p_name) {

	for (int i = 0; i < editor_plugins.size(); i++) {

		if (editor_plugins[i]->get_name() == p_name)
			return editor_plugins[i];
	}

	return NULL;
}

void EditorData::copy_object_params(Object *p_object) {

	clipboard.clear();

	List<PropertyInfo> pinfo;
	p_object->get_property_list(&pinfo);

	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {

		if (!(E->get().usage & PROPERTY_USAGE_EDITOR))
			continue;

		PropertyData pd;
		pd.name = E->get().name;
		pd.value = p_object->get(pd.name);
		clipboard.push_back(pd);
	}
}

void EditorData::get_editor_breakpoints(List<String> *p_breakpoints) {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->get_breakpoints(p_breakpoints);
	}
}

Dictionary EditorData::get_editor_states() const {

	Dictionary metadata;
	for (int i = 0; i < editor_plugins.size(); i++) {

		Dictionary state = editor_plugins[i]->get_state();
		if (state.empty())
			continue;
		metadata[editor_plugins[i]->get_name()] = state;
	}

	return metadata;
}

Dictionary EditorData::get_scene_editor_states(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Dictionary());
	EditedScene es = edited_scene[p_idx];
	return es.editor_states;
}

void EditorData::set_editor_states(const Dictionary &p_states) {

	List<Variant> keys;
	p_states.get_key_list(&keys);

	List<Variant>::Element *E = keys.front();
	for (; E; E = E->next()) {

		String name = E->get();
		int idx = -1;
		for (int i = 0; i < editor_plugins.size(); i++) {

			if (editor_plugins[i]->get_name() == name) {
				idx = i;
				break;
			}
		}

		if (idx == -1)
			continue;
		editor_plugins[idx]->set_state(p_states[name]);
	}
}

void EditorData::notify_edited_scene_changed() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->edited_scene_changed();
	}
}

void EditorData::clear_editor_states() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->clear();
	}
}

void EditorData::save_editor_external_data() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->save_external_data();
	}
}

void EditorData::apply_changes_in_editors() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->apply_changes();
	}
}

void EditorData::save_editor_global_states() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->save_global_state();
	}
}

void EditorData::restore_editor_global_states() {

	for (int i = 0; i < editor_plugins.size(); i++) {

		editor_plugins[i]->restore_global_state();
	}
}

void EditorData::paste_object_params(Object *p_object) {

	for (List<PropertyData>::Element *E = clipboard.front(); E; E = E->next()) {

		p_object->set(E->get().name, E->get().value);
	}
}

UndoRedo &EditorData::get_undo_redo() {

	return undo_redo;
}

void EditorData::remove_editor_plugin(EditorPlugin *p_plugin) {

	p_plugin->undo_redo = NULL;
	editor_plugins.erase(p_plugin);
}

void EditorData::add_editor_plugin(EditorPlugin *p_plugin) {

	p_plugin->undo_redo = &undo_redo;
	editor_plugins.push_back(p_plugin);
}

int EditorData::get_editor_plugin_count() const {
	return editor_plugins.size();
}
EditorPlugin *EditorData::get_editor_plugin(int p_idx) {

	ERR_FAIL_INDEX_V(p_idx, editor_plugins.size(), NULL);
	return editor_plugins[p_idx];
}

void EditorData::add_custom_type(const String &p_type, const String &p_inherits, const Ref<Script> &p_script, const Ref<Texture> &p_icon) {

	ERR_FAIL_COND(p_script.is_null());
	CustomType ct;
	ct.name = p_type;
	ct.icon = p_icon;
	ct.script = p_script;
	if (!custom_types.has(p_inherits)) {
		custom_types[p_inherits] = Vector<CustomType>();
	}

	custom_types[p_inherits].push_back(ct);
}

void EditorData::remove_custom_type(const String &p_type) {

	for (Map<String, Vector<CustomType> >::Element *E = custom_types.front(); E; E = E->next()) {

		for (int i = 0; i < E->get().size(); i++) {
			if (E->get()[i].name == p_type) {
				E->get().remove(i);
				if (E->get().empty()) {
					custom_types.erase(E->key());
				}
				return;
			}
		}
	}
}

int EditorData::add_edited_scene(int p_at_pos) {

	if (p_at_pos < 0)
		p_at_pos = edited_scene.size();
	EditedScene es;
	es.root = NULL;
	es.history_current = -1;
	es.version = 0;
	es.live_edit_root = NodePath(String("/root"));

	if (p_at_pos == edited_scene.size())
		edited_scene.push_back(es);
	else
		edited_scene.insert(p_at_pos, es);

	if (current_edited_scene < 0)
		current_edited_scene = 0;
	return p_at_pos;
}

void EditorData::move_edited_scene_index(int p_idx, int p_to_idx) {

	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	ERR_FAIL_INDEX(p_to_idx, edited_scene.size());
	SWAP(edited_scene[p_idx], edited_scene[p_to_idx]);
}
void EditorData::remove_scene(int p_idx) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	if (edited_scene[p_idx].root)
		memdelete(edited_scene[p_idx].root);

	if (current_edited_scene > p_idx)
		current_edited_scene--;
	else if (current_edited_scene == p_idx && current_edited_scene > 0) {
		current_edited_scene--;
	}

	edited_scene.remove(p_idx);
}

bool EditorData::_find_updated_instances(Node *p_root, Node *p_node, Set<String> &checked_paths) {

	//	if (p_root!=p_node && p_node->get_owner()!=p_root && !p_root->is_editable_instance(p_node->get_owner()))
	//		return false;

	Ref<SceneState> ss;

	if (p_node == p_root) {
		ss = p_node->get_scene_inherited_state();
	} else if (p_node->get_filename() != String()) {
		ss = p_node->get_scene_instance_state();
	}

	if (ss.is_valid()) {
		String path = ss->get_path();

		if (!checked_paths.has(path)) {

			uint64_t modified_time = FileAccess::get_modified_time(path);
			if (modified_time != ss->get_last_modified_time()) {
				return true; //external scene changed
			}

			checked_paths.insert(path);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {

		bool found = _find_updated_instances(p_root, p_node->get_child(i), checked_paths);
		if (found)
			return true;
	}

	return false;
}

bool EditorData::check_and_update_scene(int p_idx) {

	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), false);
	if (!edited_scene[p_idx].root)
		return false;

	Set<String> checked_scenes;

	bool must_reload = _find_updated_instances(edited_scene[p_idx].root, edited_scene[p_idx].root, checked_scenes);

	if (must_reload) {
		Ref<PackedScene> pscene;
		pscene.instance();

		EditorProgress ep("update_scene", TTR("Updating Scene"), 2);
		ep.step(TTR("Storing local changes.."), 0);
		//pack first, so it stores diffs to previous version of saved scene
		Error err = pscene->pack(edited_scene[p_idx].root);
		ERR_FAIL_COND_V(err != OK, false);
		ep.step(TTR("Updating scene.."), 1);
		Node *new_scene = pscene->instance(true);
		ERR_FAIL_COND_V(!new_scene, false);

		//transfer selection
		List<Node *> new_selection;
		for (List<Node *>::Element *E = edited_scene[p_idx].selection.front(); E; E = E->next()) {
			NodePath p = edited_scene[p_idx].root->get_path_to(E->get());
			Node *new_node = new_scene->get_node(p);
			if (new_node)
				new_selection.push_back(new_node);
		}

		new_scene->set_filename(edited_scene[p_idx].root->get_filename());

		memdelete(edited_scene[p_idx].root);
		edited_scene[p_idx].root = new_scene;
		edited_scene[p_idx].selection = new_selection;

		return true;
	}

	return false;
}

int EditorData::get_edited_scene() const {

	return current_edited_scene;
}
void EditorData::set_edited_scene(int p_idx) {

	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	current_edited_scene = p_idx;
	//swap
}
Node *EditorData::get_edited_scene_root(int p_idx) {
	if (p_idx < 0) {
		ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), NULL);
		return edited_scene[current_edited_scene].root;
	} else {
		ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), NULL);
		return edited_scene[p_idx].root;
	}
}
void EditorData::set_edited_scene_root(Node *p_root) {

	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	edited_scene[current_edited_scene].root = p_root;
}

int EditorData::get_edited_scene_count() const {

	return edited_scene.size();
}

void EditorData::set_edited_scene_version(uint64_t version, int scene_idx) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	if (scene_idx < 0) {
		edited_scene[current_edited_scene].version = version;
	} else {
		ERR_FAIL_INDEX(scene_idx, edited_scene.size());
		edited_scene[scene_idx].version = version;
	}
}

uint64_t EditorData::get_edited_scene_version() const {

	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), 0);
	return edited_scene[current_edited_scene].version;
}
uint64_t EditorData::get_scene_version(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), false);
	return edited_scene[p_idx].version;
}

String EditorData::get_scene_type(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());
	if (!edited_scene[p_idx].root)
		return "";
	return edited_scene[p_idx].root->get_type();
}
void EditorData::move_edited_scene_to_index(int p_idx) {

	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	ERR_FAIL_INDEX(p_idx, edited_scene.size());

	EditedScene es = edited_scene[current_edited_scene];
	edited_scene.remove(current_edited_scene);
	edited_scene.insert(p_idx, es);
	current_edited_scene = p_idx;
}

Ref<Script> EditorData::get_scene_root_script(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Ref<Script>());
	if (!edited_scene[p_idx].root)
		return Ref<Script>();
	Ref<Script> s = edited_scene[p_idx].root->get_script();
	if (!s.is_valid() && edited_scene[p_idx].root->get_child_count()) {
		Node *n = edited_scene[p_idx].root->get_child(0);
		while (!s.is_valid() && n && n->get_filename() == String()) {
			s = n->get_script();
			n = n->get_parent();
		}
	}
	return s;
}

String EditorData::get_scene_title(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());
	if (!edited_scene[p_idx].root)
		return "[empty]";
	if (edited_scene[p_idx].root->get_filename() == "")
		return "[unsaved]";
	return edited_scene[p_idx].root->get_filename().get_file();
}

String EditorData::get_scene_path(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());

	if (!edited_scene[p_idx].root)
		return "";
	return edited_scene[p_idx].root->get_filename();
}

void EditorData::set_edited_scene_live_edit_root(const NodePath &p_root) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

	edited_scene[current_edited_scene].live_edit_root = p_root;
}
NodePath EditorData::get_edited_scene_live_edit_root() {

	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), String());

	return edited_scene[current_edited_scene].live_edit_root;
}

void EditorData::save_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history, const Dictionary &p_custom) {

	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

	EditedScene &es = edited_scene[current_edited_scene];
	es.selection = p_selection->get_selected_node_list();
	es.history_current = p_history->current;
	es.history_stored = p_history->history;
	es.editor_states = get_editor_states();
	es.custom_state = p_custom;
}

Dictionary EditorData::restore_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history) {
	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), Dictionary());

	EditedScene &es = edited_scene[current_edited_scene];

	p_history->current = es.history_current;
	p_history->history = es.history_stored;

	p_selection->clear();
	for (List<Node *>::Element *E = es.selection.front(); E; E = E->next()) {
		p_selection->add_node(E->get());
	}
	set_editor_states(es.editor_states);

	return es.custom_state;
}

void EditorData::set_edited_scene_import_metadata(Ref<ResourceImportMetadata> p_mdata) {

	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	edited_scene[current_edited_scene].medatata = p_mdata;
}

Ref<ResourceImportMetadata> EditorData::get_edited_scene_import_metadata(int idx) const {

	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), Ref<ResourceImportMetadata>());
	if (idx < 0) {
		return edited_scene[current_edited_scene].medatata;
	} else {
		ERR_FAIL_INDEX_V(idx, edited_scene.size(), Ref<ResourceImportMetadata>());
		return edited_scene[idx].medatata;
	}
}

void EditorData::clear_edited_scenes() {

	for (int i = 0; i < edited_scene.size(); i++) {
		if (edited_scene[i].root) {
			memdelete(edited_scene[i].root);
		}
	}
	edited_scene.clear();
}

void EditorData::set_plugin_window_layout(Ref<ConfigFile> p_layout) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->set_window_layout(p_layout);
	}
}

void EditorData::get_plugin_window_layout(Ref<ConfigFile> p_layout) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->get_window_layout(p_layout);
	}
}

EditorData::EditorData() {

	current_edited_scene = -1;

	//	load_imported_scenes_from_globals();
}

///////////
void EditorSelection::_node_removed(Node *p_node) {

	if (!selection.has(p_node))
		return;

	Object *meta = selection[p_node];
	if (meta)
		memdelete(meta);
	selection.erase(p_node);
	changed = true;
	nl_changed = true;
}

void EditorSelection::add_node(Node *p_node) {

	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(!p_node->is_inside_tree());
	if (selection.has(p_node))
		return;

	changed = true;
	nl_changed = true;
	Object *meta = NULL;
	for (List<Object *>::Element *E = editor_plugins.front(); E; E = E->next()) {

		meta = E->get()->call("_get_editor_data", p_node);
		if (meta) {
			break;
		}
	}
	selection[p_node] = meta;

	p_node->connect("exit_tree", this, "_node_removed", varray(p_node), CONNECT_ONESHOT);

	//emit_signal("selection_changed");
}

void EditorSelection::remove_node(Node *p_node) {

	ERR_FAIL_NULL(p_node);

	if (!selection.has(p_node))
		return;

	changed = true;
	nl_changed = true;
	Object *meta = selection[p_node];
	if (meta)
		memdelete(meta);
	selection.erase(p_node);
	p_node->disconnect("exit_tree", this, "_node_removed");
	//emit_signal("selection_changed");
}
bool EditorSelection::is_selected(Node *p_node) const {

	return selection.has(p_node);
}

Array EditorSelection::_get_selected_nodes() {

	Array ret;

	for (List<Node *>::Element *E = selected_node_list.front(); E; E = E->next()) {

		ret.push_back(E->get());
	}

	return ret;
}

void EditorSelection::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_node_removed"), &EditorSelection::_node_removed);
	ObjectTypeDB::bind_method(_MD("clear"), &EditorSelection::clear);
	ObjectTypeDB::bind_method(_MD("add_node", "node:Node"), &EditorSelection::add_node);
	ObjectTypeDB::bind_method(_MD("remove_node", "node:Node"), &EditorSelection::remove_node);
	ObjectTypeDB::bind_method(_MD("get_selected_nodes"), &EditorSelection::_get_selected_nodes);
	ADD_SIGNAL(MethodInfo("selection_changed"));
}

void EditorSelection::add_editor_plugin(Object *p_object) {

	editor_plugins.push_back(p_object);
}

void EditorSelection::_update_nl() {

	if (!nl_changed)
		return;

	selected_node_list.clear();

	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {

		Node *parent = E->key();
		parent = parent->get_parent();
		bool skip = false;
		while (parent) {
			if (selection.has(parent)) {
				skip = true;
				break;
			}
			parent = parent->get_parent();
		}

		if (skip)
			continue;
		selected_node_list.push_back(E->key());
	}

	nl_changed = true;
}

void EditorSelection::update() {

	_update_nl();

	if (!changed)
		return;
	emit_signal("selection_changed");
	changed = false;
}

List<Node *> &EditorSelection::get_selected_node_list() {

	if (changed)
		update();
	else
		_update_nl();
	return selected_node_list;
}

void EditorSelection::clear() {

	while (!selection.empty()) {

		remove_node(selection.front()->key());
	}

	changed = true;
	nl_changed = true;
}
EditorSelection::EditorSelection() {

	changed = false;
	nl_changed = false;
}

EditorSelection::~EditorSelection() {

	clear();
}
