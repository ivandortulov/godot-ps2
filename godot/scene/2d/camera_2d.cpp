/*************************************************************************/
/*  camera_2d.cpp                                                        */
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
#include "camera_2d.h"
#include "scene/scene_string_names.h"
#include "servers/visual_server.h"

void Camera2D::_update_scroll() {

	if (!is_inside_tree())
		return;

	if (get_tree()->is_editor_hint()) {
		update(); //will just be drawn
		return;
	}

	if (current) {

		ERR_FAIL_COND(custom_viewport && !ObjectDB::get_instance(custom_viewport_id));

		Matrix32 xform = get_camera_transform();

		if (viewport) {
			viewport->set_canvas_transform(xform);
		}
		get_tree()->call_group(SceneTree::GROUP_CALL_REALTIME, group_name, "_camera_moved", xform);
	};
}

void Camera2D::set_zoom(const Vector2 &p_zoom) {

	zoom = p_zoom;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
};

Vector2 Camera2D::get_zoom() const {

	return zoom;
};

Matrix32 Camera2D::get_camera_transform() {

	if (!get_tree())
		return Matrix32();

	Size2 screen_size = get_viewport_rect().size;
	screen_size = get_viewport_rect().size;

	Point2 new_camera_pos = get_global_transform().get_origin();
	Point2 ret_camera_pos;

	if (!first) {

		if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {

			if (h_drag_enabled && !get_tree()->is_editor_hint()) {
				camera_pos.x = MIN(camera_pos.x, (new_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_RIGHT]));
				camera_pos.x = MAX(camera_pos.x, (new_camera_pos.x - screen_size.x * 0.5 * drag_margin[MARGIN_LEFT]));
			} else {

				if (h_ofs < 0) {
					camera_pos.x = new_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_RIGHT] * h_ofs;
				} else {
					camera_pos.x = new_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_LEFT] * h_ofs;
				}
			}

			if (v_drag_enabled && !get_tree()->is_editor_hint()) {

				camera_pos.y = MIN(camera_pos.y, (new_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_BOTTOM]));
				camera_pos.y = MAX(camera_pos.y, (new_camera_pos.y - screen_size.y * 0.5 * drag_margin[MARGIN_TOP]));

			} else {

				if (v_ofs < 0) {
					camera_pos.y = new_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_TOP] * v_ofs;
				} else {
					camera_pos.y = new_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_BOTTOM] * v_ofs;
				}
			}

		} else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {

			camera_pos = new_camera_pos;
		}

		if (smoothing_enabled && !get_tree()->is_editor_hint()) {

			float c = smoothing * get_fixed_process_delta_time();
			smoothed_camera_pos = ((camera_pos - smoothed_camera_pos) * c) + smoothed_camera_pos;
			ret_camera_pos = smoothed_camera_pos;
			//			camera_pos=camera_pos*(1.0-smoothing)+new_camera_pos*smoothing;
		} else {

			ret_camera_pos = smoothed_camera_pos = camera_pos;
		}

	} else {
		ret_camera_pos = smoothed_camera_pos = camera_pos = new_camera_pos;
		first = false;
	}

	Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5 * zoom) : Point2());

	float angle = get_global_transform().get_rotation();
	if (rotating) {
		screen_offset = screen_offset.rotated(angle);
	}

	Rect2 screen_rect(-screen_offset + ret_camera_pos, screen_size * zoom);
	if (screen_rect.pos.x + screen_rect.size.x > limit[MARGIN_RIGHT])
		screen_rect.pos.x = limit[MARGIN_RIGHT] - screen_rect.size.x;

	if (screen_rect.pos.y + screen_rect.size.y > limit[MARGIN_BOTTOM])
		screen_rect.pos.y = limit[MARGIN_BOTTOM] - screen_rect.size.y;

	if (screen_rect.pos.x < limit[MARGIN_LEFT])
		screen_rect.pos.x = limit[MARGIN_LEFT];

	if (screen_rect.pos.y < limit[MARGIN_TOP])
		screen_rect.pos.y = limit[MARGIN_TOP];

	if (offset != Vector2()) {

		screen_rect.pos += offset;
		if (screen_rect.pos.x + screen_rect.size.x > limit[MARGIN_RIGHT])
			screen_rect.pos.x = limit[MARGIN_RIGHT] - screen_rect.size.x;

		if (screen_rect.pos.y + screen_rect.size.y > limit[MARGIN_BOTTOM])
			screen_rect.pos.y = limit[MARGIN_BOTTOM] - screen_rect.size.y;

		if (screen_rect.pos.x < limit[MARGIN_LEFT])
			screen_rect.pos.x = limit[MARGIN_LEFT];

		if (screen_rect.pos.y < limit[MARGIN_TOP])
			screen_rect.pos.y = limit[MARGIN_TOP];
	}

	camera_screen_center = screen_rect.pos + screen_rect.size * 0.5;

	Matrix32 xform;
	if (rotating) {
		xform.set_rotation(angle);
	}
	xform.scale_basis(zoom);
	xform.set_origin(screen_rect.pos /*.floor()*/);

	/*
	if (0) {

		xform = get_global_transform() * xform;
	} else {

		xform.elements[2]+=get_global_transform().get_origin();
	}
*/

	return (xform).affine_inverse();
}

void Camera2D::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_FIXED_PROCESS: {

			_update_scroll();

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {

			if (!is_fixed_processing())
				_update_scroll();

		} break;
		case NOTIFICATION_ENTER_TREE: {

			if (custom_viewport && ObjectDB::get_instance(custom_viewport_id)) {
				viewport = custom_viewport;
			} else {
				viewport = get_viewport();
			}

			canvas = get_canvas();

			RID vp = viewport->get_viewport();

			group_name = "__cameras_" + itos(vp.get_id());
			canvas_group_name = "__cameras_c" + itos(canvas.get_id());
			add_to_group(group_name);
			add_to_group(canvas_group_name);

			if (get_tree()->is_editor_hint()) {
				set_fixed_process(false);
			}

			_update_scroll();
			first = true;

		} break;
		case NOTIFICATION_EXIT_TREE: {

			if (is_current()) {
				if (viewport && !(custom_viewport && !ObjectDB::get_instance(custom_viewport_id))) {
					viewport->set_canvas_transform(Matrix32());
				}
			}
			remove_from_group(group_name);
			remove_from_group(canvas_group_name);
			viewport = NULL;

		} break;
		case NOTIFICATION_DRAW: {

			if (!is_inside_tree() || !get_tree()->is_editor_hint())
				break;

			Color area_axis_color(0.5, 0.42, 0.87, 0.63);
			float area_axis_width = 1;
			if (current)
				area_axis_width = 3;

			Matrix32 inv_camera_transform = get_camera_transform().affine_inverse();
			Size2 screen_size = get_viewport_rect().size;

			Vector2 screen_endpoints[4] = {
				inv_camera_transform.xform(Vector2(0, 0)),
				inv_camera_transform.xform(Vector2(screen_size.width, 0)),
				inv_camera_transform.xform(Vector2(screen_size.width, screen_size.height)),
				inv_camera_transform.xform(Vector2(0, screen_size.height))
			};

			Matrix32 inv_transform = get_global_transform().affine_inverse(); // undo global space

			for (int i = 0; i < 4; i++) {
				draw_line(inv_transform.xform(screen_endpoints[i]), inv_transform.xform(screen_endpoints[(i + 1) % 4]), area_axis_color, area_axis_width);
			}

		} break;
	}
}

void Camera2D::set_offset(const Vector2 &p_offset) {

	offset = p_offset;
	_update_scroll();
}

Vector2 Camera2D::get_offset() const {

	return offset;
}

void Camera2D::set_anchor_mode(AnchorMode p_anchor_mode) {

	anchor_mode = p_anchor_mode;
	_update_scroll();
}

Camera2D::AnchorMode Camera2D::get_anchor_mode() const {

	return anchor_mode;
}

void Camera2D::set_rotating(bool p_rotating) {

	rotating = p_rotating;
	_update_scroll();
}

bool Camera2D::is_rotating() const {

	return rotating;
}

void Camera2D::_make_current(Object *p_which) {

	if (p_which == this) {

		current = true;
		_update_scroll();
	} else {
		current = false;
	}
}

void Camera2D::_set_current(bool p_current) {

	if (p_current)
		make_current();

	current = p_current;
}

bool Camera2D::is_current() const {

	return current;
}

void Camera2D::make_current() {

	if (!is_inside_tree()) {
		current = true;
	} else {
		get_tree()->call_group(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", this);
	}
}

void Camera2D::clear_current() {

	current = false;
	if (is_inside_tree()) {
		get_tree()->call_group(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", (Object *)(NULL));
	}
}

void Camera2D::set_limit(Margin p_margin, int p_limit) {

	ERR_FAIL_INDEX(p_margin, 4);
	limit[p_margin] = p_limit;
}

int Camera2D::get_limit(Margin p_margin) const {

	ERR_FAIL_INDEX_V(p_margin, 4, 0);
	return limit[p_margin];
}

void Camera2D::set_drag_margin(Margin p_margin, float p_drag_margin) {

	ERR_FAIL_INDEX(p_margin, 4);
	drag_margin[p_margin] = p_drag_margin;
}

float Camera2D::get_drag_margin(Margin p_margin) const {

	ERR_FAIL_INDEX_V(p_margin, 4, 0);
	return drag_margin[p_margin];
}

Vector2 Camera2D::get_camera_pos() const {

	return camera_pos;
}

void Camera2D::force_update_scroll() {

	_update_scroll();
}

void Camera2D::reset_smoothing() {

	smoothed_camera_pos = camera_pos;
	_update_scroll();
}

void Camera2D::align() {

	Size2 screen_size = get_viewport_rect().size;
	screen_size = get_viewport_rect().size;
	Point2 current_camera_pos = get_global_transform().get_origin();
	if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {
		if (h_ofs < 0) {
			camera_pos.x = current_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_RIGHT] * h_ofs;
		} else {
			camera_pos.x = current_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_LEFT] * h_ofs;
		}
		if (v_ofs < 0) {
			camera_pos.y = current_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_TOP] * v_ofs;
		} else {
			camera_pos.y = current_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_BOTTOM] * v_ofs;
		}
	} else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {

		camera_pos = current_camera_pos;
	}

	_update_scroll();
}

void Camera2D::set_follow_smoothing(float p_speed) {

	smoothing = p_speed;
	if (smoothing > 0 && !(is_inside_tree() && get_tree()->is_editor_hint()))
		set_fixed_process(true);
	else
		set_fixed_process(false);
}

float Camera2D::get_follow_smoothing() const {

	return smoothing;
}

Point2 Camera2D::get_camera_screen_center() const {

	return camera_screen_center;
}

void Camera2D::set_h_drag_enabled(bool p_enabled) {

	h_drag_enabled = p_enabled;
}

bool Camera2D::is_h_drag_enabled() const {

	return h_drag_enabled;
}

void Camera2D::set_v_drag_enabled(bool p_enabled) {

	v_drag_enabled = p_enabled;
}

bool Camera2D::is_v_drag_enabled() const {

	return v_drag_enabled;
}

void Camera2D::set_v_offset(float p_offset) {

	v_ofs = p_offset;
}

float Camera2D::get_v_offset() const {

	return v_ofs;
}

void Camera2D::set_h_offset(float p_offset) {

	h_ofs = p_offset;
}
float Camera2D::get_h_offset() const {

	return h_ofs;
}

void Camera2D::_set_old_smoothing(float p_val) {
	//compatibility
	if (p_val > 0) {
		smoothing_enabled = true;
		set_follow_smoothing(p_val);
	}
}

void Camera2D::set_enable_follow_smoothing(bool p_enabled) {

	smoothing_enabled = p_enabled;
}

bool Camera2D::is_follow_smoothing_enabled() const {

	return smoothing_enabled;
}

void Camera2D::set_custom_viewport(Node *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	if (is_inside_tree()) {
		remove_from_group(group_name);
		remove_from_group(canvas_group_name);
	}

	custom_viewport = p_viewport->cast_to<Viewport>();

	if (custom_viewport) {
		custom_viewport_id = custom_viewport->get_instance_ID();
	} else {
		custom_viewport_id = 0;
	}

	if (is_inside_tree()) {

		if (custom_viewport)
			viewport = custom_viewport;
		else
			viewport = get_viewport();

		RID vp = viewport->get_viewport();
		group_name = "__cameras_" + itos(vp.get_id());
		canvas_group_name = "__cameras_c" + itos(canvas.get_id());
		add_to_group(group_name);
		add_to_group(canvas_group_name);
	}
}

Node *Camera2D::get_custom_viewport() const {

	return custom_viewport;
}

void Camera2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_offset", "offset"), &Camera2D::set_offset);
	ObjectTypeDB::bind_method(_MD("get_offset"), &Camera2D::get_offset);

	ObjectTypeDB::bind_method(_MD("set_anchor_mode", "anchor_mode"), &Camera2D::set_anchor_mode);
	ObjectTypeDB::bind_method(_MD("get_anchor_mode"), &Camera2D::get_anchor_mode);

	ObjectTypeDB::bind_method(_MD("set_rotating", "rotating"), &Camera2D::set_rotating);
	ObjectTypeDB::bind_method(_MD("is_rotating"), &Camera2D::is_rotating);

	ObjectTypeDB::bind_method(_MD("make_current"), &Camera2D::make_current);
	ObjectTypeDB::bind_method(_MD("clear_current"), &Camera2D::clear_current);
	ObjectTypeDB::bind_method(_MD("_make_current"), &Camera2D::_make_current);

	ObjectTypeDB::bind_method(_MD("_update_scroll"), &Camera2D::_update_scroll);

	ObjectTypeDB::bind_method(_MD("_set_current", "current"), &Camera2D::_set_current);
	ObjectTypeDB::bind_method(_MD("is_current"), &Camera2D::is_current);

	ObjectTypeDB::bind_method(_MD("set_limit", "margin", "limit"), &Camera2D::set_limit);
	ObjectTypeDB::bind_method(_MD("get_limit", "margin"), &Camera2D::get_limit);

	ObjectTypeDB::bind_method(_MD("set_v_drag_enabled", "enabled"), &Camera2D::set_v_drag_enabled);
	ObjectTypeDB::bind_method(_MD("is_v_drag_enabled"), &Camera2D::is_v_drag_enabled);

	ObjectTypeDB::bind_method(_MD("set_h_drag_enabled", "enabled"), &Camera2D::set_h_drag_enabled);
	ObjectTypeDB::bind_method(_MD("is_h_drag_enabled"), &Camera2D::is_h_drag_enabled);

	ObjectTypeDB::bind_method(_MD("set_v_offset", "ofs"), &Camera2D::set_v_offset);
	ObjectTypeDB::bind_method(_MD("get_v_offset"), &Camera2D::get_v_offset);

	ObjectTypeDB::bind_method(_MD("set_h_offset", "ofs"), &Camera2D::set_h_offset);
	ObjectTypeDB::bind_method(_MD("get_h_offset"), &Camera2D::get_h_offset);

	ObjectTypeDB::bind_method(_MD("set_drag_margin", "margin", "drag_margin"), &Camera2D::set_drag_margin);
	ObjectTypeDB::bind_method(_MD("get_drag_margin", "margin"), &Camera2D::get_drag_margin);

	ObjectTypeDB::bind_method(_MD("get_camera_pos"), &Camera2D::get_camera_pos);
	ObjectTypeDB::bind_method(_MD("get_camera_screen_center"), &Camera2D::get_camera_screen_center);

	ObjectTypeDB::bind_method(_MD("set_zoom", "zoom"), &Camera2D::set_zoom);
	ObjectTypeDB::bind_method(_MD("get_zoom"), &Camera2D::get_zoom);

	ObjectTypeDB::bind_method(_MD("set_custom_viewport", "viewport:Viewport"), &Camera2D::set_custom_viewport);
	ObjectTypeDB::bind_method(_MD("get_custom_viewport:Viewport"), &Camera2D::get_custom_viewport);

	ObjectTypeDB::bind_method(_MD("set_follow_smoothing", "follow_smoothing"), &Camera2D::set_follow_smoothing);
	ObjectTypeDB::bind_method(_MD("get_follow_smoothing"), &Camera2D::get_follow_smoothing);

	ObjectTypeDB::bind_method(_MD("set_enable_follow_smoothing", "follow_smoothing"), &Camera2D::set_enable_follow_smoothing);
	ObjectTypeDB::bind_method(_MD("is_follow_smoothing_enabled"), &Camera2D::is_follow_smoothing_enabled);

	ObjectTypeDB::bind_method(_MD("force_update_scroll"), &Camera2D::force_update_scroll);
	ObjectTypeDB::bind_method(_MD("reset_smoothing"), &Camera2D::reset_smoothing);
	ObjectTypeDB::bind_method(_MD("align"), &Camera2D::align);

	ObjectTypeDB::bind_method(_MD("_set_old_smoothing", "follow_smoothing"), &Camera2D::_set_old_smoothing);

	ADD_PROPERTYNZ(PropertyInfo(Variant::VECTOR2, "offset"), _SCS("set_offset"), _SCS("get_offset"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "anchor_mode", PROPERTY_HINT_ENUM, "Fixed TopLeft,Drag Center"), _SCS("set_anchor_mode"), _SCS("get_anchor_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rotating"), _SCS("set_rotating"), _SCS("is_rotating"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "current"), _SCS("_set_current"), _SCS("is_current"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "zoom"), _SCS("set_zoom"), _SCS("get_zoom"));

	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit/left"), _SCS("set_limit"), _SCS("get_limit"), MARGIN_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit/top"), _SCS("set_limit"), _SCS("get_limit"), MARGIN_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit/right"), _SCS("set_limit"), _SCS("get_limit"), MARGIN_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit/bottom"), _SCS("set_limit"), _SCS("get_limit"), MARGIN_BOTTOM);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_margin/h_enabled"), _SCS("set_h_drag_enabled"), _SCS("is_h_drag_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_margin/v_enabled"), _SCS("set_v_drag_enabled"), _SCS("is_v_drag_enabled"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smoothing/enable"), _SCS("set_enable_follow_smoothing"), _SCS("is_follow_smoothing_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "smoothing/speed"), _SCS("set_follow_smoothing"), _SCS("get_follow_smoothing"));

	//compatibility
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "smoothing", PROPERTY_HINT_NONE, "", 0), _SCS("_set_old_smoothing"), _SCS("get_follow_smoothing"));

	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin/left", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_drag_margin"), _SCS("get_drag_margin"), MARGIN_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin/top", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_drag_margin"), _SCS("get_drag_margin"), MARGIN_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin/right", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_drag_margin"), _SCS("get_drag_margin"), MARGIN_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin/bottom", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_drag_margin"), _SCS("get_drag_margin"), MARGIN_BOTTOM);

	BIND_CONSTANT(ANCHOR_MODE_DRAG_CENTER);
	BIND_CONSTANT(ANCHOR_MODE_FIXED_TOP_LEFT);
}

Camera2D::Camera2D() {

	anchor_mode = ANCHOR_MODE_DRAG_CENTER;
	rotating = false;
	current = false;
	limit[MARGIN_LEFT] = -10000000;
	limit[MARGIN_TOP] = -10000000;
	limit[MARGIN_RIGHT] = 10000000;
	limit[MARGIN_BOTTOM] = 10000000;
	drag_margin[MARGIN_LEFT] = 0.2;
	drag_margin[MARGIN_TOP] = 0.2;
	drag_margin[MARGIN_RIGHT] = 0.2;
	drag_margin[MARGIN_BOTTOM] = 0.2;
	camera_pos = Vector2();
	first = true;
	smoothing_enabled = false;
	custom_viewport = NULL;
	custom_viewport_id = 0;

	smoothing = 5.0;
	zoom = Vector2(1, 1);

	h_drag_enabled = true;
	v_drag_enabled = true;
	h_ofs = 0;
	v_ofs = 0;
}
