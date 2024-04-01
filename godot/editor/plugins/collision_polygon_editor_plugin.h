/*************************************************************************/
/*  collision_polygon_editor_plugin.h                                    */
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
#ifndef COLLISION_POLYGON_EDITOR_PLUGIN_H
#define COLLISION_POLYGON_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "scene/3d/collision_polygon.h"
#include "scene/3d/immediate_geometry.h"
#include "scene/3d/mesh_instance.h"
#include "scene/gui/button_group.h"
#include "scene/gui/tool_button.h"

/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class CanvasItemEditor;

class CollisionPolygonEditor : public HBoxContainer {

	OBJ_TYPE(CollisionPolygonEditor, HBoxContainer);

	UndoRedo *undo_redo;
	enum Mode {

		MODE_CREATE,
		MODE_EDIT,

	};

	Mode mode;

	ToolButton *button_create;
	ToolButton *button_edit;

	Ref<FixedMaterial> line_material;
	Ref<FixedMaterial> handle_material;

	EditorNode *editor;
	Panel *panel;
	CollisionPolygon *node;
	ImmediateGeometry *imgeom;
	MeshInstance *pointsm;
	Ref<Mesh> m;

	MenuButton *options;

	int edited_point;
	Vector2 edited_point_pos;
	Vector<Vector2> pre_move_edit;
	Vector<Vector2> wip;
	bool wip_active;

	float prev_depth;

	void _wip_close();
	void _polygon_draw();
	void _menu_option(int p_option);

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);
	static void _bind_methods();

public:
	virtual bool forward_spatial_input_event(Camera *p_camera, const InputEvent &p_event);
	void edit(Node *p_collision_polygon);
	CollisionPolygonEditor(EditorNode *p_editor);
	~CollisionPolygonEditor();
};

class CollisionPolygonEditorPlugin : public EditorPlugin {

	OBJ_TYPE(CollisionPolygonEditorPlugin, EditorPlugin);

	CollisionPolygonEditor *collision_polygon_editor;
	EditorNode *editor;

public:
	virtual bool forward_spatial_input_event(Camera *p_camera, const InputEvent &p_event) { return collision_polygon_editor->forward_spatial_input_event(p_camera, p_event); }

	virtual String get_name() const { return "CollisionPolygon"; }
	bool has_main_screen() const { return false; }
	virtual void edit(Object *p_node);
	virtual bool handles(Object *p_node) const;
	virtual void make_visible(bool p_visible);

	CollisionPolygonEditorPlugin(EditorNode *p_node);
	~CollisionPolygonEditorPlugin();
};

#endif // COLLISION_POLYGON_EDITOR_PLUGIN_H
