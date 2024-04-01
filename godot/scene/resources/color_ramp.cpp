/*************************************************************************/
/*  color_ramp.cpp                                                       */
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
#include "color_ramp.h"

//setter and getter names for property serialization
#define COLOR_RAMP_GET_OFFSETS "get_offsets"
#define COLOR_RAMP_GET_COLORS "get_colors"
#define COLOR_RAMP_SET_OFFSETS "set_offsets"
#define COLOR_RAMP_SET_COLORS "set_colors"

ColorRamp::ColorRamp() {
	//Set initial color ramp transition from black to white
	points.resize(2);
	points[0].color = Color(0, 0, 0, 1);
	points[0].offset = 0;
	points[1].color = Color(1, 1, 1, 1);
	points[1].offset = 1;
	is_sorted = true;
}

ColorRamp::~ColorRamp() {
}

void ColorRamp::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_point", "offset", "color"), &ColorRamp::add_point);
	ObjectTypeDB::bind_method(_MD("remove_point", "offset", "color"), &ColorRamp::remove_point);

	ObjectTypeDB::bind_method(_MD("set_offset", "point", "offset"), &ColorRamp::set_offset);
	ObjectTypeDB::bind_method(_MD("get_offset", "point"), &ColorRamp::get_offset);

	ObjectTypeDB::bind_method(_MD("set_color", "point", "color"), &ColorRamp::set_color);
	ObjectTypeDB::bind_method(_MD("get_color", "point"), &ColorRamp::get_color);

	ObjectTypeDB::bind_method(_MD("interpolate", "offset"), &ColorRamp::get_color_at_offset);

	ObjectTypeDB::bind_method(_MD("get_point_count"), &ColorRamp::get_points_count);

	ObjectTypeDB::bind_method(_MD(COLOR_RAMP_SET_OFFSETS, "offsets"), &ColorRamp::set_offsets);
	ObjectTypeDB::bind_method(_MD(COLOR_RAMP_GET_OFFSETS), &ColorRamp::get_offsets);

	ObjectTypeDB::bind_method(_MD(COLOR_RAMP_SET_COLORS, "colors"), &ColorRamp::set_colors);
	ObjectTypeDB::bind_method(_MD(COLOR_RAMP_GET_COLORS), &ColorRamp::get_colors);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "offsets"), _SCS(COLOR_RAMP_SET_OFFSETS), _SCS(COLOR_RAMP_GET_OFFSETS));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "colors"), _SCS(COLOR_RAMP_SET_COLORS), _SCS(COLOR_RAMP_GET_COLORS));
}

Vector<float> ColorRamp::get_offsets() const {
	Vector<float> offsets;
	offsets.resize(points.size());
	for (int i = 0; i < points.size(); i++) {
		offsets[i] = points[i].offset;
	}
	return offsets;
}

Vector<Color> ColorRamp::get_colors() const {
	Vector<Color> colors;
	colors.resize(points.size());
	for (int i = 0; i < points.size(); i++) {
		colors[i] = points[i].color;
	}
	return colors;
}

void ColorRamp::set_offsets(const Vector<float> &p_offsets) {
	points.resize(p_offsets.size());
	for (int i = 0; i < points.size(); i++) {
		points[i].offset = p_offsets[i];
	}
	is_sorted = false;
}

void ColorRamp::set_colors(const Vector<Color> &p_colors) {
	if (points.size() < p_colors.size())
		is_sorted = false;
	points.resize(p_colors.size());
	for (int i = 0; i < points.size(); i++) {
		points[i].color = p_colors[i];
	}
}

Vector<ColorRamp::Point> &ColorRamp::get_points() {
	return points;
}

void ColorRamp::add_point(float p_offset, const Color &p_color) {

	Point p;
	p.offset = p_offset;
	p.color = p_color;
	is_sorted = false;
	points.push_back(p);
}

void ColorRamp::remove_point(int p_index) {

	ERR_FAIL_INDEX(p_index, points.size());
	ERR_FAIL_COND(points.size() <= 2);
	points.remove(p_index);
}

void ColorRamp::set_points(Vector<ColorRamp::Point> &p_points) {
	points = p_points;
	is_sorted = false;
}

void ColorRamp::set_offset(int pos, const float offset) {
	if (points.size() <= pos)
		points.resize(pos + 1);
	points[pos].offset = offset;
	is_sorted = false;
}

float ColorRamp::get_offset(int pos) const {
	if (points.size() && points.size() > pos)
		return points[pos].offset;
	return 0; //TODO: Maybe throw some error instead?
}

void ColorRamp::set_color(int pos, const Color &color) {
	if (points.size() <= pos) {
		points.resize(pos + 1);
		is_sorted = false;
	}
	points[pos].color = color;
}

Color ColorRamp::get_color(int pos) const {
	if (points.size() && points.size() > pos)
		return points[pos].color;
	return Color(0, 0, 0, 1); //TODO: Maybe throw some error instead?
}

int ColorRamp::get_points_count() const {
	return points.size();
}
