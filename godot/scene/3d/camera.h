/*************************************************************************/
/*  camera.h                                                             */
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
#ifndef CAMERA_H
#define CAMERA_H

#include "scene/3d/spatial.h"
#include "scene/main/viewport.h"
#include "scene/resources/environment.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class Camera : public Spatial {

	OBJ_TYPE(Camera, Spatial);

public:
	enum Projection {

		PROJECTION_PERSPECTIVE,
		PROJECTION_ORTHOGONAL
	};

	enum KeepAspect {
		KEEP_WIDTH,
		KEEP_HEIGHT
	};

private:
	bool force_change;
	bool current;

	Projection mode;

	float fov;
	float size;
	float near, far;
	float v_offset;
	float h_offset;
	KeepAspect keep_aspect;

	RID camera;
	RID scenario_id;

	//String camera_group;

	uint32_t layers;

	Ref<Environment> environment;

	virtual bool _can_gizmo_scale() const;
	virtual RES _get_gizmo_geometry() const;

	//void _camera_make_current(Node *p_camera);
	friend class Viewport;
	void _update_audio_listener_state();

protected:
	void _update_camera();
	virtual void _request_camera_update();
	void _update_camera_mode();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _notification(int p_what);

	static void _bind_methods();

public:
	enum {

		NOTIFICATION_BECAME_CURRENT = 50,
		NOTIFICATION_LOST_CURRENT = 51
	};

	void set_perspective(float p_fovy_degrees, float p_z_near, float p_z_far);
	void set_orthogonal(float p_size, float p_z_near, float p_z_far);

	void make_current();
	void clear_current();
	bool is_current() const;

	RID get_camera() const;

	float get_fov() const;
	float get_size() const;
	float get_zfar() const;
	float get_znear() const;
	Projection get_projection() const;

	virtual Transform get_camera_transform() const;

	Vector3 project_ray_normal(const Point2 &p_point) const;
	Vector3 project_ray_origin(const Point2 &p_point) const;
	Vector3 project_local_ray_normal(const Point2 &p_point) const;
	Point2 unproject_position(const Vector3 &p_pos) const;
	bool is_position_behind(const Vector3 &p_pos) const;
	Vector3 project_position(const Point2 &p_point) const;

	void set_visible_layers(uint32_t p_layers);
	uint32_t get_visible_layers() const;

	Vector<Plane> get_frustum() const;

	void set_environment(const Ref<Environment> &p_environment);
	Ref<Environment> get_environment() const;

	void set_keep_aspect_mode(KeepAspect p_aspect);
	KeepAspect get_keep_aspect_mode() const;

	void set_v_offset(float p_offset);
	float get_v_offset() const;

	void set_h_offset(float p_offset);
	float get_h_offset() const;

	Camera();
	~Camera();
};

VARIANT_ENUM_CAST(Camera::Projection);
VARIANT_ENUM_CAST(Camera::KeepAspect);

#endif
