/*************************************************************************/
/*  camera_matrix.h                                                      */
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
#ifndef CAMERA_MATRIX_H
#define CAMERA_MATRIX_H

#include "transform.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/

struct CameraMatrix {

	enum Planes {
		PLANE_NEAR,
		PLANE_FAR,
		PLANE_LEFT,
		PLANE_TOP,
		PLANE_RIGHT,
		PLANE_BOTTOM
	};

	float matrix[4][4];

	void set_identity();
	void set_zero();
	void set_light_bias();
	void set_perspective(float p_fovy_degrees, float p_aspect, float p_z_near, float p_z_far, bool p_flip_fov = false);
	void set_orthogonal(float p_left, float p_right, float p_bottom, float p_top, float p_znear, float p_zfar);
	void set_orthogonal(float p_size, float p_aspect, float p_znear, float p_zfar, bool p_flip_fov = false);
	void set_frustum(float p_left, float p_right, float p_bottom, float p_top, float p_near, float p_far);

	static float get_fovy(float p_fovx, float p_aspect) {

		return Math::rad2deg(Math::atan(p_aspect * Math::tan(Math::deg2rad(p_fovx) * 0.5)) * 2.0);
	}

	float get_z_far() const;
	float get_z_near() const;
	float get_aspect() const;
	float get_fov() const;

	Vector<Plane> get_projection_planes(const Transform &p_transform) const;

	bool get_endpoints(const Transform &p_transform, Vector3 *p_8points) const;
	void get_viewport_size(float &r_width, float &r_height) const;

	void invert();
	CameraMatrix inverse() const;

	CameraMatrix operator*(const CameraMatrix &p_matrix) const;

	Plane xform4(const Plane &p_vec4);
	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_vec3) const;

	operator String() const;

	void scale_translate_to_fit(const AABB &p_aabb);
	void make_scale(const Vector3 &p_scale);
	operator Transform() const;

	CameraMatrix();
	CameraMatrix(const Transform &p_transform);
	~CameraMatrix();
};

Vector3 CameraMatrix::xform(const Vector3 &p_vec3) const {

	Vector3 ret;
	ret.x = matrix[0][0] * p_vec3.x + matrix[1][0] * p_vec3.y + matrix[2][0] * p_vec3.z + matrix[3][0];
	ret.y = matrix[0][1] * p_vec3.x + matrix[1][1] * p_vec3.y + matrix[2][1] * p_vec3.z + matrix[3][1];
	ret.z = matrix[0][2] * p_vec3.x + matrix[1][2] * p_vec3.y + matrix[2][2] * p_vec3.z + matrix[3][2];
	float w = matrix[0][3] * p_vec3.x + matrix[1][3] * p_vec3.y + matrix[2][3] * p_vec3.z + matrix[3][3];
	return ret / w;
}

#endif
