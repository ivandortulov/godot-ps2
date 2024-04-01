/*************************************************************************/
/*  matrix3.cpp                                                          */
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
#include "matrix3.h"
#include "math_funcs.h"
#include "os/copymem.h"

#define cofac(row1, col1, row2, col2) \
	(elements[row1][col1] * elements[row2][col2] - elements[row1][col2] * elements[row2][col1])

void Matrix3::from_z(const Vector3 &p_z) {

	if (Math::abs(p_z.z) > Math_SQRT12) {

		// choose p in y-z plane
		real_t a = p_z[1] * p_z[1] + p_z[2] * p_z[2];
		real_t k = 1.0 / Math::sqrt(a);
		elements[0] = Vector3(0, -p_z[2] * k, p_z[1] * k);
		elements[1] = Vector3(a * k, -p_z[0] * elements[0][2], p_z[0] * elements[0][1]);
	} else {

		// choose p in x-y plane
		real_t a = p_z.x * p_z.x + p_z.y * p_z.y;
		real_t k = 1.0 / Math::sqrt(a);
		elements[0] = Vector3(-p_z.y * k, p_z.x * k, 0);
		elements[1] = Vector3(-p_z.z * elements[0].y, p_z.z * elements[0].x, a * k);
	}
	elements[2] = p_z;
}

void Matrix3::invert() {

	real_t co[3] = {
		cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1)
	};
	real_t det = elements[0][0] * co[0] +
				 elements[0][1] * co[1] +
				 elements[0][2] * co[2];

	ERR_FAIL_COND(det == 0);
	real_t s = 1.0 / det;

	set(co[0] * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
			co[1] * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
			co[2] * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
}

void Matrix3::orthonormalize() {

	// Gram-Schmidt Process

	Vector3 x = get_axis(0);
	Vector3 y = get_axis(1);
	Vector3 z = get_axis(2);

	x.normalize();
	y = (y - x * (x.dot(y)));
	y.normalize();
	z = (z - x * (x.dot(z)) - y * (y.dot(z)));
	z.normalize();

	set_axis(0, x);
	set_axis(1, y);
	set_axis(2, z);
}

Matrix3 Matrix3::orthonormalized() const {

	Matrix3 c = *this;
	c.orthonormalize();
	return c;
}

Matrix3 Matrix3::inverse() const {

	Matrix3 inv = *this;
	inv.invert();
	return inv;
}

void Matrix3::transpose() {

	SWAP(elements[0][1], elements[1][0]);
	SWAP(elements[0][2], elements[2][0]);
	SWAP(elements[1][2], elements[2][1]);
}

Matrix3 Matrix3::transposed() const {

	Matrix3 tr = *this;
	tr.transpose();
	return tr;
}

void Matrix3::scale(const Vector3 &p_scale) {

	elements[0][0] *= p_scale.x;
	elements[1][0] *= p_scale.x;
	elements[2][0] *= p_scale.x;
	elements[0][1] *= p_scale.y;
	elements[1][1] *= p_scale.y;
	elements[2][1] *= p_scale.y;
	elements[0][2] *= p_scale.z;
	elements[1][2] *= p_scale.z;
	elements[2][2] *= p_scale.z;
}

Matrix3 Matrix3::scaled(const Vector3 &p_scale) const {

	Matrix3 m = *this;
	m.scale(p_scale);
	return m;
}

Vector3 Matrix3::get_scale() const {

	return Vector3(
			Vector3(elements[0][0], elements[1][0], elements[2][0]).length(),
			Vector3(elements[0][1], elements[1][1], elements[2][1]).length(),
			Vector3(elements[0][2], elements[1][2], elements[2][2]).length());
}
void Matrix3::rotate(const Vector3 &p_axis, real_t p_phi) {

	*this = *this * Matrix3(p_axis, p_phi);
}

Matrix3 Matrix3::rotated(const Vector3 &p_axis, real_t p_phi) const {

	return *this * Matrix3(p_axis, p_phi);
}

Vector3 Matrix3::get_euler() const {

	// rot =  cy*cz          -cy*sz           sy
	//        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
	//       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy

	Matrix3 m = *this;
	m.orthonormalize();

	Vector3 euler;

	euler.y = Math::asin(m[0][2]);
	if (euler.y < Math_PI * 0.5) {
		if (euler.y > -Math_PI * 0.5) {
			euler.x = Math::atan2(-m[1][2], m[2][2]);
			euler.z = Math::atan2(-m[0][1], m[0][0]);

		} else {
			real_t r = Math::atan2(m[1][0], m[1][1]);
			euler.z = 0.0;
			euler.x = euler.z - r;
		}
	} else {
		real_t r = Math::atan2(m[0][1], m[1][1]);
		euler.z = 0;
		euler.x = r - euler.z;
	}

	return euler;
}

void Matrix3::set_euler(const Vector3 &p_euler) {

	real_t c, s;

	c = Math::cos(p_euler.x);
	s = Math::sin(p_euler.x);
	Matrix3 xmat(1.0, 0.0, 0.0, 0.0, c, -s, 0.0, s, c);

	c = Math::cos(p_euler.y);
	s = Math::sin(p_euler.y);
	Matrix3 ymat(c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c);

	c = Math::cos(p_euler.z);
	s = Math::sin(p_euler.z);
	Matrix3 zmat(c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0);

	//optimizer will optimize away all this anyway
	*this = xmat * (ymat * zmat);
}

bool Matrix3::operator==(const Matrix3 &p_matrix) const {

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (elements[i][j] != p_matrix.elements[i][j])
				return false;
		}
	}

	return true;
}
bool Matrix3::operator!=(const Matrix3 &p_matrix) const {

	return (!(*this == p_matrix));
}

Matrix3::operator String() const {

	String mtx;
	for (int i = 0; i < 3; i++) {

		for (int j = 0; j < 3; j++) {

			if (i != 0 || j != 0)
				mtx += ", ";

			mtx += rtos(elements[i][j]);
		}
	}

	return mtx;
}

Matrix3::operator Quat() const {

	Matrix3 m = *this;
	m.orthonormalize();

	real_t trace = m.elements[0][0] + m.elements[1][1] + m.elements[2][2];
	real_t temp[4];

	if (trace > 0.0) {
		real_t s = Math::sqrt(trace + 1.0);
		temp[3] = (s * 0.5);
		s = 0.5 / s;

		temp[0] = ((m.elements[2][1] - m.elements[1][2]) * s);
		temp[1] = ((m.elements[0][2] - m.elements[2][0]) * s);
		temp[2] = ((m.elements[1][0] - m.elements[0][1]) * s);
	} else {
		int i = m.elements[0][0] < m.elements[1][1] ?
						(m.elements[1][1] < m.elements[2][2] ? 2 : 1) :
						(m.elements[0][0] < m.elements[2][2] ? 2 : 0);
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		real_t s = Math::sqrt(m.elements[i][i] - m.elements[j][j] - m.elements[k][k] + 1.0);
		temp[i] = s * 0.5;
		s = 0.5 / s;

		temp[3] = (m.elements[k][j] - m.elements[j][k]) * s;
		temp[j] = (m.elements[j][i] + m.elements[i][j]) * s;
		temp[k] = (m.elements[k][i] + m.elements[i][k]) * s;
	}

	return Quat(temp[0], temp[1], temp[2], temp[3]);
}

static const Matrix3 _ortho_bases[24] = {
	Matrix3(1, 0, 0, 0, 1, 0, 0, 0, 1),
	Matrix3(0, -1, 0, 1, 0, 0, 0, 0, 1),
	Matrix3(-1, 0, 0, 0, -1, 0, 0, 0, 1),
	Matrix3(0, 1, 0, -1, 0, 0, 0, 0, 1),
	Matrix3(1, 0, 0, 0, 0, -1, 0, 1, 0),
	Matrix3(0, 0, 1, 1, 0, 0, 0, 1, 0),
	Matrix3(-1, 0, 0, 0, 0, 1, 0, 1, 0),
	Matrix3(0, 0, -1, -1, 0, 0, 0, 1, 0),
	Matrix3(1, 0, 0, 0, -1, 0, 0, 0, -1),
	Matrix3(0, 1, 0, 1, 0, 0, 0, 0, -1),
	Matrix3(-1, 0, 0, 0, 1, 0, 0, 0, -1),
	Matrix3(0, -1, 0, -1, 0, 0, 0, 0, -1),
	Matrix3(1, 0, 0, 0, 0, 1, 0, -1, 0),
	Matrix3(0, 0, -1, 1, 0, 0, 0, -1, 0),
	Matrix3(-1, 0, 0, 0, 0, -1, 0, -1, 0),
	Matrix3(0, 0, 1, -1, 0, 0, 0, -1, 0),
	Matrix3(0, 0, 1, 0, 1, 0, -1, 0, 0),
	Matrix3(0, -1, 0, 0, 0, 1, -1, 0, 0),
	Matrix3(0, 0, -1, 0, -1, 0, -1, 0, 0),
	Matrix3(0, 1, 0, 0, 0, -1, -1, 0, 0),
	Matrix3(0, 0, 1, 0, -1, 0, 1, 0, 0),
	Matrix3(0, 1, 0, 0, 0, 1, 1, 0, 0),
	Matrix3(0, 0, -1, 0, 1, 0, 1, 0, 0),
	Matrix3(0, -1, 0, 0, 0, -1, 1, 0, 0)
};

int Matrix3::get_orthogonal_index() const {

	//could be sped up if i come up with a way
	Matrix3 orth = *this;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {

			float v = orth[i][j];
			if (v > 0.5)
				v = 1.0;
			else if (v < -0.5)
				v = -1.0;
			else
				v = 0;

			orth[i][j] = v;
		}
	}

	for (int i = 0; i < 24; i++) {

		if (_ortho_bases[i] == orth)
			return i;
	}

	return 0;
}

void Matrix3::set_orthogonal_index(int p_index) {

	//there only exist 24 orthogonal bases in r3
	ERR_FAIL_INDEX(p_index, 24);

	*this = _ortho_bases[p_index];
}

void Matrix3::get_axis_and_angle(Vector3 &r_axis, real_t &r_angle) const {

	double angle, x, y, z; // variables for result
	double epsilon = 0.01; // margin to allow for rounding errors
	double epsilon2 = 0.1; // margin to distinguish between 0 and 180 degrees

	if ((Math::abs(elements[1][0] - elements[0][1]) < epsilon) && (Math::abs(elements[2][0] - elements[0][2]) < epsilon) && (Math::abs(elements[2][1] - elements[1][2]) < epsilon)) {
		// singularity found
		// first check for identity matrix which must have +1 for all terms
		//  in leading diagonaland zero in other terms
		if ((Math::abs(elements[1][0] + elements[0][1]) < epsilon2) && (Math::abs(elements[2][0] + elements[0][2]) < epsilon2) && (Math::abs(elements[2][1] + elements[1][2]) < epsilon2) && (Math::abs(elements[0][0] + elements[1][1] + elements[2][2] - 3) < epsilon2)) {
			// this singularity is identity matrix so angle = 0
			r_axis = Vector3(0, 1, 0);
			r_angle = 0;
			return;
		}
		// otherwise this singularity is angle = 180
		angle = Math_PI;
		double xx = (elements[0][0] + 1) / 2;
		double yy = (elements[1][1] + 1) / 2;
		double zz = (elements[2][2] + 1) / 2;
		double xy = (elements[1][0] + elements[0][1]) / 4;
		double xz = (elements[2][0] + elements[0][2]) / 4;
		double yz = (elements[2][1] + elements[1][2]) / 4;
		if ((xx > yy) && (xx > zz)) { // elements[0][0] is the largest diagonal term
			if (xx < epsilon) {
				x = 0;
				y = 0.7071;
				z = 0.7071;
			} else {
				x = Math::sqrt(xx);
				y = xy / x;
				z = xz / x;
			}
		} else if (yy > zz) { // elements[1][1] is the largest diagonal term
			if (yy < epsilon) {
				x = 0.7071;
				y = 0;
				z = 0.7071;
			} else {
				y = Math::sqrt(yy);
				x = xy / y;
				z = yz / y;
			}
		} else { // elements[2][2] is the largest diagonal term so base result on this
			if (zz < epsilon) {
				x = 0.7071;
				y = 0.7071;
				z = 0;
			} else {
				z = Math::sqrt(zz);
				x = xz / z;
				y = yz / z;
			}
		}
		r_axis = Vector3(x, y, z);
		r_angle = angle;
		return;
	}
	// as we have reached here there are no singularities so we can handle normally
	double s = Math::sqrt((elements[1][2] - elements[2][1]) * (elements[1][2] - elements[2][1]) + (elements[2][0] - elements[0][2]) * (elements[2][0] - elements[0][2]) + (elements[0][1] - elements[1][0]) * (elements[0][1] - elements[1][0])); // used to normalise
	if (Math::abs(s) < 0.001) s = 1;
	// prevent divide by zero, should not happen if matrix is orthogonal and should be
	// caught by singularity test above, but I've left it in just in case
	angle = Math::acos((elements[0][0] + elements[1][1] + elements[2][2] - 1) / 2);
	x = (elements[1][2] - elements[2][1]) / s;
	y = (elements[2][0] - elements[0][2]) / s;
	z = (elements[0][1] - elements[1][0]) / s;

	r_axis = Vector3(x, y, z);
	r_angle = angle;
}

Matrix3::Matrix3(const Vector3 &p_euler) {

	set_euler(p_euler);
}

Matrix3::Matrix3(const Quat &p_quat) {

	real_t d = p_quat.length_squared();
	real_t s = 2.0 / d;
	real_t xs = p_quat.x * s, ys = p_quat.y * s, zs = p_quat.z * s;
	real_t wx = p_quat.w * xs, wy = p_quat.w * ys, wz = p_quat.w * zs;
	real_t xx = p_quat.x * xs, xy = p_quat.x * ys, xz = p_quat.x * zs;
	real_t yy = p_quat.y * ys, yz = p_quat.y * zs, zz = p_quat.z * zs;
	set(1.0 - (yy + zz), xy - wz, xz + wy,
			xy + wz, 1.0 - (xx + zz), yz - wx,
			xz - wy, yz + wx, 1.0 - (xx + yy));
}

Matrix3::Matrix3(const Vector3 &p_axis, real_t p_phi) {

	Vector3 axis_sq(p_axis.x * p_axis.x, p_axis.y * p_axis.y, p_axis.z * p_axis.z);

	real_t cosine = Math::cos(p_phi);
	real_t sine = Math::sin(p_phi);

	elements[0][0] = axis_sq.x + cosine * (1.0 - axis_sq.x);
	elements[0][1] = p_axis.x * p_axis.y * (1.0 - cosine) + p_axis.z * sine;
	elements[0][2] = p_axis.z * p_axis.x * (1.0 - cosine) - p_axis.y * sine;

	elements[1][0] = p_axis.x * p_axis.y * (1.0 - cosine) - p_axis.z * sine;
	elements[1][1] = axis_sq.y + cosine * (1.0 - axis_sq.y);
	elements[1][2] = p_axis.y * p_axis.z * (1.0 - cosine) + p_axis.x * sine;

	elements[2][0] = p_axis.z * p_axis.x * (1.0 - cosine) + p_axis.y * sine;
	elements[2][1] = p_axis.y * p_axis.z * (1.0 - cosine) - p_axis.x * sine;
	elements[2][2] = axis_sq.z + cosine * (1.0 - axis_sq.z);
}
