/*************************************************************************/
/*  color.h                                                              */
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
#ifndef COLOR_H
#define COLOR_H

#include "math_funcs.h"
#include "ustring.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
struct Color {

	union {

		struct {
			float r;
			float g;
			float b;
			float a;
		};
		float components[4];
	};

	bool operator==(const Color &p_color) const { return (r == p_color.r && g == p_color.g && b == p_color.b && a == p_color.a); }
	bool operator!=(const Color &p_color) const { return (r != p_color.r || g != p_color.g || b != p_color.b || a != p_color.a); }

	uint32_t to_32() const;
	uint32_t to_ARGB32() const;
	float gray() const;
	float get_h() const;
	float get_s() const;
	float get_v() const;
	void set_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0);

	_FORCE_INLINE_ float &operator[](int idx) {
		return components[idx];
	}
	_FORCE_INLINE_ const float &operator[](int idx) const {
		return components[idx];
	}

	void invert();
	void contrast();
	Color inverted() const;
	Color contrasted() const;

	_FORCE_INLINE_ Color linear_interpolate(const Color &p_b, float p_t) const {

		Color res = *this;

		res.r += (p_t * (p_b.r - r));
		res.g += (p_t * (p_b.g - g));
		res.b += (p_t * (p_b.b - b));
		res.a += (p_t * (p_b.a - a));

		return res;
	}

	_FORCE_INLINE_ Color blend(const Color &p_over) const {

		Color res;
		float sa = 1.0 - p_over.a;
		res.a = a * sa + p_over.a;
		if (res.a == 0) {
			return Color(0, 0, 0, 0);
		} else {
			res.r = (r * a * sa + p_over.r * p_over.a) / res.a;
			res.g = (g * a * sa + p_over.g * p_over.a) / res.a;
			res.b = (b * a * sa + p_over.b * p_over.a) / res.a;
		}
		return res;
	}

	_FORCE_INLINE_ Color to_linear() const {

		return Color(
				r < 0.04045 ? r * (1.0 / 12.92) : Math::pow((r + 0.055) * (1.0 / (1 + 0.055)), 2.4),
				g < 0.04045 ? g * (1.0 / 12.92) : Math::pow((g + 0.055) * (1.0 / (1 + 0.055)), 2.4),
				b < 0.04045 ? b * (1.0 / 12.92) : Math::pow((b + 0.055) * (1.0 / (1 + 0.055)), 2.4),
				a);
	}

	static Color hex(uint32_t p_hex);
	static Color html(const String &p_color);
	static bool html_is_valid(const String &p_color);
	static Color named(const String &p_name);
	String to_html(bool p_alpha = true) const;

	_FORCE_INLINE_ bool operator<(const Color &p_color) const; //used in set keys
	operator String() const;

	/**
	 * No construct parameters, r=0, g=0, b=0. a=255
	 */
	_FORCE_INLINE_ Color() {
		r = 0;
		g = 0;
		b = 0;
		a = 1.0;
	}

	/**
	 * RGB / RGBA construct parameters. Alpha is optional, but defaults to 1.0
	 */
	_FORCE_INLINE_ Color(float p_r, float p_g, float p_b, float p_a = 1.0) {
		r = p_r;
		g = p_g;
		b = p_b;
		a = p_a;
	}

private:
	friend void unregister_core_types();
	static void cleanup();
};

bool Color::operator<(const Color &p_color) const {

	if (r == p_color.r) {
		if (g == p_color.g) {
			if (b == p_color.b) {
				return (a < p_color.a);
			} else
				return (b < p_color.b);
		} else
			return g < p_color.g;
	} else
		return r < p_color.r;
}

#endif
