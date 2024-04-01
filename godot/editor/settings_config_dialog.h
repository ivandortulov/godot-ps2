/*************************************************************************/
/*  settings_config_dialog.h                                             */
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
#ifndef SETTINGS_CONFIG_DIALOG_H
#define SETTINGS_CONFIG_DIALOG_H

#include "property_editor.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/tab_container.h"

class EditorSettingsDialog : public AcceptDialog {

	OBJ_TYPE(EditorSettingsDialog, AcceptDialog);

	bool updating;

	TabContainer *tabs;

	LineEdit *search_box;
	LineEdit *shortcut_search_box;
	ToolButton *clear_button;
	ToolButton *shortcut_clear_button;
	SectionedPropertyEditor *property_editor;

	Timer *timer;

	Tree *shortcuts;

	ConfirmationDialog *press_a_key;
	Label *press_a_key_label;
	InputEvent last_wait_for_key;
	String shortcut_configured;
	String shortcut_filter;

	virtual void cancel_pressed();
	virtual void ok_pressed();

	void _settings_changed();
	void _settings_property_edited(const String &p_name);
	void _settings_save();

	void _notification(int p_what);

	void _press_a_key_confirm();
	void _wait_for_key(const InputEvent &p_event);

	void _clear_shortcut_search_box();
	void _clear_search_box();

	void _filter_shortcuts(const String &p_filter);

	void _update_shortcuts();
	void _shortcut_button_pressed(Object *p_item, int p_column, int p_idx);

protected:
	static void _bind_methods();

public:
	void popup_edit_settings();

	EditorSettingsDialog();
};

#endif // SETTINGS_CONFIG_DIALOG_H
