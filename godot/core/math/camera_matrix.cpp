/*************************************************************************/
/*  camera_matrix.cpp                                                    */
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
#include "camera_matrix.h"
#include "math_funcs.h"
#include "print_string.h"

void CameraMatrix::set_identity() {

	for (int i = 0; i < 4; i++) {

		for (int j = 0; j < 4; j++) {

			matrix[i][j] = (i == j) ? 1 : 0;
		}
	}
}

void CameraMatrix::set_zero() {

	for (int i = 0; i < 4; i++) {

		for (int j = 0; j < 4; j++) {

			matrix[i][j] = 0;
		}
	}
}

Plane CameraMatrix::xform4(const Plane &p_vec4) {

	Plane ret;

	ret.normal.x = matrix[0][0] * p_vec4.normal.x + matrix[1][0] * p_vec4.normal.y + matrix[2][0] * p_vec4.normal.z + matrix[3][0] * p_vec4.d;
	ret.normal.y = matrix[0][1] * p_vec4.normal.x + matrix[1][1] * p_vec4.normal.y + matrix[2][1] * p_vec4.normal.z + matrix[3][1] * p_vec4.d;
	ret.normal.z = matrix[0][2] * p_vec4.normal.x + matrix[1][2] * p_vec4.normal.y + matrix[2][2] * p_vec4.normal.z + matrix[3][2] * p_vec4.d;
	ret.d = matrix[0][3] * p_vec4.normal.x + matrix[1][3] * p_vec4.normal.y + matrix[2][3] * p_vec4.normal.z + matrix[3][3] * p_vec4.d;
	return ret;
}

void CameraMatrix::set_perspective(float p_fovy_degrees, float p_aspect, float p_z_near, float p_z_far, bool p_flip_fov) {

	if (p_flip_fov) {
		p_fovy_degrees = get_fovy(p_fovy_degrees, 1.0 / p_aspect);
	}

	float sine, cotangent, deltaZ;
	float radians = p_fovy_degrees / 2.0 * Math_PI / 180.0;

	deltaZ = p_z_far - p_z_near;
	sine = Math::sin(radians);

	if ((deltaZ == 0) || (sine == 0) || (p_aspect == 0)) {
		return;
	}
	cotangent = Math::cos(radians) / sine;

	set_identity();

	matrix[0][0] = cotangent / p_aspect;
	matrix[1][1] = cotangent;
	matrix[2][2] = -(p_z_far + p_z_near) / deltaZ;
	matrix[2][3] = -1;
	matrix[3][2] = -2 * p_z_near * p_z_far / deltaZ;
	matrix[3][3] = 0;
}

void CameraMatrix::set_orthogonal(float p_left, float p_right, float p_bottom, float p_top, float p_znear, float p_zfar) {

	set_identity();

	matrix[0][0] = 2.0 / (p_right - p_left);
	matrix[3][0] = -((p_right + p_left) / (p_right - p_left));
	matrix[1][1] = 2.0 / (p_top - p_bottom);
	matrix[3][1] = -((p_top + p_bottom) / (p_top - p_bottom));
	matrix[2][2] = -2.0 / (p_zfar - p_znear);
	matrix[3][2] = -((p_zfar + p_znear) / (p_zfar - p_znear));
	matrix[3][3] = 1.0;
}

void CameraMatrix::set_orthogonal(float p_size, float p_aspect, float p_znear, float p_zfar, bool p_flip_fov) {

	if (!p_flip_fov) {
		p_size *= p_aspect;
	}

	set_orthogonal(-p_size / 2, +p_size / 2, -p_size / p_aspect / 2, +p_size / p_aspect / 2, p_znear, p_zfar);
}

void CameraMatrix::set_frustum(float p_left, float p_right, float p_bottom, float p_top, float p_near, float p_far) {
#if 0
	///@TODO, give a check to this. I'm not sure if it's working.
	set_identity();

	matrix[0][0]=(2*p_near) / (p_right-p_left);
	matrix[0][2]=(p_right+p_left) / (p_right-p_left);
	matrix[1][1]=(2*p_near) / (p_top-p_bottom);
	matrix[1][2]=(p_top+p_bottom) / (p_top-p_bottom);
	matrix[2][2]=-(p_far+p_near) / ( p_far-p_near);
	matrix[2][3]=-(2*p_far*p_near) / (p_far-p_near);
	matrix[3][2]=-1;
	matrix[3][3]=0;
#else
	float *te = &matrix[0][0];
	float x = 2 * p_near / (p_right - p_left);
	float y = 2 * p_near / (p_top - p_bottom);

	float a = (p_right + p_left) / (p_right - p_left);
	float b = (p_top + p_bottom) / (p_top - p_bottom);
	float c = -(p_far + p_near) / (p_far - p_near);
	float d = -2 * p_far * p_near / (p_far - p_near);

	te[0] = x;
	te[1] = 0;
	te[2] = 0;
	te[3] = 0;
	te[4] = 0;
	te[5] = y;
	te[6] = 0;
	te[7] = 0;
	te[8] = a;
	te[9] = b;
	te[10] = c;
	te[11] = -1;
	te[12] = 0;
	te[13] = 0;
	te[14] = d;
	te[15] = 0;

#endif
}

float CameraMatrix::get_z_far() const {

	const float *matrix = (const float *)this->matrix;
	Plane new_plane = Plane(matrix[3] - matrix[2],
			matrix[7] - matrix[6],
			matrix[11] - matrix[10],
			matrix[15] - matrix[14]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	return new_plane.d;
}
float CameraMatrix::get_z_near() const {

	const float *matrix = (const float *)this->matrix;
	Plane new_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			-matrix[15] - matrix[14]);

	new_plane.normalize();
	return new_plane.d;
}

void CameraMatrix::get_viewport_size(float &r_width, float &r_height) const {

	const float *matrix = (const float *)this->matrix;
	///////--- Near Plane ---///////
	Plane near_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			-matrix[15] - matrix[14]);
	near_plane.normalize();

	///////--- Right Plane ---///////
	Plane right_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	Plane top_plane = Plane(matrix[3] - matrix[1],
			matrix[7] - matrix[5],
			matrix[11] - matrix[9],
			-matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 res;
	near_plane.intersect_3(right_plane, top_plane, &res);

	r_width = res.x;
	r_height = res.y;
}

bool CameraMatrix::get_endpoints(const Transform &p_transform, Vector3 *p_8points) const {

	const float *matrix = (const float *)this->matrix;

	///////--- Near Plane ---///////
	Plane near_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			-matrix[15] - matrix[14]);
	near_plane.normalize();

	///////--- Far Plane ---///////
	Plane far_plane = Plane(matrix[2] - matrix[3],
			matrix[6] - matrix[7],
			matrix[10] - matrix[11],
			matrix[15] - matrix[14]);
	far_plane.normalize();

	///////--- Right Plane ---///////
	Plane right_plane = Plane(matrix[0] - matrix[3],
			matrix[4] - matrix[7],
			matrix[8] - matrix[11],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	///////--- Top Plane ---///////
	Plane top_plane = Plane(matrix[1] - matrix[3],
			matrix[5] - matrix[7],
			matrix[9] - matrix[11],
			-matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 near_endpoint;
	Vector3 far_endpoint;

	bool res = near_plane.intersect_3(right_plane, top_plane, &near_endpoint);
	ERR_FAIL_COND_V(!res, false);

	res = far_plane.intersect_3(right_plane, top_plane, &far_endpoint);
	ERR_FAIL_COND_V(!res, false);

	p_8points[0] = p_transform.xform(Vector3(near_endpoint.x, near_endpoint.y, near_endpoint.z));
	p_8points[1] = p_transform.xform(Vector3(near_endpoint.x, -near_endpoint.y, near_endpoint.z));
	p_8points[2] = p_transform.xform(Vector3(-near_endpoint.x, near_endpoint.y, near_endpoint.z));
	p_8points[3] = p_transform.xform(Vector3(-near_endpoint.x, -near_endpoint.y, near_endpoint.z));
	p_8points[4] = p_transform.xform(Vector3(far_endpoint.x, far_endpoint.y, far_endpoint.z));
	p_8points[5] = p_transform.xform(Vector3(far_endpoint.x, -far_endpoint.y, far_endpoint.z));
	p_8points[6] = p_transform.xform(Vector3(-far_endpoint.x, far_endpoint.y, far_endpoint.z));
	p_8points[7] = p_transform.xform(Vector3(-far_endpoint.x, -far_endpoint.y, far_endpoint.z));

	return true;
}

Vector<Plane> CameraMatrix::get_projection_planes(const Transform &p_transform) const {

	/** Fast Plane Extraction from combined modelview/projection matrices.
	 * References:
	 * http://www.markmorley.com/opengl/frustumculling.html
	 * http://www2.ravensoft.com/users/ggribb/plane%20extraction.pdf
	 */

	Vector<Plane> planes;

	const float *matrix = (const float *)this->matrix;

	Plane new_plane;

	///////--- Near Plane ---///////
	new_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			matrix[15] + matrix[14]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	///////--- Far Plane ---///////
	new_plane = Plane(matrix[3] - matrix[2],
			matrix[7] - matrix[6],
			matrix[11] - matrix[10],
			matrix[15] - matrix[14]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	///////--- Left Plane ---///////
	new_plane = Plane(matrix[3] + matrix[0],
			matrix[7] + matrix[4],
			matrix[11] + matrix[8],
			matrix[15] + matrix[12]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	///////--- Top Plane ---///////
	new_plane = Plane(matrix[3] - matrix[1],
			matrix[7] - matrix[5],
			matrix[11] - matrix[9],
			matrix[15] - matrix[13]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	///////--- Right Plane ---///////
	new_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			matrix[15] - matrix[12]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	///////--- Bottom Plane ---///////
	new_plane = Plane(matrix[3] + matrix[1],
			matrix[7] + matrix[5],
			matrix[11] + matrix[9],
			matrix[15] + matrix[13]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.push_back(p_transform.xform(new_plane));

	return planes;
}

CameraMatrix CameraMatrix::inverse() const {

	CameraMatrix cm = *this;
	cm.invert();
	return cm;
}

void CameraMatrix::invert() {

	int i, j, k;
	int pvt_i[4], pvt_j[4]; /* Locations of pivot matrix */
	float pvt_val; /* Value of current pivot element */
	float hold; /* Temporary storage */
	float determinat; /* Determinant */

	determinat = 1.0;
	for (k = 0; k < 4; k++) {
		/** Locate k'th pivot element **/
		pvt_val = matrix[k][k]; /** Initialize for search **/
		pvt_i[k] = k;
		pvt_j[k] = k;
		for (i = k; i < 4; i++) {
			for (j = k; j < 4; j++) {
				if (Math::absd(matrix[i][j]) > Math::absd(pvt_val)) {
					pvt_i[k] = i;
					pvt_j[k] = j;
					pvt_val = matrix[i][j];
				}
			}
		}

		/** Product of pivots, gives determinant when finished **/
		determinat *= pvt_val;
		if (Math::absd(determinat) < 1e-7) {
			return; //(false);  /** Matrix is singular (zero determinant). **/
		}

		/** "Interchange" rows (with sign change stuff) **/
		i = pvt_i[k];
		if (i != k) { /** If rows are different **/
			for (j = 0; j < 4; j++) {
				hold = -matrix[k][j];
				matrix[k][j] = matrix[i][j];
				matrix[i][j] = hold;
			}
		}

		/** "Interchange" columns **/
		j = pvt_j[k];
		if (j != k) { /** If columns are different **/
			for (i = 0; i < 4; i++) {
				hold = -matrix[i][k];
				matrix[i][k] = matrix[i][j];
				matrix[i][j] = hold;
			}
		}

		/** Divide column by minus pivot value **/
		for (i = 0; i < 4; i++) {
			if (i != k) matrix[i][k] /= (-pvt_val);
		}

		/** Reduce the matrix **/
		for (i = 0; i < 4; i++) {
			hold = matrix[i][k];
			for (j = 0; j < 4; j++) {
				if (i != k && j != k) matrix[i][j] += hold * matrix[k][j];
			}
		}

		/** Divide row by pivot **/
		for (j = 0; j < 4; j++) {
			if (j != k) matrix[k][j] /= pvt_val;
		}

		/** Replace pivot by reciprocal (at last we can touch it). **/
		matrix[k][k] = 1.0 / pvt_val;
	}

	/* That was most of the work, one final pass of row/column interchange */
	/* to finish */
	for (k = 4 - 2; k >= 0; k--) { /* Don't need to work with 1 by 1 corner*/
		i = pvt_j[k]; /* Rows to swap correspond to pivot COLUMN */
		if (i != k) { /* If rows are different */
			for (j = 0; j < 4; j++) {
				hold = matrix[k][j];
				matrix[k][j] = -matrix[i][j];
				matrix[i][j] = hold;
			}
		}

		j = pvt_i[k]; /* Columns to swap correspond to pivot ROW */
		if (j != k) /* If columns are different */
			for (i = 0; i < 4; i++) {
				hold = matrix[i][k];
				matrix[i][k] = -matrix[i][j];
				matrix[i][j] = hold;
			}
	}
}

CameraMatrix::CameraMatrix() {

	set_identity();
}

CameraMatrix CameraMatrix::operator*(const CameraMatrix &p_matrix) const {

	CameraMatrix new_matrix;

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			real_t ab = 0;
			for (int k = 0; k < 4; k++)
				ab += matrix[k][i] * p_matrix.matrix[j][k];
			new_matrix.matrix[j][i] = ab;
		}
	}

	return new_matrix;
}

void CameraMatrix::set_light_bias() {

	float *m = &matrix[0][0];

	m[0] = 0.5,
	m[1] = 0.0,
	m[2] = 0.0,
	m[3] = 0.0,
	m[4] = 0.0,
	m[5] = 0.5,
	m[6] = 0.0,
	m[7] = 0.0,
	m[8] = 0.0,
	m[9] = 0.0,
	m[10] = 0.5,
	m[11] = 0.0,
	m[12] = 0.5,
	m[13] = 0.5,
	m[14] = 0.5,
	m[15] = 1.0;
}

CameraMatrix::operator String() const {

	String str;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			str += String((j > 0) ? ", " : "\n") + rtos(matrix[i][j]);

	return str;
}

float CameraMatrix::get_aspect() const {

	float w, h;
	get_viewport_size(w, h);
	return w / h;
}

float CameraMatrix::get_fov() const {
	const float *matrix = (const float *)this->matrix;

	Plane right_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	return Math::rad2deg(Math::acos(Math::abs(right_plane.normal.x))) * 2.0;
}

void CameraMatrix::make_scale(const Vector3 &p_scale) {

	set_identity();
	matrix[0][0] = p_scale.x;
	matrix[1][1] = p_scale.y;
	matrix[2][2] = p_scale.z;
}

void CameraMatrix::scale_translate_to_fit(const AABB &p_aabb) {

	Vector3 min = p_aabb.pos;
	Vector3 max = p_aabb.pos + p_aabb.size;

	matrix[0][0] = 2 / (max.x - min.x);
	matrix[1][0] = 0;
	matrix[2][0] = 0;
	matrix[3][0] = -(max.x + min.x) / (max.x - min.x);

	matrix[0][1] = 0;
	matrix[1][1] = 2 / (max.y - min.y);
	matrix[2][1] = 0;
	matrix[3][1] = -(max.y + min.y) / (max.y - min.y);

	matrix[0][2] = 0;
	matrix[1][2] = 0;
	matrix[2][2] = 2 / (max.z - min.z);
	matrix[3][2] = -(max.z + min.z) / (max.z - min.z);

	matrix[0][3] = 0;
	matrix[1][3] = 0;
	matrix[2][3] = 0;
	matrix[3][3] = 1;
}

CameraMatrix::operator Transform() const {

	Transform tr;
	const float *m = &matrix[0][0];

	tr.basis.elements[0][0] = m[0];
	tr.basis.elements[1][0] = m[1];
	tr.basis.elements[2][0] = m[2];

	tr.basis.elements[0][1] = m[4];
	tr.basis.elements[1][1] = m[5];
	tr.basis.elements[2][1] = m[6];

	tr.basis.elements[0][2] = m[8];
	tr.basis.elements[1][2] = m[9];
	tr.basis.elements[2][2] = m[10];

	tr.origin.x = m[12];
	tr.origin.y = m[13];
	tr.origin.z = m[14];

	return tr;
}

CameraMatrix::CameraMatrix(const Transform &p_transform) {

	const Transform &tr = p_transform;
	float *m = &matrix[0][0];

	m[0] = tr.basis.elements[0][0];
	m[1] = tr.basis.elements[1][0];
	m[2] = tr.basis.elements[2][0];
	m[3] = 0.0;
	m[4] = tr.basis.elements[0][1];
	m[5] = tr.basis.elements[1][1];
	m[6] = tr.basis.elements[2][1];
	m[7] = 0.0;
	m[8] = tr.basis.elements[0][2];
	m[9] = tr.basis.elements[1][2];
	m[10] = tr.basis.elements[2][2];
	m[11] = 0.0;
	m[12] = tr.origin.x;
	m[13] = tr.origin.y;
	m[14] = tr.origin.z;
	m[15] = 1.0;
}

CameraMatrix::~CameraMatrix() {
}
