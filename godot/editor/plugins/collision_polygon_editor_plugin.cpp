/*************************************************************************/
/*  collision_polygon_editor_plugin.cpp                                  */
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
#include "collision_polygon_editor_plugin.h"
#include "canvas_item_editor_plugin.h"
#include "editor/editor_settings.h"
#include "os/file_access.h"
#include "scene/3d/camera.h"
#include "spatial_editor_plugin.h"

void CollisionPolygonEditor::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_READY: {

			button_create->set_icon(get_icon("Edit", "EditorIcons"));
			button_edit->set_icon(get_icon("MovePoint", "EditorIcons"));
			button_edit->set_pressed(true);
			get_tree()->connect("node_removed", this, "_node_removed");

		} break;
		case NOTIFICATION_PROCESS: {

			if (node->get_depth() != prev_depth) {
				_polygon_draw();
				prev_depth = node->get_depth();
			}

		} break;
	}
}
void CollisionPolygonEditor::_node_removed(Node *p_node) {

	if (p_node == node) {
		node = NULL;
		if (imgeom->get_parent() == p_node)
			p_node->remove_child(imgeom);
		hide();
		set_process(false);
	}
}

void CollisionPolygonEditor::_menu_option(int p_option) {

	switch (p_option) {

		case MODE_CREATE: {

			mode = MODE_CREATE;
			button_create->set_pressed(true);
			button_edit->set_pressed(false);
		} break;
		case MODE_EDIT: {

			mode = MODE_EDIT;
			button_create->set_pressed(false);
			button_edit->set_pressed(true);
		} break;
	}
}

void CollisionPolygonEditor::_wip_close() {

	undo_redo->create_action(TTR("Create Poly3D"));
	undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
	undo_redo->add_do_method(node, "set_polygon", wip);
	undo_redo->add_do_method(this, "_polygon_draw");
	undo_redo->add_undo_method(this, "_polygon_draw");
	wip.clear();
	wip_active = false;
	mode = MODE_EDIT;
	button_edit->set_pressed(true);
	button_create->set_pressed(false);
	edited_point = -1;
	undo_redo->commit_action();
}

bool CollisionPolygonEditor::forward_spatial_input_event(Camera *p_camera, const InputEvent &p_event) {

	if (!node)
		return false;

	Transform gt = node->get_global_transform();
	Transform gi = gt.affine_inverse();
	float depth = node->get_depth() * 0.5;
	Vector3 n = gt.basis.get_axis(2).normalized();
	Plane p(gt.origin + n * depth, n);

	switch (p_event.type) {

		case InputEvent::MOUSE_BUTTON: {

			const InputEventMouseButton &mb = p_event.mouse_button;

			Vector2 gpoint = Point2(mb.x, mb.y);
			Vector3 ray_from = p_camera->project_ray_origin(gpoint);
			Vector3 ray_dir = p_camera->project_ray_normal(gpoint);

			Vector3 spoint;

			if (!p.intersects_ray(ray_from, ray_dir, &spoint))
				break;

			spoint = gi.xform(spoint);

			Vector2 cpoint(spoint.x, spoint.y);

			cpoint = CanvasItemEditor::get_singleton()->snap_point(cpoint);

			Vector<Vector2> poly = node->get_polygon();

			//first check if a point is to be added (segment split)
			real_t grab_treshold = EDITOR_DEF("poly_editor/point_grab_radius", 8);

			switch (mode) {

				case MODE_CREATE: {

					if (mb.button_index == BUTTON_LEFT && mb.pressed) {

						if (!wip_active) {

							wip.clear();
							wip.push_back(cpoint);
							wip_active = true;
							edited_point_pos = cpoint;
							_polygon_draw();
							edited_point = 1;
							return true;
						} else {

							if (wip.size() > 1 && p_camera->unproject_position(gt.xform(Vector3(wip[0].x, wip[0].y, depth))).distance_to(gpoint) < grab_treshold) {
								//wip closed
								_wip_close();

								return true;
							} else {

								wip.push_back(cpoint);
								edited_point = wip.size();
								_polygon_draw();
								return true;

								//add wip point
							}
						}
					} else if (mb.button_index == BUTTON_RIGHT && mb.pressed && wip_active) {
						_wip_close();
					}

				} break;

				case MODE_EDIT: {

					if (mb.button_index == BUTTON_LEFT) {
						if (mb.pressed) {

							if (mb.mod.control) {

								if (poly.size() < 3) {

									undo_redo->create_action(TTR("Edit Poly"));
									undo_redo->add_undo_method(node, "set_polygon", poly);
									poly.push_back(cpoint);
									undo_redo->add_do_method(node, "set_polygon", poly);
									undo_redo->add_do_method(this, "_polygon_draw");
									undo_redo->add_undo_method(this, "_polygon_draw");
									undo_redo->commit_action();
									return true;
								}

								//search edges
								int closest_idx = -1;
								Vector2 closest_pos;
								real_t closest_dist = 1e10;
								for (int i = 0; i < poly.size(); i++) {

									Vector2 points[2] = {
										p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth))),
										p_camera->unproject_position(gt.xform(Vector3(poly[(i + 1) % poly.size()].x, poly[(i + 1) % poly.size()].y, depth)))
									};

									Vector2 cp = Geometry::get_closest_point_to_segment_2d(gpoint, points);
									if (cp.distance_squared_to(points[0]) < CMP_EPSILON2 || cp.distance_squared_to(points[1]) < CMP_EPSILON2)
										continue; //not valid to reuse point

									real_t d = cp.distance_to(gpoint);
									if (d < closest_dist && d < grab_treshold) {
										closest_dist = d;
										closest_pos = cp;
										closest_idx = i;
									}
								}

								if (closest_idx >= 0) {

									pre_move_edit = poly;
									poly.insert(closest_idx + 1, cpoint);
									edited_point = closest_idx + 1;
									edited_point_pos = cpoint;
									node->set_polygon(poly);
									_polygon_draw();
									return true;
								}
							} else {

								//look for points to move

								int closest_idx = -1;
								Vector2 closest_pos;
								real_t closest_dist = 1e10;
								for (int i = 0; i < poly.size(); i++) {

									Vector2 cp = p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth)));

									real_t d = cp.distance_to(gpoint);
									if (d < closest_dist && d < grab_treshold) {
										closest_dist = d;
										closest_pos = cp;
										closest_idx = i;
									}
								}

								if (closest_idx >= 0) {

									pre_move_edit = poly;
									edited_point = closest_idx;
									edited_point_pos = poly[closest_idx];
									_polygon_draw();
									return true;
								}
							}
						} else {

							if (edited_point != -1) {

								//apply

								ERR_FAIL_INDEX_V(edited_point, poly.size(), false);
								poly[edited_point] = edited_point_pos;
								undo_redo->create_action(TTR("Edit Poly"));
								undo_redo->add_do_method(node, "set_polygon", poly);
								undo_redo->add_undo_method(node, "set_polygon", pre_move_edit);
								undo_redo->add_do_method(this, "_polygon_draw");
								undo_redo->add_undo_method(this, "_polygon_draw");
								undo_redo->commit_action();

								edited_point = -1;
								return true;
							}
						}
					}
					if (mb.button_index == BUTTON_RIGHT && mb.pressed && edited_point == -1) {

						int closest_idx = -1;
						Vector2 closest_pos;
						real_t closest_dist = 1e10;
						for (int i = 0; i < poly.size(); i++) {

							Vector2 cp = p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth)));

							real_t d = cp.distance_to(gpoint);
							if (d < closest_dist && d < grab_treshold) {
								closest_dist = d;
								closest_pos = cp;
								closest_idx = i;
							}
						}

						if (closest_idx >= 0) {

							undo_redo->create_action(TTR("Edit Poly (Remove Point)"));
							undo_redo->add_undo_method(node, "set_polygon", poly);
							poly.remove(closest_idx);
							undo_redo->add_do_method(node, "set_polygon", poly);
							undo_redo->add_do_method(this, "_polygon_draw");
							undo_redo->add_undo_method(this, "_polygon_draw");
							undo_redo->commit_action();
							return true;
						}
					}

				} break;
			}

		} break;
		case InputEvent::MOUSE_MOTION: {

			const InputEventMouseMotion &mm = p_event.mouse_motion;

			if (edited_point != -1 && (wip_active || mm.button_mask & BUTTON_MASK_LEFT)) {

				Vector2 gpoint = Point2(mm.x, mm.y);

				Vector3 ray_from = p_camera->project_ray_origin(gpoint);
				Vector3 ray_dir = p_camera->project_ray_normal(gpoint);

				Vector3 spoint;

				if (!p.intersects_ray(ray_from, ray_dir, &spoint))
					break;

				spoint = gi.xform(spoint);

				Vector2 cpoint(spoint.x, spoint.y);

				cpoint = CanvasItemEditor::get_singleton()->snap_point(cpoint);
				edited_point_pos = cpoint;

				_polygon_draw();
			}

		} break;
	}

	return false;
}
void CollisionPolygonEditor::_polygon_draw() {

	if (!node)
		return;

	Vector<Vector2> poly;

	if (wip_active)
		poly = wip;
	else
		poly = node->get_polygon();

	float depth = node->get_depth() * 0.5;

	imgeom->clear();
	imgeom->set_material_override(line_material);
	imgeom->begin(Mesh::PRIMITIVE_LINES, Ref<Texture>());

	Rect2 rect;

	for (int i = 0; i < poly.size(); i++) {

		Vector2 p, p2;
		p = i == edited_point ? edited_point_pos : poly[i];
		if ((wip_active && i == poly.size() - 1) || (((i + 1) % poly.size()) == edited_point))
			p2 = edited_point_pos;
		else
			p2 = poly[(i + 1) % poly.size()];

		if (i == 0)
			rect.pos = p;
		else
			rect.expand_to(p);

		Vector3 point = Vector3(p.x, p.y, depth);
		Vector3 next_point = Vector3(p2.x, p2.y, depth);

		imgeom->set_color(Color(1, 0.3, 0.1, 0.8));
		imgeom->add_vertex(point);
		imgeom->set_color(Color(1, 0.3, 0.1, 0.8));
		imgeom->add_vertex(next_point);

		//Color col=Color(1,0.3,0.1,0.8);
		//vpc->draw_line(point,next_point,col,2);
		//vpc->draw_texture(handle,point-handle->get_size()*0.5);
	}

	rect = rect.grow(1);

	AABB r;
	r.pos.x = rect.pos.x;
	r.pos.y = rect.pos.y;
	r.pos.z = depth;
	r.size.x = rect.size.x;
	r.size.y = rect.size.y;
	r.size.z = 0;

	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos);
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0.3, 0, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos);
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0.0, 0.3, 0));

	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(r.size.x, 0, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(r.size.x, 0, 0) - Vector3(0.3, 0, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(r.size.x, 0, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(r.size.x, 0, 0) + Vector3(0, 0.3, 0));

	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0, r.size.y, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0, r.size.y, 0) - Vector3(0, 0.3, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0, r.size.y, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + Vector3(0, r.size.y, 0) + Vector3(0.3, 0, 0));

	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + r.size);
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + r.size - Vector3(0.3, 0, 0));
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + r.size);
	imgeom->set_color(Color(0.8, 0.8, 0.8, 0.2));
	imgeom->add_vertex(r.pos + r.size - Vector3(0.0, 0.3, 0));

	imgeom->end();

	while (m->get_surface_count()) {
		m->surface_remove(0);
	}

	if (poly.size() == 0)
		return;

	Array a;
	a.resize(Mesh::ARRAY_MAX);
	DVector<Vector3> va;
	{

		va.resize(poly.size());
		DVector<Vector3>::Write w = va.write();
		for (int i = 0; i < poly.size(); i++) {

			Vector2 p, p2;
			p = i == edited_point ? edited_point_pos : poly[i];

			Vector3 point = Vector3(p.x, p.y, depth);
			w[i] = point;
		}
	}
	a[Mesh::ARRAY_VERTEX] = va;
	m->add_surface(Mesh::PRIMITIVE_POINTS, a);
	m->surface_set_material(0, handle_material);
}

void CollisionPolygonEditor::edit(Node *p_collision_polygon) {

	if (p_collision_polygon) {

		node = p_collision_polygon->cast_to<CollisionPolygon>();
		wip.clear();
		wip_active = false;
		edited_point = -1;
		p_collision_polygon->add_child(imgeom);
		_polygon_draw();
		set_process(true);
		prev_depth = -1;

	} else {
		node = NULL;

		if (imgeom->get_parent())
			imgeom->get_parent()->remove_child(imgeom);

		set_process(false);
	}
}

void CollisionPolygonEditor::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_menu_option"), &CollisionPolygonEditor::_menu_option);
	ObjectTypeDB::bind_method(_MD("_polygon_draw"), &CollisionPolygonEditor::_polygon_draw);
	ObjectTypeDB::bind_method(_MD("_node_removed"), &CollisionPolygonEditor::_node_removed);
}

CollisionPolygonEditor::CollisionPolygonEditor(EditorNode *p_editor) {

	node = NULL;
	editor = p_editor;
	undo_redo = editor->get_undo_redo();

	add_child(memnew(VSeparator));
	button_create = memnew(ToolButton);
	add_child(button_create);
	button_create->connect("pressed", this, "_menu_option", varray(MODE_CREATE));
	button_create->set_toggle_mode(true);

	button_edit = memnew(ToolButton);
	add_child(button_edit);
	button_edit->connect("pressed", this, "_menu_option", varray(MODE_EDIT));
	button_edit->set_toggle_mode(true);

	//add_constant_override("separation",0);

#if 0
	options = memnew( MenuButton );
	add_child(options);
	options->set_area_as_parent_rect();
	options->set_text("Polygon");
	//options->get_popup()->add_item("Parse BBCode",PARSE_BBCODE);
	options->get_popup()->connect("item_pressed", this,"_menu_option");
#endif

	mode = MODE_EDIT;
	wip_active = false;
	imgeom = memnew(ImmediateGeometry);
	imgeom->set_transform(Transform(Matrix3(), Vector3(0, 0, 0.00001)));

	line_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	line_material->set_flag(Material::FLAG_UNSHADED, true);
	line_material->set_line_width(3.0);
	line_material->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	line_material->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
	line_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1));

	handle_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	handle_material->set_flag(Material::FLAG_UNSHADED, true);
	handle_material->set_fixed_flag(FixedMaterial::FLAG_USE_POINT_SIZE, true);
	handle_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1));
	handle_material->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	handle_material->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, false);
	Ref<Texture> handle = editor->get_gui_base()->get_icon("Editor3DHandle", "EditorIcons");
	handle_material->set_point_size(handle->get_width());
	handle_material->set_texture(FixedMaterial::PARAM_DIFFUSE, handle);

	pointsm = memnew(MeshInstance);
	imgeom->add_child(pointsm);
	m = Ref<Mesh>(memnew(Mesh));
	pointsm->set_mesh(m);
	pointsm->set_transform(Transform(Matrix3(), Vector3(0, 0, 0.00001)));
}

CollisionPolygonEditor::~CollisionPolygonEditor() {

	memdelete(imgeom);
}

void CollisionPolygonEditorPlugin::edit(Object *p_object) {

	collision_polygon_editor->edit(p_object->cast_to<Node>());
}

bool CollisionPolygonEditorPlugin::handles(Object *p_object) const {

	return p_object->is_type("CollisionPolygon");
}

void CollisionPolygonEditorPlugin::make_visible(bool p_visible) {

	if (p_visible) {
		collision_polygon_editor->show();
	} else {

		collision_polygon_editor->hide();
		collision_polygon_editor->edit(NULL);
	}
}

CollisionPolygonEditorPlugin::CollisionPolygonEditorPlugin(EditorNode *p_node) {

	editor = p_node;
	collision_polygon_editor = memnew(CollisionPolygonEditor(p_node));
	SpatialEditor::get_singleton()->add_control_to_menu_panel(collision_polygon_editor);

	collision_polygon_editor->hide();
}

CollisionPolygonEditorPlugin::~CollisionPolygonEditorPlugin() {
}
