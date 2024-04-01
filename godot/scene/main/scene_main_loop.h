/*************************************************************************/
/*  scene_main_loop.h                                                    */
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
#ifndef SCENE_MAIN_LOOP_H
#define SCENE_MAIN_LOOP_H

#include "os/main_loop.h"
#include "os/thread_safe.h"
#include "scene/resources/world.h"
#include "scene/resources/world_2d.h"
#include "self_list.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/

class SceneTree;
class PackedScene;
class Node;
class Viewport;
class Material;
class Mesh;

class SceneTree : public MainLoop {

	_THREAD_SAFE_CLASS_

	OBJ_TYPE(SceneTree, MainLoop);

public:
	enum StretchMode {

		STRETCH_MODE_DISABLED,
		STRETCH_MODE_2D,
		STRETCH_MODE_VIEWPORT,
	};

	enum StretchAspect {

		STRETCH_ASPECT_IGNORE,
		STRETCH_ASPECT_KEEP,
		STRETCH_ASPECT_KEEP_WIDTH,
		STRETCH_ASPECT_KEEP_HEIGHT,
		STRETCH_ASPECT_EXPAND,
	};

private:
	struct Group {

		Vector<Node *> nodes;
		//uint64_t last_tree_version;
		bool changed;
		Group() { changed = false; };
	};

	Viewport *root;

	uint64_t tree_version;
	float fixed_process_time;
	float idle_process_time;
	bool accept_quit;
	uint32_t last_id;

#ifdef TOOLS_ENABLED
	bool editor_hint;
#endif
#ifdef DEBUG_ENABLED
	bool debug_collisions_hint;
	bool debug_navigation_hint;
#endif
	bool pause;
	int root_lock;

	Map<StringName, Group> group_map;
	bool _quit;
	bool initialized;
	bool input_handled;
	Size2 last_screen_size;
	StringName tree_changed_name;
	StringName node_removed_name;

	int64_t current_frame;
	int node_count;

#ifdef TOOLS_ENABLED
	Node *edited_scene_root;
#endif
	struct UGCall {

		StringName group;
		StringName call;

		bool operator<(const UGCall &p_with) const { return group == p_with.group ? call < p_with.call : group < p_with.group; }
	};

	//safety for when a node is deleted while a group is being called
	int call_lock;
	Set<Node *> call_skip; //skip erased nodes

	StretchMode stretch_mode;
	StretchAspect stretch_aspect;
	Size2i stretch_min;

	void _update_root_rect();

	List<ObjectID> delete_queue;

	Map<UGCall, Vector<Variant> > unique_group_calls;
	bool ugc_locked;
	void _flush_ugc();
	void _flush_transform_notifications();

	_FORCE_INLINE_ void _update_group_order(Group &g);
	void _update_listener();

	Array _get_nodes_in_group(const StringName &p_group);

	Node *current_scene;

	Color debug_collisions_color;
	Color debug_collision_contact_color;
	Color debug_navigation_color;
	Color debug_navigation_disabled_color;
	Ref<Mesh> debug_contact_mesh;
	Ref<Material> navigation_material;
	Ref<Material> navigation_disabled_material;
	Ref<Material> collision_material;
	int collision_debug_contacts;

	void _change_scene(Node *p_to);
	//void _call_group(uint32_t p_call_flags,const StringName& p_group,const StringName& p_function,const Variant& p_arg1,const Variant& p_arg2);

	static SceneTree *singleton;
	friend class Node;

	void tree_changed();
	void node_removed(Node *p_node);

	Group *add_to_group(const StringName &p_group, Node *p_node);
	void remove_from_group(const StringName &p_group, Node *p_node);

	void _notify_group_pause(const StringName &p_group, int p_notification);
	void _call_input_pause(const StringName &p_group, const StringName &p_method, const InputEvent &p_input);
	Variant _call_group(const Variant **p_args, int p_argcount, Variant::CallError &r_error);

	static void _debugger_request_tree(void *self);
	void _flush_delete_queue();
	//optimization
	friend class CanvasItem;
	friend class Spatial;
	friend class Viewport;

	SelfList<Node>::List xform_change_list;

#ifdef DEBUG_ENABLED

	Map<int, NodePath> live_edit_node_path_cache;
	Map<int, String> live_edit_resource_cache;

	NodePath live_edit_root;
	String live_edit_scene;

	Map<String, Set<Node *> > live_scene_edit_cache;
	Map<Node *, Map<ObjectID, Node *> > live_edit_remove_list;

	ScriptDebugger::LiveEditFuncs live_edit_funcs;

	void _live_edit_node_path_func(const NodePath &p_path, int p_id);
	void _live_edit_res_path_func(const String &p_path, int p_id);

	void _live_edit_node_set_func(int p_id, const StringName &p_prop, const Variant &p_value);
	void _live_edit_node_set_res_func(int p_id, const StringName &p_prop, const String &p_value);
	void _live_edit_node_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE);
	void _live_edit_res_set_func(int p_id, const StringName &p_prop, const Variant &p_value);
	void _live_edit_res_set_res_func(int p_id, const StringName &p_prop, const String &p_value);
	void _live_edit_res_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE);
	void _live_edit_root_func(const NodePath &p_scene_path, const String &p_scene_from);

	void _live_edit_create_node_func(const NodePath &p_parent, const String &p_type, const String &p_name);
	void _live_edit_instance_node_func(const NodePath &p_parent, const String &p_path, const String &p_name);
	void _live_edit_remove_node_func(const NodePath &p_at);
	void _live_edit_remove_and_keep_node_func(const NodePath &p_at, ObjectID p_keep_id);
	void _live_edit_restore_node_func(ObjectID p_id, const NodePath &p_at, int p_at_pos);
	void _live_edit_duplicate_node_func(const NodePath &p_at, const String &p_new_name);
	void _live_edit_reparent_node_func(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos);

	static void _live_edit_node_path_funcs(void *self, const NodePath &p_path, int p_id) { reinterpret_cast<SceneTree *>(self)->_live_edit_node_path_func(p_path, p_id); }
	static void _live_edit_res_path_funcs(void *self, const String &p_path, int p_id) { reinterpret_cast<SceneTree *>(self)->_live_edit_res_path_func(p_path, p_id); }

	static void _live_edit_node_set_funcs(void *self, int p_id, const StringName &p_prop, const Variant &p_value) { reinterpret_cast<SceneTree *>(self)->_live_edit_node_set_func(p_id, p_prop, p_value); }
	static void _live_edit_node_set_res_funcs(void *self, int p_id, const StringName &p_prop, const String &p_value) { reinterpret_cast<SceneTree *>(self)->_live_edit_node_set_res_func(p_id, p_prop, p_value); }
	static void _live_edit_node_call_funcs(void *self, int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) { reinterpret_cast<SceneTree *>(self)->_live_edit_node_call_func(p_id, p_method, VARIANT_ARG_PASS); }
	static void _live_edit_res_set_funcs(void *self, int p_id, const StringName &p_prop, const Variant &p_value) { reinterpret_cast<SceneTree *>(self)->_live_edit_res_set_func(p_id, p_prop, p_value); }
	static void _live_edit_res_set_res_funcs(void *self, int p_id, const StringName &p_prop, const String &p_value) { reinterpret_cast<SceneTree *>(self)->_live_edit_res_set_res_func(p_id, p_prop, p_value); }
	static void _live_edit_res_call_funcs(void *self, int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) { reinterpret_cast<SceneTree *>(self)->_live_edit_res_call_func(p_id, p_method, VARIANT_ARG_PASS); }
	static void _live_edit_root_funcs(void *self, const NodePath &p_scene_path, const String &p_scene_from) { reinterpret_cast<SceneTree *>(self)->_live_edit_root_func(p_scene_path, p_scene_from); }

	static void _live_edit_create_node_funcs(void *self, const NodePath &p_parent, const String &p_type, const String &p_name) { reinterpret_cast<SceneTree *>(self)->_live_edit_create_node_func(p_parent, p_type, p_name); }
	static void _live_edit_instance_node_funcs(void *self, const NodePath &p_parent, const String &p_path, const String &p_name) { reinterpret_cast<SceneTree *>(self)->_live_edit_instance_node_func(p_parent, p_path, p_name); }
	static void _live_edit_remove_node_funcs(void *self, const NodePath &p_at) { reinterpret_cast<SceneTree *>(self)->_live_edit_remove_node_func(p_at); }
	static void _live_edit_remove_and_keep_node_funcs(void *self, const NodePath &p_at, ObjectID p_keep_id) { reinterpret_cast<SceneTree *>(self)->_live_edit_remove_and_keep_node_func(p_at, p_keep_id); }
	static void _live_edit_restore_node_funcs(void *self, ObjectID p_id, const NodePath &p_at, int p_at_pos) { reinterpret_cast<SceneTree *>(self)->_live_edit_restore_node_func(p_id, p_at, p_at_pos); }
	static void _live_edit_duplicate_node_funcs(void *self, const NodePath &p_at, const String &p_new_name) { reinterpret_cast<SceneTree *>(self)->_live_edit_duplicate_node_func(p_at, p_new_name); }
	static void _live_edit_reparent_node_funcs(void *self, const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) { reinterpret_cast<SceneTree *>(self)->_live_edit_reparent_node_func(p_at, p_new_place, p_new_name, p_at_pos); }

#endif
protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	enum {
		NOTIFICATION_TRANSFORM_CHANGED = 29
	};

	enum CallGroupFlags {
		GROUP_CALL_DEFAULT = 0,
		GROUP_CALL_REVERSE = 1,
		GROUP_CALL_REALTIME = 2,
		GROUP_CALL_UNIQUE = 4,
		GROUP_CALL_MULIILEVEL = 8,
	};

	_FORCE_INLINE_ Viewport *get_root() const { return root; }

	uint32_t get_last_event_id() const;

	void call_group(uint32_t p_call_flags, const StringName &p_group, const StringName &p_function, VARIANT_ARG_LIST);
	void notify_group(uint32_t p_call_flags, const StringName &p_group, int p_notification);
	void set_group(uint32_t p_call_flags, const StringName &p_group, const String &p_name, const Variant &p_value);

	virtual void input_text(const String &p_text);
	virtual void input_event(const InputEvent &p_event);
	virtual void init();

	virtual bool iteration(float p_time);
	virtual bool idle(float p_time);

	virtual void finish();

	void set_auto_accept_quit(bool p_enable);

	void quit();

	void set_input_as_handled();
	bool is_input_handled();
	_FORCE_INLINE_ float get_fixed_process_time() const { return fixed_process_time; }
	_FORCE_INLINE_ float get_idle_process_time() const { return idle_process_time; }

#ifdef TOOLS_ENABLED
	void set_editor_hint(bool p_enabled);

	bool is_editor_hint() const;
	bool is_node_being_edited(const Node *p_node) const;
#else
	void set_editor_hint(bool p_enabled) {}

	bool is_editor_hint() const { return false; }
	bool is_node_being_edited(const Node *p_node) const { return false; }
#endif

	void set_pause(bool p_enabled);
	bool is_paused() const;

	void set_camera(const RID &p_camera);
	RID get_camera() const;

#ifdef DEBUG_ENABLED
	void set_debug_collisions_hint(bool p_enabled);
	bool is_debugging_collisions_hint() const;

	void set_debug_navigation_hint(bool p_enabled);
	bool is_debugging_navigation_hint() const;
#else
	void set_debug_collisions_hint(bool p_enabled) {}
	bool is_debugging_collisions_hint() const { return false; }

	void set_debug_navigation_hint(bool p_enabled) {}
	bool is_debugging_navigation_hint() const { return false; }
#endif

	void set_debug_collisions_color(const Color &p_color);
	Color get_debug_collisions_color() const;

	void set_debug_collision_contact_color(const Color &p_color);
	Color get_debug_collision_contact_color() const;

	void set_debug_navigation_color(const Color &p_color);
	Color get_debug_navigation_color() const;

	void set_debug_navigation_disabled_color(const Color &p_color);
	Color get_debug_navigation_disabled_color() const;

	Ref<Material> get_debug_navigation_material();
	Ref<Material> get_debug_navigation_disabled_material();
	Ref<Material> get_debug_collision_material();
	Ref<Mesh> get_debug_contact_mesh();

	int get_collision_debug_contact_count() { return collision_debug_contacts; }

	int64_t get_frame() const;

	int get_node_count() const;

	void queue_delete(Object *p_object);

	void get_nodes_in_group(const StringName &p_group, List<Node *> *p_list);
	bool has_group(const StringName &p_identifier) const;

	void set_screen_stretch(StretchMode p_mode, StretchAspect p_aspect, const Size2 p_minsize);

	//void change_scene(const String& p_path);
	//Node *get_loaded_scene();

#ifdef TOOLS_ENABLED
	void set_edited_scene_root(Node *p_node);
	Node *get_edited_scene_root() const;
#endif

	void set_current_scene(Node *p_scene);
	Node *get_current_scene() const;
	Error change_scene(const String &p_path);
	Error change_scene_to(const Ref<PackedScene> &p_scene);
	Error reload_current_scene();

	//used by Main::start, don't use otherwise
	void add_current_scene(Node *p_current);

	static SceneTree *get_singleton() { return singleton; }

	void drop_files(const Vector<String> &p_files, int p_from_screen = 0);

	SceneTree();
	~SceneTree();
};

VARIANT_ENUM_CAST(SceneTree::StretchMode);
VARIANT_ENUM_CAST(SceneTree::StretchAspect);

#endif
