/*************************************************************************/
/*  spatial_editor_plugin.cpp                                            */
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
#include "spatial_editor_plugin.h"
#include "print_string.h"

#include "camera_matrix.h"
#include "core/os/input.h"
#include "editor/animation_editor.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/spatial_editor_gizmos.h"
#include "globals.h"
#include "os/keyboard.h"
#include "scene/3d/camera.h"
#include "scene/3d/visual_instance.h"
#include "scene/resources/surface_tool.h"
#include "sort.h"

#define DISTANCE_DEFAULT 4

#define GIZMO_ARROW_SIZE 0.3
#define GIZMO_RING_HALF_WIDTH 0.1
//#define GIZMO_SCALE_DEFAULT 0.28
#define GIZMO_SCALE_DEFAULT 0.15

void SpatialEditorViewport::_update_camera() {
	if (orthogonal) {
		//camera->set_orthogonal(size.width*cursor.distance,get_znear(),get_zfar());
		camera->set_orthogonal(2 * cursor.distance, 0.1, 8192);
	} else
		camera->set_perspective(get_fov(), get_znear(), get_zfar());

	Transform camera_transform;
	camera_transform.translate(cursor.pos);
	camera_transform.basis.rotate(Vector3(0, 1, 0), cursor.y_rot);
	camera_transform.basis.rotate(Vector3(1, 0, 0), cursor.x_rot);

	if (orthogonal)
		camera_transform.translate(0, 0, 4096);
	else
		camera_transform.translate(0, 0, cursor.distance);

	if (camera->get_global_transform() != camera_transform) {
		camera->set_global_transform(camera_transform);
		update_transform_gizmo_view();
	}
}

String SpatialEditorGizmo::get_handle_name(int p_idx) const {

	if (get_script_instance() && get_script_instance()->has_method("get_handle_name"))
		return get_script_instance()->call("get_handle_name", p_idx);

	return "";
}

Variant SpatialEditorGizmo::get_handle_value(int p_idx) const {

	if (get_script_instance() && get_script_instance()->has_method("get_handle_value"))
		return get_script_instance()->call("get_handle_value", p_idx);

	return Variant();
}

void SpatialEditorGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {

	if (get_script_instance() && get_script_instance()->has_method("set_handle"))
		get_script_instance()->call("set_handle", p_idx, p_camera, p_point);
}

void SpatialEditorGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {

	if (get_script_instance() && get_script_instance()->has_method("commit_handle"))
		get_script_instance()->call("commit_handle", p_idx, p_restore, p_cancel);
}

bool SpatialEditorGizmo::intersect_frustum(const Camera *p_camera, const Vector<Plane> &p_frustum) {

	return false;
}

bool SpatialEditorGizmo::intersect_ray(const Camera *p_camera, const Point2 &p_point, Vector3 &r_pos, Vector3 &r_normal, int *r_gizmo_handle, bool p_sec_first) {

	return false;
}

SpatialEditorGizmo::SpatialEditorGizmo() {

	selected = false;
}

int SpatialEditorViewport::get_selected_count() const {

	Map<Node *, Object *> &selection = editor_selection->get_selection();

	int count = 0;

	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {

		Spatial *sp = E->key()->cast_to<Spatial>();
		if (!sp)
			continue;

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se)
			continue;

		count++;
	}

	return count;
}

float SpatialEditorViewport::get_znear() const {

	float val = spatial_editor->get_znear();
	if (val < 0.001)
		val = 0.001;
	return val;
}
float SpatialEditorViewport::get_zfar() const {

	float val = spatial_editor->get_zfar();
	if (val < 0.001)
		val = 0.001;
	return val;
}
float SpatialEditorViewport::get_fov() const {

	float val = spatial_editor->get_fov();
	if (val < 0.001)
		val = 0.001;
	if (val > 89)
		val = 89;
	return val;
}

Transform SpatialEditorViewport::_get_camera_transform() const {

	return camera->get_global_transform();
}

Vector3 SpatialEditorViewport::_get_camera_pos() const {

	return _get_camera_transform().origin;
}

Point2 SpatialEditorViewport::_point_to_screen(const Vector3 &p_point) {

	return camera->unproject_position(p_point);
}

Vector3 SpatialEditorViewport::_get_ray_pos(const Vector2 &p_pos) const {

	return camera->project_ray_origin(p_pos);
}

Vector3 SpatialEditorViewport::_get_camera_normal() const {

	return -_get_camera_transform().basis.get_axis(2);
}

Vector3 SpatialEditorViewport::_get_ray(const Vector2 &p_pos) {

	return camera->project_ray_normal(p_pos);
}
/*
void SpatialEditorViewport::_clear_id(Spatial *p_node) {


	editor_selection->remove_node(p_node);


}
*/
void SpatialEditorViewport::_clear_selected() {

	editor_selection->clear();
}

void SpatialEditorViewport::_select_clicked(bool p_append, bool p_single) {

	if (!clicked)
		return;

	Object *obj = ObjectDB::get_instance(clicked);
	if (!obj)
		return;

	Spatial *sp = obj->cast_to<Spatial>();
	if (!sp)
		return;

	_select(sp, clicked_wants_append, true);
}

void SpatialEditorViewport::_select(Spatial *p_node, bool p_append, bool p_single) {

	if (!p_append) {

		// should not modify the selection..

		editor_selection->clear();
		editor_selection->add_node(p_node);

	} else {

		if (editor_selection->is_selected(p_node) && p_single) {
			//erase
			editor_selection->remove_node(p_node);
		} else {

			editor_selection->add_node(p_node);
		}
	}
}

ObjectID SpatialEditorViewport::_select_ray(const Point2 &p_pos, bool p_append, bool &r_includes_current, int *r_gizmo_handle, bool p_alt_select) {

	if (r_gizmo_handle)
		*r_gizmo_handle = -1;

	Vector3 ray = _get_ray(p_pos);
	Vector3 pos = _get_ray_pos(p_pos);

	Vector<RID> instances = VisualServer::get_singleton()->instances_cull_ray(pos, ray, get_tree()->get_root()->get_world()->get_scenario());
	Set<Ref<SpatialEditorGizmo> > found_gizmos;

	//uint32_t closest=0;
	//	float closest_dist=0;

	r_includes_current = false;

	List<_RayResult> results;
	Vector<Spatial *> subscenes = Vector<Spatial *>();
	Vector<Vector3> subscenes_positions = Vector<Vector3>();

	for (int i = 0; i < instances.size(); i++) {

		uint32_t id = VisualServer::get_singleton()->instance_get_object_instance_ID(instances[i]);
		Object *obj = ObjectDB::get_instance(id);
		if (!obj)
			continue;

		Spatial *spat = obj->cast_to<Spatial>();

		if (!spat)
			continue;

		Ref<SpatialEditorGizmo> seg = spat->get_gizmo();

		if (!seg.is_valid() || found_gizmos.has(seg)) {
			Node *subscene_candidate = spat;
			Vector3 source_click_spatial_pos = spat->get_global_transform().origin;

			while ((subscene_candidate->get_owner() != NULL) && (subscene_candidate->get_owner() != editor->get_edited_scene()))
				subscene_candidate = subscene_candidate->get_owner();

			spat = subscene_candidate->cast_to<Spatial>();
			if (spat && (spat->get_filename() != "") && (subscene_candidate->get_owner() != NULL)) {
				subscenes.push_back(spat);
				subscenes_positions.push_back(source_click_spatial_pos);
			}

			continue;
		}

		found_gizmos.insert(seg);
		Vector3 point;
		Vector3 normal;

		int handle = -1;
		bool inters = seg->intersect_ray(camera, p_pos, point, normal, NULL, p_alt_select);

		if (!inters)
			continue;

		float dist = pos.distance_to(point);

		if (dist < 0)
			continue;

		if (editor_selection->is_selected(spat))
			r_includes_current = true;

		_RayResult res;
		res.item = spat;
		res.depth = dist;
		res.handle = handle;
		results.push_back(res);
	}
	for (int idx_subscene = 0; idx_subscene < subscenes.size(); idx_subscene++) {

		Spatial *subscene = subscenes.get(idx_subscene);
		float dist = ray.cross(subscenes_positions.get(idx_subscene) - pos).length();

		if ((dist < 0) || (dist > 1.2))
			continue;

		if (editor_selection->is_selected(subscene))
			r_includes_current = true;

		_RayResult res;
		res.item = subscene;
		res.depth = dist;
		res.handle = -1;
		results.push_back(res);
	}

	if (results.empty())
		return 0;

	results.sort();
	Spatial *s = NULL;

	if (!r_includes_current || results.size() == 1 || (r_gizmo_handle && results.front()->get().handle >= 0)) {

		//return the nearest one
		s = results.front()->get().item;
		if (r_gizmo_handle)
			*r_gizmo_handle = results.front()->get().handle;

	} else {

		//returns the next one from a curent selection
		List<_RayResult>::Element *E = results.front();
		List<_RayResult>::Element *S = NULL;

		while (true) {

			//very strange loop algorithm that complies with object selection standards (tm).

			if (S == E) {
				//went all around and anothing was found
				//since can't rotate the selection
				//just return the first one

				s = results.front()->get().item;
				break;
			}

			if (!S && editor_selection->is_selected(E->get().item)) {
				//found an item currently in the selection,
				//so start from this one
				S = E;
			}

			if (S && !editor_selection->is_selected(E->get().item)) {
				// free item after a selected item, this one is desired.
				s = E->get().item;
				break;
			}

			E = E->next();
			if (!E) {

				if (!S) {
					//did a loop but nothing was selected, select first
					s = results.front()->get().item;
					break;
				}
				E = results.front();
			}
		}
	}

	if (!s)
		return 0;

	return s->get_instance_ID();
}

void SpatialEditorViewport::_find_items_at_pos(const Point2 &p_pos, bool &r_includes_current, Vector<_RayResult> &results, bool p_alt_select) {

	Vector3 ray = _get_ray(p_pos);
	Vector3 pos = _get_ray_pos(p_pos);

	Vector<RID> instances = VisualServer::get_singleton()->instances_cull_ray(pos, ray, get_tree()->get_root()->get_world()->get_scenario());
	Set<Ref<SpatialEditorGizmo> > found_gizmos;

	r_includes_current = false;

	for (int i = 0; i < instances.size(); i++) {

		uint32_t id = VisualServer::get_singleton()->instance_get_object_instance_ID(instances[i]);
		Object *obj = ObjectDB::get_instance(id);
		if (!obj)
			continue;

		Spatial *spat = obj->cast_to<Spatial>();

		if (!spat)
			continue;

		Ref<SpatialEditorGizmo> seg = spat->get_gizmo();

		if (!seg.is_valid())
			continue;

		if (found_gizmos.has(seg))
			continue;

		found_gizmos.insert(seg);
		Vector3 point;
		Vector3 normal;

		int handle = -1;
		bool inters = seg->intersect_ray(camera, p_pos, point, normal, NULL, p_alt_select);

		if (!inters)
			continue;

		float dist = pos.distance_to(point);

		if (dist < 0)
			continue;

		if (editor_selection->is_selected(spat))
			r_includes_current = true;

		_RayResult res;
		res.item = spat;
		res.depth = dist;
		res.handle = handle;
		results.push_back(res);
	}

	if (results.empty())
		return;

	results.sort();
}

Vector3 SpatialEditorViewport::_get_screen_to_space(const Vector3 &p_pos) {

	CameraMatrix cm;
	cm.set_perspective(get_fov(), get_size().get_aspect(), get_znear(), get_zfar());
	float screen_w, screen_h;
	cm.get_viewport_size(screen_w, screen_h);

	Transform camera_transform;
	camera_transform.translate(cursor.pos);
	camera_transform.basis.rotate(Vector3(0, 1, 0), cursor.y_rot);
	camera_transform.basis.rotate(Vector3(1, 0, 0), cursor.x_rot);
	camera_transform.translate(0, 0, cursor.distance);

	return camera_transform.xform(Vector3(((p_pos.x / get_size().width) * 2.0 - 1.0) * screen_w, ((1.0 - (p_pos.y / get_size().height)) * 2.0 - 1.0) * screen_h, -get_znear()));
}

void SpatialEditorViewport::_select_region() {

	if (cursor.region_begin == cursor.region_end)
		return; //nothing really

	Vector3 box[4] = {
		Vector3(
				MIN(cursor.region_begin.x, cursor.region_end.x),
				MIN(cursor.region_begin.y, cursor.region_end.y),
				0),
		Vector3(
				MAX(cursor.region_begin.x, cursor.region_end.x),
				MIN(cursor.region_begin.y, cursor.region_end.y),
				0),
		Vector3(
				MAX(cursor.region_begin.x, cursor.region_end.x),
				MAX(cursor.region_begin.y, cursor.region_end.y),
				0),
		Vector3(
				MIN(cursor.region_begin.x, cursor.region_end.x),
				MAX(cursor.region_begin.y, cursor.region_end.y),
				0)
	};

	Vector<Plane> frustum;

	Vector3 cam_pos = _get_camera_pos();
	Set<Ref<SpatialEditorGizmo> > found_gizmos;

	for (int i = 0; i < 4; i++) {

		Vector3 a = _get_screen_to_space(box[i]);
		Vector3 b = _get_screen_to_space(box[(i + 1) % 4]);
		frustum.push_back(Plane(a, b, cam_pos));
	}

	Plane near(cam_pos, -_get_camera_normal());
	near.d -= get_znear();

	frustum.push_back(near);

	Plane far = -near;
	far.d += 500.0;

	frustum.push_back(far);

	Vector<RID> instances = VisualServer::get_singleton()->instances_cull_convex(frustum, get_tree()->get_root()->get_world()->get_scenario());

	for (int i = 0; i < instances.size(); i++) {

		uint32_t id = VisualServer::get_singleton()->instance_get_object_instance_ID(instances[i]);

		Object *obj = ObjectDB::get_instance(id);
		if (!obj)
			continue;
		Spatial *sp = obj->cast_to<Spatial>();
		if (!sp)
			continue;

		Ref<SpatialEditorGizmo> seg = sp->get_gizmo();

		if (!seg.is_valid())
			continue;

		if (found_gizmos.has(seg))
			continue;

		if (seg->intersect_frustum(camera, frustum))
			_select(sp, true, false);
	}
}

void SpatialEditorViewport::_update_name() {

	String ortho = orthogonal ? TTR("Orthogonal") : TTR("Perspective");

	if (name != "")
		view_menu->set_text("[ " + name + " " + ortho + " ]");
	else
		view_menu->set_text("[ " + ortho + " ]");
}

void SpatialEditorViewport::_compute_edit(const Point2 &p_point) {

	_edit.click_ray = _get_ray(Vector2(p_point.x, p_point.y));
	_edit.click_ray_pos = _get_ray_pos(Vector2(p_point.x, p_point.y));
	_edit.plane = TRANSFORM_VIEW;
	spatial_editor->update_transform_gizmo();
	_edit.center = spatial_editor->get_gizmo_transform().origin;

	List<Node *> &selection = editor_selection->get_selected_node_list();

	//	Vector3 center;
	//	int nc=0;
	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

		Spatial *sp = E->get()->cast_to<Spatial>();
		if (!sp)
			continue;

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se)
			continue;

		se->original = se->sp->get_global_transform();
		//		center+=se->original.origin;
		//		nc++;
	}

	//	if (nc)
	//		_edit.center=center/float(nc);
}

static int _get_key_modifier(const String &p_property) {

	switch (EditorSettings::get_singleton()->get(p_property).operator int()) {

		case 0: return 0;
		case 1: return KEY_SHIFT;
		case 2: return KEY_ALT;
		case 3: return KEY_META;
		case 4: return KEY_CONTROL;
	}
	return 0;
}

SpatialEditorViewport::NavigationScheme SpatialEditorViewport::_get_navigation_schema(const String &p_property) {
	switch (EditorSettings::get_singleton()->get(p_property).operator int()) {
		case 0: return NAVIGATION_GODOT;
		case 1: return NAVIGATION_MAYA;
		case 2: return NAVIGATION_MODO;
	}
	return NAVIGATION_GODOT;
}

SpatialEditorViewport::NavigationZoomStyle SpatialEditorViewport::_get_navigation_zoom_style(const String &p_property) {
	switch (EditorSettings::get_singleton()->get(p_property).operator int()) {
		case 0: return NAVIGATION_ZOOM_VERTICAL;
		case 1: return NAVIGATION_ZOOM_HORIZONTAL;
	}
	return NAVIGATION_ZOOM_VERTICAL;
}

bool SpatialEditorViewport::_gizmo_select(const Vector2 &p_screenpos, bool p_hilite_only) {

	if (!spatial_editor->is_gizmo_visible())
		return false;
	if (get_selected_count() == 0) {
		if (p_hilite_only)
			spatial_editor->select_gizmo_highlight_axis(-1);
		return false;
	}

	Vector3 ray_pos = _get_ray_pos(Vector2(p_screenpos.x, p_screenpos.y));
	Vector3 ray = _get_ray(Vector2(p_screenpos.x, p_screenpos.y));

	Transform gt = spatial_editor->get_gizmo_transform();
	float gs = gizmo_scale;

	if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE) {

		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {

			Vector3 grabber_pos = gt.origin + gt.basis.get_axis(i) * gs;
			float grabber_radius = gs * GIZMO_ARROW_SIZE;

			Vector3 r;
			if (Geometry::segment_intersects_sphere(ray_pos, ray_pos + ray * 10000.0, grabber_pos, grabber_radius, &r)) {
				float d = r.distance_to(ray_pos);
				if (d < col_d) {
					col_d = d;
					col_axis = i;
				}
			}
		}

		if (col_axis != -1) {

			if (p_hilite_only) {

				spatial_editor->select_gizmo_highlight_axis(col_axis);

			} else {
				//handle rotate
				_edit.mode = TRANSFORM_TRANSLATE;
				_compute_edit(Point2(p_screenpos.x, p_screenpos.y));
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis);
			}
			return true;
		}
	}

	if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE) {

		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {

			Plane plane(gt.origin, gt.basis.get_axis(i).normalized());
			Vector3 r;
			if (!plane.intersects_ray(ray_pos, ray, &r))
				continue;

			float dist = r.distance_to(gt.origin);

			if (dist > gs * (1 - GIZMO_RING_HALF_WIDTH) && dist < gs * (1 + GIZMO_RING_HALF_WIDTH)) {

				float d = ray_pos.distance_to(r);
				if (d < col_d) {
					col_d = d;
					col_axis = i;
				}
			}
		}

		if (col_axis != -1) {

			if (p_hilite_only) {

				spatial_editor->select_gizmo_highlight_axis(col_axis + 3);
			} else {
				//handle rotate
				_edit.mode = TRANSFORM_ROTATE;
				_compute_edit(Point2(p_screenpos.x, p_screenpos.y));
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis);
			}
			return true;
		}
	}

	if (p_hilite_only)
		spatial_editor->select_gizmo_highlight_axis(-1);

	return false;
}

void SpatialEditorViewport::_smouseenter() {

	if (!surface->has_focus() && (!get_focus_owner() || !get_focus_owner()->is_text_field()))
		surface->grab_focus();
}

void SpatialEditorViewport::_list_select(InputEventMouseButton b) {

	_find_items_at_pos(Vector2(b.x, b.y), clicked_includes_current, selection_results, b.mod.shift);

	Node *scene = editor->get_edited_scene();

	for (int i = 0; i < selection_results.size(); i++) {
		Spatial *item = selection_results[i].item;
		if (item != scene && item->get_owner() != scene && !scene->is_editable_instance(item->get_owner())) {
			//invalid result
			selection_results.remove(i);
			i--;
		}
	}

	clicked_wants_append = b.mod.shift;

	if (selection_results.size() == 1) {

		clicked = selection_results[0].item->get_instance_ID();
		selection_results.clear();

		if (clicked) {
			_select_clicked(clicked_wants_append, true);
			clicked = 0;
		}

	} else if (!selection_results.empty()) {

		NodePath root_path = get_tree()->get_edited_scene_root()->get_path();
		StringName root_name = root_path.get_name(root_path.get_name_count() - 1);

		for (int i = 0; i < selection_results.size(); i++) {

			Spatial *spat = selection_results[i].item;

			Ref<Texture> icon;
			if (spat->has_meta("_editor_icon"))
				icon = spat->get_meta("_editor_icon");
			else
				icon = get_icon(has_icon(spat->get_type(), "EditorIcons") ? spat->get_type() : String("Object"), "EditorIcons");

			String node_path = "/" + root_name + "/" + root_path.rel_path_to(spat->get_path());

			selection_menu->add_item(spat->get_name());
			selection_menu->set_item_icon(i, icon);
			selection_menu->set_item_metadata(i, node_path);
			selection_menu->set_item_tooltip(i, String(spat->get_name()) + "\nType: " + spat->get_type() + "\nPath: " + node_path);
		}

		selection_menu->set_global_pos(Vector2(b.global_x, b.global_y));
		selection_menu->popup();
		selection_menu->call_deferred("grab_click_focus");
		selection_menu->set_invalidate_click_until_motion();
	}
}
void SpatialEditorViewport::_sinput(const InputEvent &p_event) {

	if (previewing)
		return; //do NONE

	{

		EditorNode *en = editor;
		EditorPluginList *over_plugin_list = en->get_editor_plugins_over();

		if (!over_plugin_list->empty()) {
			bool discard = over_plugin_list->forward_spatial_input_event(camera, p_event);
			if (discard)
				return;
		}
	}

	switch (p_event.type) {
		case InputEvent::MOUSE_BUTTON: {

			const InputEventMouseButton &b = p_event.mouse_button;

			switch (b.button_index) {

				case BUTTON_WHEEL_UP: {

					cursor.distance /= 1.08;
					if (cursor.distance < 0.001)
						cursor.distance = 0.001;

				} break;
				case BUTTON_WHEEL_DOWN: {

					if (cursor.distance < 0.001)
						cursor.distance = 0.001;
					cursor.distance *= 1.08;

				} break;
				case BUTTON_RIGHT: {

					NavigationScheme nav_scheme = _get_navigation_schema("3d_editor/navigation_scheme");

					if (b.pressed && _edit.gizmo.is_valid()) {
						//restore
						_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_initial_value, true);
						_edit.gizmo = Ref<SpatialEditorGizmo>();
					}

					if (_edit.mode == TRANSFORM_NONE && b.pressed) {

						Plane cursor_plane(cursor.cursor_pos, _get_camera_normal());

						Vector3 ray_origin = _get_ray_pos(Vector2(b.x, b.y));
						Vector3 ray_dir = _get_ray(Vector2(b.x, b.y));

						//gizmo modify

						if (b.mod.control) {

							Vector<RID> instances = VisualServer::get_singleton()->instances_cull_ray(ray_origin, ray_dir, get_tree()->get_root()->get_world()->get_scenario());

							Plane p(ray_origin, _get_camera_normal());

							float min_d = 1e10;
							bool found = false;

							for (int i = 0; i < instances.size(); i++) {

								uint32_t id = VisualServer::get_singleton()->instance_get_object_instance_ID(instances[i]);
								Object *obj = ObjectDB::get_instance(id);
								if (!obj)
									continue;

								VisualInstance *vi = obj->cast_to<VisualInstance>();
								if (!vi)
									continue;

								//optimize by checking AABB (although should pre sort by distance)
								AABB aabb = vi->get_global_transform().xform(vi->get_aabb());
								if (p.distance_to(aabb.get_support(-ray_dir)) > min_d)
									continue;

								DVector<Face3> faces = vi->get_faces(VisualInstance::FACES_SOLID);
								int c = faces.size();
								if (c > 0) {
									DVector<Face3>::Read r = faces.read();

									for (int j = 0; j < c; j++) {

										Vector3 inters;
										if (r[j].intersects_ray(ray_origin, ray_dir, &inters)) {

											float d = p.distance_to(inters);
											if (d < 0)
												continue;

											if (d < min_d) {
												min_d = d;
												found = true;
											}
										}
									}
								}
							}

							if (found) {

								//cursor.cursor_pos=ray_origin+ray_dir*min_d;
								//VisualServer::get_singleton()->instance_set_transform(cursor_instance,Transform(Matrix3(),cursor.cursor_pos));
							}

						} else {
							Vector3 new_pos;
							if (cursor_plane.intersects_ray(ray_origin, ray_dir, &new_pos)) {

								//cursor.cursor_pos=new_pos;
								//VisualServer::get_singleton()->instance_set_transform(cursor_instance,Transform(Matrix3(),cursor.cursor_pos));
							}
						}

						if (b.mod.alt) {

							if (nav_scheme == NAVIGATION_MAYA)
								break;

							_list_select(b);
							return;
						}
					}

					if (_edit.mode != TRANSFORM_NONE && b.pressed) {
						//cancel motion
						_edit.mode = TRANSFORM_NONE;
						//_validate_selection();

						List<Node *> &selection = editor_selection->get_selected_node_list();

						for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

							Spatial *sp = E->get()->cast_to<Spatial>();
							if (!sp)
								continue;

							SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
							if (!se)
								continue;

							sp->set_global_transform(se->original);
						}
						surface->update();
						//VisualServer::get_singleton()->poly_clear(indicators);
						set_message(TTR("Transform Aborted."), 3);
					}
				} break;
				case BUTTON_MIDDLE: {

					if (b.pressed && _edit.mode != TRANSFORM_NONE) {

						switch (_edit.plane) {

							case TRANSFORM_VIEW: {

								_edit.plane = TRANSFORM_X_AXIS;
								set_message(TTR("X-Axis Transform."), 2);
								name = "";
								_update_name();
							} break;
							case TRANSFORM_X_AXIS: {

								_edit.plane = TRANSFORM_Y_AXIS;
								set_message(TTR("Y-Axis Transform."), 2);

							} break;
							case TRANSFORM_Y_AXIS: {

								_edit.plane = TRANSFORM_Z_AXIS;
								set_message(TTR("Z-Axis Transform."), 2);

							} break;
							case TRANSFORM_Z_AXIS: {

								_edit.plane = TRANSFORM_VIEW;
								set_message(TTR("View Plane Transform."), 2);

							} break;
						}
					}
				} break;
				case BUTTON_LEFT: {

					if (b.pressed) {

						NavigationScheme nav_scheme = _get_navigation_schema("3d_editor/navigation_scheme");
						if ((nav_scheme == NAVIGATION_MAYA || nav_scheme == NAVIGATION_MODO) && b.mod.alt) {
							break;
						}

						if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_LIST_SELECT) {
							_list_select(b);
							break;
						}

						_edit.mouse_pos = Point2(b.x, b.y);
						_edit.snap = false;
						_edit.mode = TRANSFORM_NONE;

						//gizmo has priority over everything

						bool can_select_gizmos = true;

						{
							int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
							can_select_gizmos = view_menu->get_popup()->is_item_checked(idx);
						}

						if (can_select_gizmos && spatial_editor->get_selected()) {

							Ref<SpatialEditorGizmo> seg = spatial_editor->get_selected()->get_gizmo();
							if (seg.is_valid()) {
								int handle = -1;
								Vector3 point;
								Vector3 normal;
								bool inters = seg->intersect_ray(camera, _edit.mouse_pos, point, normal, &handle, b.mod.shift);
								if (inters && handle != -1) {

									_edit.gizmo = seg;
									_edit.gizmo_handle = handle;
									//_edit.gizmo_initial_pos=seg->get_handle_pos(gizmo_handle);
									_edit.gizmo_initial_value = seg->get_handle_value(handle);
									break;
								}
							}
						}

						if (_gizmo_select(_edit.mouse_pos))
							break;

						clicked = 0;
						clicked_includes_current = false;

						if ((spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT && b.mod.control) || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE) {

							/* HANDLE ROTATION */
							if (get_selected_count() == 0)
								break; //bye
							//handle rotate
							_edit.mode = TRANSFORM_ROTATE;
							_compute_edit(Point2(b.x, b.y));
							break;
						}

						if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE) {

							if (get_selected_count() == 0)
								break; //bye
							//handle rotate
							_edit.mode = TRANSFORM_TRANSLATE;
							_compute_edit(Point2(b.x, b.y));
							break;
						}

						if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SCALE) {

							if (get_selected_count() == 0)
								break; //bye
							//handle rotate
							_edit.mode = TRANSFORM_SCALE;
							_compute_edit(Point2(b.x, b.y));
							break;
						}

						// todo scale

						int gizmo_handle = -1;

						clicked = _select_ray(Vector2(b.x, b.y), b.mod.shift, clicked_includes_current, &gizmo_handle, b.mod.shift);

						//clicking is always deferred to either move or release

						clicked_wants_append = b.mod.shift;

						if (!clicked) {

							if (!clicked_wants_append)
								_clear_selected();

							//default to regionselect
							cursor.region_select = true;
							cursor.region_begin = Point2(b.x, b.y);
							cursor.region_end = Point2(b.x, b.y);
						}

						if (clicked && gizmo_handle >= 0) {

							Object *obj = ObjectDB::get_instance(clicked);
							if (obj) {

								Spatial *spa = obj->cast_to<Spatial>();
								if (spa) {

									Ref<SpatialEditorGizmo> seg = spa->get_gizmo();
									if (seg.is_valid()) {

										_edit.gizmo = seg;
										_edit.gizmo_handle = gizmo_handle;
										//_edit.gizmo_initial_pos=seg->get_handle_pos(gizmo_handle);
										_edit.gizmo_initial_value = seg->get_handle_value(gizmo_handle);
										//print_line("GIZMO: "+itos(gizmo_handle)+" FROMPOS: "+_edit.orig_gizmo_pos);
										break;
									}
								}
							}
							//_compute_edit(Point2(b.x,b.y)); //in case a motion happens..
						}

						surface->update();
					} else {

						if (_edit.gizmo.is_valid()) {

							_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_initial_value, false);
							_edit.gizmo = Ref<SpatialEditorGizmo>();
							break;
						}
						if (clicked) {
							_select_clicked(clicked_wants_append, true);
							//clickd processing was deferred
							clicked = 0;
						}

						if (cursor.region_select) {
							_select_region();
							cursor.region_select = false;
							surface->update();
						}

						if (_edit.mode != TRANSFORM_NONE) {

							static const char *_transform_name[4] = { "None", "Rotate", "Translate", "Scale" };
							undo_redo->create_action(_transform_name[_edit.mode]);

							List<Node *> &selection = editor_selection->get_selected_node_list();

							for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

								Spatial *sp = E->get()->cast_to<Spatial>();
								if (!sp)
									continue;

								SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
								if (!se)
									continue;

								undo_redo->add_do_method(sp, "set_global_transform", sp->get_global_transform());
								undo_redo->add_undo_method(sp, "set_global_transform", se->original);
							}
							undo_redo->commit_action();
							_edit.mode = TRANSFORM_NONE;
							//VisualServer::get_singleton()->poly_clear(indicators);
							set_message("");
						}

						surface->update();
					}
				} break;
			}
		} break;
		case InputEvent::MOUSE_MOTION: {
			const InputEventMouseMotion &m = p_event.mouse_motion;
			_edit.mouse_pos = Point2(p_event.mouse_motion.x, p_event.mouse_motion.y);

			if (spatial_editor->get_selected()) {

				Ref<SpatialEditorGizmo> seg = spatial_editor->get_selected()->get_gizmo();
				if (seg.is_valid()) {

					int selected_handle = -1;

					int handle = -1;
					Vector3 point;
					Vector3 normal;
					bool inters = seg->intersect_ray(camera, _edit.mouse_pos, point, normal, &handle, false);
					if (inters && handle != -1) {

						selected_handle = handle;
					}

					if (selected_handle != spatial_editor->get_over_gizmo_handle()) {
						spatial_editor->set_over_gizmo_handle(selected_handle);
						spatial_editor->get_selected()->update_gizmo();
						if (selected_handle != -1)
							spatial_editor->select_gizmo_highlight_axis(-1);
					}
				}
			}

			if (spatial_editor->get_over_gizmo_handle() == -1 && !(m.button_mask & 1) && !_edit.gizmo.is_valid()) {

				_gizmo_select(_edit.mouse_pos, true);
			}

			NavigationScheme nav_scheme = _get_navigation_schema("3d_editor/navigation_scheme");
			NavigationMode nav_mode = NAVIGATION_NONE;

			if (_edit.gizmo.is_valid()) {

				_edit.gizmo->set_handle(_edit.gizmo_handle, camera, Vector2(m.x, m.y));
				Variant v = _edit.gizmo->get_handle_value(_edit.gizmo_handle);
				String n = _edit.gizmo->get_handle_name(_edit.gizmo_handle);
				set_message(n + ": " + String(v));

			} else if (m.button_mask & 1) {

				if (nav_scheme == NAVIGATION_MAYA && m.mod.alt) {
					nav_mode = NAVIGATION_ORBIT;
				} else if (nav_scheme == NAVIGATION_MODO && m.mod.alt && m.mod.shift) {
					nav_mode = NAVIGATION_PAN;
				} else if (nav_scheme == NAVIGATION_MODO && m.mod.alt && m.mod.control) {
					nav_mode = NAVIGATION_ZOOM;
				} else if (nav_scheme == NAVIGATION_MODO && m.mod.alt) {
					nav_mode = NAVIGATION_ORBIT;
				} else {
					if (clicked) {

						if (!clicked_includes_current) {

							_select_clicked(clicked_wants_append, true);
							//clickd processing was deferred
						}

						_compute_edit(_edit.mouse_pos);
						clicked = 0;

						_edit.mode = TRANSFORM_TRANSLATE;
					}

					if (cursor.region_select && nav_mode == NAVIGATION_NONE) {

						cursor.region_end = Point2(m.x, m.y);
						surface->update();
						return;
					}

					if (_edit.mode == TRANSFORM_NONE && nav_mode == NAVIGATION_NONE)
						break;

					Vector3 ray_pos = _get_ray_pos(Vector2(m.x, m.y));
					Vector3 ray = _get_ray(Vector2(m.x, m.y));

					switch (_edit.mode) {

						case TRANSFORM_SCALE: {

							Plane plane = Plane(_edit.center, _get_camera_normal());

							Vector3 intersection;
							if (!plane.intersects_ray(ray_pos, ray, &intersection))
								break;

							Vector3 click;
							if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click))
								break;

							float center_click_dist = click.distance_to(_edit.center);
							float center_inters_dist = intersection.distance_to(_edit.center);
							if (center_click_dist == 0)
								break;

							float scale = (center_inters_dist / center_click_dist) * 100.0;

							if (_edit.snap || spatial_editor->is_snap_enabled()) {

								scale = Math::stepify(scale, spatial_editor->get_scale_snap());
							}

							set_message(vformat(TTR("Scaling to %s%%."), String::num(scale, 1)));
							scale /= 100.0;

							Transform r;
							r.basis.scale(Vector3(scale, scale, scale));

							List<Node *> &selection = editor_selection->get_selected_node_list();

							for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

								Spatial *sp = E->get()->cast_to<Spatial>();
								if (!sp)
									continue;

								SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
								if (!se)
									continue;

								Transform original = se->original;

								Transform base = Transform(Matrix3(), _edit.center);
								Transform t = base * (r * (base.inverse() * original));

								sp->set_global_transform(t);
							}

							surface->update();

						} break;

						case TRANSFORM_TRANSLATE: {

							Vector3 motion_mask;
							Plane plane;

							switch (_edit.plane) {
								case TRANSFORM_VIEW:
									motion_mask = Vector3(0, 0, 0);
									plane = Plane(_edit.center, _get_camera_normal());
									break;
								case TRANSFORM_X_AXIS:
									motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(0);
									plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
									break;
								case TRANSFORM_Y_AXIS:
									motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(1);
									plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
									break;
								case TRANSFORM_Z_AXIS:
									motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(2);
									plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
									break;
							}

							Vector3 intersection;
							if (!plane.intersects_ray(ray_pos, ray, &intersection))
								break;

							Vector3 click;
							if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click))
								break;

							//_validate_selection();
							Vector3 motion = intersection - click;
							if (motion_mask != Vector3()) {
								motion = motion_mask.dot(motion) * motion_mask;
							}

							float snap = 0;

							if (_edit.snap || spatial_editor->is_snap_enabled()) {

								snap = spatial_editor->get_translate_snap();
								motion.snap(snap);
							}

							//set_message("Translating: "+motion);

							List<Node *> &selection = editor_selection->get_selected_node_list();

							for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

								Spatial *sp = E->get()->cast_to<Spatial>();
								if (!sp) {
									continue;
								}

								SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
								if (!se) {
									continue;
								}

								Transform t = se->original;
								t.origin += motion;
								sp->set_global_transform(t);
							}
						} break;

						case TRANSFORM_ROTATE: {

							Plane plane;

							switch (_edit.plane) {
								case TRANSFORM_VIEW:
									plane = Plane(_edit.center, _get_camera_normal());
									break;
								case TRANSFORM_X_AXIS:
									plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(0));
									break;
								case TRANSFORM_Y_AXIS:
									plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(1));
									break;
								case TRANSFORM_Z_AXIS:
									plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(2));
									break;
							}

							Vector3 intersection;
							if (!plane.intersects_ray(ray_pos, ray, &intersection))
								break;

							Vector3 click;
							if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click))
								break;

							Vector3 y_axis = (click - _edit.center).normalized();
							Vector3 x_axis = plane.normal.cross(y_axis).normalized();

							float angle = Math::atan2(x_axis.dot(intersection - _edit.center), y_axis.dot(intersection - _edit.center));
							if (_edit.snap || spatial_editor->is_snap_enabled()) {

								float snap = spatial_editor->get_rotate_snap();

								if (snap) {
									angle = Math::rad2deg(angle) + snap * 0.5; //else it wont reach +180
									angle -= Math::fmod(angle, snap);
									set_message(vformat(TTR("Rotating %s degrees."), rtos(angle)));
									angle = Math::deg2rad(angle);
								} else
									set_message(vformat(TTR("Rotating %s degrees."), rtos(Math::rad2deg(angle))));

							} else {
								set_message(vformat(TTR("Rotating %s degrees."), rtos(Math::rad2deg(angle))));
							}

							Transform r;
							r.basis.rotate(plane.normal, -angle);

							List<Node *> &selection = editor_selection->get_selected_node_list();

							for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

								Spatial *sp = E->get()->cast_to<Spatial>();
								if (!sp)
									continue;

								SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
								if (!se)
									continue;

								Transform original = se->original;

								Transform base = Transform(Matrix3(), _edit.center);
								Transform t = base * (r * (base.inverse() * original));

								sp->set_global_transform(t);
							}

							surface->update();
							/*
							VisualServer::get_singleton()->poly_clear(indicators);

							Vector<Vector3> points;
							Vector<Vector3> empty;
							Vector<Color> colors;
							points.push_back(intersection);
							points.push_back(_edit.original.origin);
							colors.push_back( Color(255,155,100) );
							colors.push_back( Color(255,155,100) );
							VisualServer::get_singleton()->poly_add_primitive(indicators,points,empty,colors,empty);
							*/
						} break;
						default: {}
					}
				}

			} else if (m.button_mask & 2) {

				if (nav_scheme == NAVIGATION_MAYA && m.mod.alt) {
					nav_mode = NAVIGATION_ZOOM;
				}

			} else if (m.button_mask & 4) {

				if (nav_scheme == NAVIGATION_GODOT) {

					int mod = 0;
					if (m.mod.shift)
						mod = KEY_SHIFT;
					if (m.mod.alt)
						mod = KEY_ALT;
					if (m.mod.control)
						mod = KEY_CONTROL;
					if (m.mod.meta)
						mod = KEY_META;

					if (mod == _get_key_modifier("3d_editor/pan_modifier"))
						nav_mode = NAVIGATION_PAN;
					else if (mod == _get_key_modifier("3d_editor/zoom_modifier"))
						nav_mode = NAVIGATION_ZOOM;
					else if (mod == _get_key_modifier("3d_editor/orbit_modifier"))
						nav_mode = NAVIGATION_ORBIT;

				} else if (nav_scheme == NAVIGATION_MAYA) {
					if (m.mod.alt)
						nav_mode = NAVIGATION_PAN;
				}

			} else if (EditorSettings::get_singleton()->get("3d_editor/emulate_3_button_mouse")) {
				// Handle trackpad (no external mouse) use case
				int mod = 0;
				if (m.mod.shift)
					mod = KEY_SHIFT;
				if (m.mod.alt)
					mod = KEY_ALT;
				if (m.mod.control)
					mod = KEY_CONTROL;
				if (m.mod.meta)
					mod = KEY_META;

				if (mod) {
					if (mod == _get_key_modifier("3d_editor/pan_modifier"))
						nav_mode = NAVIGATION_PAN;
					else if (mod == _get_key_modifier("3d_editor/zoom_modifier"))
						nav_mode = NAVIGATION_ZOOM;
					else if (mod == _get_key_modifier("3d_editor/orbit_modifier"))
						nav_mode = NAVIGATION_ORBIT;
				}
			}

			switch (nav_mode) {
				case NAVIGATION_PAN: {

					real_t pan_speed = 1 / 150.0;
					int pan_speed_modifier = 10;
					if (nav_scheme == NAVIGATION_MAYA && m.mod.shift)
						pan_speed *= pan_speed_modifier;

					Point2i relative;
					if (bool(EditorSettings::get_singleton()->get("3d_editor/warped_mouse_panning"))) {
						relative = Input::get_singleton()->warp_mouse_motion(m, surface->get_global_rect());
					} else {
						relative = Point2i(m.relative_x, m.relative_y);
					}

					Transform camera_transform;

					camera_transform.translate(cursor.pos);
					camera_transform.basis.rotate(Vector3(0, 1, 0), cursor.y_rot);
					camera_transform.basis.rotate(Vector3(1, 0, 0), cursor.x_rot);
					Vector3 translation(-relative.x * pan_speed, relative.y * pan_speed, 0);
					translation *= cursor.distance / DISTANCE_DEFAULT;
					camera_transform.translate(translation);
					cursor.pos = camera_transform.origin;

				} break;

				case NAVIGATION_ZOOM: {
					real_t zoom_speed = 1 / 80.0;
					int zoom_speed_modifier = 10;
					if (nav_scheme == NAVIGATION_MAYA && m.mod.shift)
						zoom_speed *= zoom_speed_modifier;

					NavigationZoomStyle zoom_style = _get_navigation_zoom_style("3d_editor/zoom_style");
					if (zoom_style == NAVIGATION_ZOOM_HORIZONTAL) {
						if (m.relative_x > 0)
							cursor.distance *= 1 - m.relative_x * zoom_speed;
						else if (m.relative_x < 0)
							cursor.distance /= 1 + m.relative_x * zoom_speed;
					} else {
						if (m.relative_y > 0)
							cursor.distance *= 1 + m.relative_y * zoom_speed;
						else if (m.relative_y < 0)
							cursor.distance /= 1 - m.relative_y * zoom_speed;
					}

				} break;

				case NAVIGATION_ORBIT: {
					cursor.x_rot += m.relative_y / 80.0;
					cursor.y_rot += m.relative_x / 80.0;
					if (cursor.x_rot > Math_PI / 2.0)
						cursor.x_rot = Math_PI / 2.0;
					if (cursor.x_rot < -Math_PI / 2.0)
						cursor.x_rot = -Math_PI / 2.0;
					name = "";
					_update_name();
				} break;

				default: {}
			}
		} break;
		case InputEvent::KEY: {
			const InputEventKey &k = p_event.key;
			if (!k.pressed)
				break;

			if (ED_IS_SHORTCUT("spatial_editor/snap", p_event)) {
				if (_edit.mode != TRANSFORM_NONE) {
					_edit.snap = true;
				}
			}
			if (ED_IS_SHORTCUT("spatial_editor/bottom_view", p_event)) {
				cursor.y_rot = 0;
				cursor.x_rot = -Math_PI / 2.0;
				set_message(TTR("Bottom View."), 2);
				name = TTR("Bottom");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/top_view", p_event)) {
				cursor.y_rot = 0;
				cursor.x_rot = Math_PI / 2.0;
				set_message(TTR("Top View."), 2);
				name = TTR("Top");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/rear_view", p_event)) {
				cursor.x_rot = 0;
				cursor.y_rot = Math_PI;
				set_message(TTR("Rear View."), 2);
				name = TTR("Rear");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/front_view", p_event)) {
				cursor.x_rot = 0;
				cursor.y_rot = 0;
				set_message(TTR("Front View."), 2);
				name = TTR("Front");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/left_view", p_event)) {
				cursor.x_rot = 0;
				cursor.y_rot = Math_PI / 2.0;
				set_message(TTR("Left View."), 2);
				name = TTR("Left");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/right_view", p_event)) {
				cursor.x_rot = 0;
				cursor.y_rot = -Math_PI / 2.0;
				set_message(TTR("Right View."), 2);
				name = TTR("Right");
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/switch_perspective_orthogonal", p_event)) {
				_menu_option(orthogonal ? VIEW_PERSPECTIVE : VIEW_ORTHOGONAL);
				_update_name();
			}
			if (ED_IS_SHORTCUT("spatial_editor/insert_anim_key", p_event)) {
				if (!get_selected_count() || _edit.mode != TRANSFORM_NONE)
					break;

				if (!AnimationPlayerEditor::singleton->get_key_editor()->has_keying()) {
					set_message(TTR("Keying is disabled (no key inserted)."));
					break;
				}

				List<Node *> &selection = editor_selection->get_selected_node_list();

				for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

					Spatial *sp = E->get()->cast_to<Spatial>();
					if (!sp)
						continue;

					emit_signal("transform_key_request", sp, "", sp->get_transform());
				}

				set_message(TTR("Animation Key Inserted."));
			}

			if (k.scancode == KEY_SPACE) {
				if (!k.pressed) emit_signal("toggle_maximize_view", this);
			}

		} break;
	}
}

void SpatialEditorViewport::set_message(String p_message, float p_time) {

	message = p_message;
	message_time = p_time;
}

void SpatialEditorViewport::_notification(int p_what) {

	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

		bool visible = is_visible();

		set_process(visible);

		if (visible)
			_update_camera();

		call_deferred("update_transform_gizmo_view");
	}

	if (p_what == NOTIFICATION_RESIZED) {

		call_deferred("update_transform_gizmo_view");
	}

	if (p_what == NOTIFICATION_PROCESS) {

		//force editr camera
		/*
		current_camera=get_root_node()->get_current_camera();
		if (current_camera!=camera) {


		}
		*/

		_update_camera();

		Map<Node *, Object *> &selection = editor_selection->get_selection();

		bool changed = false;
		bool exist = false;

		for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {

			Spatial *sp = E->key()->cast_to<Spatial>();
			if (!sp)
				continue;

			SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
			if (!se)
				continue;

			VisualInstance *vi = sp->cast_to<VisualInstance>();

			if (se->aabb.has_no_surface()) {

				se->aabb = vi ? vi->get_aabb() : AABB(Vector3(-0.2, -0.2, -0.2), Vector3(0.4, 0.4, 0.4));
			}

			Transform t = sp->get_global_transform();
			t.translate(se->aabb.pos);
			t.basis.scale(se->aabb.size);

			exist = true;
			if (se->last_xform == t)
				continue;
			changed = true;
			se->last_xform = t;
			VisualServer::get_singleton()->instance_set_transform(se->sbox_instance, t);
		}

		if (changed || (spatial_editor->is_gizmo_visible() && !exist)) {
			spatial_editor->update_transform_gizmo();
		}

		if (message_time > 0) {

			if (message != last_message) {
				surface->update();
				last_message = message;
			}

			message_time -= get_fixed_process_delta_time();
			if (message_time < 0)
				surface->update();
		}

		// FPS Counter.
		bool show_fps = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_FPS)) && !previewing;
		if (show_fps != fps->is_visible()) {
			if (show_fps)
				fps->show();
			else
				fps->hide();
		}

		if (show_fps) {
			const float os_fps = OS::get_singleton()->get_frames_per_second();
			String text = TTR("FPS") + ": " + itos(os_fps) + " (" + String::num(1000.0f / os_fps, 2) + " ms)";

			if (fps_label->get_text() != text || surface->get_size() != prev_size) {
				fps_label->set_text(text);
				Vector2 container_position = Vector2(surface->get_size().x - fps->get_size().x - 20, 20);
				if (preview)
					container_position.y += 20;

				container_position *= EDSCALE;
				fps->set_pos(container_position);
			}
		}
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {

		surface->connect("draw", this, "_draw");
		surface->connect("input_event", this, "_sinput");
		surface->connect("mouse_enter", this, "_smouseenter");
		fps->add_style_override("panel", get_stylebox("panel", "Panel"));
		preview_camera->set_icon(get_icon("Camera", "EditorIcons"));
		_init_gizmo_instance(index);
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {

		_finish_gizmo_instances();
	}

	if (p_what == NOTIFICATION_MOUSE_ENTER) {
	}

	if (p_what == NOTIFICATION_DRAW) {
	}
}

void SpatialEditorViewport::_draw() {

	if (surface->has_focus()) {
		Size2 size = surface->get_size();
		Rect2 r = Rect2(Point2(), size);
		get_stylebox("EditorFocus", "EditorStyles")->draw(surface->get_canvas_item(), r);
	}

	RID ci = surface->get_canvas_item();

	if (cursor.region_select) {

		VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(cursor.region_begin, cursor.region_end - cursor.region_begin), Color(0.7, 0.7, 1.0, 0.3));
	}

	if (message_time > 0) {
		Ref<Font> font = get_font("font", "Label");
		Point2 msgpos = Point2(5, get_size().y - 20);
		font->draw(ci, msgpos + Point2(1, 1), message, Color(0, 0, 0, 0.8));
		font->draw(ci, msgpos + Point2(-1, -1), message, Color(0, 0, 0, 0.8));
		font->draw(ci, msgpos, message, Color(1, 1, 1, 1));
	}

	if (_edit.mode == TRANSFORM_ROTATE) {

		Point2 center = _point_to_screen(_edit.center);
		VisualServer::get_singleton()->canvas_item_add_line(ci, _edit.mouse_pos, center, Color(0.4, 0.7, 1.0, 0.8));
	}

	if (previewing) {

		Size2 ss = Size2(Globals::get_singleton()->get("display/width"), Globals::get_singleton()->get("display/height"));
		float aspect = ss.get_aspect();
		Size2 s = get_size();

		Rect2 draw_rect;

		switch (previewing->get_keep_aspect_mode()) {
			case Camera::KEEP_WIDTH: {

				draw_rect.size = Size2(s.width, s.width / aspect);
				draw_rect.pos.x = 0;
				draw_rect.pos.y = (s.height - draw_rect.size.y) * 0.5;

			} break;
			case Camera::KEEP_HEIGHT: {

				draw_rect.size = Size2(s.height * aspect, s.height);
				draw_rect.pos.y = 0;
				draw_rect.pos.x = (s.width - draw_rect.size.x) * 0.5;

			} break;
		}

		draw_rect = Rect2(Vector2(), s).clip(draw_rect);

		surface->draw_line(draw_rect.pos, draw_rect.pos + Vector2(draw_rect.size.x, 0), Color(0.6, 0.6, 0.1, 0.5), 2.0);
		surface->draw_line(draw_rect.pos + Vector2(draw_rect.size.x, 0), draw_rect.pos + draw_rect.size, Color(0.6, 0.6, 0.1, 0.5), 2.0);
		surface->draw_line(draw_rect.pos + draw_rect.size, draw_rect.pos + Vector2(0, draw_rect.size.y), Color(0.6, 0.6, 0.1, 0.5), 2.0);
		surface->draw_line(draw_rect.pos, draw_rect.pos + Vector2(0, draw_rect.size.y), Color(0.6, 0.6, 0.1, 0.5), 2.0);
	}
}

void SpatialEditorViewport::_menu_option(int p_option) {

	switch (p_option) {

		case VIEW_TOP: {

			cursor.x_rot = Math_PI / 2.0;
			cursor.y_rot = 0;
			name = TTR("Top");
			_update_name();
		} break;
		case VIEW_BOTTOM: {

			cursor.x_rot = -Math_PI / 2.0;
			cursor.y_rot = 0;
			name = TTR("Bottom");
			_update_name();

		} break;
		case VIEW_LEFT: {

			cursor.y_rot = Math_PI / 2.0;
			cursor.x_rot = 0;
			name = TTR("Left");
			_update_name();

		} break;
		case VIEW_RIGHT: {

			cursor.y_rot = -Math_PI / 2.0;
			cursor.x_rot = 0;
			name = TTR("Right");
			_update_name();

		} break;
		case VIEW_FRONT: {

			cursor.y_rot = 0;
			cursor.x_rot = 0;
			name = TTR("Front");
			_update_name();

		} break;
		case VIEW_REAR: {

			cursor.y_rot = Math_PI;
			cursor.x_rot = 0;
			name = TTR("Rear");
			_update_name();

		} break;
		case VIEW_CENTER_TO_ORIGIN: {

			cursor.pos = Vector3(0, 0, 0);

		} break;
		case VIEW_CENTER_TO_SELECTION: {

			if (!get_selected_count())
				break;

			Vector3 center;
			int count = 0;

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

				Spatial *sp = E->get()->cast_to<Spatial>();
				if (!sp)
					continue;

				SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
				if (!se)
					continue;

				center += sp->get_global_transform().origin;
				count++;
			}

			if (count != 0) {
				center /= float(count);
			}

			cursor.pos = center;
		} break;
		case VIEW_ALIGN_SELECTION_WITH_VIEW: {

			if (!get_selected_count())
				break;

			Transform camera_transform = camera->get_global_transform();

			List<Node *> &selection = editor_selection->get_selected_node_list();

			undo_redo->create_action(TTR("Align with view"));
			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

				Spatial *sp = E->get()->cast_to<Spatial>();
				if (!sp)
					continue;

				SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
				if (!se)
					continue;

				Transform xform = camera_transform;
				xform.scale_basis(sp->get_scale());

				undo_redo->add_do_method(sp, "set_global_transform", xform);
				undo_redo->add_undo_method(sp, "set_global_transform", sp->get_global_transform());
			}
			undo_redo->commit_action();
		} break;
		case VIEW_ENVIRONMENT: {

			int idx = view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			if (current) {

				camera->set_environment(RES());
			} else {

				camera->set_environment(SpatialEditor::get_singleton()->get_viewport_environment());
			}

			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_PERSPECTIVE: {

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), false);
			orthogonal = false;
			call_deferred("update_transform_gizmo_view");
			_update_name();

		} break;
		case VIEW_ORTHOGONAL: {

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), true);
			orthogonal = true;
			call_deferred("update_transform_gizmo_view");
			_update_name();

		} break;
		case VIEW_AUDIO_LISTENER: {

			int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			viewport->set_as_audio_listener(current);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_GIZMOS: {

			int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			if (current)
				camera->set_visible_layers(((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + index)) | (1 << GIZMO_EDIT_LAYER) | (1 << GIZMO_GRID_LAYER));
			else
				camera->set_visible_layers(((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + index)) | (1 << GIZMO_GRID_LAYER));
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_FPS: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_FPS);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);

		} break;
	}
}

void SpatialEditorViewport::_preview_exited_scene() {

	preview_camera->set_pressed(false);
	_toggle_camera_preview(false);
	view_menu->show();
}

void SpatialEditorViewport::_init_gizmo_instance(int p_idx) {

	uint32_t layer = 1 << (GIZMO_BASE_LAYER + p_idx); //|(1<<GIZMO_GRID_LAYER);

	for (int i = 0; i < 3; i++) {
		move_gizmo_instance[i] = VS::get_singleton()->instance_create();
		VS::get_singleton()->instance_set_base(move_gizmo_instance[i], spatial_editor->get_move_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(move_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_geometry_set_flag(move_gizmo_instance[i], VS::INSTANCE_FLAG_VISIBLE, false);
		//VS::get_singleton()->instance_geometry_set_flag(move_gizmo_instance[i],VS::INSTANCE_FLAG_DEPH_SCALE,true);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(move_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(move_gizmo_instance[i], layer);

		rotate_gizmo_instance[i] = VS::get_singleton()->instance_create();
		VS::get_singleton()->instance_set_base(rotate_gizmo_instance[i], spatial_editor->get_rotate_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(rotate_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[i], VS::INSTANCE_FLAG_VISIBLE, false);
		//VS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[i],VS::INSTANCE_FLAG_DEPH_SCALE,true);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(rotate_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(rotate_gizmo_instance[i], layer);
	}
}

void SpatialEditorViewport::_finish_gizmo_instances() {

	for (int i = 0; i < 3; i++) {
		VS::get_singleton()->free(move_gizmo_instance[i]);
		VS::get_singleton()->free(rotate_gizmo_instance[i]);
	}
}
void SpatialEditorViewport::_toggle_camera_preview(bool p_activate) {

	ERR_FAIL_COND(p_activate && !preview);
	ERR_FAIL_COND(!p_activate && !previewing);

	if (!p_activate) {

		previewing->disconnect("exit_tree", this, "_preview_exited_scene");
		previewing = NULL;
		VS::get_singleton()->viewport_attach_camera(viewport->get_viewport(), camera->get_camera()); //restore
		if (!preview)
			preview_camera->hide();
		view_menu->show();
		surface->update();

	} else {

		previewing = preview;
		previewing->connect("exit_tree", this, "_preview_exited_scene");
		VS::get_singleton()->viewport_attach_camera(viewport->get_viewport(), preview->get_camera()); //replace
		view_menu->hide();
		surface->update();
	}
}

void SpatialEditorViewport::_selection_result_pressed(int p_result) {

	if (selection_results.size() <= p_result)
		return;

	clicked = selection_results[p_result].item->get_instance_ID();

	if (clicked) {
		_select_clicked(clicked_wants_append, true);
		clicked = 0;
	}
}

void SpatialEditorViewport::_selection_menu_hide() {

	selection_results.clear();
	selection_menu->clear();
	selection_menu->set_size(Vector2(0, 0));
}

void SpatialEditorViewport::set_can_preview(Camera *p_preview) {

	preview = p_preview;

	if (!preview_camera->is_pressed()) {

		if (p_preview) {
			preview_camera->show();
		} else {
			preview_camera->hide();
		}
	}
}

void SpatialEditorViewport::update_transform_gizmo_view() {

	if (!is_visible())
		return;

	Transform xform = spatial_editor->get_gizmo_transform();

	Transform camera_xform = camera->get_transform();
	Vector3 camz = -camera_xform.get_basis().get_axis(2).normalized();
	Vector3 camy = -camera_xform.get_basis().get_axis(1).normalized();
	Plane p(camera_xform.origin, camz);
	float gizmo_d = Math::abs(p.distance_to(xform.origin));
	float d0 = camera->unproject_position(camera_xform.origin + camz * gizmo_d).y;
	float d1 = camera->unproject_position(camera_xform.origin + camz * gizmo_d + camy).y;
	float dd = Math::abs(d0 - d1);
	if (dd == 0)
		dd = 0.0001;

	float gsize = EditorSettings::get_singleton()->get("3d_editor/manipulator_gizmo_size");
	gizmo_scale = (gsize / Math::abs(dd));
	Vector3 scale = Vector3(1, 1, 1) * gizmo_scale;

	xform.basis.scale(scale);

	//xform.basis.scale(GIZMO_SCALE_DEFAULT*Vector3(1,1,1));

	for (int i = 0; i < 3; i++) {
		VisualServer::get_singleton()->instance_set_transform(move_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_geometry_set_flag(move_gizmo_instance[i], VS::INSTANCE_FLAG_VISIBLE, spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE));
		VisualServer::get_singleton()->instance_set_transform(rotate_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[i], VS::INSTANCE_FLAG_VISIBLE, spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE));
	}
}

void SpatialEditorViewport::set_state(const Dictionary &p_state) {

	cursor.pos = p_state["pos"];
	cursor.x_rot = p_state["x_rot"];
	cursor.y_rot = p_state["y_rot"];
	cursor.distance = p_state["distance"];
	bool env = p_state["use_environment"];
	bool orth = p_state["use_orthogonal"];
	if (orth)
		_menu_option(VIEW_ORTHOGONAL);
	else
		_menu_option(VIEW_PERSPECTIVE);
	if (env != camera->get_environment().is_valid())
		_menu_option(VIEW_ENVIRONMENT);
	if (p_state.has("listener")) {
		bool listener = p_state["listener"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER);
		viewport->set_as_audio_listener(listener);
		view_menu->get_popup()->set_item_checked(idx, listener);
	}

	if (p_state.has("previewing")) {
		Node *pv = EditorNode::get_singleton()->get_edited_scene()->get_node(p_state["previewing"]);
		if (pv && pv->cast_to<Camera>()) {
			previewing = pv->cast_to<Camera>();
			previewing->connect("exit_tree", this, "_preview_exited_scene");
			VS::get_singleton()->viewport_attach_camera(viewport->get_viewport(), previewing->get_camera()); //replace
			view_menu->hide();
			surface->update();
			preview_camera->set_pressed(true);
			preview_camera->show();
		}
	}
}

Dictionary SpatialEditorViewport::get_state() const {

	Dictionary d;
	d["pos"] = cursor.pos;
	d["x_rot"] = cursor.x_rot;
	d["y_rot"] = cursor.y_rot;
	d["distance"] = cursor.distance;
	d["use_environment"] = camera->get_environment().is_valid();
	d["use_orthogonal"] = camera->get_projection() == Camera::PROJECTION_ORTHOGONAL;
	d["listener"] = viewport->is_audio_listener();
	if (previewing) {
		d["previewing"] = EditorNode::get_singleton()->get_edited_scene()->get_path_to(previewing);
	}

	return d;
}

void SpatialEditorViewport::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_draw"), &SpatialEditorViewport::_draw);
	ObjectTypeDB::bind_method(_MD("_smouseenter"), &SpatialEditorViewport::_smouseenter);
	ObjectTypeDB::bind_method(_MD("_sinput"), &SpatialEditorViewport::_sinput);
	ObjectTypeDB::bind_method(_MD("_menu_option"), &SpatialEditorViewport::_menu_option);
	ObjectTypeDB::bind_method(_MD("_toggle_camera_preview"), &SpatialEditorViewport::_toggle_camera_preview);
	ObjectTypeDB::bind_method(_MD("_preview_exited_scene"), &SpatialEditorViewport::_preview_exited_scene);
	ObjectTypeDB::bind_method(_MD("update_transform_gizmo_view"), &SpatialEditorViewport::update_transform_gizmo_view);
	ObjectTypeDB::bind_method(_MD("_selection_result_pressed"), &SpatialEditorViewport::_selection_result_pressed);
	ObjectTypeDB::bind_method(_MD("_selection_menu_hide"), &SpatialEditorViewport::_selection_menu_hide);

	ADD_SIGNAL(MethodInfo("toggle_maximize_view", PropertyInfo(Variant::OBJECT, "viewport")));
}

void SpatialEditorViewport::reset() {

	orthogonal = false;
	message_time = 0;
	message = "";
	last_message = "";
	name = "";

	cursor.x_rot = 0.5;
	cursor.y_rot = 0.5;
	cursor.distance = 4;
	cursor.region_select = false;
	_update_name();
}

SpatialEditorViewport::SpatialEditorViewport(SpatialEditor *p_spatial_editor, EditorNode *p_editor, int p_index) {

	_edit.mode = TRANSFORM_NONE;
	_edit.plane = TRANSFORM_VIEW;
	_edit.edited_gizmo = 0;
	_edit.snap = 1;
	_edit.gizmo_handle = 0;

	index = p_index;
	editor = p_editor;
	editor_selection = editor->get_editor_selection();
	undo_redo = editor->get_undo_redo();
	clicked = 0;
	clicked_includes_current = false;
	orthogonal = false;
	message_time = 0;

	spatial_editor = p_spatial_editor;
	Control *c = memnew(Control);
	add_child(c);
	c->set_area_as_parent_rect();
	viewport = memnew(Viewport);
	viewport->set_disable_input(true);
	c->add_child(viewport);
	surface = memnew(Control);
	add_child(surface);
	surface->set_area_as_parent_rect();
	camera = memnew(Camera);
	camera->set_disable_gizmo(true);
	camera->set_visible_layers(((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + p_index)) | (1 << GIZMO_EDIT_LAYER) | (1 << GIZMO_GRID_LAYER));
	//camera->set_environment(SpatialEditor::get_singleton()->get_viewport_environment());
	viewport->add_child(camera);
	camera->make_current();
	surface->set_focus_mode(FOCUS_ALL);

	view_menu = memnew(MenuButton);
	surface->add_child(view_menu);
	view_menu->set_pos(Point2(4, 4));
	view_menu->set_self_opacity(0.5);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/top_view"), VIEW_TOP);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/bottom_view"), VIEW_BOTTOM);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/left_view"), VIEW_LEFT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/right_view"), VIEW_RIGHT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/front_view"), VIEW_FRONT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/rear_view"), VIEW_REAR);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_item(TTR("Perspective") + " (" + ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal")->get_as_text() + ")", VIEW_PERSPECTIVE);
	view_menu->get_popup()->add_check_item(TTR("Orthogonal") + " (" + ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal")->get_as_text() + ")", VIEW_ORTHOGONAL);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_environment", TTR("Environment")), VIEW_ENVIRONMENT);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_audio_listener", TTR("Audio Listener")), VIEW_AUDIO_LISTENER);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_gizmos", TTR("Gizmos")), VIEW_GIZMOS);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_GIZMOS), true);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_fps", TTR("View FPS")), VIEW_FPS);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_origin"), VIEW_CENTER_TO_ORIGIN);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_selection"), VIEW_CENTER_TO_SELECTION);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_selection_with_view"), VIEW_ALIGN_SELECTION_WITH_VIEW);
	view_menu->get_popup()->connect("item_pressed", this, "_menu_option");

	preview_camera = memnew(Button);
	preview_camera->set_toggle_mode(true);
	preview_camera->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, 90);
	preview_camera->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 10);
	preview_camera->set_text("preview");
	surface->add_child(preview_camera);
	preview_camera->hide();
	preview_camera->connect("toggled", this, "_toggle_camera_preview");
	previewing = NULL;
	preview = NULL;
	gizmo_scale = 1.0;

	// FPS Counter.
	fps = memnew(PanelContainer);
	fps->set_self_opacity(0.4f);
	surface->add_child(fps);
	fps_label = memnew(Label);
	fps->add_child(fps_label);
	fps->hide();

	selection_menu = memnew(PopupMenu);
	add_child(selection_menu);
	selection_menu->set_custom_minimum_size(Vector2(100, 0));
	selection_menu->connect("item_pressed", this, "_selection_result_pressed");
	selection_menu->connect("popup_hide", this, "_selection_menu_hide");

	if (p_index == 0) {
		view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER), true);
		viewport->set_as_audio_listener(true);
	}

	name = TTR("Top");
	_update_name();

	EditorSettings::get_singleton()->connect("settings_changed", this, "update_transform_gizmo_view");
}

SpatialEditor *SpatialEditor::singleton = NULL;

SpatialEditorSelectedItem::~SpatialEditorSelectedItem() {

	if (sbox_instance.is_valid())
		VisualServer::get_singleton()->free(sbox_instance);
}

void SpatialEditor::select_gizmo_highlight_axis(int p_axis) {

	for (int i = 0; i < 3; i++) {

		move_gizmo[i]->surface_set_material(0, i == p_axis ? gizmo_hl : gizmo_color[i]);
		rotate_gizmo[i]->surface_set_material(0, (i + 3) == p_axis ? gizmo_hl : gizmo_color[i]);
	}
}

void SpatialEditor::update_transform_gizmo() {

	List<Node *> &selection = editor_selection->get_selected_node_list();
	AABB center;
	bool first = true;

	Matrix3 gizmo_basis;
	bool local_gizmo_coords = transform_menu->get_popup()->is_item_checked(transform_menu->get_popup()->get_item_index(MENU_TRANSFORM_LOCAL_COORDS));

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

		Spatial *sp = E->get()->cast_to<Spatial>();
		if (!sp)
			continue;

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se)
			continue;

		Transform xf = se->sp->get_global_transform();
		if (first) {
			center.pos = xf.origin;
			first = false;
			if (local_gizmo_coords) {
				gizmo_basis = xf.basis;
				gizmo_basis.orthonormalize();
			}
		} else {
			center.expand_to(xf.origin);
			gizmo_basis = Matrix3();
		}
		//		count++;
	}

	Vector3 pcenter = center.pos + center.size * 0.5;
	gizmo.visible = !first;
	gizmo.transform.origin = pcenter;
	gizmo.transform.basis = gizmo_basis;

	for (int i = 0; i < 4; i++) {
		viewports[i]->update_transform_gizmo_view();
	}
}

Object *SpatialEditor::_get_editor_data(Object *p_what) {

	Spatial *sp = p_what->cast_to<Spatial>();
	if (!sp)
		return NULL;

	SpatialEditorSelectedItem *si = memnew(SpatialEditorSelectedItem);

	si->sp = sp;
	si->sbox_instance = VisualServer::get_singleton()->instance_create2(selection_box->get_rid(), sp->get_world()->get_scenario());
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(si->sbox_instance, VS::SHADOW_CASTING_SETTING_OFF);

	if (get_tree()->is_editor_hint())
		editor->call("edit_node", sp);

	return si;
}

void SpatialEditor::_generate_selection_box() {

	AABB aabb(Vector3(), Vector3(1, 1, 1));
	aabb.grow_by(aabb.get_longest_axis_size() / 20.0);

	Ref<SurfaceTool> st = memnew(SurfaceTool);

	st->begin(Mesh::PRIMITIVE_LINES);
	for (int i = 0; i < 12; i++) {

		Vector3 a, b;
		aabb.get_edge(i, a, b);

		/*Vector<Vector3> points;
		Vector<Color> colors;
		points.push_back(a);
		points.push_back(b);*/

		st->add_color(Color(1.0, 1.0, 0.8, 0.8));
		st->add_vertex(a);
		st->add_color(Color(1.0, 1.0, 0.8, 0.4));
		st->add_vertex(a.linear_interpolate(b, 0.2));

		st->add_color(Color(1.0, 1.0, 0.8, 0.4));
		st->add_vertex(a.linear_interpolate(b, 0.8));
		st->add_color(Color(1.0, 1.0, 0.8, 0.8));
		st->add_vertex(b);
	}

	Ref<FixedMaterial> mat = memnew(FixedMaterial);
	mat->set_flag(Material::FLAG_UNSHADED, true);
	mat->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1));
	mat->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	mat->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
	st->set_material(mat);
	selection_box = st->commit();
}

Dictionary SpatialEditor::get_state() const {

	Dictionary d;

	d["snap_enabled"] = snap_enabled;
	d["translate_snap"] = get_translate_snap();
	d["rotate_snap"] = get_rotate_snap();
	d["scale_snap"] = get_scale_snap();

	int local_coords_index = transform_menu->get_popup()->get_item_index(MENU_TRANSFORM_LOCAL_COORDS);
	d["local_coords"] = transform_menu->get_popup()->is_item_checked(local_coords_index);

	int vc = 0;
	if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT)))
		vc = 1;
	else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS)))
		vc = 2;
	else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS)))
		vc = 3;
	else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS)))
		vc = 4;
	else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT)))
		vc = 5;
	else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT)))
		vc = 6;

	d["viewport_mode"] = vc;
	Array vpdata;
	for (int i = 0; i < 4; i++) {
		vpdata.push_back(viewports[i]->get_state());
	}

	d["viewports"] = vpdata;

	d["default_light"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_DEFAULT_LIGHT));
	d["ambient_light_color"] = settings_ambient_color->get_color();

	d["default_srgb"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_DEFAULT_SRGB));
	d["show_grid"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID));
	d["show_origin"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN));
	d["fov"] = get_fov();
	d["znear"] = get_znear();
	d["zfar"] = get_zfar();
	d["deflight_rot_x"] = settings_default_light_rot_x;
	d["deflight_rot_y"] = settings_default_light_rot_y;

	return d;
}
void SpatialEditor::set_state(const Dictionary &p_state) {

	Dictionary d = p_state;

	if (d.has("snap_enabled")) {
		snap_enabled = d["snap_enabled"];
		int snap_enabled_idx = transform_menu->get_popup()->get_item_index(MENU_TRANSFORM_USE_SNAP);
		transform_menu->get_popup()->set_item_checked(snap_enabled_idx, snap_enabled);
	}

	if (d.has("translate_snap"))
		snap_translate->set_text(d["translate_snap"]);

	if (d.has("rotate_snap"))
		snap_rotate->set_text(d["rotate_snap"]);

	if (d.has("scale_snap"))
		snap_scale->set_text(d["scale_snap"]);

	if (d.has("local_coords")) {
		int local_coords_idx = transform_menu->get_popup()->get_item_index(MENU_TRANSFORM_LOCAL_COORDS);
		transform_menu->get_popup()->set_item_checked(local_coords_idx, d["local_coords"]);
		update_transform_gizmo();
	}

	if (d.has("viewport_mode")) {
		int vc = d["viewport_mode"];

		if (vc == 1)
			_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);
		else if (vc == 2)
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS);
		else if (vc == 3)
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS);
		else if (vc == 4)
			_menu_item_pressed(MENU_VIEW_USE_4_VIEWPORTS);
		else if (vc == 5)
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS_ALT);
		else if (vc == 6)
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS_ALT);
	}

	if (d.has("viewports")) {
		Array vp = d["viewports"];
		ERR_FAIL_COND(vp.size() > 4);

		for (int i = 0; i < 4; i++) {
			viewports[i]->set_state(vp[i]);
		}
	}

	if (d.has("zfar"))
		settings_zfar->set_val(float(d["zfar"]));
	if (d.has("znear"))
		settings_znear->set_val(float(d["znear"]));
	if (d.has("fov"))
		settings_fov->set_val(float(d["fov"]));

	if (d.has("default_light")) {
		bool use = d["default_light"];

		bool existing = light_instance.is_valid();
		if (use != existing) {
			if (existing) {
				VisualServer::get_singleton()->free(light_instance);
				light_instance = RID();
			} else {
				light_instance = VisualServer::get_singleton()->instance_create2(light, get_tree()->get_root()->get_world()->get_scenario());
				VisualServer::get_singleton()->instance_set_transform(light_instance, light_transform);
			}
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_DEFAULT_LIGHT), light_instance.is_valid());
		}
	}
	if (d.has("ambient_light_color")) {
		settings_ambient_color->set_color(d["ambient_light_color"]);
		viewport_environment->fx_set_param(Environment::FX_PARAM_AMBIENT_LIGHT_COLOR, d["ambient_light_color"]);
	}

	if (d.has("default_srgb")) {
		bool use = d["default_srgb"];

		viewport_environment->set_enable_fx(Environment::FX_SRGB, use);
		view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_DEFAULT_SRGB), use);
	}
	if (d.has("show_grid")) {
		bool use = d["show_grid"];

		if (use != view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID))) {
			_menu_item_pressed(MENU_VIEW_GRID);
		}
	}

	if (d.has("show_origin")) {
		bool use = d["show_origin"];

		if (use != view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN))) {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN), use);
			VisualServer::get_singleton()->instance_geometry_set_flag(origin_instance, VS::INSTANCE_FLAG_VISIBLE, use);
		}
	}

	if (d.has("deflight_rot_x"))
		settings_default_light_rot_x = d["deflight_rot_x"];
	if (d.has("deflight_rot_y"))
		settings_default_light_rot_y = d["deflight_rot_y"];

	_update_default_light_angle();
}

void SpatialEditor::edit(Spatial *p_spatial) {

	if (p_spatial != selected) {
		if (selected) {

			Ref<SpatialEditorGizmo> seg = selected->get_gizmo();
			if (seg.is_valid()) {
				seg->set_selected(false);
				selected->update_gizmo();
			}
		}

		selected = p_spatial;
		over_gizmo_handle = -1;

		if (selected) {

			Ref<SpatialEditorGizmo> seg = selected->get_gizmo();
			if (seg.is_valid()) {
				seg->set_selected(true);
				selected->update_gizmo();
			}
		}
	}

	if (p_spatial) {
		//_validate_selection();
		//if (selected.has(p_spatial->get_instance_ID()) && selected.size()==1)
		//	return;
		//_select(p_spatial->get_instance_ID(),false,true);

		// should become the selection
	}
}

void SpatialEditor::_xform_dialog_action() {

	Transform t;
	//translation
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;

	for (int i = 0; i < 3; i++) {
		translate[i] = xform_translate[i]->get_text().to_double();
		rotate[i] = Math::deg2rad(xform_rotate[i]->get_text().to_double());
		scale[i] = xform_scale[i]->get_text().to_double();
	}

	t.origin = translate;
	for (int i = 0; i < 3; i++) {
		if (!rotate[i])
			continue;
		Vector3 axis;
		axis[i] = 1.0;
		t.basis.rotate(axis, rotate[i]);
	}

	for (int i = 0; i < 3; i++) {
		if (scale[i] == 1)
			continue;
		t.basis.set_axis(i, t.basis.get_axis(i) * scale[i]);
	}

	undo_redo->create_action(TTR("XForm Dialog"));

	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {

		Spatial *sp = E->get()->cast_to<Spatial>();
		if (!sp)
			continue;

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se)
			continue;

		bool post = xform_type->get_selected() > 0;

		Transform tr = sp->get_global_transform();
		if (post)
			tr = tr * t;
		else {

			tr.basis = t.basis * tr.basis;
			tr.origin += t.origin;
		}

		undo_redo->add_do_method(sp, "set_global_transform", tr);
		undo_redo->add_undo_method(sp, "set_global_transform", sp->get_global_transform());
	}
	undo_redo->commit_action();
}

void SpatialEditor::_menu_item_pressed(int p_option) {

	switch (p_option) {

		case MENU_TOOL_SELECT:
		case MENU_TOOL_MOVE:
		case MENU_TOOL_ROTATE:
		case MENU_TOOL_SCALE:
		case MENU_TOOL_LIST_SELECT: {

			for (int i = 0; i < TOOL_MAX; i++)
				tool_button[i]->set_pressed(i == p_option);
			tool_mode = (ToolMode)p_option;

			//		static const char *_mode[]={"Selection Mode.","Translation Mode.","Rotation Mode.","Scale Mode.","List Selection Mode."};
			//			set_message(_mode[p_option],3);
			update_transform_gizmo();

		} break;
		case MENU_TRANSFORM_USE_SNAP: {

			bool is_checked = transform_menu->get_popup()->is_item_checked(transform_menu->get_popup()->get_item_index(p_option));
			snap_enabled = !is_checked;
			transform_menu->get_popup()->set_item_checked(transform_menu->get_popup()->get_item_index(p_option), snap_enabled);
		} break;
		case MENU_TRANSFORM_CONFIGURE_SNAP: {

			snap_dialog->popup_centered(Size2(200, 180));
		} break;
		case MENU_TRANSFORM_LOCAL_COORDS: {

			bool is_checked = transform_menu->get_popup()->is_item_checked(transform_menu->get_popup()->get_item_index(p_option));
			transform_menu->get_popup()->set_item_checked(transform_menu->get_popup()->get_item_index(p_option), !is_checked);
			update_transform_gizmo();

		} break;
		case MENU_TRANSFORM_DIALOG: {

			for (int i = 0; i < 3; i++) {

				xform_translate[i]->set_text("0");
				xform_rotate[i]->set_text("0");
				xform_scale[i]->set_text("1");
			}

			xform_dialog->popup_centered(Size2(200, 200));

		} break;
		case MENU_VIEW_USE_DEFAULT_LIGHT: {

			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			if (is_checked) {
				VisualServer::get_singleton()->free(light_instance);
				light_instance = RID();
			} else {
				light_instance = VisualServer::get_singleton()->instance_create2(light, get_tree()->get_root()->get_world()->get_scenario());
				VisualServer::get_singleton()->instance_set_transform(light_instance, light_transform);

				_update_default_light_angle();
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), light_instance.is_valid());

		} break;
		case MENU_VIEW_USE_DEFAULT_SRGB: {

			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			if (is_checked) {
				viewport_environment->set_enable_fx(Environment::FX_SRGB, false);
			} else {
				viewport_environment->set_enable_fx(Environment::FX_SRGB, true);
			}

			is_checked = !is_checked;

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), is_checked);

		} break;
		case MENU_VIEW_USE_1_VIEWPORT: {

			for (int i = 1; i < 4; i++) {

				viewports[i]->hide();
			}

			viewports[0]->set_area_as_parent_rect();

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS: {

			for (int i = 1; i < 4; i++) {

				if (i == 1 || i == 3)
					viewports[i]->hide();
				else
					viewports[i]->show();
			}
			viewports[0]->set_area_as_parent_rect();
			viewports[0]->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_RATIO, 0.5);
			viewports[2]->set_area_as_parent_rect();
			viewports[2]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS_ALT: {

			for (int i = 1; i < 4; i++) {

				if (i == 1 || i == 3)
					viewports[i]->hide();
				else
					viewports[i]->show();
			}
			viewports[0]->set_area_as_parent_rect();
			viewports[0]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[2]->set_area_as_parent_rect();
			viewports[2]->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_RATIO, 0.5);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS: {

			for (int i = 1; i < 4; i++) {

				if (i == 1)
					viewports[i]->hide();
				else
					viewports[i]->show();
			}
			viewports[0]->set_area_as_parent_rect();
			viewports[0]->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_RATIO, 0.5);
			viewports[2]->set_area_as_parent_rect();
			viewports[2]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[2]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);
			viewports[3]->set_area_as_parent_rect();
			viewports[3]->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_RATIO, 0.5);
			viewports[3]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS_ALT: {

			for (int i = 1; i < 4; i++) {

				if (i == 1)
					viewports[i]->hide();
				else
					viewports[i]->show();
			}
			viewports[0]->set_area_as_parent_rect();
			viewports[0]->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_RATIO, 0.5);
			viewports[0]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[2]->set_area_as_parent_rect();
			viewports[2]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);
			viewports[2]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[3]->set_area_as_parent_rect();
			viewports[3]->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_RATIO, 0.5);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), true);

		} break;
		case MENU_VIEW_USE_4_VIEWPORTS: {

			for (int i = 1; i < 4; i++) {

				viewports[i]->show();
			}
			viewports[0]->set_area_as_parent_rect();
			viewports[0]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[0]->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_RATIO, 0.5);
			viewports[1]->set_area_as_parent_rect();
			viewports[1]->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_RATIO, 0.5);
			viewports[1]->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_RATIO, 0.5);
			viewports[2]->set_area_as_parent_rect();
			viewports[2]->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_RATIO, 0.5);
			viewports[2]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);
			viewports[3]->set_area_as_parent_rect();
			viewports[3]->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_RATIO, 0.5);
			viewports[3]->set_anchor_and_margin(MARGIN_TOP, ANCHOR_RATIO, 0.5);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_DISPLAY_NORMAL: {

			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_DISABLED);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_NORMAL), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_SHADELESS), false);

		} break;
		case MENU_VIEW_DISPLAY_WIREFRAME: {

			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_WIREFRAME);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_WIREFRAME), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_SHADELESS), false);

		} break;
		case MENU_VIEW_DISPLAY_OVERDRAW: {

			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_OVERDRAW);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_OVERDRAW), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_SHADELESS), false);

		} break;
		case MENU_VIEW_DISPLAY_SHADELESS: {

			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_SHADELESS);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_SHADELESS), true);

		} break;
		case MENU_VIEW_ORIGIN: {

			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			is_checked = !is_checked;
			VisualServer::get_singleton()->instance_geometry_set_flag(origin_instance, VS::INSTANCE_FLAG_VISIBLE, is_checked);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), is_checked);
		} break;
		case MENU_VIEW_GRID: {

			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			grid_enabled = !is_checked;

			for (int i = 0; i < 3; ++i) {
				if (grid_enable[i]) {
					VisualServer::get_singleton()->instance_geometry_set_flag(grid_instance[i], VS::INSTANCE_FLAG_VISIBLE, grid_enabled);
					grid_visible[i] = grid_enabled;
				}
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), grid_enabled);

		} break;
		case MENU_VIEW_CAMERA_SETTINGS: {

			settings_dialog->popup_centered(settings_vbc->get_combined_minimum_size() + Size2(50, 50));
		} break;
	}
}

void SpatialEditor::_init_indicators() {

	//make sure that the camera indicator is not selectable
	light = VisualServer::get_singleton()->light_create(VisualServer::LIGHT_DIRECTIONAL);
	//VisualServer::get_singleton()->light_set_shadow( light, true );
	light_instance = VisualServer::get_singleton()->instance_create2(light, get_tree()->get_root()->get_world()->get_scenario());

	light_transform.rotate(Vector3(1, 0, 0), Math_PI / 5.0);
	VisualServer::get_singleton()->instance_set_transform(light_instance, light_transform);

	//RID mat = VisualServer::get_singleton()->fixed_material_create();
	///VisualServer::get_singleton()->fixed_material_set_flag(mat, VisualServer::FIXED_MATERIAL_FLAG_USE_ALPHA,true);
	//VisualServer::get_singleton()->fixed_material_set_flag(mat, VisualServer::FIXED_MATERIAL_FLAG_USE_COLOR_ARRAY,true);

	{

		indicator_mat = VisualServer::get_singleton()->fixed_material_create();
		VisualServer::get_singleton()->material_set_flag(indicator_mat, VisualServer::MATERIAL_FLAG_UNSHADED, true);
		VisualServer::get_singleton()->material_set_flag(indicator_mat, VisualServer::MATERIAL_FLAG_ONTOP, false);
		VisualServer::get_singleton()->fixed_material_set_flag(indicator_mat, VisualServer::FIXED_MATERIAL_FLAG_USE_ALPHA, true);
		VisualServer::get_singleton()->fixed_material_set_flag(indicator_mat, VisualServer::FIXED_MATERIAL_FLAG_USE_COLOR_ARRAY, true);

		DVector<Color> grid_colors[3];
		DVector<Vector3> grid_points[3];
		Vector<Color> origin_colors;
		Vector<Vector3> origin_points;

		Color grid_color = EditorSettings::get_singleton()->get("3d_editor/grid_color");

		for (int i = 0; i < 3; i++) {
			Vector3 axis;
			axis[i] = 1;
			Vector3 axis_n1;
			axis_n1[(i + 1) % 3] = 1;
			Vector3 axis_n2;
			axis_n2[(i + 2) % 3] = 1;

			origin_colors.push_back(Color(axis.x, axis.y, axis.z));
			origin_colors.push_back(Color(axis.x, axis.y, axis.z));
			origin_points.push_back(axis * 4096);
			origin_points.push_back(axis * -4096);
#define ORIGIN_GRID_SIZE 25

			for (int j = -ORIGIN_GRID_SIZE; j <= ORIGIN_GRID_SIZE; j++) {

				grid_colors[i].push_back(grid_color);
				grid_colors[i].push_back(grid_color);
				grid_colors[i].push_back(grid_color);
				grid_colors[i].push_back(grid_color);
				grid_points[i].push_back(axis_n1 * ORIGIN_GRID_SIZE + axis_n2 * j);
				grid_points[i].push_back(-axis_n1 * ORIGIN_GRID_SIZE + axis_n2 * j);
				grid_points[i].push_back(axis_n2 * ORIGIN_GRID_SIZE + axis_n1 * j);
				grid_points[i].push_back(-axis_n2 * ORIGIN_GRID_SIZE + axis_n1 * j);
			}

			grid[i] = VisualServer::get_singleton()->mesh_create();
			Array d;
			d.resize(VS::ARRAY_MAX);
			d[VisualServer::ARRAY_VERTEX] = grid_points[i];
			d[VisualServer::ARRAY_COLOR] = grid_colors[i];
			VisualServer::get_singleton()->mesh_add_surface(grid[i], VisualServer::PRIMITIVE_LINES, d);
			VisualServer::get_singleton()->mesh_surface_set_material(grid[i], 0, indicator_mat);
			grid_instance[i] = VisualServer::get_singleton()->instance_create2(grid[i], get_tree()->get_root()->get_world()->get_scenario());

			grid_visible[i] = false;
			grid_enable[i] = false;
			VisualServer::get_singleton()->instance_geometry_set_flag(grid_instance[i], VS::INSTANCE_FLAG_VISIBLE, false);
			VisualServer::get_singleton()->instance_geometry_set_cast_shadows_setting(grid_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
			VS::get_singleton()->instance_set_layer_mask(grid_instance[i], 1 << SpatialEditorViewport::GIZMO_GRID_LAYER);
		}

		origin = VisualServer::get_singleton()->mesh_create();
		Array d;
		d.resize(VS::ARRAY_MAX);
		d[VisualServer::ARRAY_VERTEX] = origin_points;
		d[VisualServer::ARRAY_COLOR] = origin_colors;

		VisualServer::get_singleton()->mesh_add_surface(origin, VisualServer::PRIMITIVE_LINES, d);
		VisualServer::get_singleton()->mesh_surface_set_material(origin, 0, indicator_mat);

		//		origin = VisualServer::get_singleton()->poly_create();
		//		VisualServer::get_singleton()->poly_add_primitive(origin,origin_points,Vector<Vector3>(),origin_colors,Vector<Vector3>());
		//		VisualServer::get_singleton()->poly_set_material(origin,indicator_mat,true);
		origin_instance = VisualServer::get_singleton()->instance_create2(origin, get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_layer_mask(origin_instance, 1 << SpatialEditorViewport::GIZMO_GRID_LAYER);

		VisualServer::get_singleton()->instance_geometry_set_cast_shadows_setting(origin_instance, VS::SHADOW_CASTING_SETTING_OFF);

		VisualServer::get_singleton()->instance_geometry_set_flag(grid_instance[1], VS::INSTANCE_FLAG_VISIBLE, true);
		grid_enable[1] = true;
		grid_visible[1] = true;
		grid_enabled = true;
		last_grid_snap = 1;
	}

	{
		cursor_mesh = VisualServer::get_singleton()->mesh_create();
		DVector<Vector3> cursor_points;
		float cs = 0.25;
		cursor_points.push_back(Vector3(+cs, 0, 0));
		cursor_points.push_back(Vector3(-cs, 0, 0));
		cursor_points.push_back(Vector3(0, +cs, 0));
		cursor_points.push_back(Vector3(0, -cs, 0));
		cursor_points.push_back(Vector3(0, 0, +cs));
		cursor_points.push_back(Vector3(0, 0, -cs));
		cursor_material = VisualServer::get_singleton()->fixed_material_create();
		VisualServer::get_singleton()->fixed_material_set_param(cursor_material, VS::FIXED_MATERIAL_PARAM_DIFFUSE, Color(0, 1, 1));
		VisualServer::get_singleton()->material_set_flag(cursor_material, VisualServer::MATERIAL_FLAG_UNSHADED, true);
		VisualServer::get_singleton()->fixed_material_set_flag(cursor_material, VisualServer::FIXED_MATERIAL_FLAG_USE_ALPHA, true);
		VisualServer::get_singleton()->fixed_material_set_flag(cursor_material, VisualServer::FIXED_MATERIAL_FLAG_USE_COLOR_ARRAY, true);

		Array d;
		d.resize(VS::ARRAY_MAX);
		d[VS::ARRAY_VERTEX] = cursor_points;
		VisualServer::get_singleton()->mesh_add_surface(cursor_mesh, VS::PRIMITIVE_LINES, d);
		VisualServer::get_singleton()->mesh_surface_set_material(cursor_mesh, 0, cursor_material);

		cursor_instance = VisualServer::get_singleton()->instance_create2(cursor_mesh, get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_layer_mask(cursor_instance, 1 << SpatialEditorViewport::GIZMO_GRID_LAYER);

		VisualServer::get_singleton()->instance_geometry_set_cast_shadows_setting(cursor_instance, VS::SHADOW_CASTING_SETTING_OFF);
	}

	{

		//move gizmo

		float gizmo_alph = EditorSettings::get_singleton()->get("3d_editor/manipulator_gizmo_opacity");

		gizmo_hl = Ref<FixedMaterial>(memnew(FixedMaterial));
		gizmo_hl->set_flag(Material::FLAG_UNSHADED, true);
		gizmo_hl->set_flag(Material::FLAG_ONTOP, true);
		gizmo_hl->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
		gizmo_hl->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, gizmo_alph + 0.2f));

		for (int i = 0; i < 3; i++) {

			move_gizmo[i] = Ref<Mesh>(memnew(Mesh));
			rotate_gizmo[i] = Ref<Mesh>(memnew(Mesh));

			Ref<FixedMaterial> mat = memnew(FixedMaterial);
			mat->set_flag(Material::FLAG_UNSHADED, true);
			mat->set_flag(Material::FLAG_ONTOP, true);
			mat->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
			Color col;
			col[i] = 1.0;
			col.a = gizmo_alph;
			mat->set_parameter(FixedMaterial::PARAM_DIFFUSE, col);
			gizmo_color[i] = mat;

			Vector3 ivec;
			ivec[i] = 1;
			Vector3 nivec;
			nivec[(i + 1) % 3] = 1;
			nivec[(i + 2) % 3] = 1;
			Vector3 ivec2;
			ivec2[(i + 1) % 3] = 1;
			Vector3 ivec3;
			ivec3[(i + 2) % 3] = 1;

			{

				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				//translate

				const int arrow_points = 5;
				Vector3 arrow[5] = {
					nivec * 0.0 + ivec * 0.0,
					nivec * 0.01 + ivec * 0.0,
					nivec * 0.01 + ivec * 1.0,
					nivec * 0.1 + ivec * 1.0,
					nivec * 0.0 + ivec * (1 + GIZMO_ARROW_SIZE),
				};

				int arrow_sides = 6;

				for (int k = 0; k < 7; k++) {

					Matrix3 ma(ivec, Math_PI * 2 * float(k) / arrow_sides);
					Matrix3 mb(ivec, Math_PI * 2 * float(k + 1) / arrow_sides);

					for (int j = 0; j < arrow_points - 1; j++) {

						Vector3 points[4] = {
							ma.xform(arrow[j]),
							mb.xform(arrow[j]),
							mb.xform(arrow[j + 1]),
							ma.xform(arrow[j + 1]),
						};
						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[1]);
						surftool->add_vertex(points[2]);

						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[2]);
						surftool->add_vertex(points[3]);
					}
				}

				surftool->set_material(mat);
				surftool->commit(move_gizmo[i]);
			}

			{

				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				Vector3 circle[5] = {
					ivec * 0.02 + ivec2 * 0.02 + ivec2 * 1.0,
					ivec * -0.02 + ivec2 * 0.02 + ivec2 * 1.0,
					ivec * -0.02 + ivec2 * -0.02 + ivec2 * 1.0,
					ivec * 0.02 + ivec2 * -0.02 + ivec2 * 1.0,
					ivec * 0.02 + ivec2 * 0.02 + ivec2 * 1.0,
				};

				for (int k = 0; k < 33; k++) {

					Matrix3 ma(ivec, Math_PI * 2 * float(k) / 32);
					Matrix3 mb(ivec, Math_PI * 2 * float(k + 1) / 32);

					for (int j = 0; j < 4; j++) {

						Vector3 points[4] = {
							ma.xform(circle[j]),
							mb.xform(circle[j]),
							mb.xform(circle[j + 1]),
							ma.xform(circle[j + 1]),
						};
						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[1]);
						surftool->add_vertex(points[2]);

						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[2]);
						surftool->add_vertex(points[3]);
					}
				}

				surftool->set_material(mat);
				surftool->commit(rotate_gizmo[i]);
			}
		}
	}

	/*for(int i=0;i<4;i++) {

		viewports[i]->init_gizmo_instance(i);
	}*/

	_generate_selection_box();

	//get_scene()->get_root_node()->cast_to<EditorNode>()->get_scene_root()->add_child(camera);

	//current_camera=camera;
}

void SpatialEditor::_finish_indicators() {

	VisualServer::get_singleton()->free(origin_instance);
	VisualServer::get_singleton()->free(origin);
	for (int i = 0; i < 3; i++) {
		VisualServer::get_singleton()->free(grid_instance[i]);
		VisualServer::get_singleton()->free(grid[i]);
	}
	VisualServer::get_singleton()->free(light_instance);
	VisualServer::get_singleton()->free(light);
	//VisualServer::get_singleton()->free(poly);
	//VisualServer::get_singleton()->free(indicators_instance);
	//VisualServer::get_singleton()->free(indicators);

	VisualServer::get_singleton()->free(cursor_instance);
	VisualServer::get_singleton()->free(cursor_mesh);
	VisualServer::get_singleton()->free(indicator_mat);
	VisualServer::get_singleton()->free(cursor_material);
}

void SpatialEditor::_instance_scene() {
#if 0
	EditorNode *en = get_scene()->get_root_node()->cast_to<EditorNode>();
	ERR_FAIL_COND(!en);
	String path = en->get_filesystem_dock()->get_selected_path();
	if (path=="") {
		set_message(TTR("No scene selected to instance!"));
		return;
	}

	undo_redo->create_action(TTR("Instance at Cursor"));

	Node* scene = en->request_instance_scene(path);

	if (!scene) {
		set_message(TTR("Could not instance scene!"));
		undo_redo->commit_action(); //bleh
		return;
	}

	Spatial *s = scene->cast_to<Spatial>();
	if (s) {

		undo_redo->add_do_method(s,"set_global_transform",Transform(Matrix3(),cursor.cursor_pos));
	}

	undo_redo->commit_action();
#endif
}

void SpatialEditor::_unhandled_key_input(InputEvent p_event) {

	if (!is_visible() || get_viewport()->gui_has_modal_stack())
		return;

	{

		EditorNode *en = editor;
		EditorPluginList *over_plugin_list = en->get_editor_plugins_over();

		if (!over_plugin_list->empty() && over_plugin_list->forward_input_event(p_event)) {

			return; //ate the over input event
		}
	}

	switch (p_event.type) {

		case InputEvent::KEY: {

			const InputEventKey &k = p_event.key;

			if (!k.pressed)
				break;

			switch (k.scancode) {

				case KEY_Q: _menu_item_pressed(MENU_TOOL_SELECT); break;
				case KEY_W: _menu_item_pressed(MENU_TOOL_MOVE); break;
				case KEY_E: _menu_item_pressed(MENU_TOOL_ROTATE); break;
				case KEY_R: _menu_item_pressed(MENU_TOOL_SCALE); break;

				case KEY_Z: {
					if (k.mod.shift || k.mod.control || k.mod.command)
						break;

					if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_DISPLAY_WIREFRAME))) {
						_menu_item_pressed(MENU_VIEW_DISPLAY_NORMAL);
					} else {
						_menu_item_pressed(MENU_VIEW_DISPLAY_WIREFRAME);
					}
				} break;

#if 0
#endif
			}

		} break;
	}
}
void SpatialEditor::_notification(int p_what) {

	if (p_what == NOTIFICATION_READY) {

		tool_button[SpatialEditor::TOOL_MODE_SELECT]->set_icon(get_icon("ToolSelect", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_MOVE]->set_icon(get_icon("ToolMove", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_ROTATE]->set_icon(get_icon("ToolRotate", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_SCALE]->set_icon(get_icon("ToolScale", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_LIST_SELECT]->set_icon(get_icon("ListSelect", "EditorIcons"));
		instance_button->set_icon(get_icon("SpatialAdd", "EditorIcons"));
		instance_button->hide();

		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), get_icon("Panels1", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), get_icon("Panels2", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), get_icon("Panels2Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), get_icon("Panels3", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), get_icon("Panels3Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), get_icon("Panels4", "EditorIcons"));

		_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);

		get_tree()->connect("node_removed", this, "_node_removed");
		VS::get_singleton()->scenario_set_fallback_environment(get_viewport()->find_world()->get_scenario(), viewport_environment->get_rid());
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {

		gizmos = memnew(SpatialEditorGizmos);
		_init_indicators();
		_update_default_light_angle();
	}

	if (p_what == NOTIFICATION_EXIT_TREE) {

		_finish_indicators();
		memdelete(gizmos);
	}
}

void SpatialEditor::add_control_to_menu_panel(Control *p_control) {

	hbc_menu->add_child(p_control);
}

void SpatialEditor::set_can_preview(Camera *p_preview) {

	for (int i = 0; i < 4; i++) {
		viewports[i]->set_can_preview(p_preview);
	}
}

VSplitContainer *SpatialEditor::get_shader_split() {

	return shader_split;
}

HSplitContainer *SpatialEditor::get_palette_split() {

	return palette_split;
}

void SpatialEditor::_request_gizmo(Object *p_obj) {

	Spatial *sp = p_obj->cast_to<Spatial>();
	if (!sp)
		return;
	if (editor->get_edited_scene() && (sp == editor->get_edited_scene() || sp->get_owner() == editor->get_edited_scene() || editor->get_edited_scene()->is_editable_instance(sp->get_owner()))) {

		Ref<SpatialEditorGizmo> seg;

		for (int i = 0; i < EditorNode::get_singleton()->get_editor_data().get_editor_plugin_count(); i++) {

			seg = EditorNode::get_singleton()->get_editor_data().get_editor_plugin(i)->create_spatial_gizmo(sp);
			if (seg.is_valid())
				break;
		}

		if (!seg.is_valid()) {
			seg = gizmos->get_gizmo(sp);
		}

		if (seg.is_valid()) {
			sp->set_gizmo(seg);
		}

		if (seg.is_valid() && sp == selected) {
			seg->set_selected(true);
			selected->update_gizmo();
		}
	}
}

void SpatialEditor::_toggle_maximize_view(Object *p_viewport) {
	if (!p_viewport) return;
	SpatialEditorViewport *current_viewport = p_viewport->cast_to<SpatialEditorViewport>();
	if (!current_viewport) return;

	int index = -1;
	bool maximized = false;
	for (int i = 0; i < 4; i++) {
		if (viewports[i] == current_viewport) {
			index = i;
			if (current_viewport->get_global_rect() == viewport_base->get_global_rect())
				maximized = true;
			break;
		}
	}
	if (index == -1) return;

	if (!maximized) {

		for (int i = 0; i < 4; i++) {
			if (i == index)
				viewports[i]->set_area_as_parent_rect();
			else
				viewports[i]->hide();
		}
	} else {

		for (int i = 0; i < 4; i++)
			viewports[i]->show();

		if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT)))
			_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);
		else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS)))
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS);
		else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT)))
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS_ALT);
		else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS)))
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS);
		else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT)))
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS_ALT);
		else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS)))
			_menu_item_pressed(MENU_VIEW_USE_4_VIEWPORTS);
	}
}

void SpatialEditor::_node_removed(Node *p_node) {

	if (p_node == selected)
		selected = NULL;
}

void SpatialEditor::_bind_methods() {

	//	ObjectTypeDB::bind_method("_input_event",&SpatialEditor::_input_event);
	ObjectTypeDB::bind_method("_unhandled_key_input", &SpatialEditor::_unhandled_key_input);
	ObjectTypeDB::bind_method("_node_removed", &SpatialEditor::_node_removed);
	ObjectTypeDB::bind_method("_menu_item_pressed", &SpatialEditor::_menu_item_pressed);
	ObjectTypeDB::bind_method("_xform_dialog_action", &SpatialEditor::_xform_dialog_action);
	ObjectTypeDB::bind_method("_instance_scene", &SpatialEditor::_instance_scene);
	ObjectTypeDB::bind_method("_get_editor_data", &SpatialEditor::_get_editor_data);
	ObjectTypeDB::bind_method("_request_gizmo", &SpatialEditor::_request_gizmo);
	ObjectTypeDB::bind_method("_default_light_angle_input", &SpatialEditor::_default_light_angle_input);
	ObjectTypeDB::bind_method("_update_ambient_light_color", &SpatialEditor::_update_ambient_light_color);
	ObjectTypeDB::bind_method("_toggle_maximize_view", &SpatialEditor::_toggle_maximize_view);

	ADD_SIGNAL(MethodInfo("transform_key_request"));
}

void SpatialEditor::clear() {

	settings_fov->set_val(EDITOR_DEF("3d_editor/default_fov", 55.0));
	settings_znear->set_val(EDITOR_DEF("3d_editor/default_z_near", 0.1));
	settings_zfar->set_val(EDITOR_DEF("3d_editor/default_z_far", 1500.0));

	for (int i = 0; i < 4; i++) {
		viewports[i]->reset();
	}

	_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);
	_menu_item_pressed(MENU_VIEW_DISPLAY_NORMAL);

	VisualServer::get_singleton()->instance_geometry_set_flag(origin_instance, VS::INSTANCE_FLAG_VISIBLE, true);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN), true);
	for (int i = 0; i < 3; ++i) {
		if (grid_enable[i]) {
			VisualServer::get_singleton()->instance_geometry_set_flag(grid_instance[i], VS::INSTANCE_FLAG_VISIBLE, true);
			grid_visible[i] = true;
		}
	}

	for (int i = 0; i < 4; i++) {

		viewports[i]->view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(SpatialEditorViewport::VIEW_AUDIO_LISTENER), i == 0);
		viewports[i]->viewport->set_as_audio_listener(i == 0);
	}

	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID), true);

	settings_default_light_rot_x = Math_PI * 0.3;
	settings_default_light_rot_y = Math_PI * 0.2;

	viewport_environment->fx_set_param(Environment::FX_PARAM_AMBIENT_LIGHT_COLOR, Color(0.15, 0.15, 0.15));
	settings_ambient_color->set_color(Color(0.15, 0.15, 0.15));
	if (!light_instance.is_valid())
		_menu_item_pressed(MENU_VIEW_USE_DEFAULT_LIGHT);

	_update_default_light_angle();
}

void SpatialEditor::_update_ambient_light_color(const Color &p_color) {

	viewport_environment->fx_set_param(Environment::FX_PARAM_AMBIENT_LIGHT_COLOR, settings_ambient_color->get_color());
}

void SpatialEditor::_update_default_light_angle() {

	Transform t;
	t.basis.rotate(Vector3(1, 0, 0), settings_default_light_rot_x);
	t.basis.rotate(Vector3(0, 1, 0), settings_default_light_rot_y);
	settings_dlight->set_transform(t);
	if (light_instance.is_valid()) {
		VS::get_singleton()->instance_set_transform(light_instance, t);
	}
}

void SpatialEditor::_default_light_angle_input(const InputEvent &p_event) {

	if (p_event.type == InputEvent::MOUSE_MOTION && p_event.mouse_motion.button_mask & (0x1 | 0x2 | 0x4)) {

		settings_default_light_rot_y = Math::fposmod(settings_default_light_rot_y - p_event.mouse_motion.relative_x * 0.01, Math_PI * 2.0);
		settings_default_light_rot_x = Math::fposmod(settings_default_light_rot_x - p_event.mouse_motion.relative_y * 0.01, Math_PI * 2.0);
		_update_default_light_angle();
	}
}

SpatialEditor::SpatialEditor(EditorNode *p_editor) {

	gizmo.visible = true;
	gizmo.scale = 1.0;

	viewport_environment = Ref<Environment>(memnew(Environment));
	undo_redo = p_editor->get_undo_redo();
	VBoxContainer *vbc = this;

	custom_camera = NULL;
	singleton = this;
	editor = p_editor;
	editor_selection = editor->get_editor_selection();
	editor_selection->add_editor_plugin(this);

	snap_enabled = false;
	tool_mode = TOOL_MODE_SELECT;

	//set_focus_mode(FOCUS_ALL);

	hbc_menu = memnew(HBoxContainer);
	vbc->add_child(hbc_menu);

	Vector<Variant> button_binds;
	button_binds.resize(1);

	tool_button[TOOL_MODE_SELECT] = memnew(ToolButton);
	hbc_menu->add_child(tool_button[TOOL_MODE_SELECT]);
	tool_button[TOOL_MODE_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SELECT]->set_flat(true);
	tool_button[TOOL_MODE_SELECT]->set_pressed(true);
	button_binds[0] = MENU_TOOL_SELECT;
	tool_button[TOOL_MODE_SELECT]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_SELECT]->set_tooltip("Select Mode (Q)\n" + keycode_get_string(KEY_MASK_CMD) + "Drag: Rotate\nAlt+Drag: Move\nAlt+RMB: Depth list selection");

	tool_button[TOOL_MODE_MOVE] = memnew(ToolButton);

	hbc_menu->add_child(tool_button[TOOL_MODE_MOVE]);
	tool_button[TOOL_MODE_MOVE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_MOVE]->set_flat(true);
	button_binds[0] = MENU_TOOL_MOVE;
	tool_button[TOOL_MODE_MOVE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_MOVE]->set_tooltip(TTR("Move Mode (W)"));

	tool_button[TOOL_MODE_ROTATE] = memnew(ToolButton);
	hbc_menu->add_child(tool_button[TOOL_MODE_ROTATE]);
	tool_button[TOOL_MODE_ROTATE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_ROTATE]->set_flat(true);
	button_binds[0] = MENU_TOOL_ROTATE;
	tool_button[TOOL_MODE_ROTATE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_ROTATE]->set_tooltip(TTR("Rotate Mode (E)"));

	tool_button[TOOL_MODE_SCALE] = memnew(ToolButton);
	hbc_menu->add_child(tool_button[TOOL_MODE_SCALE]);
	tool_button[TOOL_MODE_SCALE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SCALE]->set_flat(true);
	button_binds[0] = MENU_TOOL_SCALE;
	tool_button[TOOL_MODE_SCALE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_SCALE]->set_tooltip(TTR("Scale Mode (R)"));

	instance_button = memnew(Button);
	hbc_menu->add_child(instance_button);
	instance_button->set_flat(true);
	instance_button->connect("pressed", this, "_instance_scene");
	instance_button->hide();

	VSeparator *vs = memnew(VSeparator);
	hbc_menu->add_child(vs);

	tool_button[TOOL_MODE_LIST_SELECT] = memnew(ToolButton);
	hbc_menu->add_child(tool_button[TOOL_MODE_LIST_SELECT]);
	tool_button[TOOL_MODE_LIST_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_LIST_SELECT]->set_flat(true);
	button_binds[0] = MENU_TOOL_LIST_SELECT;
	tool_button[TOOL_MODE_LIST_SELECT]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_LIST_SELECT]->set_tooltip(TTR("Show a list of all objects at the position clicked\n(same as Alt+RMB in select mode)."));

	vs = memnew(VSeparator);
	hbc_menu->add_child(vs);

	ED_SHORTCUT("spatial_editor/bottom_view", TTR("Bottom View"), KEY_MASK_ALT + KEY_KP_7);
	ED_SHORTCUT("spatial_editor/top_view", TTR("Top View"), KEY_KP_7);
	ED_SHORTCUT("spatial_editor/rear_view", TTR("Rear View"), KEY_MASK_ALT + KEY_KP_1);
	ED_SHORTCUT("spatial_editor/front_view", TTR("Front View"), KEY_KP_1);
	ED_SHORTCUT("spatial_editor/left_view", TTR("Left View"), KEY_MASK_ALT + KEY_KP_3);
	ED_SHORTCUT("spatial_editor/right_view", TTR("Right View"), KEY_KP_3);
	ED_SHORTCUT("spatial_editor/switch_perspective_orthogonal", TTR("Switch Perspective/Orthogonal view"), KEY_KP_5);
	ED_SHORTCUT("spatial_editor/snap", TTR("Snap"), KEY_S);
	ED_SHORTCUT("spatial_editor/insert_anim_key", TTR("Insert Animation Key"), KEY_K);
	ED_SHORTCUT("spatial_editor/focus_origin", TTR("Focus Origin"), KEY_O);
	ED_SHORTCUT("spatial_editor/focus_selection", TTR("Focus Selection"), KEY_F);
	ED_SHORTCUT("spatial_editor/align_selection_with_view", TTR("Align Selection With View"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_F);

	PopupMenu *p;

	transform_menu = memnew(MenuButton);
	transform_menu->set_text(TTR("Transform"));
	hbc_menu->add_child(transform_menu);

	p = transform_menu->get_popup();
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/use_snap", TTR("Use Snap")), MENU_TRANSFORM_USE_SNAP);
	p->add_shortcut(ED_SHORTCUT("spatial_editor/configure_snap", TTR("Configure Snap..")), MENU_TRANSFORM_CONFIGURE_SNAP);
	p->add_separator();
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/local_coords", TTR("Local Coords")), MENU_TRANSFORM_LOCAL_COORDS);
	//p->set_item_checked(p->get_item_count()-1,true);
	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/transform_dialog", TTR("Transform Dialog..")), MENU_TRANSFORM_DIALOG);

	p->connect("item_pressed", this, "_menu_item_pressed");

	view_menu = memnew(MenuButton);
	view_menu->set_text(TTR("View"));
	view_menu->set_pos(Point2(212, 0));
	hbc_menu->add_child(view_menu);

	p = view_menu->get_popup();

	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/use_default_light", TTR("Use Default Light")), MENU_VIEW_USE_DEFAULT_LIGHT);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/use_default_srgb", TTR("Use Default sRGB")), MENU_VIEW_USE_DEFAULT_SRGB);
	p->add_separator();

	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/1_viewport", TTR("1 Viewport"), KEY_MASK_CMD + KEY_1), MENU_VIEW_USE_1_VIEWPORT);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports", TTR("2 Viewports"), KEY_MASK_CMD + KEY_2), MENU_VIEW_USE_2_VIEWPORTS);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports_alt", TTR("2 Viewports (Alt)"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_2), MENU_VIEW_USE_2_VIEWPORTS_ALT);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports", TTR("3 Viewports"), KEY_MASK_CMD + KEY_3), MENU_VIEW_USE_3_VIEWPORTS);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports_alt", TTR("3 Viewports (Alt)"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_3), MENU_VIEW_USE_3_VIEWPORTS_ALT);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/4_viewports", TTR("4 Viewports"), KEY_MASK_CMD + KEY_4), MENU_VIEW_USE_4_VIEWPORTS);
	p->add_separator();

	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/display_normal", TTR("Display Normal")), MENU_VIEW_DISPLAY_NORMAL);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/display_wireframe", TTR("Display Wireframe")), MENU_VIEW_DISPLAY_WIREFRAME);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/display_overdraw", TTR("Display Overdraw")), MENU_VIEW_DISPLAY_OVERDRAW);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/display_shadeless", TTR("Display Shadeless")), MENU_VIEW_DISPLAY_SHADELESS);
	p->add_separator();
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_origin", TTR("View Origin")), MENU_VIEW_ORIGIN);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_grid", TTR("View Grid")), MENU_VIEW_GRID);
	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/settings", TTR("Settings")), MENU_VIEW_CAMERA_SETTINGS);

	p->set_item_checked(p->get_item_index(MENU_VIEW_USE_DEFAULT_LIGHT), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_DISPLAY_NORMAL), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_ORIGIN), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_GRID), true);

	p->connect("item_pressed", this, "_menu_item_pressed");

	/* REST OF MENU */

	palette_split = memnew(HSplitContainer);
	palette_split->set_v_size_flags(SIZE_EXPAND_FILL);
	vbc->add_child(palette_split);

	shader_split = memnew(VSplitContainer);
	shader_split->set_h_size_flags(SIZE_EXPAND_FILL);
	palette_split->add_child(shader_split);
	viewport_base = memnew(Control);
	shader_split->add_child(viewport_base);
	viewport_base->set_v_size_flags(SIZE_EXPAND_FILL);
	for (int i = 0; i < 4; i++) {

		viewports[i] = memnew(SpatialEditorViewport(this, editor, i));
		viewports[i]->connect("toggle_maximize_view", this, "_toggle_maximize_view");
		viewport_base->add_child(viewports[i]);
	}
	//vbc->add_child(viewport_base);

	/* SNAP DIALOG */

	snap_dialog = memnew(ConfirmationDialog);
	snap_dialog->set_title(TTR("Snap Settings"));
	add_child(snap_dialog);

	VBoxContainer *snap_dialog_vbc = memnew(VBoxContainer);
	snap_dialog->add_child(snap_dialog_vbc);
	snap_dialog->set_child_rect(snap_dialog_vbc);

	snap_translate = memnew(LineEdit);
	snap_translate->set_text("1");
	snap_dialog_vbc->add_margin_child(TTR("Translate Snap:"), snap_translate);

	snap_rotate = memnew(LineEdit);
	snap_rotate->set_text("5");
	snap_dialog_vbc->add_margin_child(TTR("Rotate Snap (deg.):"), snap_rotate);

	snap_scale = memnew(LineEdit);
	snap_scale->set_text("5");
	snap_dialog_vbc->add_margin_child(TTR("Scale Snap (%):"), snap_scale);

	/* SETTINGS DIALOG */

	settings_dialog = memnew(ConfirmationDialog);
	settings_dialog->set_title(TTR("Viewport Settings"));
	add_child(settings_dialog);
	settings_vbc = memnew(VBoxContainer);
	settings_vbc->set_custom_minimum_size(Size2(200, 0));
	settings_dialog->add_child(settings_vbc);
	settings_dialog->set_child_rect(settings_vbc);

	settings_light_base = memnew(Control);
	settings_light_base->set_custom_minimum_size(Size2(128, 128));
	settings_light_base->connect("input_event", this, "_default_light_angle_input");
	settings_vbc->add_margin_child(TTR("Default Light Normal:"), settings_light_base);
	settings_light_vp = memnew(Viewport);
	settings_light_vp->set_disable_input(true);
	settings_light_vp->set_use_own_world(true);
	settings_light_base->add_child(settings_light_vp);

	settings_dlight = memnew(DirectionalLight);
	settings_light_vp->add_child(settings_dlight);
	settings_sphere = memnew(ImmediateGeometry);
	settings_sphere->begin(Mesh::PRIMITIVE_TRIANGLES, Ref<Texture>());
	settings_sphere->set_color(Color(1, 1, 1));
	settings_sphere->add_sphere(32, 16, 1);
	settings_sphere->end();
	settings_light_vp->add_child(settings_sphere);
	settings_camera = memnew(Camera);
	settings_light_vp->add_child(settings_camera);
	settings_camera->set_translation(Vector3(0, 0, 2));
	settings_camera->set_orthogonal(2.1, 0.1, 5);

	settings_default_light_rot_x = Math_PI * 0.3;
	settings_default_light_rot_y = Math_PI * 0.2;

	settings_ambient_color = memnew(ColorPickerButton);
	settings_vbc->add_margin_child(TTR("Ambient Light Color:"), settings_ambient_color);
	settings_ambient_color->connect("color_changed", this, "_update_ambient_light_color");

	viewport_environment->set_enable_fx(Environment::FX_AMBIENT_LIGHT, true);
	viewport_environment->fx_set_param(Environment::FX_PARAM_AMBIENT_LIGHT_COLOR, Color(0.15, 0.15, 0.15));
	settings_ambient_color->set_color(Color(0.15, 0.15, 0.15));

	settings_fov = memnew(SpinBox);
	settings_fov->set_max(179);
	settings_fov->set_min(1);
	settings_fov->set_step(0.01);
	settings_fov->set_val(EDITOR_DEF("3d_editor/default_fov", 55.0));
	settings_vbc->add_margin_child(TTR("Perspective FOV (deg.):"), settings_fov);

	settings_znear = memnew(SpinBox);
	settings_znear->set_max(10000);
	settings_znear->set_min(0.1);
	settings_znear->set_step(0.01);
	settings_znear->set_val(EDITOR_DEF("3d_editor/default_z_near", 0.1));
	settings_vbc->add_margin_child(TTR("View Z-Near:"), settings_znear);

	settings_zfar = memnew(SpinBox);
	settings_zfar->set_max(10000);
	settings_zfar->set_min(0.1);
	settings_zfar->set_step(0.01);
	settings_zfar->set_val(EDITOR_DEF("3d_editor/default_z_far", 1500));
	settings_vbc->add_margin_child(TTR("View Z-Far:"), settings_zfar);

	//settings_dialog->get_cancel()->hide();
	/* XFORM DIALOG */

	xform_dialog = memnew(ConfirmationDialog);
	xform_dialog->set_title(TTR("Transform Change"));
	add_child(xform_dialog);
	Label *l = memnew(Label);
	l->set_text(TTR("Translate:"));
	l->set_pos(Point2(5, 5));
	xform_dialog->add_child(l);

	for (int i = 0; i < 3; i++) {

		xform_translate[i] = memnew(LineEdit);
		xform_translate[i]->set_pos(Point2(15 + i * 60, 22));
		xform_translate[i]->set_size(Size2(50, 12));
		xform_dialog->add_child(xform_translate[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Rotate (deg.):"));
	l->set_pos(Point2(5, 45));
	xform_dialog->add_child(l);

	for (int i = 0; i < 3; i++) {
		xform_rotate[i] = memnew(LineEdit);
		xform_rotate[i]->set_pos(Point2(15 + i * 60, 62));
		xform_rotate[i]->set_size(Size2(50, 22));
		xform_dialog->add_child(xform_rotate[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Scale (ratio):"));
	l->set_pos(Point2(5, 85));
	xform_dialog->add_child(l);

	for (int i = 0; i < 3; i++) {
		xform_scale[i] = memnew(LineEdit);
		xform_scale[i]->set_pos(Point2(15 + i * 60, 102));
		xform_scale[i]->set_size(Size2(50, 22));
		xform_dialog->add_child(xform_scale[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Transform Type"));
	l->set_pos(Point2(5, 125));
	xform_dialog->add_child(l);

	xform_type = memnew(OptionButton);
	xform_type->set_anchor(MARGIN_RIGHT, ANCHOR_END);
	xform_type->set_begin(Point2(15, 142));
	xform_type->set_end(Point2(15, 75));
	xform_type->add_item(TTR("Pre"));
	xform_type->add_item(TTR("Post"));
	xform_dialog->add_child(xform_type);

	xform_dialog->connect("confirmed", this, "_xform_dialog_action");

	scenario_debug = VisualServer::SCENARIO_DEBUG_DISABLED;

	selected = NULL;

	set_process_unhandled_key_input(true);
	add_to_group("_spatial_editor_group");

	EDITOR_DEF("3d_editor/manipulator_gizmo_size", 80);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "3d_editor/manipulator_gizmo_size", PROPERTY_HINT_RANGE, "16,1024,1"));
	EDITOR_DEF("3d_editor/manipulator_gizmo_opacity", 0.2);

	over_gizmo_handle = -1;
}

SpatialEditor::~SpatialEditor() {
}

void SpatialEditorPlugin::make_visible(bool p_visible) {

	if (p_visible) {

		spatial_editor->show();
		spatial_editor->set_process(true);
		//VisualServer::get_singleton()->viewport_set_hide_scenario(editor->get_scene_root()->get_viewport(),false);
		spatial_editor->grab_focus();

	} else {

		spatial_editor->hide();
		spatial_editor->set_process(false);
		//VisualServer::get_singleton()->viewport_set_hide_scenario(editor->get_scene_root()->get_viewport(),true);
	}
}
void SpatialEditorPlugin::edit(Object *p_object) {

	spatial_editor->edit(p_object->cast_to<Spatial>());
}

bool SpatialEditorPlugin::handles(Object *p_object) const {

	return p_object->is_type("Spatial");
}

Dictionary SpatialEditorPlugin::get_state() const {
	return spatial_editor->get_state();
}

void SpatialEditorPlugin::set_state(const Dictionary &p_state) {

	spatial_editor->set_state(p_state);
}

void SpatialEditor::snap_cursor_to_plane(const Plane &p_plane) {

	//	cursor.pos=p_plane.project(cursor.pos);
}

void SpatialEditorPlugin::_bind_methods() {

	ObjectTypeDB::bind_method("snap_cursor_to_plane", &SpatialEditorPlugin::snap_cursor_to_plane);
}

void SpatialEditorPlugin::snap_cursor_to_plane(const Plane &p_plane) {

	spatial_editor->snap_cursor_to_plane(p_plane);
}

SpatialEditorPlugin::SpatialEditorPlugin(EditorNode *p_node) {

	editor = p_node;
	spatial_editor = memnew(SpatialEditor(p_node));
	spatial_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	editor->get_viewport()->add_child(spatial_editor);

	//spatial_editor->set_area_as_parent_rect();
	spatial_editor->hide();
	spatial_editor->connect("transform_key_request", editor, "_transform_keyed");

	//spatial_editor->set_process(true);
}

SpatialEditorPlugin::~SpatialEditorPlugin() {
}
