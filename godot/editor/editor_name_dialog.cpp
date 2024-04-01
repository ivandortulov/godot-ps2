/*************************************************************************/
/*  editor_name_dialog.cpp                                               */
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

#include "editor_name_dialog.h"
#include "object_type_db.h"
#include "os/keyboard.h"

void EditorNameDialog::_line_input_event(const InputEvent &p_event) {

	if (p_event.type == InputEvent::KEY) {

		if (!p_event.key.pressed)
			return;

		switch (p_event.key.scancode) {
			case KEY_ENTER:
			case KEY_RETURN: {

				if (get_hide_on_ok())
					hide();
				ok_pressed();
				accept_event();
			} break;
			case KEY_ESCAPE: {

				hide();
				accept_event();
			} break;
		}
	}
}

void EditorNameDialog::_post_popup() {

	ConfirmationDialog::_post_popup();
	name->clear();
	name->grab_focus();
}

void EditorNameDialog::ok_pressed() {

	if (name->get_text() != "") {
		emit_signal("name_confirmed", name->get_text());
	}
}

void EditorNameDialog::_bind_methods() {

	ObjectTypeDB::bind_method("_line_input_event", &EditorNameDialog::_line_input_event);

	ADD_SIGNAL(MethodInfo("name_confirmed", PropertyInfo(Variant::STRING, "name")));
}

EditorNameDialog::EditorNameDialog() {
	makevb = memnew(VBoxContainer);
	makevb->set_margin(MARGIN_TOP, 5);
	makevb->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 5);
	makevb->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 5);
	add_child(makevb);

	name = memnew(LineEdit);
	makevb->add_child(name);
	name->connect("input_event", this, "_line_input_event");
}
