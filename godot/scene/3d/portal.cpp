/*************************************************************************/
/*  portal.cpp                                                           */
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
#include "portal.h"
#include "globals.h"
#include "scene/resources/surface_tool.h"
#include "servers/visual_server.h"

bool Portal::_set(const StringName &p_name, const Variant &p_value) {

	if (p_name == "shape") {
		DVector<float> src_coords = p_value;
		Vector<Point2> points;
		int src_coords_size = src_coords.size();
		ERR_FAIL_COND_V(src_coords_size % 2, false);
		points.resize(src_coords_size / 2);
		for (int i = 0; i < points.size(); i++) {

			points[i].x = src_coords[i * 2 + 0];
			points[i].y = src_coords[i * 2 + 1];
			set_shape(points);
		}
	} else if (p_name == "enabled") {
		set_enabled(p_value);
	} else if (p_name == "disable_distance") {
		set_disable_distance(p_value);
	} else if (p_name == "disabled_color") {
		set_disabled_color(p_value);
	} else if (p_name == "connect_range") {
		set_connect_range(p_value);
	} else
		return false;

	return true;
}

bool Portal::_get(const StringName &p_name, Variant &r_ret) const {

	if (p_name == "shape") {
		Vector<Point2> points = get_shape();
		DVector<float> dst_coords;
		dst_coords.resize(points.size() * 2);

		for (int i = 0; i < points.size(); i++) {

			dst_coords.set(i * 2 + 0, points[i].x);
			dst_coords.set(i * 2 + 1, points[i].y);
		}

		r_ret = dst_coords;
	} else if (p_name == "enabled") {
		r_ret = is_enabled();
	} else if (p_name == "disable_distance") {
		r_ret = get_disable_distance();
	} else if (p_name == "disabled_color") {
		r_ret = get_disabled_color();
	} else if (p_name == "connect_range") {
		r_ret = get_connect_range();
	} else
		return false;
	return true;
}

void Portal::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::REAL_ARRAY, "shape"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "enabled"));
	p_list->push_back(PropertyInfo(Variant::REAL, "disable_distance", PROPERTY_HINT_RANGE, "0,4096,0.01"));
	p_list->push_back(PropertyInfo(Variant::COLOR, "disabled_color"));
	p_list->push_back(PropertyInfo(Variant::REAL, "connect_range", PROPERTY_HINT_RANGE, "0.1,4096,0.01"));
}

RES Portal::_get_gizmo_geometry() const {

	Ref<SurfaceTool> surface_tool(memnew(SurfaceTool));

	Ref<FixedMaterial> mat(memnew(FixedMaterial));

	mat->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1.0, 0.8, 0.8, 0.7));
	mat->set_line_width(4);
	mat->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	mat->set_flag(Material::FLAG_UNSHADED, true);
	//	mat->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER,true);

	surface_tool->begin(Mesh::PRIMITIVE_LINES);
	surface_tool->set_material(mat);

	Vector<Point2> shape = get_shape();

	Vector2 center;
	for (int i = 0; i < shape.size(); i++) {

		int n = (i + 1) % shape.size();
		Vector<Vector3> points;
		surface_tool->add_vertex(Vector3(shape[i].x, shape[i].y, 0));
		surface_tool->add_vertex(Vector3(shape[n].x, shape[n].y, 0));
		center += shape[i];
	}

	if (shape.size() > 0) {

		center /= shape.size();
		Vector<Vector3> points;
		surface_tool->add_vertex(Vector3(center.x, center.y, 0));
		surface_tool->add_vertex(Vector3(center.x, center.y, 1.0));
	}

	return surface_tool->commit();
}

AABB Portal::get_aabb() const {

	return aabb;
}
DVector<Face3> Portal::get_faces(uint32_t p_usage_flags) const {

	if (!(p_usage_flags & FACES_ENCLOSING))
		return DVector<Face3>();

	Vector<Point2> shape = get_shape();
	if (shape.size() == 0)
		return DVector<Face3>();

	Vector2 center;
	for (int i = 0; i < shape.size(); i++) {

		center += shape[i];
	}

	DVector<Face3> ret;
	center /= shape.size();

	for (int i = 0; i < shape.size(); i++) {

		int n = (i + 1) % shape.size();

		Face3 f;
		f.vertex[0] = Vector3(center.x, center.y, 0);
		f.vertex[1] = Vector3(shape[i].x, shape[i].y, 0);
		f.vertex[2] = Vector3(shape[n].x, shape[n].y, 0);
		ret.push_back(f);
	}

	return ret;
}

void Portal::set_shape(const Vector<Point2> &p_shape) {

	VisualServer::get_singleton()->portal_set_shape(portal, p_shape);
	update_gizmo();
}

Vector<Point2> Portal::get_shape() const {

	return VisualServer::get_singleton()->portal_get_shape(portal);
}

void Portal::set_connect_range(float p_range) {

	connect_range = p_range;
	VisualServer::get_singleton()->portal_set_connect_range(portal, p_range);
}

float Portal::get_connect_range() const {

	return connect_range;
}

void Portal::set_enabled(bool p_enabled) {

	enabled = p_enabled;
	VisualServer::get_singleton()->portal_set_enabled(portal, enabled);
}

bool Portal::is_enabled() const {

	return enabled;
}

void Portal::set_disable_distance(float p_distance) {

	disable_distance = p_distance;
	VisualServer::get_singleton()->portal_set_disable_distance(portal, disable_distance);
}
float Portal::get_disable_distance() const {

	return disable_distance;
}

void Portal::set_disabled_color(const Color &p_disabled_color) {

	disabled_color = p_disabled_color;
	VisualServer::get_singleton()->portal_set_disabled_color(portal, disabled_color);
}

Color Portal::get_disabled_color() const {

	return disabled_color;
}

void Portal::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_shape", "points"), &Portal::set_shape);
	ObjectTypeDB::bind_method(_MD("get_shape"), &Portal::get_shape);

	ObjectTypeDB::bind_method(_MD("set_enabled", "enable"), &Portal::set_enabled);
	ObjectTypeDB::bind_method(_MD("is_enabled"), &Portal::is_enabled);

	ObjectTypeDB::bind_method(_MD("set_disable_distance", "distance"), &Portal::set_disable_distance);
	ObjectTypeDB::bind_method(_MD("get_disable_distance"), &Portal::get_disable_distance);

	ObjectTypeDB::bind_method(_MD("set_disabled_color", "color"), &Portal::set_disabled_color);
	ObjectTypeDB::bind_method(_MD("get_disabled_color"), &Portal::get_disabled_color);

	ObjectTypeDB::bind_method(_MD("set_connect_range", "range"), &Portal::set_connect_range);
	ObjectTypeDB::bind_method(_MD("get_connect_range"), &Portal::get_connect_range);
}

Portal::Portal() {

	portal = VisualServer::get_singleton()->portal_create();
	Vector<Point2> points;
	points.push_back(Point2(-1, 1));
	points.push_back(Point2(1, 1));
	points.push_back(Point2(1, -1));
	points.push_back(Point2(-1, -1));
	set_shape(points); // default shape

	set_connect_range(0.8);
	set_disable_distance(50);
	set_enabled(true);

	set_base(portal);
}

Portal::~Portal() {

	VisualServer::get_singleton()->free(portal);
}
