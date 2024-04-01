/*************************************************************************/
/*  editor_help.h                                                        */
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
#ifndef EDITOR_HELP_H
#define EDITOR_HELP_H

#include "editor/editor_plugin.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"

#include "editor/code_editor.h"
#include "editor/doc/doc_data.h"
#include "scene/main/timer.h"

class EditorNode;

class EditorHelpSearch : public ConfirmationDialog {

	OBJ_TYPE(EditorHelpSearch, ConfirmationDialog)

	EditorNode *editor;
	LineEdit *search_box;
	Tree *search_options;
	String base_type;

	void _update_search();

	void _sbox_input(const InputEvent &p_ie);

	void _confirmed();
	void _text_changed(const String &p_newtext);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void popup();
	void popup(const String &p_term);

	EditorHelpSearch();
};

class EditorHelpIndex : public ConfirmationDialog {
	OBJ_TYPE(EditorHelpIndex, ConfirmationDialog);

	LineEdit *search_box;
	Tree *class_list;
	HashMap<String, TreeItem *> tree_item_map;

	void _tree_item_selected();
	void _text_changed(const String &p_text);
	void _sbox_input(const InputEvent &p_ie);

	void _update_class_list();

	void add_type(const String &p_type, HashMap<String, TreeItem *> &p_types, TreeItem *p_root);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void select_class(const String &p_class);

	void popup();

	EditorHelpIndex();
};

class EditorHelp : public VBoxContainer {
	OBJ_TYPE(EditorHelp, VBoxContainer);

	enum Page {

		PAGE_CLASS_LIST,
		PAGE_CLASS_DESC,
		PAGE_CLASS_PREV,
		PAGE_CLASS_NEXT,
		PAGE_SEARCH,
		CLASS_SEARCH,

	};

	bool select_locked;

	String prev_search;

	String edited_class;

	EditorNode *editor;
	Map<String, int> method_line;
	Map<String, int> signal_line;
	Map<String, int> property_line;
	Map<String, int> theme_property_line;
	Map<String, int> constant_line;
	int description_line;

	RichTextLabel *class_desc;
	HSplitContainer *h_split;
	static DocData *doc;

	ConfirmationDialog *search_dialog;
	LineEdit *search;

	String base_path;

	void _help_callback(const String &p_topic);

	void _add_text(const String &p_text);
	bool scroll_locked;

	//void _button_pressed(int p_idx);
	void _add_type(const String &p_type);

	void _scroll_changed(double p_scroll);
	void _class_list_select(const String &p_select);
	void _class_desc_select(const String &p_select);
	void _class_desc_input(const InputEvent &p_input);

	Error _goto_desc(const String &p_class, int p_vscr = -1);
	//void _update_history_buttons();
	void _update_doc();

	void _request_help(const String &p_string);
	void _search(const String &p_str);
	void _search_cbk();

	void _unhandled_key_input(const InputEvent &p_ev);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static void generate_doc();
	static DocData *get_doc_data() { return doc; }

	void go_to_help(const String &p_help);
	void go_to_class(const String &p_class, int p_scroll = 0);

	void popup_search();
	void search_again();

	String get_class_name();

	void set_focused() { class_desc->grab_focus(); }

	int get_scroll() const;
	void set_scroll(int p_scroll);

	EditorHelp();
	~EditorHelp();
};

class EditorHelpBit : public Panel {

	OBJ_TYPE(EditorHelpBit, Panel);

	RichTextLabel *rich_text;
	void _go_to_help(String p_what);
	void _meta_clicked(String p_what);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_text(const String &p_text);
	EditorHelpBit();
};

#endif // EDITOR_HELP_H
