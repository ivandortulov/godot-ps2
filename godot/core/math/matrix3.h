/*************************************************************************/
/*  matrix3.h                                                            */
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
#ifndef MATRIX3_H
#define MATRIX3_H

#include "quat.h"
#include "vector3.h"

/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class Matrix3 {
public:
	Vector3 elements[3];

	_FORCE_INLINE_ const Vector3 &operator[](int axis) const {

		return elements[axis];
	}
	_FORCE_INLINE_ Vector3 &operator[](int axis) {

		return elements[axis];
	}

	void invert();
	void transpose();

	Matrix3 inverse() const;
	Matrix3 transposed() const;

	_FORCE_INLINE_ float determinant() const;

	void from_z(const Vector3 &p_z);

	_FORCE_INLINE_ Vector3 get_axis(int p_axis) const {
		// get actual basis axis (elements is transposed for performance)
		return Vector3(elements[0][p_axis], elements[1][p_axis], elements[2][p_axis]);
	}
	_FORCE_INLINE_ void set_axis(int p_axis, const Vector3 &p_value) {
		// get actual basis axis (elements is transposed for performance)
		elements[0][p_axis] = p_value.x;
		elements[1][p_axis] = p_value.y;
		elements[2][p_axis] = p_value.z;
	}

	void rotate(const Vector3 &p_axis, real_t p_phi);
	Matrix3 rotated(const Vector3 &p_axis, real_t p_phi) const;

	void scale(const Vector3 &p_scale);
	Matrix3 scaled(const Vector3 &p_scale) const;
	Vector3 get_scale() const;

	Vector3 get_euler() const;
	void set_euler(const Vector3 &p_euler);

	// transposed dot products
	_FORCE_INLINE_ real_t tdotx(const Vector3 &v) const {
		return elements[0][0] * v[0] + elements[1][0] * v[1] + elements[2][0] * v[2];
	}
	_FORCE_INLINE_ real_t tdoty(const Vector3 &v) const {
		return elements[0][1] * v[0] + elements[1][1] * v[1] + elements[2][1] * v[2];
	}
	_FORCE_INLINE_ real_t tdotz(const Vector3 &v) const {
		return elements[0][2] * v[0] + elements[1][2] * v[1] + elements[2][2] * v[2];
	}

	bool operator==(const Matrix3 &p_matrix) const;
	bool operator!=(const Matrix3 &p_matrix) const;

	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_vector) const;
	_FORCE_INLINE_ Vector3 xform_inv(const Vector3 &p_vector) const;
	_FORCE_INLINE_ void operator*=(const Matrix3 &p_matrix);
	_FORCE_INLINE_ Matrix3 operator*(const Matrix3 &p_matrix) const;

	int get_orthogonal_index() const;
	void set_orthogonal_index(int p_index);

	operator String() const;

	void get_axis_and_angle(Vector3 &r_axis, real_t &r_angle) const;

	/* create / set */

	_FORCE_INLINE_ void set(real_t xx, real_t xy, real_t xz, real_t yx, real_t yy, real_t yz, real_t zx, real_t zy, real_t zz) {

		elements[0][0] = xx;
		elements[0][1] = xy;
		elements[0][2] = xz;
		elements[1][0] = yx;
		elements[1][1] = yy;
		elements[1][2] = yz;
		elements[2][0] = zx;
		elements[2][1] = zy;
		elements[2][2] = zz;
	}
	_FORCE_INLINE_ Vector3 get_column(int i) const {

		return Vector3(elements[0][i], elements[1][i], elements[2][i]);
	}

	_FORCE_INLINE_ Vector3 get_row(int i) const {

		return Vector3(elements[i][0], elements[i][1], elements[i][2]);
	}
	_FORCE_INLINE_ void set_row(int i, const Vector3 &p_row) {
		elements[i][0] = p_row.x;
		elements[i][1] = p_row.y;
		elements[i][2] = p_row.z;
	}

	_FORCE_INLINE_ void set_zero() {
		elements[0].zero();
		elements[1].zero();
		elements[2].zero();
	}

	_FORCE_INLINE_ Matrix3 transpose_xform(const Matrix3 &m) const {
		return Matrix3(
				elements[0].x * m[0].x + elements[1].x * m[1].x + elements[2].x * m[2].x,
				elements[0].x * m[0].y + elements[1].x * m[1].y + elements[2].x * m[2].y,
				elements[0].x * m[0].z + elements[1].x * m[1].z + elements[2].x * m[2].z,
				elements[0].y * m[0].x + elements[1].y * m[1].x + elements[2].y * m[2].x,
				elements[0].y * m[0].y + elements[1].y * m[1].y + elements[2].y * m[2].y,
				elements[0].y * m[0].z + elements[1].y * m[1].z + elements[2].y * m[2].z,
				elements[0].z * m[0].x + elements[1].z * m[1].x + elements[2].z * m[2].x,
				elements[0].z * m[0].y + elements[1].z * m[1].y + elements[2].z * m[2].y,
				elements[0].z * m[0].z + elements[1].z * m[1].z + elements[2].z * m[2].z);
	}
	Matrix3(real_t xx, real_t xy, real_t xz, real_t yx, real_t yy, real_t yz, real_t zx, real_t zy, real_t zz) {

		set(xx, xy, xz, yx, yy, yz, zx, zy, zz);
	}

	void orthonormalize();
	Matrix3 orthonormalized() const;

	operator Quat() const;

	Matrix3(const Quat &p_quat); // euler
	Matrix3(const Vector3 &p_euler); // euler
	Matrix3(const Vector3 &p_axis, real_t p_phi);

	_FORCE_INLINE_ Matrix3() {

		elements[0][0] = 1;
		elements[0][1] = 0;
		elements[0][2] = 0;
		elements[1][0] = 0;
		elements[1][1] = 1;
		elements[1][2] = 0;
		elements[2][0] = 0;
		elements[2][1] = 0;
		elements[2][2] = 1;
	}
};

_FORCE_INLINE_ void Matrix3::operator*=(const Matrix3 &p_matrix) {

	set(
			p_matrix.tdotx(elements[0]), p_matrix.tdoty(elements[0]), p_matrix.tdotz(elements[0]),
			p_matrix.tdotx(elements[1]), p_matrix.tdoty(elements[1]), p_matrix.tdotz(elements[1]),
			p_matrix.tdotx(elements[2]), p_matrix.tdoty(elements[2]), p_matrix.tdotz(elements[2]));
}

_FORCE_INLINE_ Matrix3 Matrix3::operator*(const Matrix3 &p_matrix) const {

	return Matrix3(
			p_matrix.tdotx(elements[0]), p_matrix.tdoty(elements[0]), p_matrix.tdotz(elements[0]),
			p_matrix.tdotx(elements[1]), p_matrix.tdoty(elements[1]), p_matrix.tdotz(elements[1]),
			p_matrix.tdotx(elements[2]), p_matrix.tdoty(elements[2]), p_matrix.tdotz(elements[2]));
}

Vector3 Matrix3::xform(const Vector3 &p_vector) const {

	return Vector3(
			elements[0].dot(p_vector),
			elements[1].dot(p_vector),
			elements[2].dot(p_vector));
}

Vector3 Matrix3::xform_inv(const Vector3 &p_vector) const {

	return Vector3(
			(elements[0][0] * p_vector.x) + (elements[1][0] * p_vector.y) + (elements[2][0] * p_vector.z),
			(elements[0][1] * p_vector.x) + (elements[1][1] * p_vector.y) + (elements[2][1] * p_vector.z),
			(elements[0][2] * p_vector.x) + (elements[1][2] * p_vector.y) + (elements[2][2] * p_vector.z));
}

float Matrix3::determinant() const {

	return elements[0][0] * (elements[1][1] * elements[2][2] - elements[2][1] * elements[1][2]) -
		   elements[1][0] * (elements[0][1] * elements[2][2] - elements[2][1] * elements[0][2]) +
		   elements[2][0] * (elements[0][1] * elements[1][2] - elements[1][1] * elements[0][2]);
}
#endif
