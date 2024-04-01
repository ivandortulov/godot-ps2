/*************************************************************************/
/*  editor_plugin.cpp                                                    */
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
#include "editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "plugins/canvas_item_editor_plugin.h"
#include "plugins/spatial_editor_plugin.h"
#include "scene/3d/camera.h"

void EditorPlugin::add_custom_type(const String &p_type, const String &p_base, const Ref<Script> &p_script, const Ref<Texture> &p_icon) {

	EditorNode::get_editor_data().add_custom_type(p_type, p_base, p_script, p_icon);
}

void EditorPlugin::remove_custom_type(const String &p_type) {

	EditorNode::get_editor_data().remove_custom_type(p_type);
}

ToolButton *EditorPlugin::add_control_to_bottom_panel(Control *p_control, const String &p_title) {

	return EditorNode::get_singleton()->add_bottom_panel_item(p_title, p_control);
}

void EditorPlugin::add_control_to_dock(DockSlot p_slot, Control *p_control) {

	ERR_FAIL_NULL(p_control);
	EditorNode::get_singleton()->add_control_to_dock(EditorNode::DockSlot(p_slot), p_control);
}

void EditorPlugin::remove_control_from_docks(Control *p_control) {

	ERR_FAIL_NULL(p_control);
	EditorNode::get_singleton()->remove_control_from_dock(p_control);
}

void EditorPlugin::remove_control_from_bottom_panel(Control *p_control) {

	ERR_FAIL_NULL(p_control);
	EditorNode::get_singleton()->remove_bottom_panel_item(p_control);
}

Control *EditorPlugin::get_editor_viewport() {

	return EditorNode::get_singleton()->get_viewport();
}

void EditorPlugin::edit_resource(const Ref<Resource> &p_resource) {

	EditorNode::get_singleton()->edit_resource(p_resource);
}

void EditorPlugin::add_control_to_container(CustomControlContainer p_location, Control *p_control) {

	switch (p_location) {

		case CONTAINER_TOOLBAR: {

			EditorNode::get_menu_hb()->add_child(p_control);
		} break;

		case CONTAINER_SPATIAL_EDITOR_MENU: {

			SpatialEditor::get_singleton()->add_control_to_menu_panel(p_control);

		} break;
		case CONTAINER_SPATIAL_EDITOR_SIDE: {

			SpatialEditor::get_singleton()->get_palette_split()->add_child(p_control);
			SpatialEditor::get_singleton()->get_palette_split()->move_child(p_control, 0);

		} break;
		case CONTAINER_SPATIAL_EDITOR_BOTTOM: {

			SpatialEditor::get_singleton()->get_shader_split()->add_child(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_MENU: {

			CanvasItemEditor::get_singleton()->add_control_to_menu_panel(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_SIDE: {

			CanvasItemEditor::get_singleton()->get_palette_split()->add_child(p_control);
			CanvasItemEditor::get_singleton()->get_palette_split()->move_child(p_control, 0);

		} break;
		case CONTAINER_CANVAS_EDITOR_BOTTOM: {

			CanvasItemEditor::get_singleton()->get_bottom_split()->add_child(p_control);

		} break;
		case CONTAINER_PROPERTY_EDITOR_BOTTOM: {

			EditorNode::get_singleton()->get_property_editor_vb()->add_child(p_control);

		} break;
	}
}

Ref<SpatialEditorGizmo> EditorPlugin::create_spatial_gizmo(Spatial *p_spatial) {
	//??
	if (get_script_instance() && get_script_instance()->has_method("create_spatial_gizmo")) {
		return get_script_instance()->call("create_spatial_gizmo", p_spatial);
	}

	return Ref<SpatialEditorGizmo>();
}

bool EditorPlugin::forward_input_event(const InputEvent &p_event) {

	if (get_script_instance() && get_script_instance()->has_method("forward_input_event")) {
		return get_script_instance()->call("forward_input_event", p_event);
	}
	return false;
}
bool EditorPlugin::forward_spatial_input_event(Camera *p_camera, const InputEvent &p_event) {

	if (get_script_instance() && get_script_instance()->has_method("forward_spatial_input_event")) {
		return get_script_instance()->call("forward_spatial_input_event", p_camera, p_event);
	}

	return false;
}
String EditorPlugin::get_name() const {

	if (get_script_instance() && get_script_instance()->has_method("get_name")) {
		return get_script_instance()->call("get_name");
	}

	return String();
}
bool EditorPlugin::has_main_screen() const {

	if (get_script_instance() && get_script_instance()->has_method("has_main_screen")) {
		return get_script_instance()->call("has_main_screen");
	}

	return false;
}
void EditorPlugin::make_visible(bool p_visible) {

	if (get_script_instance() && get_script_instance()->has_method("make_visible")) {
		get_script_instance()->call("make_visible", p_visible);
	}
}

void EditorPlugin::edit(Object *p_object) {

	if (get_script_instance() && get_script_instance()->has_method("edit")) {
		Resource *obj = p_object ? p_object->cast_to<Resource>() : NULL;
		if (obj) {
			get_script_instance()->call("edit", Ref<Resource>(obj));
		} else {
			get_script_instance()->call("edit", p_object);
		}
	}
}

bool EditorPlugin::handles(Object *p_object) const {

	if (get_script_instance() && get_script_instance()->has_method("handles")) {
		return get_script_instance()->call("handles", p_object);
	}

	return false;
}
Dictionary EditorPlugin::get_state() const {

	if (get_script_instance() && get_script_instance()->has_method("get_state")) {
		return get_script_instance()->call("get_state");
	}

	return Dictionary();
}

void EditorPlugin::set_state(const Dictionary &p_state) {

	if (get_script_instance() && get_script_instance()->has_method("set_state")) {
		get_script_instance()->call("set_state", p_state);
	}
}

void EditorPlugin::clear() {

	if (get_script_instance() && get_script_instance()->has_method("clear")) {
		get_script_instance()->call("clear");
	}
}

// if editor references external resources/scenes, save them
void EditorPlugin::save_external_data() {

	if (get_script_instance() && get_script_instance()->has_method("save_external_data")) {
		get_script_instance()->call("save_external_data");
	}
}

// if changes are pending in editor, apply them
void EditorPlugin::apply_changes() {

	if (get_script_instance() && get_script_instance()->has_method("apply_changes")) {
		get_script_instance()->call("apply_changes");
	}
}

void EditorPlugin::get_breakpoints(List<String> *p_breakpoints) {

	if (get_script_instance() && get_script_instance()->has_method("get_breakpoints")) {
		StringArray arr = get_script_instance()->call("get_breakpoints");
		for (int i = 0; i < arr.size(); i++)
			p_breakpoints->push_back(arr[i]);
	}
}
bool EditorPlugin::get_remove_list(List<Node *> *p_list) {

	return false;
}

void EditorPlugin::restore_global_state() {}
void EditorPlugin::save_global_state() {}

void EditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {

	if (get_script_instance() && get_script_instance()->has_method("set_window_layout")) {
		get_script_instance()->call("set_window_layout", p_layout);
	}
}

void EditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {

	if (get_script_instance() && get_script_instance()->has_method("get_window_layout")) {
		get_script_instance()->call("get_window_layout", p_layout);
	}
}

void EditorPlugin::queue_save_layout() const {

	EditorNode::get_singleton()->save_layout();
}

EditorSelection *EditorPlugin::get_selection() {
	return EditorNode::get_singleton()->get_editor_selection();
}

EditorSettings *EditorPlugin::get_editor_settings() {
	return EditorSettings::get_singleton();
}

void EditorPlugin::add_import_plugin(const Ref<EditorImportPlugin> &p_editor_import) {

	EditorNode::get_singleton()->add_editor_import_plugin(p_editor_import);
}

void EditorPlugin::remove_import_plugin(const Ref<EditorImportPlugin> &p_editor_import) {

	EditorNode::get_singleton()->remove_editor_import_plugin(p_editor_import);
}

void EditorPlugin::add_export_plugin(const Ref<EditorExportPlugin> &p_editor_export) {

	EditorImportExport::get_singleton()->add_export_plugin(p_editor_export);
}
void EditorPlugin::remove_export_plugin(const Ref<EditorExportPlugin> &p_editor_export) {

	EditorImportExport::get_singleton()->remove_export_plugin(p_editor_export);
}

Control *EditorPlugin::get_base_control() {

	return EditorNode::get_singleton()->get_gui_base();
}

void EditorPlugin::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_control_to_container", "container", "control:Control"), &EditorPlugin::add_control_to_container);
	ObjectTypeDB::bind_method(_MD("add_control_to_bottom_panel:ToolButton", "control:Control", "title"), &EditorPlugin::add_control_to_bottom_panel);
	ObjectTypeDB::bind_method(_MD("add_control_to_dock", "slot", "control:Control"), &EditorPlugin::add_control_to_dock);
	ObjectTypeDB::bind_method(_MD("remove_control_from_docks", "control:Control"), &EditorPlugin::remove_control_from_docks);
	ObjectTypeDB::bind_method(_MD("remove_control_from_bottom_panel", "control:Control"), &EditorPlugin::remove_control_from_bottom_panel);
	ObjectTypeDB::bind_method(_MD("add_custom_type", "type", "base", "script:Script", "icon:Texture"), &EditorPlugin::add_custom_type);
	ObjectTypeDB::bind_method(_MD("remove_custom_type", "type"), &EditorPlugin::remove_custom_type);
	ObjectTypeDB::bind_method(_MD("get_editor_viewport:Control"), &EditorPlugin::get_editor_viewport);

	ObjectTypeDB::bind_method(_MD("add_import_plugin", "plugin:EditorImportPlugin"), &EditorPlugin::add_import_plugin);
	ObjectTypeDB::bind_method(_MD("remove_import_plugin", "plugin:EditorImportPlugin"), &EditorPlugin::remove_import_plugin);

	ObjectTypeDB::bind_method(_MD("add_export_plugin", "plugin:EditorExportPlugin"), &EditorPlugin::add_export_plugin);
	ObjectTypeDB::bind_method(_MD("remove_export_plugin", "plugin:EditorExportPlugin"), &EditorPlugin::remove_export_plugin);

	ObjectTypeDB::bind_method(_MD("get_base_control:Control"), &EditorPlugin::get_base_control);
	ObjectTypeDB::bind_method(_MD("get_undo_redo:UndoRedo"), &EditorPlugin::_get_undo_redo);
	ObjectTypeDB::bind_method(_MD("get_selection:EditorSelection"), &EditorPlugin::get_selection);
	ObjectTypeDB::bind_method(_MD("get_editor_settings:EditorSettings"), &EditorPlugin::get_editor_settings);
	ObjectTypeDB::bind_method(_MD("queue_save_layout"), &EditorPlugin::queue_save_layout);
	ObjectTypeDB::bind_method(_MD("edit_resource"), &EditorPlugin::edit_resource);

	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::BOOL, "forward_input_event", PropertyInfo(Variant::INPUT_EVENT, "event")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::BOOL, "forward_spatial_input_event", PropertyInfo(Variant::OBJECT, "camera", PROPERTY_HINT_RESOURCE_TYPE, "Camera"), PropertyInfo(Variant::INPUT_EVENT, "event")));
	MethodInfo gizmo = MethodInfo(Variant::OBJECT, "create_spatial_gizmo", PropertyInfo(Variant::OBJECT, "for_spatial:Spatial"));
	gizmo.return_val.hint = PROPERTY_HINT_RESOURCE_TYPE;
	gizmo.return_val.hint_string = "EditorSpatialGizmo";
	ObjectTypeDB::add_virtual_method(get_type_static(), gizmo);
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::STRING, "get_name"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::BOOL, "has_main_screen"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("make_visible", PropertyInfo(Variant::BOOL, "visible")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("edit", PropertyInfo(Variant::OBJECT, "object")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::BOOL, "handles", PropertyInfo(Variant::OBJECT, "object")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::DICTIONARY, "get_state"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("set_state", PropertyInfo(Variant::DICTIONARY, "state")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("clear"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("save_external_data"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("apply_changes"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo(Variant::STRING_ARRAY, "get_breakpoints"));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("set_window_layout", PropertyInfo(Variant::OBJECT, "layout", PROPERTY_HINT_RESOURCE_TYPE, "ConfigFile")));
	ObjectTypeDB::add_virtual_method(get_type_static(), MethodInfo("get_window_layout", PropertyInfo(Variant::OBJECT, "layout", PROPERTY_HINT_RESOURCE_TYPE, "ConfigFile")));

	BIND_CONSTANT(CONTAINER_TOOLBAR);
	BIND_CONSTANT(CONTAINER_SPATIAL_EDITOR_MENU);
	BIND_CONSTANT(CONTAINER_SPATIAL_EDITOR_SIDE);
	BIND_CONSTANT(CONTAINER_SPATIAL_EDITOR_BOTTOM);
	BIND_CONSTANT(CONTAINER_CANVAS_EDITOR_MENU);
	BIND_CONSTANT(CONTAINER_CANVAS_EDITOR_SIDE);
	BIND_CONSTANT(CONTAINER_PROPERTY_EDITOR_BOTTOM);

	BIND_CONSTANT(DOCK_SLOT_LEFT_UL);
	BIND_CONSTANT(DOCK_SLOT_LEFT_BL);
	BIND_CONSTANT(DOCK_SLOT_LEFT_UR);
	BIND_CONSTANT(DOCK_SLOT_LEFT_BR);
	BIND_CONSTANT(DOCK_SLOT_RIGHT_UL);
	BIND_CONSTANT(DOCK_SLOT_RIGHT_BL);
	BIND_CONSTANT(DOCK_SLOT_RIGHT_UR);
	BIND_CONSTANT(DOCK_SLOT_RIGHT_BR);
	BIND_CONSTANT(DOCK_SLOT_MAX);
}

EditorPlugin::EditorPlugin() {
	undo_redo = NULL;
}

EditorPlugin::~EditorPlugin() {
}

EditorPluginCreateFunc EditorPlugins::creation_funcs[MAX_CREATE_FUNCS];

int EditorPlugins::creation_func_count = 0;
