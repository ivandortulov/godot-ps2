/*************************************************************************/
/*  shader_editor_plugin.h                                               */
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
#ifndef SHADER_EDITOR_PLUGIN_H
#define SHADER_EDITOR_PLUGIN_H

#include "editor/code_editor.h"
#include "editor/editor_plugin.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/main/timer.h"
#include "scene/resources/shader.h"
#include "servers/visual/shader_language.h"

class ShaderTextEditor : public CodeTextEditor {

	OBJ_TYPE(ShaderTextEditor, CodeTextEditor);

	Ref<Shader> shader;
	ShaderLanguage::ShaderType type;

protected:
	static void _bind_methods();
	virtual void _load_theme_settings();

public:
	virtual void _validate_script();

	Ref<Shader> get_edited_shader() const;
	void set_edited_shader(const Ref<Shader> &p_shader, ShaderLanguage::ShaderType p_type);
	ShaderTextEditor();
};

class ShaderEditor : public Control {

	OBJ_TYPE(ShaderEditor, Control);

	enum {

		EDIT_UNDO,
		EDIT_REDO,
		EDIT_CUT,
		EDIT_COPY,
		EDIT_PASTE,
		EDIT_SELECT_ALL,
		SEARCH_FIND,
		SEARCH_FIND_NEXT,
		SEARCH_FIND_PREV,
		SEARCH_REPLACE,
		//SEARCH_LOCATE_SYMBOL,
		SEARCH_GOTO_LINE,

	};

	MenuButton *edit_menu;
	MenuButton *search_menu;
	MenuButton *settings_menu;
	uint64_t idle;

	TabContainer *tab_container;
	GotoLineDialog *goto_line_dialog;
	ConfirmationDialog *erase_tab_confirm;

	TextureButton *close;

	ShaderTextEditor *vertex_editor;
	ShaderTextEditor *fragment_editor;
	ShaderTextEditor *light_editor;

	void _tab_changed(int p_which);
	void _menu_option(int p_optin);
	void _params_changed();
	mutable Ref<Shader> shader;

	void _close_callback();

	void _editor_settings_changed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void apply_shaders();

	void ensure_select_current();
	void edit(const Ref<Shader> &p_shader);

	Dictionary get_state() const;
	void set_state(const Dictionary &p_state);
	void clear();

	virtual Size2 get_minimum_size() const { return Size2(0, 200); }
	void save_external_data();

	ShaderEditor();
};

class ShaderEditorPlugin : public EditorPlugin {

	OBJ_TYPE(ShaderEditorPlugin, EditorPlugin);

	bool _2d;
	ShaderEditor *shader_editor;
	EditorNode *editor;

public:
	virtual String get_name() const { return "Shader"; }
	bool has_main_screen() const { return false; }
	virtual void edit(Object *p_node);
	virtual bool handles(Object *p_node) const;
	virtual void make_visible(bool p_visible);
	virtual void selected_notify();

	Dictionary get_state() const;
	virtual void set_state(const Dictionary &p_state);
	virtual void clear();

	virtual void save_external_data();
	virtual void apply_changes();

	ShaderEditorPlugin(EditorNode *p_node, bool p_2d);
	~ShaderEditorPlugin();
};
#endif
