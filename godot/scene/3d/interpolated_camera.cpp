/*************************************************************************/
/*  interpolated_camera.cpp                                              */
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
#include "interpolated_camera.h"

void InterpolatedCamera::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {

			if (get_tree()->is_editor_hint() && enabled)
				set_fixed_process(false);

		} break;
		case NOTIFICATION_PROCESS: {

			if (!enabled)
				break;
			if (has_node(target)) {

				Spatial *node = get_node(target)->cast_to<Spatial>();
				if (!node)
					break;

				float delta = speed * get_process_delta_time();
				Transform target_xform = node->get_global_transform();
				Transform local_transform = get_global_transform();
				local_transform = local_transform.interpolate_with(target_xform, delta);
				set_global_transform(local_transform);

				if (node->cast_to<Camera>()) {
					Camera *cam = node->cast_to<Camera>();
					if (cam->get_projection() == get_projection()) {

						float new_near = Math::lerp(get_znear(), cam->get_znear(), delta);
						float new_far = Math::lerp(get_zfar(), cam->get_zfar(), delta);

						if (cam->get_projection() == PROJECTION_ORTHOGONAL) {

							float size = Math::lerp(get_size(), cam->get_size(), delta);
							set_orthogonal(size, new_near, new_far);
						} else {

							float fov = Math::lerp(get_fov(), cam->get_fov(), delta);
							set_perspective(fov, new_near, new_far);
						}
					}
				}
			}

		} break;
	}
}

void InterpolatedCamera::_set_target(const Object *p_target) {

	ERR_FAIL_NULL(p_target);
	set_target(p_target->cast_to<Spatial>());
}

void InterpolatedCamera::set_target(const Spatial *p_target) {

	ERR_FAIL_NULL(p_target);
	target = get_path_to(p_target);
}

void InterpolatedCamera::set_target_path(const NodePath &p_path) {

	target = p_path;
}

NodePath InterpolatedCamera::get_target_path() const {

	return target;
}

void InterpolatedCamera::set_interpolation_enabled(bool p_enable) {

	if (enabled == p_enable)
		return;
	enabled = p_enable;
	if (p_enable) {
		if (is_inside_tree() && get_tree()->is_editor_hint())
			return;
		set_process(true);
	} else
		set_process(false);
}

bool InterpolatedCamera::is_interpolation_enabled() const {

	return enabled;
}

void InterpolatedCamera::set_speed(real_t p_speed) {

	speed = p_speed;
}

real_t InterpolatedCamera::get_speed() const {

	return speed;
}

void InterpolatedCamera::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_target_path", "target_path"), &InterpolatedCamera::set_target_path);
	ObjectTypeDB::bind_method(_MD("get_target_path"), &InterpolatedCamera::get_target_path);
	ObjectTypeDB::bind_method(_MD("set_target", "target:Camera"), &InterpolatedCamera::_set_target);

	ObjectTypeDB::bind_method(_MD("set_speed", "speed"), &InterpolatedCamera::set_speed);
	ObjectTypeDB::bind_method(_MD("get_speed"), &InterpolatedCamera::get_speed);

	ObjectTypeDB::bind_method(_MD("set_interpolation_enabled", "target_path"), &InterpolatedCamera::set_interpolation_enabled);
	ObjectTypeDB::bind_method(_MD("is_interpolation_enabled"), &InterpolatedCamera::is_interpolation_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target"), _SCS("set_target_path"), _SCS("get_target_path"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "speed"), _SCS("set_speed"), _SCS("get_speed"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), _SCS("set_interpolation_enabled"), _SCS("is_interpolation_enabled"));
}

InterpolatedCamera::InterpolatedCamera() {

	enabled = false;
	speed = 1;
}
