/*************************************************************************/
/*  vector3.cpp                                                          */
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
#include "vector3.h"
#include "matrix3.h"

void Vector3::rotate(const Vector3 &p_axis, float p_phi) {

	*this = Matrix3(p_axis, p_phi).xform(*this);
}

Vector3 Vector3::rotated(const Vector3 &p_axis, float p_phi) const {

	Vector3 r = *this;
	r.rotate(p_axis, p_phi);
	return r;
}

void Vector3::set_axis(int p_axis, real_t p_value) {
	ERR_FAIL_INDEX(p_axis, 3);
	coord[p_axis] = p_value;
}
real_t Vector3::get_axis(int p_axis) const {

	ERR_FAIL_INDEX_V(p_axis, 3, 0);
	return operator[](p_axis);
}

int Vector3::min_axis() const {

	return x < y ? (x < z ? 0 : 2) : (y < z ? 1 : 2);
}
int Vector3::max_axis() const {

	return x < y ? (y < z ? 2 : 1) : (x < z ? 2 : 0);
}

void Vector3::snap(float p_val) {

	x = Math::stepify(x, p_val);
	y = Math::stepify(y, p_val);
	z = Math::stepify(z, p_val);
}
Vector3 Vector3::snapped(float p_val) const {

	Vector3 v = *this;
	v.snap(p_val);
	return v;
}

Vector3 Vector3::cubic_interpolaten(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, float p_t) const {

	Vector3 p0 = p_pre_a;
	Vector3 p1 = *this;
	Vector3 p2 = p_b;
	Vector3 p3 = p_post_b;

	{
		//normalize

		float ab = p0.distance_to(p1);
		float bc = p1.distance_to(p2);
		float cd = p2.distance_to(p3);

		if (ab > 0)
			p0 = p1 + (p0 - p1) * (bc / ab);
		if (cd > 0)
			p3 = p2 + (p3 - p2) * (bc / cd);
	}

	float t = p_t;
	float t2 = t * t;
	float t3 = t2 * t;

	Vector3 out;
	out = 0.5f * ((p1 * 2.0f) +
						 (-p0 + p2) * t +
						 (2.0f * p0 - 5.0f * p1 + 4 * p2 - p3) * t2 +
						 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	return out;
}

Vector3 Vector3::cubic_interpolate(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, float p_t) const {

	Vector3 p0 = p_pre_a;
	Vector3 p1 = *this;
	Vector3 p2 = p_b;
	Vector3 p3 = p_post_b;

	float t = p_t;
	float t2 = t * t;
	float t3 = t2 * t;

	Vector3 out;
	out = 0.5f * ((p1 * 2.0f) +
						 (-p0 + p2) * t +
						 (2.0f * p0 - 5.0f * p1 + 4 * p2 - p3) * t2 +
						 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	return out;
}

#if 0
Vector3 Vector3::cubic_interpolate(const Vector3& p_b,const Vector3& p_pre_a, const Vector3& p_post_b,float p_t) const {

	Vector3 p0=p_pre_a;
	Vector3 p1=*this;
	Vector3 p2=p_b;
	Vector3 p3=p_post_b;

	if (true) {

		float ab = p0.distance_to(p1);
		float bc = p1.distance_to(p2);
		float cd = p2.distance_to(p3);

		//if (ab>bc) {
		if (ab>0)
			p0 = p1+(p0-p1)*(bc/ab);
		//}

		//if (cd>bc) {
		if (cd>0)
			p3 = p2+(p3-p2)*(bc/cd);
		//}
	}

	float t = p_t;
	float t2 = t * t;
	float t3 = t2 * t;

	Vector3 out;
	out.x = 0.5f * ( ( 2.0f * p1.x ) +
	( -p0.x + p2.x ) * t +
	( 2.0f * p0.x - 5.0f * p1.x + 4 * p2.x - p3.x ) * t2 +
	( -p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x ) * t3 );
	out.y = 0.5f * ( ( 2.0f * p1.y ) +
	( -p0.y + p2.y ) * t +
	( 2.0f * p0.y - 5.0f * p1.y + 4 * p2.y - p3.y ) * t2 +
	( -p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y ) * t3 );
	out.z = 0.5f * ( ( 2.0f * p1.z ) +
	( -p0.z + p2.z ) * t +
	( 2.0f * p0.z - 5.0f * p1.z + 4 * p2.z - p3.z ) * t2 +
	( -p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z ) * t3 );
	return out;
}
#endif
Vector3::operator String() const {

	return (rtos(x) + ", " + rtos(y) + ", " + rtos(z));
}
