/*************************************************************************/
/*  control.cpp                                                          */
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
#include "control.h"
#include "globals.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/viewport.h"
#include "servers/visual_server.h"

#include "message_queue.h"
#include "os/keyboard.h"
#include "os/os.h"
#include "print_string.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/scene_string_names.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif
#include <stdio.h>

Variant Control::edit_get_state() const {

	Dictionary s;
	s["rect"] = get_rect();
	s["rot"] = get_rotation();
	s["scale"] = get_scale();
	return s;
}
void Control::edit_set_state(const Variant &p_state) {

	Dictionary s = p_state;

	Rect2 state = s["rect"];
	set_pos(state.pos);
	set_size(state.size);
	set_rotation(s["rot"]);
	set_scale(s["scale"]);
}

void Control::set_custom_minimum_size(const Size2 &p_custom) {

	if (p_custom == data.custom_minimum_size)
		return;
	data.custom_minimum_size = p_custom;
	minimum_size_changed();
}

Size2 Control::get_custom_minimum_size() const {

	return data.custom_minimum_size;
}

Size2 Control::get_combined_minimum_size() const {

	Size2 minsize = get_minimum_size();
	minsize.x = MAX(minsize.x, data.custom_minimum_size.x);
	minsize.y = MAX(minsize.y, data.custom_minimum_size.y);
	return minsize;
}

Size2 Control::edit_get_minimum_size() const {

	return get_combined_minimum_size();
}

void Control::edit_set_rect(const Rect2 &p_edit_rect) {

	Matrix32 postxf;
	postxf.set_rotation_and_scale(data.rotation, data.scale);
	Vector2 new_pos = postxf.xform(p_edit_rect.pos);

	Vector2 pos = get_pos() + new_pos;

	Rect2 new_rect = get_rect();
	new_rect.pos = pos.snapped(Vector2(1, 1));
	new_rect.size = p_edit_rect.size.snapped(Vector2(1, 1));

	set_pos(new_rect.pos);
	set_size(new_rect.size);
}

bool Control::_set(const StringName &p_name, const Variant &p_value) {

	String name = p_name;
	if (!name.begins_with("custom")) {
		if (name.begins_with("margin/")) {
			String dname = name.get_slicec('/', 1);
			if (dname == "left") {
				set_margin(MARGIN_LEFT, p_value);
				return true;
			} else if (dname == "top") {
				set_margin(MARGIN_TOP, p_value);
				return true;
			} else if (dname == "right") {
				set_margin(MARGIN_RIGHT, p_value);
				return true;
			} else if (dname == "bottom") {
				set_margin(MARGIN_BOTTOM, p_value);
				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
	}

	if (p_value.get_type() == Variant::NIL) {

		if (name.begins_with("custom_icons/")) {
			String dname = name.get_slicec('/', 1);
			data.icon_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else if (name.begins_with("custom_shaders/")) {
			String dname = name.get_slicec('/', 1);
			data.shader_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else if (name.begins_with("custom_styles/")) {
			String dname = name.get_slicec('/', 1);
			data.style_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else if (name.begins_with("custom_fonts/")) {
			String dname = name.get_slicec('/', 1);
			if (data.font_override.has(dname)) {
				_unref_font(data.font_override[dname]);
			}
			data.font_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else if (name.begins_with("custom_colors/")) {
			String dname = name.get_slicec('/', 1);
			data.color_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else if (name.begins_with("custom_constants/")) {
			String dname = name.get_slicec('/', 1);
			data.constant_override.erase(dname);
			notification(NOTIFICATION_THEME_CHANGED);
			update();
		} else
			return false;

	} else {
		if (name.begins_with("custom_icons/")) {
			String dname = name.get_slicec('/', 1);
			notification(NOTIFICATION_THEME_CHANGED);
			add_icon_override(dname, p_value);
		} else if (name.begins_with("custom_shaders/")) {
			String dname = name.get_slicec('/', 1);
			add_shader_override(dname, p_value);
			notification(NOTIFICATION_THEME_CHANGED);
		} else if (name.begins_with("custom_styles/")) {
			String dname = name.get_slicec('/', 1);
			add_style_override(dname, p_value);
			notification(NOTIFICATION_THEME_CHANGED);
		} else if (name.begins_with("custom_fonts/")) {
			String dname = name.get_slicec('/', 1);
			add_font_override(dname, p_value);
			notification(NOTIFICATION_THEME_CHANGED);
		} else if (name.begins_with("custom_colors/")) {
			String dname = name.get_slicec('/', 1);
			add_color_override(dname, p_value);
			notification(NOTIFICATION_THEME_CHANGED);
		} else if (name.begins_with("custom_constants/")) {
			String dname = name.get_slicec('/', 1);
			add_constant_override(dname, p_value);
			notification(NOTIFICATION_THEME_CHANGED);
		} else
			return false;
	}
	return true;
}

void Control::_update_minimum_size() {

	if (!is_inside_tree())
		return;

	data.pending_min_size_update = false;
	Size2 minsize = get_combined_minimum_size();
	if (minsize.x > data.size_cache.x ||
			minsize.y > data.size_cache.y) {
		_size_changed();
	}

	emit_signal(SceneStringNames::get_singleton()->minimum_size_changed);
}

bool Control::_get(const StringName &p_name, Variant &r_ret) const {

	String sname = p_name;

	if (!sname.begins_with("custom")) {
		if (sname.begins_with("margin/")) {
			String dname = sname.get_slicec('/', 1);
			if (dname == "left") {
				r_ret = get_margin(MARGIN_LEFT);
				return true;
			} else if (dname == "top") {
				r_ret = get_margin(MARGIN_TOP);
				return true;
			} else if (dname == "right") {
				r_ret = get_margin(MARGIN_RIGHT);
				return true;
			} else if (dname == "bottom") {
				r_ret = get_margin(MARGIN_BOTTOM);
				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
	}

	if (sname.begins_with("custom_icons/")) {
		String name = sname.get_slicec('/', 1);

		r_ret = data.icon_override.has(name) ? Variant(data.icon_override[name]) : Variant();
	} else if (sname.begins_with("custom_shaders/")) {
		String name = sname.get_slicec('/', 1);

		r_ret = data.shader_override.has(name) ? Variant(data.shader_override[name]) : Variant();
	} else if (sname.begins_with("custom_styles/")) {
		String name = sname.get_slicec('/', 1);

		r_ret = data.style_override.has(name) ? Variant(data.style_override[name]) : Variant();
	} else if (sname.begins_with("custom_fonts/")) {
		String name = sname.get_slicec('/', 1);

		r_ret = data.font_override.has(name) ? Variant(data.font_override[name]) : Variant();
	} else if (sname.begins_with("custom_colors/")) {
		String name = sname.get_slicec('/', 1);
		r_ret = data.color_override.has(name) ? Variant(data.color_override[name]) : Variant();
	} else if (sname.begins_with("custom_constants/")) {
		String name = sname.get_slicec('/', 1);

		r_ret = data.constant_override.has(name) ? Variant(data.constant_override[name]) : Variant();
	} else
		return false;

	return true;
}
void Control::_get_property_list(List<PropertyInfo> *p_list) const {

	{
		if (get_anchor(MARGIN_LEFT) == ANCHOR_RATIO) {
			p_list->push_back(PropertyInfo(Variant::REAL, "margin/left", PROPERTY_HINT_RANGE, "-4096,4096,0.001"));
		} else {
			p_list->push_back(PropertyInfo(Variant::INT, "margin/left", PROPERTY_HINT_RANGE, "-4096,4096"));
		}

		if (get_anchor(MARGIN_TOP) == ANCHOR_RATIO) {
			p_list->push_back(PropertyInfo(Variant::REAL, "margin/top", PROPERTY_HINT_RANGE, "-4096,4096,0.001"));
		} else {
			p_list->push_back(PropertyInfo(Variant::INT, "margin/top", PROPERTY_HINT_RANGE, "-4096,4096"));
		}

		if (get_anchor(MARGIN_RIGHT) == ANCHOR_RATIO) {
			p_list->push_back(PropertyInfo(Variant::REAL, "margin/right", PROPERTY_HINT_RANGE, "-4096,4096,0.001"));
		} else {
			p_list->push_back(PropertyInfo(Variant::INT, "margin/right", PROPERTY_HINT_RANGE, "-4096,4096"));
		}

		if (get_anchor(MARGIN_BOTTOM) == ANCHOR_RATIO) {
			p_list->push_back(PropertyInfo(Variant::REAL, "margin/bottom", PROPERTY_HINT_RANGE, "-4096,4096,0.001"));
		} else {
			p_list->push_back(PropertyInfo(Variant::INT, "margin/bottom", PROPERTY_HINT_RANGE, "-4096,4096"));
		}
	}

	Ref<Theme> theme;
	if (data.theme.is_valid()) {

		theme = data.theme;
	} else {
		theme = Theme::get_default();
	}

	{
		List<StringName> names;
		theme->get_icon_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.icon_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::OBJECT, "custom_icons/" + E->get(), PROPERTY_HINT_RESOURCE_TYPE, "Texture", hint));
		}
	}
	{
		List<StringName> names;
		theme->get_shader_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.shader_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::OBJECT, "custom_shaders/" + E->get(), PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemShader,CanvasItemShaderGraph", hint));
		}
	}
	{
		List<StringName> names;
		theme->get_stylebox_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.style_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::OBJECT, "custom_styles/" + E->get(), PROPERTY_HINT_RESOURCE_TYPE, "StyleBox", hint));
		}
	}
	{
		List<StringName> names;
		theme->get_font_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.font_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::OBJECT, "custom_fonts/" + E->get(), PROPERTY_HINT_RESOURCE_TYPE, "Font", hint));
		}
	}
	{
		List<StringName> names;
		theme->get_color_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.color_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::COLOR, "custom_colors/" + E->get(), PROPERTY_HINT_NONE, "", hint));
		}
	}
	{
		List<StringName> names;
		theme->get_constant_list(get_type_name(), &names);
		for (List<StringName>::Element *E = names.front(); E; E = E->next()) {

			uint32_t hint = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CHECKABLE;
			if (data.constant_override.has(E->get()))
				hint |= PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_CHECKED;

			p_list->push_back(PropertyInfo(Variant::INT, "custom_constants/" + E->get(), PROPERTY_HINT_RANGE, "-16384,16384", hint));
		}
	}
}

Control *Control::get_parent_control() const {

	return data.parent;
}

void Control::_resize(const Size2 &p_size) {

	_size_changed();
}

void Control::_notification(int p_notification) {

	switch (p_notification) {

		case NOTIFICATION_ENTER_TREE: {

			_size_changed();

		} break;
		case NOTIFICATION_EXIT_TREE: {

			get_viewport()->_gui_remove_control(this);

		} break;

		case NOTIFICATION_ENTER_CANVAS: {

			data.parent = get_parent()->cast_to<Control>();

			if (is_set_as_toplevel()) {
				data.SI = get_viewport()->_gui_add_subwindow_control(this);

				if (data.theme.is_null() && data.parent && data.parent->data.theme_owner) {
					data.theme_owner = data.parent->data.theme_owner;
					notification(NOTIFICATION_THEME_CHANGED);
				}

			} else {

				Node *parent = this; //meh
				Control *parent_control = NULL;
				bool subwindow = false;

				while (parent) {

					parent = parent->get_parent();

					if (!parent)
						break;

					CanvasItem *ci = parent->cast_to<CanvasItem>();
					if (ci && ci->is_set_as_toplevel()) {
						subwindow = true;
						break;
					}

					parent_control = parent->cast_to<Control>();

					if (parent_control) {
						break;
					} else if (ci) {

					} else {
						break;
					}
				}

				if (parent_control) {
					//do nothing, has a parent control
					if (data.theme.is_null() && parent_control->data.theme_owner) {
						data.theme_owner = parent_control->data.theme_owner;
						notification(NOTIFICATION_THEME_CHANGED);
					}
				} else if (subwindow) {
					//is a subwindow (process input before other controls for that canvas)
					data.SI = get_viewport()->_gui_add_subwindow_control(this);
				} else {
					//is a regular root control
					data.RI = get_viewport()->_gui_add_root_control(this);
				}

				data.parent_canvas_item = get_parent_item();

				if (data.parent_canvas_item) {

					data.parent_canvas_item->connect("item_rect_changed", this, "_size_changed");
				} else {
					//connect viewport
					get_viewport()->connect("size_changed", this, "_size_changed");
				}
			}

			if (data.theme.is_null() && data.parent && data.parent->data.theme_owner) {
				data.theme_owner = data.parent->data.theme_owner;
				notification(NOTIFICATION_THEME_CHANGED);
			}

		} break;
		case NOTIFICATION_EXIT_CANVAS: {

			if (data.parent_canvas_item) {

				data.parent_canvas_item->disconnect("item_rect_changed", this, "_size_changed");
				data.parent_canvas_item = NULL;
			} else if (!is_set_as_toplevel()) {
				//disconnect viewport
				get_viewport()->disconnect("size_changed", this, "_size_changed");
			}

			if (data.MI) {
				get_viewport()->_gui_remove_modal_control(data.MI);
				data.MI = NULL;
			}

			if (data.SI) {
				get_viewport()->_gui_remove_subwindow_control(data.SI);
				data.SI = NULL;
			}

			if (data.RI) {
				get_viewport()->_gui_remove_root_control(data.RI);
				data.RI = NULL;
			}

			data.parent = NULL;
			data.parent_canvas_item = NULL;
			if (data.theme_owner && data.theme.is_null()) {
				data.theme_owner = NULL;
				//notification(NOTIFICATION_THEME_CHANGED);
			}

		} break;
		case NOTIFICATION_MOVED_IN_PARENT: {
			// some parents need to know the order of the childrens to draw (like TabContainer)
			// update if necesary
			if (data.parent)
				data.parent->update();
			update();

			if (data.SI) {
				get_viewport()->_gui_set_subwindow_order_dirty();
			}
			if (data.RI) {
				get_viewport()->_gui_set_root_order_dirty();
			}

		} break;
		case NOTIFICATION_RESIZED: {

			emit_signal(SceneStringNames::get_singleton()->resized);
		} break;
		case NOTIFICATION_DRAW: {

			Matrix32 xform = Matrix32(data.rotation, get_pos());
			xform.scale_basis(data.scale);
			VisualServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), xform);
			VisualServer::get_singleton()->canvas_item_set_custom_rect(get_canvas_item(), true, Rect2(Point2(), get_size()));
			//emit_signal(SceneStringNames::get_singleton()->draw);

		} break;
		case NOTIFICATION_MOUSE_ENTER: {

			emit_signal(SceneStringNames::get_singleton()->mouse_enter);
		} break;
		case NOTIFICATION_MOUSE_EXIT: {

			emit_signal(SceneStringNames::get_singleton()->mouse_exit);
		} break;
		case NOTIFICATION_FOCUS_ENTER: {

			emit_signal(SceneStringNames::get_singleton()->focus_enter);
			update();
		} break;
		case NOTIFICATION_FOCUS_EXIT: {

			emit_signal(SceneStringNames::get_singleton()->focus_exit);
			update();

		} break;
		case NOTIFICATION_THEME_CHANGED: {

			update();
		} break;
		case NOTIFICATION_MODAL_CLOSE: {

			emit_signal("modal_close");
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {

			if (!is_visible()) {

				if (get_viewport() != NULL)
					get_viewport()->_gui_hid_control(this);

				if (is_inside_tree()) {
					_modal_stack_remove();
					minimum_size_changed();
				}

				//remove key focus
				//remove modalness
			} else {

				_size_changed();
			}

		} break;
		case SceneTree::NOTIFICATION_WM_UNFOCUS_REQUEST: {

			get_viewport()->_gui_unfocus_control(this);

		} break;
	}
}

bool Control::clips_input() const {

	return false;
}
bool Control::has_point(const Point2 &p_point) const {

	if (get_script_instance()) {
		Variant v = p_point;
		const Variant *p = &v;
		Variant::CallError ce;
		Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->has_point, &p, 1, ce);
		if (ce.error == Variant::CallError::CALL_OK) {
			return ret;
		}
	}
	/*if (has_stylebox("mask")) {
		Ref<StyleBox> mask = get_stylebox("mask");
		return mask->test_mask(p_point,Rect2(Point2(),get_size()));
	}*/
	return Rect2(Point2(), get_size()).has_point(p_point);
}

void Control::set_drag_forwarding(Control *p_target) {

	if (p_target)
		data.drag_owner = p_target->get_instance_ID();
	else
		data.drag_owner = 0;
}

Variant Control::get_drag_data(const Point2 &p_point) {

	if (data.drag_owner) {
		Object *obj = ObjectDB::get_instance(data.drag_owner);
		if (obj) {
			Control *c = obj->cast_to<Control>();
			return c->call("get_drag_data_fw", p_point, this);
		}
	}

	if (get_script_instance()) {
		Variant v = p_point;
		const Variant *p = &v;
		Variant::CallError ce;
		Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->get_drag_data, &p, 1, ce);
		if (ce.error == Variant::CallError::CALL_OK)
			return ret;
	}

	return Variant();
}

bool Control::can_drop_data(const Point2 &p_point, const Variant &p_data) const {

	if (data.drag_owner) {
		Object *obj = ObjectDB::get_instance(data.drag_owner);
		if (obj) {
			Control *c = obj->cast_to<Control>();
			return c->call("can_drop_data_fw", p_point, p_data, this);
		}
	}

	if (get_script_instance()) {
		Variant v = p_point;
		const Variant *p[2] = { &v, &p_data };
		Variant::CallError ce;
		Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->can_drop_data, p, 2, ce);
		if (ce.error == Variant::CallError::CALL_OK)
			return ret;
	}

	return Variant();
}
void Control::drop_data(const Point2 &p_point, const Variant &p_data) {

	if (data.drag_owner) {
		Object *obj = ObjectDB::get_instance(data.drag_owner);
		if (obj) {
			Control *c = obj->cast_to<Control>();
			c->call("drop_data_fw", p_point, p_data, this);
			return;
		}
	}

	if (get_script_instance()) {
		Variant v = p_point;
		const Variant *p[2] = { &v, &p_data };
		Variant::CallError ce;
		Variant ret = get_script_instance()->call(SceneStringNames::get_singleton()->drop_data, p, 2, ce);
		if (ce.error == Variant::CallError::CALL_OK)
			return;
	}
}

void Control::force_drag(const Variant &p_data, Control *p_control) {

	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND(p_data.get_type() == Variant::NIL);

	get_viewport()->_gui_force_drag(this, p_data, p_control);
}

void Control::set_drag_preview(Control *p_control) {

	ERR_FAIL_COND(!is_inside_tree());
	get_viewport()->_gui_set_drag_preview(this, p_control);
}

bool Control::is_window_modal_on_top() const {

	if (!is_inside_tree())
		return false;

	return get_viewport()->_gui_is_modal_on_top(this);
}

uint64_t Control::get_modal_frame() const {

	return data.modal_frame;
}

Size2 Control::get_minimum_size() const {

	ScriptInstance *si = const_cast<Control *>(this)->get_script_instance();
	if (si) {

		Variant::CallError ce;
		Variant s = si->call(SceneStringNames::get_singleton()->get_minimum_size, NULL, 0, ce);
		if (ce.error == Variant::CallError::CALL_OK)
			return s;
	}
	return Size2();
}

Ref<Texture> Control::get_icon(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {

		const Ref<Texture> *tex = data.icon_override.getptr(p_name);
		if (tex)
			return *tex;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_icon(p_name, type))
			return theme_owner->data.theme->get_icon(p_name, type);
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_icon(p_name, type);
}

Ref<Shader> Control::get_shader(const StringName &p_name, const StringName &p_type) const {
	if (p_type == StringName() || p_type == "") {

		const Ref<Shader> *sdr = data.shader_override.getptr(p_name);
		if (sdr)
			return *sdr;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_shader(p_name, type))
			return theme_owner->data.theme->get_shader(p_name, type);
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_shader(p_name, type);
}

Ref<StyleBox> Control::get_stylebox(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		const Ref<StyleBox> *style = data.style_override.getptr(p_name);
		if (style)
			return *style;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_stylebox(p_name, type)) {
			return theme_owner->data.theme->get_stylebox(p_name, type);
		}
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_stylebox(p_name, type);
}
Ref<Font> Control::get_font(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		const Ref<Font> *font = data.font_override.getptr(p_name);
		if (font)
			return *font;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_font(p_name, type))
			return theme_owner->data.theme->get_font(p_name, type);
		if (theme_owner->data.theme->get_default_theme_font().is_valid())
			return theme_owner->data.theme->get_default_theme_font();
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_font(p_name, type);
}
Color Control::get_color(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		const Color *color = data.color_override.getptr(p_name);
		if (color)
			return *color;
	}

	StringName type = p_type ? p_type : get_type_name();
	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_color(p_name, type))
			return theme_owner->data.theme->get_color(p_name, type);
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_color(p_name, type);
}

int Control::get_constant(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		const int *constant = data.constant_override.getptr(p_name);
		if (constant)
			return *constant;
	}

	StringName type = p_type ? p_type : get_type_name();
	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_constant(p_name, type))
			return theme_owner->data.theme->get_constant(p_name, type);
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->get_constant(p_name, type);
}

bool Control::has_icon_override(const StringName &p_name) const {

	const Ref<Texture> *tex = data.icon_override.getptr(p_name);
	if (tex)
		return true;
	else
		return false;
}

bool Control::has_shader_override(const StringName &p_name) const {

	const Ref<Shader> *sdr = data.shader_override.getptr(p_name);
	if (sdr)
		return true;
	else
		return false;
}

bool Control::has_stylebox_override(const StringName &p_name) const {

	const Ref<StyleBox> *style = data.style_override.getptr(p_name);
	if (style)
		return true;
	else
		return false;
}

bool Control::has_font_override(const StringName &p_name) const {

	const Ref<Font> *font = data.font_override.getptr(p_name);
	if (font)
		return true;
	else
		return false;
}

bool Control::has_color_override(const StringName &p_name) const {

	const Color *color = data.color_override.getptr(p_name);
	if (color)
		return true;
	else
		return false;
}

bool Control::has_constant_override(const StringName &p_name) const {

	const int *constant = data.constant_override.getptr(p_name);
	if (constant)
		return true;
	else
		return false;
}

bool Control::has_icon(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_icon_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_icon(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_icon(p_name, type);
}

bool Control::has_shader(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_shader_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_shader(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_shader(p_name, type);
}
bool Control::has_stylebox(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_stylebox_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_stylebox(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_stylebox(p_name, type);
}
bool Control::has_font(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_font_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_font(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_font(p_name, type);
}

bool Control::has_color(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_color_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_color(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_color(p_name, type);
}

bool Control::has_constant(const StringName &p_name, const StringName &p_type) const {

	if (p_type == StringName() || p_type == "") {
		if (has_constant_override(p_name) == true)
			return true;
	}

	StringName type = p_type ? p_type : get_type_name();

	// try with custom themes
	Control *theme_owner = data.theme_owner;

	while (theme_owner) {

		if (theme_owner->data.theme->has_constant(p_name, type))
			return true;
		Control *parent = theme_owner->get_parent() ? theme_owner->get_parent()->cast_to<Control>() : NULL;

		if (parent)
			theme_owner = parent->data.theme_owner;
		else
			theme_owner = NULL;
	}

	return Theme::get_default()->has_constant(p_name, type);
}

Size2 Control::get_parent_area_size() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Size2());

	Size2 parent_size;

	if (data.parent_canvas_item) {

		parent_size = data.parent_canvas_item->get_item_rect().size;
	} else {

		parent_size = get_viewport()->get_visible_rect().size;
	}
	return parent_size;
}

void Control::_size_changed() {

	if (!is_inside_tree())
		return;

	Size2 parent_size = get_parent_area_size();

	float margin_pos[4];

	for (int i = 0; i < 4; i++) {

		float area = parent_size[i & 1];
		switch (data.anchor[i]) {

			case ANCHOR_BEGIN: {

				margin_pos[i] = data.margin[i];
			} break;
			case ANCHOR_END: {

				margin_pos[i] = area - data.margin[i];
			} break;
			case ANCHOR_RATIO: {

				margin_pos[i] = area * data.margin[i];
			} break;
			case ANCHOR_CENTER: {

				margin_pos[i] = (area / 2) - data.margin[i];
			} break;
		}
	}

	Point2 new_pos_cache = Point2(margin_pos[0], margin_pos[1]);
	Size2 new_size_cache = Point2(margin_pos[2], margin_pos[3]) - new_pos_cache;
	Size2 minimum_size = get_combined_minimum_size();

	new_size_cache.x = MAX(minimum_size.x, new_size_cache.x);
	new_size_cache.y = MAX(minimum_size.y, new_size_cache.y);

	if (new_pos_cache == data.pos_cache && new_size_cache == data.size_cache)
		return; // did not change, don't emit signal

	data.pos_cache = new_pos_cache;
	data.size_cache = new_size_cache;

	notification(NOTIFICATION_RESIZED);
	item_rect_changed();
	_change_notify_margins();
	_notify_transform();
}

float Control::_get_parent_range(int p_idx) const {

	if (!is_inside_tree()) {

		return 1.0;
	}
	if (data.parent_canvas_item) {

		return data.parent_canvas_item->get_item_rect().size[p_idx & 1];
	} else {
		return get_viewport()->get_visible_rect().size[p_idx & 1];
	}

	return 1.0;
}

float Control::_get_range(int p_idx) const {

	p_idx &= 1;

	float parent_range = _get_parent_range(p_idx);
	float from = _a2s(data.margin[p_idx], data.anchor[p_idx], parent_range);
	float to = _a2s(data.margin[p_idx + 2], data.anchor[p_idx + 2], parent_range);

	return to - from;
}

float Control::_s2a(float p_val, AnchorType p_anchor, float p_range) const {

	switch (p_anchor) {

		case ANCHOR_BEGIN: {
			return p_val;
		} break;
		case ANCHOR_END: {
			return p_range - p_val;
		} break;
		case ANCHOR_RATIO: {
			return p_val / p_range;
		} break;
		case ANCHOR_CENTER: {
			return (p_range / 2) - p_val;
		} break;
	}

	return 0;
}

float Control::_a2s(float p_val, AnchorType p_anchor, float p_range) const {

	switch (p_anchor) {

		case ANCHOR_BEGIN: {
			return Math::floor(p_val);
		} break;
		case ANCHOR_END: {
			return Math::floor(p_range - p_val);
		} break;
		case ANCHOR_RATIO: {
			return Math::floor(p_range * p_val);
		} break;
		case ANCHOR_CENTER: {
			return Math::floor((p_range / 2) - p_val);
		} break;
	}
	return 0;
}

void Control::set_anchor(Margin p_margin, AnchorType p_anchor, bool p_keep_margin) {

	if (!is_inside_tree()) {

		data.anchor[p_margin] = p_anchor;
	} else if (!p_keep_margin) {
		float pr = _get_parent_range(p_margin);
		float s = _a2s(data.margin[p_margin], data.anchor[p_margin], pr);
		data.anchor[p_margin] = p_anchor;
		data.margin[p_margin] = _s2a(s, p_anchor, pr);
	} else {
		data.anchor[p_margin] = p_anchor;
		_size_changed();
	}
	_change_notify();
}

void Control::_set_anchor(Margin p_margin, AnchorType p_anchor) {
#ifdef TOOLS_ENABLED
	if (is_inside_tree() && get_tree()->is_editor_hint()) {
		set_anchor(p_margin, p_anchor, EDITOR_DEF("2d_editor/keep_margins_when_changing_anchors", false));
	} else {
		set_anchor(p_margin, p_anchor, false);
	}
#else
	set_anchor(p_margin, p_anchor, false);
#endif
}

void Control::set_anchor_and_margin(Margin p_margin, AnchorType p_anchor, float p_pos) {

	set_anchor(p_margin, p_anchor);
	set_margin(p_margin, p_pos);
}

Control::AnchorType Control::get_anchor(Margin p_margin) const {

	return data.anchor[p_margin];
}

void Control::_change_notify_margins() {

	// this avoids sending the whole object data again on a change
	_change_notify("margin/left");
	_change_notify("margin/top");
	_change_notify("margin/right");
	_change_notify("margin/bottom");
	_change_notify("rect/pos");
	_change_notify("rect/size");
}

void Control::set_margin(Margin p_margin, float p_value) {

	data.margin[p_margin] = p_value;
	_size_changed();
}

void Control::set_begin(const Size2 &p_point) {

	data.margin[0] = p_point.x;
	data.margin[1] = p_point.y;
	_size_changed();
}

void Control::set_end(const Size2 &p_point) {

	data.margin[2] = p_point.x;
	data.margin[3] = p_point.y;
	_size_changed();
}

float Control::get_margin(Margin p_margin) const {

	return data.margin[p_margin];
}

Size2 Control::get_begin() const {

	return Size2(data.margin[0], data.margin[1]);
}
Size2 Control::get_end() const {

	return Size2(data.margin[2], data.margin[3]);
}

Point2 Control::get_global_pos() const {

	return get_global_transform().get_origin();
}

void Control::set_global_pos(const Point2 &p_point) {

	Matrix32 inv;

	if (data.parent_canvas_item) {

		inv = data.parent_canvas_item->get_global_transform().affine_inverse();
	}

	set_pos(inv.xform(p_point));
}

void Control::set_pos(const Size2 &p_point) {

	float pw = _get_parent_range(0);
	float ph = _get_parent_range(1);

	float x = _a2s(data.margin[0], data.anchor[0], pw);
	float y = _a2s(data.margin[1], data.anchor[1], ph);
	float x2 = _a2s(data.margin[2], data.anchor[2], pw);
	float y2 = _a2s(data.margin[3], data.anchor[3], ph);

	Size2 ret = Size2(x2 - x, y2 - y);
	Size2 min = get_combined_minimum_size();

	Size2 size = Size2(MAX(min.width, ret.width), MAX(min.height, ret.height));
	float w = size.x;
	float h = size.y;

	x = p_point.x;
	y = p_point.y;

	data.margin[0] = _s2a(x, data.anchor[0], pw);
	data.margin[1] = _s2a(y, data.anchor[1], ph);
	data.margin[2] = _s2a(x + w, data.anchor[2], pw);
	data.margin[3] = _s2a(y + h, data.anchor[3], ph);

	_size_changed();
}

void Control::set_size(const Size2 &p_size) {

	Size2 new_size = p_size;
	Size2 min = get_combined_minimum_size();
	if (new_size.x < min.x)
		new_size.x = min.x;
	if (new_size.y < min.y)
		new_size.y = min.y;

	float pw = _get_parent_range(0);
	float ph = _get_parent_range(1);

	float x = _a2s(data.margin[0], data.anchor[0], pw);
	float y = _a2s(data.margin[1], data.anchor[1], ph);

	float w = new_size.width;
	float h = new_size.height;

	data.margin[2] = _s2a(x + w, data.anchor[2], pw);
	data.margin[3] = _s2a(y + h, data.anchor[3], ph);

	_size_changed();
}

Size2 Control::get_pos() const {

	return data.pos_cache;
}

Size2 Control::get_size() const {

	return data.size_cache;
}

Rect2 Control::get_global_rect() const {

	return Rect2(get_global_pos(), get_size());
}

Rect2 Control::get_window_rect() const {
	ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
	Rect2 gr = get_global_rect();
	gr.pos += get_viewport()->get_visible_rect().pos;
	return gr;
}

Rect2 Control::get_rect() const {

	return Rect2(get_pos(), get_size());
}

Rect2 Control::get_item_rect() const {

	return Rect2(Point2(), get_size());
}

void Control::set_area_as_parent_rect(int p_margin) {

	data.anchor[MARGIN_LEFT] = ANCHOR_BEGIN;
	data.anchor[MARGIN_TOP] = ANCHOR_BEGIN;
	data.anchor[MARGIN_RIGHT] = ANCHOR_END;
	data.anchor[MARGIN_BOTTOM] = ANCHOR_END;
	for (int i = 0; i < 4; i++)
		data.margin[i] = p_margin;

	_size_changed();
}

void Control::add_icon_override(const StringName &p_name, const Ref<Texture> &p_icon) {

	ERR_FAIL_COND(p_icon.is_null());
	data.icon_override[p_name] = p_icon;
	notification(NOTIFICATION_THEME_CHANGED);
	update();
}

void Control::add_shader_override(const StringName &p_name, const Ref<Shader> &p_shader) {
	ERR_FAIL_COND(p_shader.is_null());
	data.shader_override[p_name] = p_shader;
	notification(NOTIFICATION_THEME_CHANGED);
	update();
}
void Control::add_style_override(const StringName &p_name, const Ref<StyleBox> &p_style) {

	ERR_FAIL_COND(p_style.is_null());
	data.style_override[p_name] = p_style;
	notification(NOTIFICATION_THEME_CHANGED);
	update();
}

void Control::add_font_override(const StringName &p_name, const Ref<Font> &p_font) {

	ERR_FAIL_COND(p_font.is_null());
	if (data.font_override.has(p_name)) {
		_unref_font(data.font_override[p_name]);
	}
	data.font_override[p_name] = p_font;

	if (p_font.is_valid()) {
		_ref_font(p_font);
	}

	notification(NOTIFICATION_THEME_CHANGED);
	update();
}
void Control::add_color_override(const StringName &p_name, const Color &p_color) {

	data.color_override[p_name] = p_color;
	notification(NOTIFICATION_THEME_CHANGED);
	update();
}
void Control::add_constant_override(const StringName &p_name, int p_constant) {

	data.constant_override[p_name] = p_constant;
	notification(NOTIFICATION_THEME_CHANGED);
	update();
}

void Control::set_focus_mode(FocusMode p_focus_mode) {

	if (is_inside_tree() && p_focus_mode == FOCUS_NONE && data.focus_mode != FOCUS_NONE && has_focus())
		release_focus();

	data.focus_mode = p_focus_mode;
}

static Control *_next_control(Control *p_from) {

	if (p_from->is_set_as_toplevel())
		return NULL; // can't go above

	Control *parent = p_from->get_parent() ? p_from->get_parent()->cast_to<Control>() : NULL;

	if (!parent) {

		return NULL;
	}

	int next = p_from->get_position_in_parent();
	ERR_FAIL_INDEX_V(next, parent->get_child_count(), NULL);
	for (int i = (next + 1); i < parent->get_child_count(); i++) {

		Control *c = parent->get_child(i)->cast_to<Control>();
		if (!c || !c->is_visible() || c->is_set_as_toplevel())
			continue;

		return c;
	}

	//no next in parent, try the same in parent
	return _next_control(parent);
}

Control *Control::find_next_valid_focus() const {

	Control *from = const_cast<Control *>(this);

	while (true) {

		// find next child

		Control *next_child = NULL;

		for (int i = 0; i < from->get_child_count(); i++) {

			Control *c = from->get_child(i)->cast_to<Control>();
			if (!c || !c->is_visible() || c->is_set_as_toplevel()) {
				continue;
			}

			next_child = c;
			break;
		}

		if (next_child) {

			from = next_child;
		} else {

			next_child = _next_control(from);
			if (!next_child) { //nothing else.. go up and find either window or subwindow
				next_child = const_cast<Control *>(this);
				while (next_child && !next_child->is_set_as_toplevel()) {
					if (next_child->get_parent()) {
						next_child = next_child->get_parent()->cast_to<Control>();
					} else
						next_child = NULL;
				}

				if (!next_child) {

					next_child = const_cast<Control *>(this);
					while (next_child) {

						if (next_child->data.SI || next_child->data.RI)
							break;
						next_child = next_child->get_parent_control();
					}
				}
			}
		}

		if (next_child == this) // no next control->
			return (get_focus_mode() == FOCUS_ALL) ? next_child : NULL;
		if (next_child) {
			if (next_child->get_focus_mode() == FOCUS_ALL)
				return next_child;
			from = next_child;
		} else
			break;
	}

	return NULL;
}

static Control *_prev_control(Control *p_from) {

	Control *child = NULL;
	for (int i = p_from->get_child_count() - 1; i >= 0; i--) {

		Control *c = p_from->get_child(i)->cast_to<Control>();
		if (!c || !c->is_visible() || c->is_set_as_toplevel())
			continue;

		child = c;
		break;
	}

	if (!child)
		return p_from;

	//no prev in parent, try the same in parent
	return _prev_control(child);
}

Control *Control::find_prev_valid_focus() const {
	Control *from = const_cast<Control *>(this);

	while (true) {

		// find prev child

		Control *prev_child = NULL;

		if (from->is_set_as_toplevel() || !from->get_parent() || !from->get_parent()->cast_to<Control>()) {

			//find last of the childs

			prev_child = _prev_control(from);

		} else {

			for (int i = (from->get_position_in_parent() - 1); i >= 0; i--) {

				Control *c = from->get_parent()->get_child(i)->cast_to<Control>();

				if (!c || !c->is_visible() || c->is_set_as_toplevel()) {
					continue;
				}

				prev_child = c;
				break;
			}

			if (!prev_child) {

				prev_child = from->get_parent()->cast_to<Control>();
			} else {

				prev_child = _prev_control(prev_child);
			}
		}

		if (prev_child == this) // no prev control->
			return (get_focus_mode() == FOCUS_ALL) ? prev_child : NULL;

		if (prev_child->get_focus_mode() == FOCUS_ALL)
			return prev_child;

		from = prev_child;
	}

	return NULL;

	return NULL;
}

Control::FocusMode Control::get_focus_mode() const {

	return data.focus_mode;
}
bool Control::has_focus() const {

	return is_inside_tree() && get_viewport()->_gui_control_has_focus(this);
}

void Control::grab_focus() {

	if (!is_inside_tree()) {
		ERR_FAIL_COND(!is_inside_tree());
	}
	if (data.focus_mode == FOCUS_NONE)
		return;

	get_viewport()->_gui_control_grab_focus(this);
}

void Control::release_focus() {

	ERR_FAIL_COND(!is_inside_tree());

	if (!has_focus())
		return;

	get_viewport()->_gui_remove_focus();
	update();
}

bool Control::is_toplevel_control() const {

	return is_inside_tree() && (!data.parent_canvas_item && !data.RI && is_set_as_toplevel());
}

void Control::show_modal(bool p_exclusive) {

	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND(!data.SI);

	if (is_visible())
		hide();

	ERR_FAIL_COND(data.MI != NULL);
	show();
	raise();
	data.modal_exclusive = p_exclusive;
	data.MI = get_viewport()->_gui_show_modal(this);
	data.modal_frame = OS::get_singleton()->get_frames_drawn();
}

void Control::_modal_set_prev_focus_owner(ObjectID p_prev) {
	data.modal_prev_focus_owner = p_prev;
}

void Control::_modal_stack_remove() {

	ERR_FAIL_COND(!is_inside_tree());

	if (!data.MI)
		return;

	get_viewport()->_gui_remove_from_modal_stack(data.MI, data.modal_prev_focus_owner);

	data.MI = NULL;
	data.modal_prev_focus_owner = 0;
}

void Control::_propagate_theme_changed(CanvasItem *p_at, Control *p_owner, bool p_assign) {

	Control *c = p_at->cast_to<Control>();

	if (c && c != p_owner && c->data.theme.is_valid()) // has a theme, this can't be propagated
		return;

	for (int i = 0; i < p_at->get_child_count(); i++) {

		CanvasItem *child = p_at->get_child(i)->cast_to<CanvasItem>();
		if (child) {
			_propagate_theme_changed(child, p_owner);
		}
	}

	if (c) {

		if (p_assign) {
			c->data.theme_owner = p_owner;
		}
		c->_notification(NOTIFICATION_THEME_CHANGED);
		c->update();
	}
}

void Control::_theme_changed() {

	_propagate_theme_changed(this, this, false);
}

void Control::set_theme(const Ref<Theme> &p_theme) {

	if (data.theme == p_theme)
		return;

	if (data.theme.is_valid()) {
		data.theme->disconnect("changed", this, "_theme_changed");
	}

	data.theme = p_theme;
	if (!p_theme.is_null()) {

		_propagate_theme_changed(this, this);
	} else {

		Control *parent = get_parent() ? get_parent()->cast_to<Control>() : NULL;
		if (parent && parent->data.theme_owner) {
			_propagate_theme_changed(this, parent->data.theme_owner);
		} else {

			_propagate_theme_changed(this, NULL);
		}
	}

	if (data.theme.is_valid()) {
		data.theme->connect("changed", this, "_theme_changed");
	}
}

void Control::accept_event() {

	if (is_inside_tree())
		get_viewport()->_gui_accept_event();
}

Ref<Theme> Control::get_theme() const {

	return data.theme;
}

void Control::set_tooltip(const String &p_tooltip) {

	data.tooltip = p_tooltip;
}
String Control::get_tooltip(const Point2 &p_pos) const {

	return data.tooltip;
}

void Control::set_default_cursor_shape(CursorShape p_shape) {

	data.default_cursor = p_shape;
}

Control::CursorShape Control::get_default_cursor_shape() const {

	return data.default_cursor;
}
Control::CursorShape Control::get_cursor_shape(const Point2 &p_pos) const {

	return data.default_cursor;
}

Matrix32 Control::get_transform() const {

	Matrix32 xform = Matrix32(data.rotation, get_pos());
	xform.scale_basis(data.scale);
	return xform;
}

String Control::_get_tooltip() const {

	return data.tooltip;
}

void Control::set_focus_neighbour(Margin p_margin, const NodePath &p_neighbour) {

	ERR_FAIL_INDEX(p_margin, 4);
	data.focus_neighbour[p_margin] = p_neighbour;
}

NodePath Control::get_focus_neighbour(Margin p_margin) const {

	ERR_FAIL_INDEX_V(p_margin, 4, NodePath());
	return data.focus_neighbour[p_margin];
}

#define MAX_NEIGHBOUR_SEARCH_COUNT 512

Control *Control::_get_focus_neighbour(Margin p_margin, int p_count) {

	if (p_count >= MAX_NEIGHBOUR_SEARCH_COUNT)
		return NULL;
	if (!data.focus_neighbour[p_margin].is_empty()) {

		Control *c = NULL;
		Node *n = get_node(data.focus_neighbour[p_margin]);
		if (n) {
			c = n->cast_to<Control>();

			if (!c) {

				ERR_EXPLAIN("Next focus node is not a control: " + n->get_name());
				ERR_FAIL_V(NULL);
			}
		} else {
			return NULL;
		}
		bool valid = true;
		if (c->is_hidden())
			valid = false;
		if (c->get_focus_mode() == FOCUS_NONE)
			valid = false;
		if (valid)
			return c;

		c = c->_get_focus_neighbour(p_margin, p_count + 1);
		return c;
	}

	float dist = 1e7;
	Control *result = NULL;

	Point2 points[4];

	Matrix32 xform = get_global_transform();
	Rect2 rect = get_item_rect();

	points[0] = xform.xform(rect.pos);
	points[1] = xform.xform(rect.pos + Point2(rect.size.x, 0));
	points[2] = xform.xform(rect.pos + rect.size);
	points[3] = xform.xform(rect.pos + Point2(0, rect.size.y));

	const Vector2 dir[4] = {
		Vector2(-1, 0),
		Vector2(0, -1),
		Vector2(1, 0),
		Vector2(0, 1)
	};

	Vector2 vdir = dir[p_margin];

	float maxd = -1e7;

	for (int i = 0; i < 4; i++) {

		float d = vdir.dot(points[i]);
		if (d > maxd)
			maxd = d;
	}

	Node *base = this;

	while (base) {

		Control *c = base->cast_to<Control>();
		if (c) {
			if (c->data.SI)
				break;
			if (c->data.RI)
				break;
		}
		base = base->get_parent();
	}

	if (!base)
		return NULL;

	_window_find_focus_neighbour(vdir, base, points, maxd, dist, &result);

	return result;
}

void Control::_window_find_focus_neighbour(const Vector2 &p_dir, Node *p_at, const Point2 *p_points, float p_min, float &r_closest_dist, Control **r_closest) {

	if (p_at->cast_to<Viewport>())
		return; //bye

	Control *c = p_at->cast_to<Control>();

	if (c && c != this && c->get_focus_mode() == FOCUS_ALL && c->is_visible()) {

		Point2 points[4];

		Matrix32 xform = c->get_global_transform();
		Rect2 rect = c->get_item_rect();

		points[0] = xform.xform(rect.pos);
		points[1] = xform.xform(rect.pos + Point2(rect.size.x, 0));
		points[2] = xform.xform(rect.pos + rect.size);
		points[3] = xform.xform(rect.pos + Point2(0, rect.size.y));

		float min = 1e7;

		for (int i = 0; i < 4; i++) {

			float d = p_dir.dot(points[i]);
			if (d < min)
				min = d;
		}

		if (min > (p_min - CMP_EPSILON)) {

			for (int i = 0; i < 4; i++) {

				Vector2 la = p_points[i];
				Vector2 lb = p_points[(i + 1) % 4];

				for (int j = 0; j < 4; j++) {

					Vector2 fa = points[j];
					Vector2 fb = points[(j + 1) % 4];

					Vector2 pa, pb;
					float d = Geometry::get_closest_points_between_segments(la, lb, fa, fb, pa, pb);
					//float d = Geometry::get_closest_distance_between_segments(Vector3(la.x,la.y,0),Vector3(lb.x,lb.y,0),Vector3(fa.x,fa.y,0),Vector3(fb.x,fb.y,0));
					if (d < r_closest_dist) {
						r_closest_dist = d;
						*r_closest = c;
					}
				}
			}
		}
	}

	for (int i = 0; i < p_at->get_child_count(); i++) {

		Node *child = p_at->get_child(i);
		Control *childc = child->cast_to<Control>();
		if (childc && childc->data.SI)
			continue; //subwindow, ignore
		_window_find_focus_neighbour(p_dir, p_at->get_child(i), p_points, p_min, r_closest_dist, r_closest);
	}
}

void Control::set_h_size_flags(int p_flags) {

	if (data.h_size_flags == p_flags)
		return;
	data.h_size_flags = p_flags;
	emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

int Control::get_h_size_flags() const {
	return data.h_size_flags;
}
void Control::set_v_size_flags(int p_flags) {

	if (data.v_size_flags == p_flags)
		return;
	data.v_size_flags = p_flags;
	emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

void Control::set_stretch_ratio(float p_ratio) {

	if (data.expand == p_ratio)
		return;

	data.expand = p_ratio;
	emit_signal(SceneStringNames::get_singleton()->size_flags_changed);
}

float Control::get_stretch_ratio() const {

	return data.expand;
}

void Control::grab_click_focus() {

	ERR_FAIL_COND(!is_inside_tree());

	get_viewport()->_gui_grab_click_focus(this);
}

void Control::minimum_size_changed() {

	if (!is_inside_tree())
		return;

	if (data.pending_min_size_update)
		return;

	data.pending_min_size_update = true;
	MessageQueue::get_singleton()->push_call(this, "_update_minimum_size");

	if (!is_toplevel_control()) {
		Control *pc = get_parent_control();
		if (pc)
			pc->minimum_size_changed();
	}
}

int Control::get_v_size_flags() const {
	return data.v_size_flags;
}

void Control::set_ignore_mouse(bool p_ignore) {

	data.ignore_mouse = p_ignore;
}

bool Control::is_ignoring_mouse() const {

	return data.ignore_mouse;
}

void Control::set_stop_mouse(bool p_stop) {

	data.stop_mouse = p_stop;
}

bool Control::is_stopping_mouse() const {

	return data.stop_mouse;
}

Control *Control::get_focus_owner() const {

	ERR_FAIL_COND_V(!is_inside_tree(), NULL);
	return get_viewport()->_gui_get_focus_owner();
}

void Control::warp_mouse(const Point2 &p_to_pos) {
	ERR_FAIL_COND(!is_inside_tree());
	get_viewport()->warp_mouse(get_global_transform().xform(p_to_pos));
}

bool Control::is_text_field() const {
	/*
    if (get_script_instance()) {
        Variant v=p_point;
        const Variant *p[2]={&v,&p_data};
        Variant::CallError ce;
        Variant ret = get_script_instance()->call("is_text_field",p,2,ce);
        if (ce.error==Variant::CallError::CALL_OK)
            return ret;
    }
  */
	return false;
}

void Control::set_rotation(float p_radians) {

	data.rotation = p_radians;
	update();
	_notify_transform();
	_change_notify("rect/rotation");
}

float Control::get_rotation() const {

	return data.rotation;
}

void Control::set_rotation_deg(float p_degrees) {
	set_rotation(Math::deg2rad(p_degrees));
}

float Control::get_rotation_deg() const {
	return Math::rad2deg(get_rotation());
}

// Kept for compatibility after rename to {s,g}et_rotation_deg.
// Could be removed after a couple releases.
void Control::_set_rotation_deg(float p_degrees) {
	WARN_PRINT("Deprecated method Control._set_rotation_deg(): This method was renamed to set_rotation_deg. Please adapt your code accordingly, as the old method will be obsoleted.");
	set_rotation_deg(p_degrees);
}
float Control::_get_rotation_deg() const {
	WARN_PRINT("Deprecated method Control._get_rotation_deg(): This method was renamed to get_rotation_deg. Please adapt your code accordingly, as the old method will be obsoleted.");
	return get_rotation_deg();
}
//needed to update the control if the font changes..
void Control::_ref_font(Ref<Font> p_sc) {

	if (!data.font_refcount.has(p_sc)) {
		data.font_refcount[p_sc] = 1;
		p_sc->connect("changed", this, "_font_changed");
	} else {
		data.font_refcount[p_sc] += 1;
	}
}

void Control::_unref_font(Ref<Font> p_sc) {

	ERR_FAIL_COND(!data.font_refcount.has(p_sc));
	data.font_refcount[p_sc]--;
	if (data.font_refcount[p_sc] == 0) {
		p_sc->disconnect("changed", this, "_font_changed");
		data.font_refcount.erase(p_sc);
	}
}

void Control::_font_changed() {

	update();
	notification(NOTIFICATION_THEME_CHANGED);
	minimum_size_changed(); //fonts affect minimum size pretty much almost always
}

void Control::set_scale(const Vector2 &p_scale) {

	data.scale = p_scale;
	update();
	_notify_transform();
}
Vector2 Control::get_scale() const {

	return data.scale;
}

Control *Control::get_root_parent_control() const {

	const CanvasItem *ci = this;
	const Control *root = this;

	while (ci) {

		const Control *c = ci->cast_to<Control>();
		if (c) {
			root = c;

			if (c->data.RI || c->data.MI || c->is_toplevel_control())
				break;
		}

		ci = ci->get_parent_item();
	}

	return const_cast<Control *>(root);
}

void Control::_bind_methods() {

	//	ObjectTypeDB::bind_method(_MD("_window_resize_event"),&Control::_window_resize_event);
	ObjectTypeDB::bind_method(_MD("_size_changed"), &Control::_size_changed);
	ObjectTypeDB::bind_method(_MD("_update_minimum_size"), &Control::_update_minimum_size);

	ObjectTypeDB::bind_method(_MD("accept_event"), &Control::accept_event);
	ObjectTypeDB::bind_method(_MD("get_minimum_size"), &Control::get_minimum_size);
	ObjectTypeDB::bind_method(_MD("get_combined_minimum_size"), &Control::get_combined_minimum_size);
	ObjectTypeDB::bind_method(_MD("set_anchor", "margin", "anchor_mode", "keep_margin"), &Control::set_anchor, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("_set_anchor", "margin", "anchor_mode"), &Control::_set_anchor);
	ObjectTypeDB::bind_method(_MD("get_anchor", "margin"), &Control::get_anchor);
	ObjectTypeDB::bind_method(_MD("set_margin", "margin", "offset"), &Control::set_margin);
	ObjectTypeDB::bind_method(_MD("set_anchor_and_margin", "margin", "anchor_mode", "offset"), &Control::set_anchor_and_margin);
	ObjectTypeDB::bind_method(_MD("set_begin", "pos"), &Control::set_begin);
	ObjectTypeDB::bind_method(_MD("set_end", "pos"), &Control::set_end);
	ObjectTypeDB::bind_method(_MD("set_pos", "pos"), &Control::set_pos);
	ObjectTypeDB::bind_method(_MD("set_size", "size"), &Control::set_size);
	ObjectTypeDB::bind_method(_MD("set_custom_minimum_size", "size"), &Control::set_custom_minimum_size);
	ObjectTypeDB::bind_method(_MD("set_global_pos", "pos"), &Control::set_global_pos);
	ObjectTypeDB::bind_method(_MD("set_rotation", "radians"), &Control::set_rotation);
	ObjectTypeDB::bind_method(_MD("set_rotation_deg", "degrees"), &Control::set_rotation_deg);
	// TODO: Obsolete this method (old name) properly (GH-4397)
	ObjectTypeDB::bind_method(_MD("_set_rotation_deg", "degrees"), &Control::_set_rotation_deg);
	ObjectTypeDB::bind_method(_MD("set_scale", "scale"), &Control::set_scale);
	ObjectTypeDB::bind_method(_MD("get_margin", "margin"), &Control::get_margin);
	ObjectTypeDB::bind_method(_MD("get_begin"), &Control::get_begin);
	ObjectTypeDB::bind_method(_MD("get_end"), &Control::get_end);
	ObjectTypeDB::bind_method(_MD("get_pos"), &Control::get_pos);
	ObjectTypeDB::bind_method(_MD("get_size"), &Control::get_size);
	ObjectTypeDB::bind_method(_MD("get_rotation"), &Control::get_rotation);
	ObjectTypeDB::bind_method(_MD("get_rotation_deg"), &Control::get_rotation_deg);
	// TODO: Obsolete this method (old name) properly (GH-4397)
	ObjectTypeDB::bind_method(_MD("_get_rotation_deg"), &Control::_get_rotation_deg);
	ObjectTypeDB::bind_method(_MD("get_scale"), &Control::get_scale);
	ObjectTypeDB::bind_method(_MD("get_custom_minimum_size"), &Control::get_custom_minimum_size);
	ObjectTypeDB::bind_method(_MD("get_parent_area_size"), &Control::get_size);
	ObjectTypeDB::bind_method(_MD("get_global_pos"), &Control::get_global_pos);
	ObjectTypeDB::bind_method(_MD("get_rect"), &Control::get_rect);
	ObjectTypeDB::bind_method(_MD("get_global_rect"), &Control::get_global_rect);
	ObjectTypeDB::bind_method(_MD("set_area_as_parent_rect", "margin"), &Control::set_area_as_parent_rect, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("show_modal", "exclusive"), &Control::show_modal, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("set_focus_mode", "mode"), &Control::set_focus_mode);
	ObjectTypeDB::bind_method(_MD("get_focus_mode"), &Control::get_focus_mode);
	ObjectTypeDB::bind_method(_MD("has_focus"), &Control::has_focus);
	ObjectTypeDB::bind_method(_MD("grab_focus"), &Control::grab_focus);
	ObjectTypeDB::bind_method(_MD("release_focus"), &Control::release_focus);
	ObjectTypeDB::bind_method(_MD("get_focus_owner:Control"), &Control::get_focus_owner);

	ObjectTypeDB::bind_method(_MD("set_h_size_flags", "flags"), &Control::set_h_size_flags);
	ObjectTypeDB::bind_method(_MD("get_h_size_flags"), &Control::get_h_size_flags);

	ObjectTypeDB::bind_method(_MD("set_stretch_ratio", "ratio"), &Control::set_stretch_ratio);
	ObjectTypeDB::bind_method(_MD("get_stretch_ratio"), &Control::get_stretch_ratio);

	ObjectTypeDB::bind_method(_MD("set_v_size_flags", "flags"), &Control::set_v_size_flags);
	ObjectTypeDB::bind_method(_MD("get_v_size_flags"), &Control::get_v_size_flags);

	ObjectTypeDB::bind_method(_MD("set_theme", "theme:Theme"), &Control::set_theme);
	ObjectTypeDB::bind_method(_MD("get_theme:Theme"), &Control::get_theme);

	ObjectTypeDB::bind_method(_MD("add_icon_override", "name", "texture:Texture"), &Control::add_icon_override);
	ObjectTypeDB::bind_method(_MD("add_shader_override", "name", "shader:Shader"), &Control::add_shader_override);
	ObjectTypeDB::bind_method(_MD("add_style_override", "name", "stylebox:StyleBox"), &Control::add_style_override);
	ObjectTypeDB::bind_method(_MD("add_font_override", "name", "font:Font"), &Control::add_font_override);
	ObjectTypeDB::bind_method(_MD("add_color_override", "name", "color"), &Control::add_color_override);
	ObjectTypeDB::bind_method(_MD("add_constant_override", "name", "constant"), &Control::add_constant_override);

	ObjectTypeDB::bind_method(_MD("get_icon:Texture", "name", "type"), &Control::get_icon, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("get_stylebox:StyleBox", "name", "type"), &Control::get_stylebox, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("get_font:Font", "name", "type"), &Control::get_font, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("get_color", "name", "type"), &Control::get_color, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("get_constant", "name", "type"), &Control::get_constant, DEFVAL(""));

	ObjectTypeDB::bind_method(_MD("has_icon_override", "name"), &Control::has_icon_override);
	ObjectTypeDB::bind_method(_MD("has_stylebox_override", "name"), &Control::has_stylebox_override);
	ObjectTypeDB::bind_method(_MD("has_font_override", "name"), &Control::has_font_override);
	ObjectTypeDB::bind_method(_MD("has_color_override", "name"), &Control::has_color_override);
	ObjectTypeDB::bind_method(_MD("has_constant_override", "name"), &Control::has_constant_override);

	ObjectTypeDB::bind_method(_MD("has_icon", "name", "type"), &Control::has_icon, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("has_stylebox", "name", "type"), &Control::has_stylebox, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("has_font", "name", "type"), &Control::has_font, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("has_color", "name", "type"), &Control::has_color, DEFVAL(""));
	ObjectTypeDB::bind_method(_MD("has_constant", "name", "type"), &Control::has_constant, DEFVAL(""));

	ObjectTypeDB::bind_method(_MD("get_parent_control:Control"), &Control::get_parent_control);

	ObjectTypeDB::bind_method(_MD("set_tooltip", "tooltip"), &Control::set_tooltip);
	ObjectTypeDB::bind_method(_MD("get_tooltip", "atpos"), &Control::get_tooltip, DEFVAL(Point2()));
	ObjectTypeDB::bind_method(_MD("_get_tooltip"), &Control::_get_tooltip);

	ObjectTypeDB::bind_method(_MD("set_default_cursor_shape", "shape"), &Control::set_default_cursor_shape);
	ObjectTypeDB::bind_method(_MD("get_default_cursor_shape"), &Control::get_default_cursor_shape);
	ObjectTypeDB::bind_method(_MD("get_cursor_shape", "pos"), &Control::get_cursor_shape, DEFVAL(Point2()));

	ObjectTypeDB::bind_method(_MD("set_focus_neighbour", "margin", "neighbour"), &Control::set_focus_neighbour);
	ObjectTypeDB::bind_method(_MD("get_focus_neighbour", "margin"), &Control::get_focus_neighbour);

	ObjectTypeDB::bind_method(_MD("set_ignore_mouse", "ignore"), &Control::set_ignore_mouse);
	ObjectTypeDB::bind_method(_MD("is_ignoring_mouse"), &Control::is_ignoring_mouse);

	ObjectTypeDB::bind_method(_MD("force_drag", "data", "preview"), &Control::force_drag);

	ObjectTypeDB::bind_method(_MD("set_stop_mouse", "stop"), &Control::set_stop_mouse);
	ObjectTypeDB::bind_method(_MD("is_stopping_mouse"), &Control::is_stopping_mouse);

	ObjectTypeDB::bind_method(_MD("grab_click_focus"), &Control::grab_click_focus);

	ObjectTypeDB::bind_method(_MD("set_drag_forwarding", "target:Control"), &Control::set_drag_forwarding);
	ObjectTypeDB::bind_method(_MD("set_drag_preview", "control:Control"), &Control::set_drag_preview);

	ObjectTypeDB::bind_method(_MD("warp_mouse", "to_pos"), &Control::warp_mouse);

	ObjectTypeDB::bind_method(_MD("minimum_size_changed"), &Control::minimum_size_changed);

	ObjectTypeDB::bind_method(_MD("_theme_changed"), &Control::_theme_changed);

	ObjectTypeDB::bind_method(_MD("_font_changed"), &Control::_font_changed);

	BIND_VMETHOD(MethodInfo("_input_event", PropertyInfo(Variant::INPUT_EVENT, "event")));
	BIND_VMETHOD(MethodInfo(Variant::VECTOR2, "get_minimum_size"));
	BIND_VMETHOD(MethodInfo(Variant::OBJECT, "get_drag_data", PropertyInfo(Variant::VECTOR2, "pos")));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "can_drop_data", PropertyInfo(Variant::VECTOR2, "pos"), PropertyInfo(Variant::NIL, "data")));
	BIND_VMETHOD(MethodInfo("drop_data", PropertyInfo(Variant::VECTOR2, "pos"), PropertyInfo(Variant::NIL, "data")));

	ADD_PROPERTYINZ(PropertyInfo(Variant::INT, "anchor/left", PROPERTY_HINT_ENUM, "Begin,End,Ratio,Center"), _SCS("_set_anchor"), _SCS("get_anchor"), MARGIN_LEFT);
	ADD_PROPERTYINZ(PropertyInfo(Variant::INT, "anchor/top", PROPERTY_HINT_ENUM, "Begin,End,Ratio,Center"), _SCS("_set_anchor"), _SCS("get_anchor"), MARGIN_TOP);
	ADD_PROPERTYINZ(PropertyInfo(Variant::INT, "anchor/right", PROPERTY_HINT_ENUM, "Begin,End,Ratio,Center"), _SCS("_set_anchor"), _SCS("get_anchor"), MARGIN_RIGHT);
	ADD_PROPERTYINZ(PropertyInfo(Variant::INT, "anchor/bottom", PROPERTY_HINT_ENUM, "Begin,End,Ratio,Center"), _SCS("_set_anchor"), _SCS("get_anchor"), MARGIN_BOTTOM);

	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "rect/pos", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), _SCS("set_pos"), _SCS("get_pos"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "rect/size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), _SCS("set_size"), _SCS("get_size"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "rect/min_size"), _SCS("set_custom_minimum_size"), _SCS("get_custom_minimum_size"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::REAL, "rect/rotation", PROPERTY_HINT_RANGE, "-1080,1080,0.01"), _SCS("set_rotation_deg"), _SCS("get_rotation_deg"));
	ADD_PROPERTYNO(PropertyInfo(Variant::VECTOR2, "rect/scale"), _SCS("set_scale"), _SCS("get_scale"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::STRING, "hint/tooltip", PROPERTY_HINT_MULTILINE_TEXT), _SCS("set_tooltip"), _SCS("_get_tooltip"));
	ADD_PROPERTYINZ(PropertyInfo(Variant::NODE_PATH, "focus_neighbour/left"), _SCS("set_focus_neighbour"), _SCS("get_focus_neighbour"), MARGIN_LEFT);
	ADD_PROPERTYINZ(PropertyInfo(Variant::NODE_PATH, "focus_neighbour/top"), _SCS("set_focus_neighbour"), _SCS("get_focus_neighbour"), MARGIN_TOP);
	ADD_PROPERTYINZ(PropertyInfo(Variant::NODE_PATH, "focus_neighbour/right"), _SCS("set_focus_neighbour"), _SCS("get_focus_neighbour"), MARGIN_RIGHT);
	ADD_PROPERTYINZ(PropertyInfo(Variant::NODE_PATH, "focus_neighbour/bottom"), _SCS("set_focus_neighbour"), _SCS("get_focus_neighbour"), MARGIN_BOTTOM);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "focus/ignore_mouse"), _SCS("set_ignore_mouse"), _SCS("is_ignoring_mouse"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "focus/stop_mouse"), _SCS("set_stop_mouse"), _SCS("is_stopping_mouse"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "size_flags/horizontal", PROPERTY_HINT_FLAGS, "Expand,Fill"), _SCS("set_h_size_flags"), _SCS("get_h_size_flags"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "size_flags/vertical", PROPERTY_HINT_FLAGS, "Expand,Fill"), _SCS("set_v_size_flags"), _SCS("get_v_size_flags"));
	ADD_PROPERTYNO(PropertyInfo(Variant::INT, "size_flags/stretch_ratio", PROPERTY_HINT_RANGE, "1,128,0.01"), _SCS("set_stretch_ratio"), _SCS("get_stretch_ratio"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "theme/theme", PROPERTY_HINT_RESOURCE_TYPE, "Theme"), _SCS("set_theme"), _SCS("get_theme"));

	BIND_CONSTANT(ANCHOR_BEGIN);
	BIND_CONSTANT(ANCHOR_END);
	BIND_CONSTANT(ANCHOR_RATIO);
	BIND_CONSTANT(ANCHOR_CENTER);
	BIND_CONSTANT(FOCUS_NONE);
	BIND_CONSTANT(FOCUS_CLICK);
	BIND_CONSTANT(FOCUS_ALL);

	BIND_CONSTANT(NOTIFICATION_RESIZED);
	BIND_CONSTANT(NOTIFICATION_MOUSE_ENTER);
	BIND_CONSTANT(NOTIFICATION_MOUSE_EXIT);
	BIND_CONSTANT(NOTIFICATION_FOCUS_ENTER);
	BIND_CONSTANT(NOTIFICATION_FOCUS_EXIT);
	BIND_CONSTANT(NOTIFICATION_THEME_CHANGED);
	BIND_CONSTANT(NOTIFICATION_MODAL_CLOSE);
	BIND_CONSTANT(NOTIFICATION_SCROLL_BEGIN);
	BIND_CONSTANT(NOTIFICATION_SCROLL_END);

	BIND_CONSTANT(CURSOR_ARROW);
	BIND_CONSTANT(CURSOR_IBEAM);
	BIND_CONSTANT(CURSOR_POINTING_HAND);
	BIND_CONSTANT(CURSOR_CROSS);
	BIND_CONSTANT(CURSOR_WAIT);
	BIND_CONSTANT(CURSOR_BUSY);
	BIND_CONSTANT(CURSOR_DRAG);
	BIND_CONSTANT(CURSOR_CAN_DROP);
	BIND_CONSTANT(CURSOR_FORBIDDEN);
	BIND_CONSTANT(CURSOR_VSIZE);
	BIND_CONSTANT(CURSOR_HSIZE);
	BIND_CONSTANT(CURSOR_BDIAGSIZE);
	BIND_CONSTANT(CURSOR_FDIAGSIZE);
	BIND_CONSTANT(CURSOR_MOVE);
	BIND_CONSTANT(CURSOR_VSPLIT);
	BIND_CONSTANT(CURSOR_HSPLIT);
	BIND_CONSTANT(CURSOR_HELP);

	BIND_CONSTANT(SIZE_EXPAND);
	BIND_CONSTANT(SIZE_FILL);
	BIND_CONSTANT(SIZE_EXPAND_FILL);

	ADD_SIGNAL(MethodInfo("resized"));
	ADD_SIGNAL(MethodInfo("input_event", PropertyInfo(Variant::INPUT_EVENT, "ev")));
	ADD_SIGNAL(MethodInfo("mouse_enter"));
	ADD_SIGNAL(MethodInfo("mouse_exit"));
	ADD_SIGNAL(MethodInfo("focus_enter"));
	ADD_SIGNAL(MethodInfo("focus_exit"));
	ADD_SIGNAL(MethodInfo("size_flags_changed"));
	ADD_SIGNAL(MethodInfo("minimum_size_changed"));
	ADD_SIGNAL(MethodInfo("modal_close"));

	BIND_VMETHOD(MethodInfo("has_point", PropertyInfo(Variant::VECTOR2, "point")));
}
Control::Control() {

	data.parent = NULL;

	data.ignore_mouse = false;
	data.stop_mouse = true;

	data.SI = NULL;
	data.MI = NULL;
	data.RI = NULL;
	data.modal = false;
	data.theme_owner = NULL;
	data.modal_exclusive = false;
	data.default_cursor = CURSOR_ARROW;
	data.h_size_flags = SIZE_FILL;
	data.v_size_flags = SIZE_FILL;
	data.expand = 1;
	data.pending_min_size_update = false;
	data.rotation = 0;
	data.parent_canvas_item = NULL;
	data.scale = Vector2(1, 1);
	data.drag_owner = 0;
	data.modal_frame = 0;

	for (int i = 0; i < 4; i++) {
		data.anchor[i] = ANCHOR_BEGIN;
		data.margin[i] = 0;
	}
	data.focus_mode = FOCUS_NONE;
	data.modal_prev_focus_owner = 0;
}

Control::~Control() {
}
