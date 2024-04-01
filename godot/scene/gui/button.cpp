/*************************************************************************/
/*  button.cpp                                                           */
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
#include "button.h"
#include "print_string.h"
#include "servers/visual_server.h"
#include "translation.h"

Size2 Button::get_minimum_size() const {

	Size2 minsize = get_font("font")->get_string_size(text);
	if (clip_text)
		minsize.width = 0;

	Ref<Texture> _icon;
	if (icon.is_null() && has_icon("icon"))
		_icon = Control::get_icon("icon");
	else
		_icon = icon;

	if (!_icon.is_null()) {

		minsize.height = MAX(minsize.height, _icon->get_height());
		minsize.width += _icon->get_width();
		if (text != "")
			minsize.width += get_constant("hseparation");
	}

	return get_stylebox("normal")->get_minimum_size() + minsize;
}

void Button::_notification(int p_what) {

	if (p_what == NOTIFICATION_DRAW) {

		RID ci = get_canvas_item();
		Size2 size = get_size();
		Color color;

		//print_line(get_text()+": "+itos(is_flat())+" hover "+itos(get_draw_mode()));

		switch (get_draw_mode()) {

			case DRAW_NORMAL: {

				if (!flat)
					get_stylebox("normal")->draw(ci, Rect2(Point2(0, 0), size));
				color = get_color("font_color");
			} break;
			case DRAW_PRESSED: {

				get_stylebox("pressed")->draw(ci, Rect2(Point2(0, 0), size));
				if (has_color("font_color_pressed"))
					color = get_color("font_color_pressed");
				else
					color = get_color("font_color");

			} break;
			case DRAW_HOVER: {

				get_stylebox("hover")->draw(ci, Rect2(Point2(0, 0), size));
				color = get_color("font_color_hover");

			} break;
			case DRAW_DISABLED: {

				get_stylebox("disabled")->draw(ci, Rect2(Point2(0, 0), size));
				color = get_color("font_color_disabled");

			} break;
		}

		if (has_focus()) {

			Ref<StyleBox> style = get_stylebox("focus");
			style->draw(ci, Rect2(Point2(), size));
		}

		Ref<StyleBox> style = get_stylebox("normal");
		Ref<Font> font = get_font("font");
		Ref<Texture> _icon;
		if (icon.is_null() && has_icon("icon"))
			_icon = Control::get_icon("icon");
		else
			_icon = icon;

		Point2 icon_ofs = (!_icon.is_null()) ? Point2(_icon->get_width() + get_constant("hseparation"), 0) : Point2();
		int text_clip = size.width - style->get_minimum_size().width - icon_ofs.width;
		Point2 text_ofs = (size - style->get_minimum_size() - icon_ofs - font->get_string_size(text)) / 2.0;

		switch (align) {
			case ALIGN_LEFT: {
				text_ofs.x = style->get_margin(MARGIN_LEFT) + icon_ofs.x;
				text_ofs.y += style->get_offset().y;
			} break;
			case ALIGN_CENTER: {
				if (text_ofs.x < 0)
					text_ofs.x = 0;
				text_ofs += icon_ofs;
				text_ofs += style->get_offset();
			} break;
			case ALIGN_RIGHT: {
				text_ofs.x = size.x - style->get_margin(MARGIN_RIGHT) - font->get_string_size(text).x;
				text_ofs.y += style->get_offset().y;
			} break;
		}

		text_ofs.y += font->get_ascent();
		font->draw(ci, text_ofs.floor(), text, color, clip_text ? text_clip : -1);
		if (!_icon.is_null()) {

			int valign = size.height - style->get_minimum_size().y;

			_icon->draw(ci, style->get_offset() + Point2(0, Math::floor((valign - _icon->get_height()) / 2.0)), is_disabled() ? Color(1, 1, 1, 0.4) : Color(1, 1, 1));
		}
	}
}

void Button::set_text(const String &p_text) {

	if (text == p_text)
		return;
	text = XL_MESSAGE(p_text);
	update();
	_change_notify("text");
	minimum_size_changed();
}
String Button::get_text() const {

	return text;
}

void Button::set_icon(const Ref<Texture> &p_icon) {

	if (icon == p_icon)
		return;
	icon = p_icon;
	update();
	_change_notify("icon");
	minimum_size_changed();
}

Ref<Texture> Button::get_icon() const {

	return icon;
}

void Button::set_flat(bool p_flat) {

	flat = p_flat;
	update();
	_change_notify("flat");
}

bool Button::is_flat() const {

	return flat;
}

void Button::set_clip_text(bool p_clip_text) {

	clip_text = p_clip_text;
	update();
	minimum_size_changed();
}

bool Button::get_clip_text() const {

	return clip_text;
}

void Button::set_text_align(TextAlign p_align) {

	align = p_align;
	update();
}

Button::TextAlign Button::get_text_align() const {

	return align;
}

void Button::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_text", "text"), &Button::set_text);
	ObjectTypeDB::bind_method(_MD("get_text"), &Button::get_text);
	ObjectTypeDB::bind_method(_MD("set_button_icon", "texture:Texture"), &Button::set_icon);
	ObjectTypeDB::bind_method(_MD("get_button_icon:Texture"), &Button::get_icon);
	ObjectTypeDB::bind_method(_MD("set_flat", "enabled"), &Button::set_flat);
	ObjectTypeDB::bind_method(_MD("set_clip_text", "enabled"), &Button::set_clip_text);
	ObjectTypeDB::bind_method(_MD("get_clip_text"), &Button::get_clip_text);
	ObjectTypeDB::bind_method(_MD("set_text_align", "align"), &Button::set_text_align);
	ObjectTypeDB::bind_method(_MD("get_text_align"), &Button::get_text_align);
	ObjectTypeDB::bind_method(_MD("is_flat"), &Button::is_flat);

	BIND_CONSTANT(ALIGN_LEFT);
	BIND_CONSTANT(ALIGN_CENTER);
	BIND_CONSTANT(ALIGN_RIGHT);

	ADD_PROPERTYNZ(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT_INTL), _SCS("set_text"), _SCS("get_text"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), _SCS("set_button_icon"), _SCS("get_button_icon"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat"), _SCS("set_flat"), _SCS("is_flat"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "clip_text"), _SCS("set_clip_text"), _SCS("get_clip_text"));
	ADD_PROPERTYNO(PropertyInfo(Variant::INT, "align", PROPERTY_HINT_ENUM, "Left,Center,Right"), _SCS("set_text_align"), _SCS("get_text_align"));
}

Button::Button(const String &p_text) {

	flat = false;
	clip_text = false;
	set_stop_mouse(true);
	set_text(p_text);
	align = ALIGN_CENTER;
}

Button::~Button() {
}
