/*************************************************************************/
/*  polygon_2d.h                                                         */
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
#ifndef POLYGON_2D_H
#define POLYGON_2D_H

#include "scene/2d/node_2d.h"

class Polygon2D : public Node2D {

	OBJ_TYPE(Polygon2D, Node2D);

	DVector<Vector2> polygon;
	DVector<Vector2> uv;
	DVector<Color> vertex_colors;
	Color color;
	Ref<Texture> texture;
	Size2 tex_scale;
	Vector2 tex_ofs;
	bool tex_tile;
	float tex_rot;
	bool invert;
	float invert_border;

	Vector2 offset;
	mutable bool rect_cache_dirty;
	mutable Rect2 item_rect;

	void _set_texture_rotationd(float p_rot);
	float _get_texture_rotationd() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_polygon(const DVector<Vector2> &p_polygon);
	DVector<Vector2> get_polygon() const;

	void set_uv(const DVector<Vector2> &p_uv);
	DVector<Vector2> get_uv() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_vertex_colors(const DVector<Color> &p_colors);
	DVector<Color> get_vertex_colors() const;

	void set_texture(const Ref<Texture> &p_texture);
	Ref<Texture> get_texture() const;

	void set_texture_offset(const Vector2 &p_offset);
	Vector2 get_texture_offset() const;

	void set_texture_rotation(float p_rot);
	float get_texture_rotation() const;

	void set_texture_scale(const Size2 &p_scale);
	Size2 get_texture_scale() const;

	void set_invert(bool p_rot);
	bool get_invert() const;

	void set_invert_border(float p_border);
	float get_invert_border() const;

	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	//editor stuff

	virtual void edit_set_pivot(const Point2 &p_pivot);
	virtual Point2 edit_get_pivot() const;
	virtual bool edit_has_pivot() const;

	virtual Rect2 get_item_rect() const;

	Polygon2D();
};

#endif // POLYGON_2D_H
