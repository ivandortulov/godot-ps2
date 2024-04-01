/*************************************************************************/
/*  shape_sw.cpp                                                         */
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
#include "shape_sw.h"
#include "geometry.h"
#include "quick_hull.h"
#include "sort.h"
#define _POINT_SNAP 0.001953125
#define _EDGE_IS_VALID_SUPPORT_TRESHOLD 0.0002
#define _FACE_IS_VALID_SUPPORT_TRESHOLD 0.9998

void ShapeSW::configure(const AABB &p_aabb) {
	aabb = p_aabb;
	configured = true;
	for (Map<ShapeOwnerSW *, int>::Element *E = owners.front(); E; E = E->next()) {
		ShapeOwnerSW *co = (ShapeOwnerSW *)E->key();
		co->_shape_changed();
	}
}

Vector3 ShapeSW::get_support(const Vector3 &p_normal) const {

	Vector3 res;
	int amnt;
	get_supports(p_normal, 1, &res, amnt);
	return res;
}

void ShapeSW::add_owner(ShapeOwnerSW *p_owner) {

	Map<ShapeOwnerSW *, int>::Element *E = owners.find(p_owner);
	if (E) {
		E->get()++;
	} else {
		owners[p_owner] = 1;
	}
}

void ShapeSW::remove_owner(ShapeOwnerSW *p_owner) {

	Map<ShapeOwnerSW *, int>::Element *E = owners.find(p_owner);
	ERR_FAIL_COND(!E);
	E->get()--;
	if (E->get() == 0) {
		owners.erase(E);
	}
}

bool ShapeSW::is_owner(ShapeOwnerSW *p_owner) const {

	return owners.has(p_owner);
}

const Map<ShapeOwnerSW *, int> &ShapeSW::get_owners() const {
	return owners;
}

ShapeSW::ShapeSW() {

	custom_bias = 0;
	configured = false;
}

ShapeSW::~ShapeSW() {

	ERR_FAIL_COND(owners.size());
}

Plane PlaneShapeSW::get_plane() const {

	return plane;
}

void PlaneShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	// gibberish, a plane is infinity
	r_min = -1e7;
	r_max = 1e7;
}

Vector3 PlaneShapeSW::get_support(const Vector3 &p_normal) const {

	return p_normal * 1e15;
}

bool PlaneShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	bool inters = plane.intersects_segment(p_begin, p_end, &r_result);
	if (inters)
		r_normal = plane.normal;
	return inters;
}

Vector3 PlaneShapeSW::get_moment_of_inertia(float p_mass) const {

	return Vector3(); //wtf
}

void PlaneShapeSW::_setup(const Plane &p_plane) {

	plane = p_plane;
	configure(AABB(Vector3(-1e4, -1e4, -1e4), Vector3(1e4 * 2, 1e4 * 2, 1e4 * 2)));
}

void PlaneShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant PlaneShapeSW::get_data() const {

	return plane;
}

PlaneShapeSW::PlaneShapeSW() {
}

//

float RayShapeSW::get_length() const {

	return length;
}

void RayShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	// don't think this will be even used
	r_min = 0;
	r_max = 1;
}

Vector3 RayShapeSW::get_support(const Vector3 &p_normal) const {

	if (p_normal.z > 0)
		return Vector3(0, 0, length);
	else
		return Vector3(0, 0, 0);
}

void RayShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	if (Math::abs(p_normal.z) < _EDGE_IS_VALID_SUPPORT_TRESHOLD) {

		r_amount = 2;
		r_supports[0] = Vector3(0, 0, 0);
		r_supports[1] = Vector3(0, 0, length);
	}
	if (p_normal.z > 0) {
		r_amount = 1;
		*r_supports = Vector3(0, 0, length);
	} else {
		r_amount = 1;
		*r_supports = Vector3(0, 0, 0);
	}
}

bool RayShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	return false; //simply not possible
}

Vector3 RayShapeSW::get_moment_of_inertia(float p_mass) const {

	return Vector3();
}

void RayShapeSW::_setup(float p_length) {

	length = p_length;
	configure(AABB(Vector3(0, 0, 0), Vector3(0.1, 0.1, length)));
}

void RayShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant RayShapeSW::get_data() const {

	return length;
}

RayShapeSW::RayShapeSW() {

	length = 1;
}

/********** SPHERE *************/

real_t SphereShapeSW::get_radius() const {

	return radius;
}

void SphereShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	float d = p_normal.dot(p_transform.origin);

	// figure out scale at point
	Vector3 local_normal = p_transform.basis.xform_inv(p_normal);
	float scale = local_normal.length();

	r_min = d - (radius)*scale;
	r_max = d + (radius)*scale;
}

Vector3 SphereShapeSW::get_support(const Vector3 &p_normal) const {

	return p_normal * radius;
}

void SphereShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	*r_supports = p_normal * radius;
	r_amount = 1;
}

bool SphereShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	return Geometry::segment_intersects_sphere(p_begin, p_end, Vector3(), radius, &r_result, &r_normal);
}

Vector3 SphereShapeSW::get_moment_of_inertia(float p_mass) const {

	float s = 0.4 * p_mass * radius * radius;
	return Vector3(s, s, s);
}

void SphereShapeSW::_setup(real_t p_radius) {

	radius = p_radius;
	configure(AABB(Vector3(-radius, -radius, -radius), Vector3(radius * 2.0, radius * 2.0, radius * 2.0)));
}

void SphereShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant SphereShapeSW::get_data() const {

	return radius;
}

SphereShapeSW::SphereShapeSW() {

	radius = 0;
}

/********** BOX *************/

void BoxShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	// no matter the angle, the box is mirrored anyway
	Vector3 local_normal = p_transform.basis.xform_inv(p_normal);

	float length = local_normal.abs().dot(half_extents);
	float distance = p_normal.dot(p_transform.origin);

	r_min = distance - length;
	r_max = distance + length;
}

Vector3 BoxShapeSW::get_support(const Vector3 &p_normal) const {

	Vector3 point(
			(p_normal.x < 0) ? -half_extents.x : half_extents.x,
			(p_normal.y < 0) ? -half_extents.y : half_extents.y,
			(p_normal.z < 0) ? -half_extents.z : half_extents.z);

	return point;
}

void BoxShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	static const int next[3] = { 1, 2, 0 };
	static const int next2[3] = { 2, 0, 1 };

	for (int i = 0; i < 3; i++) {

		Vector3 axis;
		axis[i] = 1.0;
		float dot = p_normal.dot(axis);
		if (Math::abs(dot) > _FACE_IS_VALID_SUPPORT_TRESHOLD) {

			//Vector3 axis_b;

			bool neg = dot < 0;
			r_amount = 4;

			Vector3 point;
			point[i] = half_extents[i];

			int i_n = next[i];
			int i_n2 = next2[i];

			static const float sign[4][2] = {

				{ -1.0, 1.0 },
				{ 1.0, 1.0 },
				{ 1.0, -1.0 },
				{ -1.0, -1.0 },
			};

			for (int j = 0; j < 4; j++) {

				point[i_n] = sign[j][0] * half_extents[i_n];
				point[i_n2] = sign[j][1] * half_extents[i_n2];
				r_supports[j] = neg ? -point : point;
			}

			if (neg) {
				SWAP(r_supports[1], r_supports[2]);
				SWAP(r_supports[0], r_supports[3]);
			}

			return;
		}

		r_amount = 0;
	}

	for (int i = 0; i < 3; i++) {

		Vector3 axis;
		axis[i] = 1.0;

		if (Math::abs(p_normal.dot(axis)) < _EDGE_IS_VALID_SUPPORT_TRESHOLD) {

			r_amount = 2;

			int i_n = next[i];
			int i_n2 = next2[i];

			Vector3 point = half_extents;

			if (p_normal[i_n] < 0) {
				point[i_n] = -point[i_n];
			}
			if (p_normal[i_n2] < 0) {
				point[i_n2] = -point[i_n2];
			}

			r_supports[0] = point;
			point[i] = -point[i];
			r_supports[1] = point;
			return;
		}
	}
	/* USE POINT */

	Vector3 point(
			(p_normal.x < 0) ? -half_extents.x : half_extents.x,
			(p_normal.y < 0) ? -half_extents.y : half_extents.y,
			(p_normal.z < 0) ? -half_extents.z : half_extents.z);

	r_amount = 1;
	r_supports[0] = point;
}

bool BoxShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	AABB aabb(-half_extents, half_extents * 2.0);

	return aabb.intersects_segment(p_begin, p_end, &r_result, &r_normal);
}

Vector3 BoxShapeSW::get_moment_of_inertia(float p_mass) const {

	float lx = half_extents.x;
	float ly = half_extents.y;
	float lz = half_extents.z;

	return Vector3((p_mass / 3.0) * (ly * ly + lz * lz), (p_mass / 3.0) * (lx * lx + lz * lz), (p_mass / 3.0) * (lx * lx + ly * ly));
}

void BoxShapeSW::_setup(const Vector3 &p_half_extents) {

	half_extents = p_half_extents.abs();

	configure(AABB(-half_extents, half_extents * 2));
}

void BoxShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant BoxShapeSW::get_data() const {

	return half_extents;
}

BoxShapeSW::BoxShapeSW() {
}

/********** CAPSULE *************/

void CapsuleShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	Vector3 n = p_transform.basis.xform_inv(p_normal).normalized();
	float h = (n.z > 0) ? height : -height;

	n *= radius;
	n.z += h * 0.5;

	r_max = p_normal.dot(p_transform.xform(n));
	r_min = p_normal.dot(p_transform.xform(-n));
	return;

	n = p_transform.basis.xform(n);

	float distance = p_normal.dot(p_transform.origin);
	float length = Math::abs(p_normal.dot(n));
	r_min = distance - length;
	r_max = distance + length;

	ERR_FAIL_COND(r_max < r_min);
}

Vector3 CapsuleShapeSW::get_support(const Vector3 &p_normal) const {

	Vector3 n = p_normal;

	float h = (n.z > 0) ? height : -height;

	n *= radius;
	n.z += h * 0.5;
	return n;
}

void CapsuleShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	Vector3 n = p_normal;

	float d = n.z;

	if (Math::abs(d) < _EDGE_IS_VALID_SUPPORT_TRESHOLD) {

		// make it flat
		n.z = 0.0;
		n.normalize();
		n *= radius;

		r_amount = 2;
		r_supports[0] = n;
		r_supports[0].z += height * 0.5;
		r_supports[1] = n;
		r_supports[1].z -= height * 0.5;

	} else {

		float h = (d > 0) ? height : -height;

		n *= radius;
		n.z += h * 0.5;
		r_amount = 1;
		*r_supports = n;
	}
}

bool CapsuleShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	Vector3 norm = (p_end - p_begin).normalized();
	float min_d = 1e20;

	Vector3 res, n;
	bool collision = false;

	Vector3 auxres, auxn;
	bool collided;

	// test against cylinder and spheres :-|

	collided = Geometry::segment_intersects_cylinder(p_begin, p_end, height, radius, &auxres, &auxn);

	if (collided) {
		float d = norm.dot(auxres);
		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	collided = Geometry::segment_intersects_sphere(p_begin, p_end, Vector3(0, 0, height * 0.5), radius, &auxres, &auxn);

	if (collided) {
		float d = norm.dot(auxres);
		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	collided = Geometry::segment_intersects_sphere(p_begin, p_end, Vector3(0, 0, height * -0.5), radius, &auxres, &auxn);

	if (collided) {
		float d = norm.dot(auxres);

		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	if (collision) {

		r_result = res;
		r_normal = n;
	}
	return collision;
}

Vector3 CapsuleShapeSW::get_moment_of_inertia(float p_mass) const {

	// use crappy AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.y * extents.y + extents.y * extents.y));
}

void CapsuleShapeSW::_setup(real_t p_height, real_t p_radius) {

	height = p_height;
	radius = p_radius;
	configure(AABB(Vector3(-radius, -radius, -height * 0.5 - radius), Vector3(radius * 2, radius * 2, height + radius * 2.0)));
}

void CapsuleShapeSW::set_data(const Variant &p_data) {

	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("radius"));
	ERR_FAIL_COND(!d.has("height"));
	_setup(d["height"], d["radius"]);
}

Variant CapsuleShapeSW::get_data() const {

	Dictionary d;
	d["radius"] = radius;
	d["height"] = height;
	return d;
}

CapsuleShapeSW::CapsuleShapeSW() {

	height = radius = 0;
}

/********** CONVEX POLYGON *************/

void ConvexPolygonShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	int vertex_count = mesh.vertices.size();
	if (vertex_count == 0)
		return;

	const Vector3 *vrts = &mesh.vertices[0];

	for (int i = 0; i < vertex_count; i++) {

		float d = p_normal.dot(p_transform.xform(vrts[i]));

		if (i == 0 || d > r_max)
			r_max = d;
		if (i == 0 || d < r_min)
			r_min = d;
	}
}

Vector3 ConvexPolygonShapeSW::get_support(const Vector3 &p_normal) const {

	Vector3 n = p_normal;

	int vert_support_idx = -1;
	float support_max;

	int vertex_count = mesh.vertices.size();
	if (vertex_count == 0)
		return Vector3();

	const Vector3 *vrts = &mesh.vertices[0];

	for (int i = 0; i < vertex_count; i++) {

		float d = n.dot(vrts[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	return vrts[vert_support_idx];
}

void ConvexPolygonShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	const Geometry::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();

	const Geometry::MeshData::Edge *edges = mesh.edges.ptr();
	int ec = mesh.edges.size();

	const Vector3 *vertices = mesh.vertices.ptr();
	int vc = mesh.vertices.size();

	//find vertex first
	real_t max;
	int vtx;

	for (int i = 0; i < vc; i++) {

		float d = p_normal.dot(vertices[i]);

		if (i == 0 || d > max) {
			max = d;
			vtx = i;
		}
	}

	for (int i = 0; i < fc; i++) {

		if (faces[i].plane.normal.dot(p_normal) > _FACE_IS_VALID_SUPPORT_TRESHOLD) {

			int ic = faces[i].indices.size();
			const int *ind = faces[i].indices.ptr();

			bool valid = false;
			for (int j = 0; j < ic; j++) {
				if (ind[j] == vtx) {
					valid = true;
					break;
				}
			}

			if (!valid)
				continue;

			int m = MIN(p_max, ic);
			for (int j = 0; j < m; j++) {

				r_supports[j] = vertices[ind[j]];
			}
			r_amount = m;
			return;
		}
	}

	for (int i = 0; i < ec; i++) {

		float dot = (vertices[edges[i].a] - vertices[edges[i].b]).normalized().dot(p_normal);
		dot = ABS(dot);
		if (dot < _EDGE_IS_VALID_SUPPORT_TRESHOLD && (edges[i].a == vtx || edges[i].b == vtx)) {

			r_amount = 2;
			r_supports[0] = vertices[edges[i].a];
			r_supports[1] = vertices[edges[i].b];
			return;
		}
	}

	r_supports[0] = vertices[vtx];
	r_amount = 1;
}

bool ConvexPolygonShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	const Geometry::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();

	const Vector3 *vertices = mesh.vertices.ptr();

	Vector3 n = p_end - p_begin;
	float min = 1e20;
	bool col = false;

	for (int i = 0; i < fc; i++) {

		if (faces[i].plane.normal.dot(n) > 0)
			continue; //opposing face

		int ic = faces[i].indices.size();
		const int *ind = faces[i].indices.ptr();

		for (int j = 1; j < ic - 1; j++) {

			Face3 f(vertices[ind[0]], vertices[ind[j]], vertices[ind[j + 1]]);
			Vector3 result;
			if (f.intersects_segment(p_begin, p_end, &result)) {
				float d = n.dot(result);
				if (d < min) {
					min = d;
					r_result = result;
					r_normal = faces[i].plane.normal;
					col = true;
				}

				break;
			}
		}
	}

	return col;
}

Vector3 ConvexPolygonShapeSW::get_moment_of_inertia(float p_mass) const {

	// use crappy AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.y * extents.y + extents.y * extents.y));
}

void ConvexPolygonShapeSW::_setup(const Vector<Vector3> &p_vertices) {

	Error err = QuickHull::build(p_vertices, mesh);
	AABB _aabb;

	for (int i = 0; i < mesh.vertices.size(); i++) {

		if (i == 0)
			_aabb.pos = mesh.vertices[i];
		else
			_aabb.expand_to(mesh.vertices[i]);
	}

	configure(_aabb);
}

void ConvexPolygonShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant ConvexPolygonShapeSW::get_data() const {

	return mesh.vertices;
}

ConvexPolygonShapeSW::ConvexPolygonShapeSW() {
}

/********** FACE POLYGON *************/

void FaceShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	for (int i = 0; i < 3; i++) {

		Vector3 v = p_transform.xform(vertex[i]);
		float d = p_normal.dot(v);

		if (i == 0 || d > r_max)
			r_max = d;

		if (i == 0 || d < r_min)
			r_min = d;
	}
}

Vector3 FaceShapeSW::get_support(const Vector3 &p_normal) const {

	int vert_support_idx = -1;
	float support_max;

	for (int i = 0; i < 3; i++) {

		float d = p_normal.dot(vertex[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	return vertex[vert_support_idx];
}

void FaceShapeSW::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount) const {

	Vector3 n = p_normal;

	/** TEST FACE AS SUPPORT **/
	if (normal.dot(n) > _FACE_IS_VALID_SUPPORT_TRESHOLD) {

		r_amount = 3;
		for (int i = 0; i < 3; i++) {

			r_supports[i] = vertex[i];
		}
		return;
	}

	/** FIND SUPPORT VERTEX **/

	int vert_support_idx = -1;
	float support_max;

	for (int i = 0; i < 3; i++) {

		float d = n.dot(vertex[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	/** TEST EDGES AS SUPPORT **/

	for (int i = 0; i < 3; i++) {

		int nx = (i + 1) % 3;
		if (i != vert_support_idx && nx != vert_support_idx)
			continue;

		// check if edge is valid as a support
		float dot = (vertex[i] - vertex[nx]).normalized().dot(n);
		dot = ABS(dot);
		if (dot < _EDGE_IS_VALID_SUPPORT_TRESHOLD) {

			r_amount = 2;
			r_supports[0] = vertex[i];
			r_supports[1] = vertex[nx];
			return;
		}
	}

	r_amount = 1;
	r_supports[0] = vertex[vert_support_idx];
}

bool FaceShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	bool c = Geometry::segment_intersects_triangle(p_begin, p_end, vertex[0], vertex[1], vertex[2], &r_result);
	if (c) {
		r_normal = Plane(vertex[0], vertex[1], vertex[2]).normal;
		if (r_normal.dot(p_end - p_begin) > 0) {
			r_normal = -r_normal;
		}
	}

	return c;
}

Vector3 FaceShapeSW::get_moment_of_inertia(float p_mass) const {

	return Vector3(); // Sorry, but i don't think anyone cares, FaceShape!
}

FaceShapeSW::FaceShapeSW() {

	configure(AABB());
}

DVector<Vector3> ConcavePolygonShapeSW::get_faces() const {

	DVector<Vector3> rfaces;
	rfaces.resize(faces.size() * 3);

	for (int i = 0; i < faces.size(); i++) {

		Face f = faces.get(i);

		for (int j = 0; j < 3; j++) {

			rfaces.set(i * 3 + j, vertices.get(f.indices[j]));
		}
	}

	return rfaces;
}

void ConcavePolygonShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	int count = vertices.size();
	if (count == 0) {
		r_min = 0;
		r_max = 0;
		return;
	}
	DVector<Vector3>::Read r = vertices.read();
	const Vector3 *vptr = r.ptr();

	for (int i = 0; i < count; i++) {

		float d = p_normal.dot(p_transform.xform(vptr[i]));

		if (i == 0 || d > r_max)
			r_max = d;
		if (i == 0 || d < r_min)
			r_min = d;
	}
}

Vector3 ConcavePolygonShapeSW::get_support(const Vector3 &p_normal) const {

	int count = vertices.size();
	if (count == 0)
		return Vector3();

	DVector<Vector3>::Read r = vertices.read();
	const Vector3 *vptr = r.ptr();

	Vector3 n = p_normal;

	int vert_support_idx = -1;
	float support_max;

	for (int i = 0; i < count; i++) {

		float d = n.dot(vptr[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	return vptr[vert_support_idx];
}

void ConcavePolygonShapeSW::_cull_segment(int p_idx, _SegmentCullParams *p_params) const {

	const BVH *bvh = &p_params->bvh[p_idx];

	//if (p_params->dir.dot(bvh->aabb.get_support(-p_params->dir))>p_params->min_d)
	//	return; //test against whole AABB, which isn't very costly

	//printf("addr: %p\n",bvh);
	if (!bvh->aabb.intersects_segment(p_params->from, p_params->to)) {

		return;
	}

	if (bvh->face_index >= 0) {

		Vector3 res;
		Vector3 vertices[3] = {
			p_params->vertices[p_params->faces[bvh->face_index].indices[0]],
			p_params->vertices[p_params->faces[bvh->face_index].indices[1]],
			p_params->vertices[p_params->faces[bvh->face_index].indices[2]]
		};

		if (Geometry::segment_intersects_triangle(
					p_params->from,
					p_params->to,
					vertices[0],
					vertices[1],
					vertices[2],
					&res)) {

			float d = p_params->dir.dot(res) - p_params->dir.dot(p_params->from);
			//TODO, seems segmen/triangle intersection is broken :(
			if (d > 0 && d < p_params->min_d) {

				p_params->min_d = d;
				p_params->result = res;
				p_params->normal = Plane(vertices[0], vertices[1], vertices[2]).normal;
				p_params->collisions++;
			}
		}

	} else {

		if (bvh->left >= 0)
			_cull_segment(bvh->left, p_params);
		if (bvh->right >= 0)
			_cull_segment(bvh->right, p_params);
	}
}

bool ConcavePolygonShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal) const {

	if (faces.size() == 0)
		return false;

	// unlock data
	DVector<Face>::Read fr = faces.read();
	DVector<Vector3>::Read vr = vertices.read();
	DVector<BVH>::Read br = bvh.read();

	_SegmentCullParams params;
	params.from = p_begin;
	params.to = p_end;
	params.collisions = 0;
	params.dir = (p_end - p_begin).normalized();

	params.faces = fr.ptr();
	params.vertices = vr.ptr();
	params.bvh = br.ptr();

	params.min_d = 1e20;
	// cull
	_cull_segment(0, &params);

	if (params.collisions > 0) {

		r_result = params.result;
		r_normal = params.normal;
		return true;
	} else {

		return false;
	}
}

void ConcavePolygonShapeSW::_cull(int p_idx, _CullParams *p_params) const {

	const BVH *bvh = &p_params->bvh[p_idx];

	if (!p_params->aabb.intersects(bvh->aabb))
		return;

	if (bvh->face_index >= 0) {

		const Face *f = &p_params->faces[bvh->face_index];
		FaceShapeSW *face = p_params->face;
		face->normal = f->normal;
		face->vertex[0] = p_params->vertices[f->indices[0]];
		face->vertex[1] = p_params->vertices[f->indices[1]];
		face->vertex[2] = p_params->vertices[f->indices[2]];
		p_params->callback(p_params->userdata, face);

	} else {

		if (bvh->left >= 0) {

			_cull(bvh->left, p_params);
		}

		if (bvh->right >= 0) {

			_cull(bvh->right, p_params);
		}
	}
}

void ConcavePolygonShapeSW::cull(const AABB &p_local_aabb, Callback p_callback, void *p_userdata) const {

	// make matrix local to concave
	if (faces.size() == 0)
		return;

	AABB local_aabb = p_local_aabb;

	// unlock data
	DVector<Face>::Read fr = faces.read();
	DVector<Vector3>::Read vr = vertices.read();
	DVector<BVH>::Read br = bvh.read();

	FaceShapeSW face; // use this to send in the callback

	_CullParams params;
	params.aabb = local_aabb;
	params.face = &face;
	params.faces = fr.ptr();
	params.vertices = vr.ptr();
	params.bvh = br.ptr();
	params.callback = p_callback;
	params.userdata = p_userdata;

	// cull
	_cull(0, &params);
}

Vector3 ConcavePolygonShapeSW::get_moment_of_inertia(float p_mass) const {

	// use crappy AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.y * extents.y + extents.y * extents.y));
}

struct _VolumeSW_BVH_Element {

	AABB aabb;
	Vector3 center;
	int face_index;
};

struct _VolumeSW_BVH_CompareX {

	_FORCE_INLINE_ bool operator()(const _VolumeSW_BVH_Element &a, const _VolumeSW_BVH_Element &b) const {

		return a.center.x < b.center.x;
	}
};

struct _VolumeSW_BVH_CompareY {

	_FORCE_INLINE_ bool operator()(const _VolumeSW_BVH_Element &a, const _VolumeSW_BVH_Element &b) const {

		return a.center.y < b.center.y;
	}
};

struct _VolumeSW_BVH_CompareZ {

	_FORCE_INLINE_ bool operator()(const _VolumeSW_BVH_Element &a, const _VolumeSW_BVH_Element &b) const {

		return a.center.z < b.center.z;
	}
};

struct _VolumeSW_BVH {

	AABB aabb;
	_VolumeSW_BVH *left;
	_VolumeSW_BVH *right;

	int face_index;
};

_VolumeSW_BVH *_volume_sw_build_bvh(_VolumeSW_BVH_Element *p_elements, int p_size, int &count) {

	_VolumeSW_BVH *bvh = memnew(_VolumeSW_BVH);

	if (p_size == 1) {
		//leaf
		bvh->aabb = p_elements[0].aabb;
		bvh->left = NULL;
		bvh->right = NULL;
		bvh->face_index = p_elements->face_index;
		count++;
		return bvh;
	} else {

		bvh->face_index = -1;
	}

	AABB aabb;
	for (int i = 0; i < p_size; i++) {

		if (i == 0)
			aabb = p_elements[i].aabb;
		else
			aabb.merge_with(p_elements[i].aabb);
	}
	bvh->aabb = aabb;
	switch (aabb.get_longest_axis_index()) {

		case 0: {

			SortArray<_VolumeSW_BVH_Element, _VolumeSW_BVH_CompareX> sort_x;
			sort_x.sort(p_elements, p_size);

		} break;
		case 1: {

			SortArray<_VolumeSW_BVH_Element, _VolumeSW_BVH_CompareY> sort_y;
			sort_y.sort(p_elements, p_size);
		} break;
		case 2: {

			SortArray<_VolumeSW_BVH_Element, _VolumeSW_BVH_CompareZ> sort_z;
			sort_z.sort(p_elements, p_size);
		} break;
	}

	int split = p_size / 2;
	bvh->left = _volume_sw_build_bvh(p_elements, split, count);
	bvh->right = _volume_sw_build_bvh(&p_elements[split], p_size - split, count);

	//	printf("branch at %p - %i: %i\n",bvh,count,bvh->face_index);
	count++;
	return bvh;
}

void ConcavePolygonShapeSW::_fill_bvh(_VolumeSW_BVH *p_bvh_tree, BVH *p_bvh_array, int &p_idx) {

	int idx = p_idx;

	p_bvh_array[idx].aabb = p_bvh_tree->aabb;
	p_bvh_array[idx].face_index = p_bvh_tree->face_index;
	//	printf("%p - %i: %i(%p)  -- %p:%p\n",%p_bvh_array[idx],p_idx,p_bvh_array[i]->face_index,&p_bvh_tree->face_index,p_bvh_tree->left,p_bvh_tree->right);

	if (p_bvh_tree->left) {
		p_bvh_array[idx].left = ++p_idx;
		_fill_bvh(p_bvh_tree->left, p_bvh_array, p_idx);

	} else {

		p_bvh_array[p_idx].left = -1;
	}

	if (p_bvh_tree->right) {
		p_bvh_array[idx].right = ++p_idx;
		_fill_bvh(p_bvh_tree->right, p_bvh_array, p_idx);

	} else {

		p_bvh_array[p_idx].right = -1;
	}

	memdelete(p_bvh_tree);
}

void ConcavePolygonShapeSW::_setup(DVector<Vector3> p_faces) {

	int src_face_count = p_faces.size();
	if (src_face_count == 0) {
		configure(AABB());
		return;
	}
	ERR_FAIL_COND(src_face_count % 3);
	src_face_count /= 3;

	DVector<Vector3>::Read r = p_faces.read();
	const Vector3 *facesr = r.ptr();

#if 0
	Map<Vector3,int> point_map;
	List<Face> face_list;


	for(int i=0;i<src_face_count;i++) {

		Face3 faceaux;

		for(int j=0;j<3;j++) {

			faceaux.vertex[j]=facesr[i*3+j].snapped(_POINT_SNAP);
			//faceaux.vertex[j]=facesr[i*3+j];//facesr[i*3+j].snapped(_POINT_SNAP);
		}

		ERR_CONTINUE( faceaux.is_degenerate() );

		Face face;

		for(int j=0;j<3;j++) {


			Map<Vector3,int>::Element *E=point_map.find(faceaux.vertex[j]);
			if (E) {

				face.indices[j]=E->value();
			} else {

				face.indices[j]=point_map.size();
				point_map.insert(faceaux.vertex[j],point_map.size());

			}
		}

		face_list.push_back(face);
	}

	vertices.resize( point_map.size() );

	DVector<Vector3>::Write vw = vertices.write();
	Vector3 *verticesw=vw.ptr();

	AABB _aabb;

	for( Map<Vector3,int>::Element *E=point_map.front();E;E=E->next()) {

		if (E==point_map.front()) {
			_aabb.pos=E->key();
		} else {

			_aabb.expand_to(E->key());
		}
		verticesw[E->value()]=E->key();
	}

	point_map.clear(); // not needed anymore

	faces.resize(face_list.size());
	DVector<Face>::Write w = faces.write();
	Face *facesw=w.ptr();

	int fc=0;

	for( List<Face>::Element *E=face_list.front();E;E=E->next()) {

		facesw[fc++]=E->get();
	}

	face_list.clear();


	DVector<_VolumeSW_BVH_Element> bvh_array;
	bvh_array.resize( fc );

	DVector<_VolumeSW_BVH_Element>::Write bvhw = bvh_array.write();
	_VolumeSW_BVH_Element *bvh_arrayw=bvhw.ptr();


	for(int i=0;i<fc;i++) {

		AABB face_aabb;
		face_aabb.pos=verticesw[facesw[i].indices[0]];
		face_aabb.expand_to( verticesw[facesw[i].indices[1]] );
		face_aabb.expand_to( verticesw[facesw[i].indices[2]] );

		bvh_arrayw[i].face_index=i;
		bvh_arrayw[i].aabb=face_aabb;
		bvh_arrayw[i].center=face_aabb.pos+face_aabb.size*0.5;

	}

	w=DVector<Face>::Write();
	vw=DVector<Vector3>::Write();


	int count=0;
	_VolumeSW_BVH *bvh_tree=_volume_sw_build_bvh(bvh_arrayw,fc,count);

	ERR_FAIL_COND(count==0);

	bvhw=DVector<_VolumeSW_BVH_Element>::Write();

	bvh.resize( count+1 );

	DVector<BVH>::Write bvhw2 = bvh.write();
	BVH*bvh_arrayw2=bvhw2.ptr();

	int idx=0;
	_fill_bvh(bvh_tree,bvh_arrayw2,idx);

	set_aabb(_aabb);

#else
	DVector<_VolumeSW_BVH_Element> bvh_array;
	bvh_array.resize(src_face_count);

	DVector<_VolumeSW_BVH_Element>::Write bvhw = bvh_array.write();
	_VolumeSW_BVH_Element *bvh_arrayw = bvhw.ptr();

	faces.resize(src_face_count);
	DVector<Face>::Write w = faces.write();
	Face *facesw = w.ptr();

	vertices.resize(src_face_count * 3);

	DVector<Vector3>::Write vw = vertices.write();
	Vector3 *verticesw = vw.ptr();

	AABB _aabb;

	for (int i = 0; i < src_face_count; i++) {

		Face3 face(facesr[i * 3 + 0], facesr[i * 3 + 1], facesr[i * 3 + 2]);

		bvh_arrayw[i].aabb = face.get_aabb();
		bvh_arrayw[i].center = bvh_arrayw[i].aabb.pos + bvh_arrayw[i].aabb.size * 0.5;
		bvh_arrayw[i].face_index = i;
		facesw[i].indices[0] = i * 3 + 0;
		facesw[i].indices[1] = i * 3 + 1;
		facesw[i].indices[2] = i * 3 + 2;
		facesw[i].normal = face.get_plane().normal;
		verticesw[i * 3 + 0] = face.vertex[0];
		verticesw[i * 3 + 1] = face.vertex[1];
		verticesw[i * 3 + 2] = face.vertex[2];
		if (i == 0)
			_aabb = bvh_arrayw[i].aabb;
		else
			_aabb.merge_with(bvh_arrayw[i].aabb);
	}

	w = DVector<Face>::Write();
	vw = DVector<Vector3>::Write();

	int count = 0;
	_VolumeSW_BVH *bvh_tree = _volume_sw_build_bvh(bvh_arrayw, src_face_count, count);

	bvh.resize(count + 1);

	DVector<BVH>::Write bvhw2 = bvh.write();
	BVH *bvh_arrayw2 = bvhw2.ptr();

	int idx = 0;
	_fill_bvh(bvh_tree, bvh_arrayw2, idx);

	configure(_aabb); // this type of shape has no margin

#endif
}

void ConcavePolygonShapeSW::set_data(const Variant &p_data) {

	_setup(p_data);
}

Variant ConcavePolygonShapeSW::get_data() const {

	return get_faces();
}

ConcavePolygonShapeSW::ConcavePolygonShapeSW() {
}

/* HEIGHT MAP SHAPE */

DVector<float> HeightMapShapeSW::get_heights() const {

	return heights;
}
int HeightMapShapeSW::get_width() const {

	return width;
}
int HeightMapShapeSW::get_depth() const {

	return depth;
}
float HeightMapShapeSW::get_cell_size() const {

	return cell_size;
}

void HeightMapShapeSW::project_range(const Vector3 &p_normal, const Transform &p_transform, real_t &r_min, real_t &r_max) const {

	//not very useful, but not very used either
	p_transform.xform(get_aabb()).project_range_in_plane(Plane(p_normal, 0), r_min, r_max);
}

Vector3 HeightMapShapeSW::get_support(const Vector3 &p_normal) const {

	//not very useful, but not very used either
	return get_aabb().get_support(p_normal);
}

bool HeightMapShapeSW::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_point, Vector3 &r_normal) const {

	return false;
}

void HeightMapShapeSW::cull(const AABB &p_local_aabb, Callback p_callback, void *p_userdata) const {
}

Vector3 HeightMapShapeSW::get_moment_of_inertia(float p_mass) const {

	// use crappy AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.y * extents.y + extents.y * extents.y));
}

void HeightMapShapeSW::_setup(DVector<real_t> p_heights, int p_width, int p_depth, real_t p_cell_size) {

	heights = p_heights;
	width = p_width;
	depth = p_depth;
	cell_size = p_cell_size;

	DVector<real_t>::Read r = heights.read();

	AABB aabb;

	for (int i = 0; i < depth; i++) {

		for (int j = 0; j < width; j++) {

			float h = r[i * width + j];

			Vector3 pos(j * cell_size, h, i * cell_size);
			if (i == 0 || j == 0)
				aabb.pos = pos;
			else
				aabb.expand_to(pos);
		}
	}

	configure(aabb);
}

void HeightMapShapeSW::set_data(const Variant &p_data) {

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);
	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("width"));
	ERR_FAIL_COND(!d.has("depth"));
	ERR_FAIL_COND(!d.has("cell_size"));
	ERR_FAIL_COND(!d.has("heights"));

	int width = d["width"];
	int depth = d["depth"];
	float cell_size = d["cell_size"];
	DVector<float> heights = d["heights"];

	ERR_FAIL_COND(width <= 0);
	ERR_FAIL_COND(depth <= 0);
	ERR_FAIL_COND(cell_size <= CMP_EPSILON);
	ERR_FAIL_COND(heights.size() != (width * depth));
	_setup(heights, width, depth, cell_size);
}

Variant HeightMapShapeSW::get_data() const {

	ERR_FAIL_V(Variant());
}

HeightMapShapeSW::HeightMapShapeSW() {

	width = 0;
	depth = 0;
	cell_size = 0;
}
