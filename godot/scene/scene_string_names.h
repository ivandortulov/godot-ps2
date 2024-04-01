/*************************************************************************/
/*  scene_string_names.h                                                 */
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
#ifndef SCENE_STRING_NAMES_H
#define SCENE_STRING_NAMES_H

#include "path_db.h"
#include "string_db.h"
class SceneStringNames {

	friend void register_scene_types();
	friend void unregister_scene_types();

	static SceneStringNames *singleton;

	static void create() { singleton = memnew(SceneStringNames); }
	static void free() {
		memdelete(singleton);
		singleton = NULL;
	}

	SceneStringNames();

public:
	_FORCE_INLINE_ static SceneStringNames *get_singleton() { return singleton; }

	StringName _estimate_cost;
	StringName _compute_cost;

	StringName resized;
	StringName dot;
	StringName doubledot;
	StringName draw;
	StringName hide;
	StringName visibility_changed;
	StringName input_event;
	StringName _input_event;
	StringName item_rect_changed;
	StringName shader_shader;
	StringName shader_unshaded;
	StringName shading_mode;
	StringName enter_tree;
	StringName exit_tree;
	StringName size_flags_changed;
	StringName minimum_size_changed;
	StringName sleeping_state_changed;
	StringName idle;
	StringName iteration;
	StringName update;
	StringName updated;

	StringName line_separation;

	StringName mouse_enter;
	StringName mouse_exit;
	StringName focus_enter;
	StringName focus_exit;

	StringName sort_children;

	StringName finished;
	StringName animation_changed;
	StringName animation_started;

	StringName body_enter_shape;
	StringName body_enter;
	StringName body_exit_shape;
	StringName body_exit;

	StringName area_enter_shape;
	StringName area_exit_shape;

	StringName _body_inout;
	StringName _area_inout;

	StringName _get_gizmo_geometry;
	StringName _can_gizmo_scale;

	StringName _fixed_process;
	StringName _process;
	StringName _enter_world;
	StringName _exit_world;
	StringName _enter_tree;
	StringName _exit_tree;
	StringName _draw;
	StringName _input;
	StringName _ready;

	StringName _pressed;
	StringName _toggled;

	StringName _update_scroll;
	StringName _update_xform;

	StringName _proxgroup_add;
	StringName _proxgroup_remove;

	StringName grouped;
	StringName ungrouped;

	StringName has_point;
	StringName get_drag_data;
	StringName can_drop_data;
	StringName drop_data;

	StringName enter_screen;
	StringName exit_screen;
	StringName enter_viewport;
	StringName exit_viewport;
	StringName enter_camera;
	StringName exit_camera;

	StringName _body_enter_tree;
	StringName _body_exit_tree;

	StringName _area_enter_tree;
	StringName _area_exit_tree;

	StringName changed;
	StringName _shader_changed;

	StringName _spatial_editor_group;
	StringName _request_gizmo;

	StringName offset;
	StringName unit_offset;
	StringName rotation_mode;
	StringName rotate;
	StringName v_offset;
	StringName h_offset;

	StringName transform_pos;
	StringName transform_rot;
	StringName transform_scale;

	StringName _update_remote;
	StringName _update_pairs;

	StringName area_enter;
	StringName area_exit;

	StringName get_minimum_size;

	StringName play_play;

	StringName _im_update;
	StringName _queue_update;

	StringName baked_light_changed;
	StringName _baked_light_changed;

	StringName _mouse_enter;
	StringName _mouse_exit;

	StringName frame_changed;

	StringName playback_speed;
	StringName playback_active;
	StringName autoplay;
	StringName blend_times;
	StringName speed;

	NodePath path_pp;

	StringName _default;

	StringName node_configuration_warning_changed;

	enum {
		MAX_MATERIALS = 32
	};
	StringName mesh_materials[MAX_MATERIALS];
	StringName _mesh_changed;
};

#endif // SCENE_STRING_NAMES_H
