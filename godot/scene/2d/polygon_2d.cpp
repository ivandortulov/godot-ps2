/*************************************************************************/
/*  polygon_2d.cpp                                                       */
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
#include "polygon_2d.h"

Rect2 Polygon2D::get_item_rect() const {

	if (rect_cache_dirty) {
		int l = polygon.size();
		DVector<Vector2>::Read r = polygon.read();
		item_rect = Rect2();
		for (int i = 0; i < l; i++) {
			Vector2 pos = r[i] + offset;
			if (i == 0)
				item_rect.pos = pos;
			else
				item_rect.expand_to(pos);
		}
		item_rect = item_rect.grow(20);
		rect_cache_dirty = false;
	}

	return item_rect;
}

void Polygon2D::edit_set_pivot(const Point2 &p_pivot) {

	set_offset(p_pivot);
}

Point2 Polygon2D::edit_get_pivot() const {

	return get_offset();
}
bool Polygon2D::edit_has_pivot() const {

	return true;
}

void Polygon2D::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_DRAW: {

			if (polygon.size() < 3)
				return;

			Vector<Vector2> points;
			Vector<Vector2> uvs;

			points.resize(polygon.size());

			int len = points.size();
			{

				DVector<Vector2>::Read polyr = polygon.read();
				for (int i = 0; i < len; i++) {
					points[i] = polyr[i] + offset;
				}
			}

			if (invert) {

				Rect2 bounds;
				int highest_idx = -1;
				float highest_y = -1e20;
				float sum = 0;

				for (int i = 0; i < len; i++) {
					if (i == 0)
						bounds.pos = points[i];
					else
						bounds.expand_to(points[i]);
					if (points[i].y > highest_y) {
						highest_idx = i;
						highest_y = points[i].y;
					}
					int ni = (i + 1) % len;
					sum += (points[ni].x - points[i].x) * (points[ni].y + points[i].y);
				}

				bounds = bounds.grow(invert_border);

				Vector2 ep[7] = {
					Vector2(points[highest_idx].x, points[highest_idx].y + invert_border),
					Vector2(bounds.pos + bounds.size),
					Vector2(bounds.pos + Vector2(bounds.size.x, 0)),
					Vector2(bounds.pos),
					Vector2(bounds.pos + Vector2(0, bounds.size.y)),
					Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y + invert_border),
					Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y),
				};

				if (sum > 0) {
					SWAP(ep[1], ep[4]);
					SWAP(ep[2], ep[3]);
					SWAP(ep[5], ep[0]);
					SWAP(ep[6], points[highest_idx]);
				}

				points.resize(points.size() + 7);
				for (int i = points.size() - 1; i >= highest_idx + 7; i--) {

					points[i] = points[i - 7];
				}

				for (int i = 0; i < 7; i++) {

					points[highest_idx + i + 1] = ep[i];
				}

				len = points.size();
			}

			if (texture.is_valid()) {

				Matrix32 texmat(tex_rot, tex_ofs);
				texmat.scale(tex_scale);
				Size2 tex_size = Vector2(1, 1);

				tex_size = texture->get_size();
				uvs.resize(points.size());

				if (points.size() == uv.size()) {

					DVector<Vector2>::Read uvr = uv.read();

					for (int i = 0; i < len; i++) {
						uvs[i] = texmat.xform(uvr[i]) / tex_size;
					}

				} else {
					for (int i = 0; i < len; i++) {
						uvs[i] = texmat.xform(points[i]) / tex_size;
					}
				}
			}

			Vector<Color> colors;
			int color_len = vertex_colors.size();
			if (color_len == 0) {
				// No vertex colors => Pass only the main color
				// The rasterizer handles this case especially, taking alpha into account
				colors.push_back(color);
			} else {
				// Vertex colors present => Fill color array and pad with main color as necessary
				colors.resize(len);
				DVector<Color>::Read color_r = vertex_colors.read();
				for (int i = 0; i < color_len && i < len; i++) {
					colors[i] = color_r[i];
				}
				for (int i = color_len; i < len; i++) {
					colors[i] = color;
				}
			}

			Vector<int> indices = Geometry::triangulate_polygon(points);

			VS::get_singleton()->canvas_item_add_triangle_array(get_canvas_item(), indices, points, colors, uvs, texture.is_valid() ? texture->get_rid() : RID());

		} break;
	}
}

void Polygon2D::set_polygon(const DVector<Vector2> &p_polygon) {
	polygon = p_polygon;
	rect_cache_dirty = true;
	update();
}

DVector<Vector2> Polygon2D::get_polygon() const {

	return polygon;
}

void Polygon2D::set_uv(const DVector<Vector2> &p_uv) {

	uv = p_uv;
	update();
}

DVector<Vector2> Polygon2D::get_uv() const {

	return uv;
}

void Polygon2D::set_color(const Color &p_color) {

	color = p_color;
	update();
}
Color Polygon2D::get_color() const {

	return color;
}

void Polygon2D::set_vertex_colors(const DVector<Color> &p_colors) {

	vertex_colors = p_colors;
	update();
}
DVector<Color> Polygon2D::get_vertex_colors() const {

	return vertex_colors;
}

void Polygon2D::set_texture(const Ref<Texture> &p_texture) {

	texture = p_texture;

	/*if (texture.is_valid()) {
		uint32_t flags=texture->get_flags();
		flags&=~Texture::FLAG_REPEAT;
		if (tex_tile)
			flags|=Texture::FLAG_REPEAT;

		texture->set_flags(flags);
	}*/
	update();
}
Ref<Texture> Polygon2D::get_texture() const {

	return texture;
}

void Polygon2D::set_texture_offset(const Vector2 &p_offset) {

	tex_ofs = p_offset;
	update();
}
Vector2 Polygon2D::get_texture_offset() const {

	return tex_ofs;
}

void Polygon2D::set_texture_rotation(float p_rot) {

	tex_rot = p_rot;
	update();
}
float Polygon2D::get_texture_rotation() const {

	return tex_rot;
}

void Polygon2D::_set_texture_rotationd(float p_rot) {

	set_texture_rotation(Math::deg2rad(p_rot));
}
float Polygon2D::_get_texture_rotationd() const {

	return Math::rad2deg(get_texture_rotation());
}

void Polygon2D::set_texture_scale(const Size2 &p_scale) {

	tex_scale = p_scale;
	update();
}
Size2 Polygon2D::get_texture_scale() const {

	return tex_scale;
}

void Polygon2D::set_invert(bool p_invert) {

	invert = p_invert;
	update();
}
bool Polygon2D::get_invert() const {

	return invert;
}

void Polygon2D::set_invert_border(float p_invert_border) {

	invert_border = p_invert_border;
	update();
}
float Polygon2D::get_invert_border() const {

	return invert_border;
}

void Polygon2D::set_offset(const Vector2 &p_offset) {

	offset = p_offset;
	rect_cache_dirty = true;
	update();
}

Vector2 Polygon2D::get_offset() const {

	return offset;
}

void Polygon2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_polygon", "polygon"), &Polygon2D::set_polygon);
	ObjectTypeDB::bind_method(_MD("get_polygon"), &Polygon2D::get_polygon);

	ObjectTypeDB::bind_method(_MD("set_uv", "uv"), &Polygon2D::set_uv);
	ObjectTypeDB::bind_method(_MD("get_uv"), &Polygon2D::get_uv);

	ObjectTypeDB::bind_method(_MD("set_color", "color"), &Polygon2D::set_color);
	ObjectTypeDB::bind_method(_MD("get_color"), &Polygon2D::get_color);

	ObjectTypeDB::bind_method(_MD("set_vertex_colors", "vertex_colors"), &Polygon2D::set_vertex_colors);
	ObjectTypeDB::bind_method(_MD("get_vertex_colors"), &Polygon2D::get_vertex_colors);

	ObjectTypeDB::bind_method(_MD("set_texture", "texture"), &Polygon2D::set_texture);
	ObjectTypeDB::bind_method(_MD("get_texture"), &Polygon2D::get_texture);

	ObjectTypeDB::bind_method(_MD("set_texture_offset", "texture_offset"), &Polygon2D::set_texture_offset);
	ObjectTypeDB::bind_method(_MD("get_texture_offset"), &Polygon2D::get_texture_offset);

	ObjectTypeDB::bind_method(_MD("set_texture_rotation", "texture_rotation"), &Polygon2D::set_texture_rotation);
	ObjectTypeDB::bind_method(_MD("get_texture_rotation"), &Polygon2D::get_texture_rotation);

	ObjectTypeDB::bind_method(_MD("_set_texture_rotationd", "texture_rotation"), &Polygon2D::_set_texture_rotationd);
	ObjectTypeDB::bind_method(_MD("_get_texture_rotationd"), &Polygon2D::_get_texture_rotationd);

	ObjectTypeDB::bind_method(_MD("set_texture_scale", "texture_scale"), &Polygon2D::set_texture_scale);
	ObjectTypeDB::bind_method(_MD("get_texture_scale"), &Polygon2D::get_texture_scale);

	ObjectTypeDB::bind_method(_MD("set_invert", "invert"), &Polygon2D::set_invert);
	ObjectTypeDB::bind_method(_MD("get_invert"), &Polygon2D::get_invert);

	ObjectTypeDB::bind_method(_MD("set_invert_border", "invert_border"), &Polygon2D::set_invert_border);
	ObjectTypeDB::bind_method(_MD("get_invert_border"), &Polygon2D::get_invert_border);

	ObjectTypeDB::bind_method(_MD("set_offset", "offset"), &Polygon2D::set_offset);
	ObjectTypeDB::bind_method(_MD("get_offset"), &Polygon2D::get_offset);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2_ARRAY, "polygon"), _SCS("set_polygon"), _SCS("get_polygon"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2_ARRAY, "uv"), _SCS("set_uv"), _SCS("get_uv"));
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), _SCS("set_color"), _SCS("get_color"));
	ADD_PROPERTY(PropertyInfo(Variant::COLOR_ARRAY, "vertex_colors"), _SCS("set_vertex_colors"), _SCS("get_vertex_colors"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), _SCS("set_offset"), _SCS("get_offset"));
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture/texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), _SCS("set_texture"), _SCS("get_texture"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "texture/offset"), _SCS("set_texture_offset"), _SCS("get_texture_offset"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "texture/scale"), _SCS("set_texture_scale"), _SCS("get_texture_scale"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "texture/rotation", PROPERTY_HINT_RANGE, "-1440,1440,0.1"), _SCS("_set_texture_rotationd"), _SCS("_get_texture_rotationd"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "invert/enable"), _SCS("set_invert"), _SCS("get_invert"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "invert/border", PROPERTY_HINT_RANGE, "0.1,16384,0.1"), _SCS("set_invert_border"), _SCS("get_invert_border"));
}

Polygon2D::Polygon2D() {

	invert = 0;
	invert_border = 100;
	tex_rot = 0;
	tex_tile = true;
	tex_scale = Vector2(1, 1);
	color = Color(1, 1, 1);
	rect_cache_dirty = true;
}
