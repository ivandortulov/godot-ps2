/*************************************************************************/
/*  collision_shape_2d_editor_plugin.cpp                                 */
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
#include "collision_shape_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"

#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/segment_shape_2d.h"
#include "scene/resources/shape_line_2d.h"

Variant CollisionShape2DEditor::get_handle_value(int idx) const {

	switch (shape_type) {
		case CAPSULE_SHAPE: {
			Ref<CapsuleShape2D> capsule = node->get_shape();

			if (idx == 0) {
				return capsule->get_radius();
			} else if (idx == 1) {
				return capsule->get_height();
			}

		} break;

		case CIRCLE_SHAPE: {
			Ref<CircleShape2D> circle = node->get_shape();

			if (idx == 0) {
				return circle->get_radius();
			}

		} break;

		case CONCAVE_POLYGON_SHAPE: {

		} break;

		case CONVEX_POLYGON_SHAPE: {

		} break;

		case LINE_SHAPE: {
			Ref<LineShape2D> line = node->get_shape();

			if (idx == 0) {
				return line->get_d();
			} else {
				return line->get_normal();
			}

		} break;

		case RAY_SHAPE: {
			Ref<RayShape2D> ray = node->get_shape();

			if (idx == 0) {
				return ray->get_length();
			}

		} break;

		case RECTANGLE_SHAPE: {
			Ref<RectangleShape2D> rect = node->get_shape();

			if (idx < 2) {
				return rect->get_extents().abs();
			}

		} break;

		case SEGMENT_SHAPE: {
			Ref<SegmentShape2D> seg = node->get_shape();

			if (idx == 0) {
				return seg->get_a();
			} else if (idx == 1) {
				return seg->get_b();
			}

		} break;
	}

	return Variant();
}

void CollisionShape2DEditor::set_handle(int idx, Point2 &p_point) {

	switch (shape_type) {
		case CAPSULE_SHAPE: {
			if (idx < 2) {
				Ref<CapsuleShape2D> capsule = node->get_shape();

				real_t parameter = Math::abs(p_point[idx]);

				if (idx == 0) {
					capsule->set_radius(parameter);
				} else if (idx == 1) {
					capsule->set_height(parameter * 2 - capsule->get_radius() * 2);
				}

				canvas_item_editor->get_viewport_control()->update();
			}

		} break;

		case CIRCLE_SHAPE: {
			Ref<CircleShape2D> circle = node->get_shape();
			circle->set_radius(p_point.length());

			canvas_item_editor->get_viewport_control()->update();

		} break;

		case CONCAVE_POLYGON_SHAPE: {

		} break;

		case CONVEX_POLYGON_SHAPE: {

		} break;

		case LINE_SHAPE: {
			if (idx < 2) {
				Ref<LineShape2D> line = node->get_shape();

				if (idx == 0) {
					line->set_d(p_point.length());
				} else {
					line->set_normal(p_point.normalized());
				}

				canvas_item_editor->get_viewport_control()->update();
			}

		} break;

		case RAY_SHAPE: {
			Ref<RayShape2D> ray = node->get_shape();

			ray->set_length(Math::abs(p_point.y));

			canvas_item_editor->get_viewport_control()->update();

		} break;

		case RECTANGLE_SHAPE: {
			if (idx < 2) {
				Ref<RectangleShape2D> rect = node->get_shape();

				Vector2 extents = rect->get_extents();
				extents[idx] = p_point[idx];

				rect->set_extents(extents.abs());

				canvas_item_editor->get_viewport_control()->update();
			}

		} break;

		case SEGMENT_SHAPE: {
			if (edit_handle < 2) {
				Ref<SegmentShape2D> seg = node->get_shape();

				if (idx == 0) {
					seg->set_a(p_point);
				} else if (idx == 1) {
					seg->set_b(p_point);
				}

				canvas_item_editor->get_viewport_control()->update();
			}

		} break;
	}
}

void CollisionShape2DEditor::commit_handle(int idx, Variant &p_org) {

	Control *c = canvas_item_editor->get_viewport_control();
	undo_redo->create_action(TTR("Set Handle"));

	switch (shape_type) {
		case CAPSULE_SHAPE: {
			Ref<CapsuleShape2D> capsule = node->get_shape();

			if (idx == 0) {
				undo_redo->add_do_method(capsule.ptr(), "set_radius", capsule->get_radius());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(capsule.ptr(), "set_radius", p_org);
				undo_redo->add_do_method(c, "update");
			} else if (idx == 1) {
				undo_redo->add_do_method(capsule.ptr(), "set_height", capsule->get_height());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(capsule.ptr(), "set_height", p_org);
				undo_redo->add_undo_method(c, "update");
			}

		} break;

		case CIRCLE_SHAPE: {
			Ref<CircleShape2D> circle = node->get_shape();

			undo_redo->add_do_method(circle.ptr(), "set_radius", circle->get_radius());
			undo_redo->add_do_method(c, "update");
			undo_redo->add_undo_method(circle.ptr(), "set_radius", p_org);
			undo_redo->add_undo_method(c, "update");

		} break;

		case CONCAVE_POLYGON_SHAPE: {

		} break;

		case CONVEX_POLYGON_SHAPE: {

		} break;

		case LINE_SHAPE: {
			Ref<LineShape2D> line = node->get_shape();

			if (idx == 0) {
				undo_redo->add_do_method(line.ptr(), "set_d", line->get_d());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(line.ptr(), "set_d", p_org);
				undo_redo->add_undo_method(c, "update");
			} else {
				undo_redo->add_do_method(line.ptr(), "set_normal", line->get_normal());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(line.ptr(), "set_normal", p_org);
				undo_redo->add_undo_method(c, "update");
			}

		} break;

		case RAY_SHAPE: {
			Ref<RayShape2D> ray = node->get_shape();

			undo_redo->add_do_method(ray.ptr(), "set_length", ray->get_length());
			undo_redo->add_do_method(c, "update");
			undo_redo->add_undo_method(ray.ptr(), "set_length", p_org);
			undo_redo->add_undo_method(c, "update");

		} break;

		case RECTANGLE_SHAPE: {
			Ref<RectangleShape2D> rect = node->get_shape();

			undo_redo->add_do_method(rect.ptr(), "set_extents", rect->get_extents());
			undo_redo->add_do_method(c, "update");
			undo_redo->add_undo_method(rect.ptr(), "set_extents", p_org);
			undo_redo->add_undo_method(c, "update");

		} break;

		case SEGMENT_SHAPE: {
			Ref<SegmentShape2D> seg = node->get_shape();
			if (idx == 0) {
				undo_redo->add_do_method(seg.ptr(), "set_a", seg->get_a());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(seg.ptr(), "set_a", p_org);
				undo_redo->add_undo_method(c, "update");
			} else if (idx == 1) {
				undo_redo->add_do_method(seg.ptr(), "set_b", seg->get_b());
				undo_redo->add_do_method(c, "update");
				undo_redo->add_undo_method(seg.ptr(), "set_b", p_org);
				undo_redo->add_undo_method(c, "update");
			}

		} break;
	}

	undo_redo->commit_action();
}

bool CollisionShape2DEditor::forward_input_event(const InputEvent &p_event) {

	if (!node) {
		return false;
	}

	if (!node->get_shape().is_valid()) {
		return false;
	}

	if (shape_type == -1) {
		return false;
	}

	switch (p_event.type) {
		case InputEvent::MOUSE_BUTTON: {
			const InputEventMouseButton &mb = p_event.mouse_button;

			Matrix32 gt = canvas_item_editor->get_canvas_transform() * node->get_global_transform();

			Point2 gpoint(mb.x, mb.y);

			if (mb.button_index == BUTTON_LEFT) {
				if (mb.pressed) {
					for (int i = 0; i < handles.size(); i++) {
						if (gt.xform(handles[i]).distance_to(gpoint) < 8) {
							edit_handle = i;

							break;
						}
					}

					if (edit_handle == -1) {
						pressed = false;

						return false;
					}

					original = get_handle_value(edit_handle);
					pressed = true;

					return true;

				} else {
					if (pressed) {
						commit_handle(edit_handle, original);

						edit_handle = -1;
						pressed = false;

						return true;
					}
				}
			}

			return false;

		} break;

		case InputEvent::MOUSE_MOTION: {
			const InputEventMouseMotion &mm = p_event.mouse_motion;

			if (edit_handle == -1 || !pressed) {
				return false;
			}

			Point2 gpoint = Point2(mm.x, mm.y);
			Point2 cpoint = canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint);
			cpoint = canvas_item_editor->snap_point(cpoint);
			cpoint = node->get_global_transform().affine_inverse().xform(cpoint);

			set_handle(edit_handle, cpoint);

			return true;

		} break;
	}

	return false;
}

void CollisionShape2DEditor::_get_current_shape_type() {

	if (!node) {
		return;
	}

	Ref<Shape2D> s = node->get_shape();

	if (!s.is_valid()) {
		return;
	}

	if (s->cast_to<CapsuleShape2D>()) {
		shape_type = CAPSULE_SHAPE;
	} else if (s->cast_to<CircleShape2D>()) {
		shape_type = CIRCLE_SHAPE;
	} else if (s->cast_to<ConcavePolygonShape2D>()) {
		shape_type = CONCAVE_POLYGON_SHAPE;
	} else if (s->cast_to<ConvexPolygonShape2D>()) {
		shape_type = CONVEX_POLYGON_SHAPE;
	} else if (s->cast_to<LineShape2D>()) {
		shape_type = LINE_SHAPE;
	} else if (s->cast_to<RayShape2D>()) {
		shape_type = RAY_SHAPE;
	} else if (s->cast_to<RectangleShape2D>()) {
		shape_type = RECTANGLE_SHAPE;
	} else if (s->cast_to<SegmentShape2D>()) {
		shape_type = SEGMENT_SHAPE;
	} else {
		shape_type = -1;
	}

	canvas_item_editor->get_viewport_control()->update();
}

void CollisionShape2DEditor::_canvas_draw() {

	if (!node) {
		return;
	}

	if (!node->get_shape().is_valid()) {
		return;
	}

	_get_current_shape_type();

	if (shape_type == -1) {
		return;
	}

	Control *c = canvas_item_editor->get_viewport_control();
	Matrix32 gt = canvas_item_editor->get_canvas_transform() * node->get_global_transform();

	Ref<Texture> h = get_icon("EditorHandle", "EditorIcons");
	Vector2 size = h->get_size() * 0.5;

	handles.clear();

	switch (shape_type) {
		case CAPSULE_SHAPE: {
			Ref<CapsuleShape2D> shape = node->get_shape();

			handles.resize(2);
			float radius = shape->get_radius();
			float height = shape->get_height() / 2;

			handles[0] = Point2(radius, -height);
			handles[1] = Point2(0, -(height + radius));

			c->draw_texture(h, gt.xform(handles[0]) - size);
			c->draw_texture(h, gt.xform(handles[1]) - size);

		} break;

		case CIRCLE_SHAPE: {
			Ref<CircleShape2D> shape = node->get_shape();

			handles.resize(1);
			handles[0] = Point2(shape->get_radius(), 0);

			c->draw_texture(h, gt.xform(handles[0]) - size);

		} break;

		case CONCAVE_POLYGON_SHAPE: {

		} break;

		case CONVEX_POLYGON_SHAPE: {

		} break;

		case LINE_SHAPE: {
			Ref<LineShape2D> shape = node->get_shape();

			handles.resize(2);
			handles[0] = shape->get_normal() * shape->get_d();
			handles[1] = shape->get_normal() * (shape->get_d() + 30.0);

			c->draw_texture(h, gt.xform(handles[0]) - size);
			c->draw_texture(h, gt.xform(handles[1]) - size);

		} break;

		case RAY_SHAPE: {
			Ref<RayShape2D> shape = node->get_shape();

			handles.resize(1);
			handles[0] = Point2(0, shape->get_length());

			c->draw_texture(h, gt.xform(handles[0]) - size);

		} break;

		case RECTANGLE_SHAPE: {
			Ref<RectangleShape2D> shape = node->get_shape();

			handles.resize(2);
			Vector2 ext = shape->get_extents();
			handles[0] = Point2(ext.x, 0);
			handles[1] = Point2(0, -ext.y);

			c->draw_texture(h, gt.xform(handles[0]) - size);
			c->draw_texture(h, gt.xform(handles[1]) - size);

		} break;

		case SEGMENT_SHAPE: {
			Ref<SegmentShape2D> shape = node->get_shape();

			handles.resize(2);
			handles[0] = shape->get_a();
			handles[1] = shape->get_b();

			c->draw_texture(h, gt.xform(handles[0]) - size);
			c->draw_texture(h, gt.xform(handles[1]) - size);

		} break;
	}
}

void CollisionShape2DEditor::edit(Node *p_node) {

	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	if (p_node) {
		node = p_node->cast_to<CollisionShape2D>();

		if (!canvas_item_editor->get_viewport_control()->is_connected("draw", this, "_canvas_draw"))
			canvas_item_editor->get_viewport_control()->connect("draw", this, "_canvas_draw");

		_get_current_shape_type();

	} else {
		edit_handle = -1;
		shape_type = -1;

		if (canvas_item_editor->get_viewport_control()->is_connected("draw", this, "_canvas_draw"))
			canvas_item_editor->get_viewport_control()->disconnect("draw", this, "_canvas_draw");

		node = NULL;
	}

	canvas_item_editor->get_viewport_control()->update();
}

void CollisionShape2DEditor::_bind_methods() {

	ObjectTypeDB::bind_method("_canvas_draw", &CollisionShape2DEditor::_canvas_draw);
	ObjectTypeDB::bind_method("_get_current_shape_type", &CollisionShape2DEditor::_get_current_shape_type);
}

CollisionShape2DEditor::CollisionShape2DEditor(EditorNode *p_editor) {

	node = NULL;
	canvas_item_editor = NULL;
	editor = p_editor;

	undo_redo = p_editor->get_undo_redo();

	edit_handle = -1;
	pressed = false;
}

void CollisionShape2DEditorPlugin::edit(Object *p_obj) {

	collision_shape_2d_editor->edit(p_obj->cast_to<Node>());
}

bool CollisionShape2DEditorPlugin::handles(Object *p_obj) const {

	return p_obj->is_type("CollisionShape2D");
}

void CollisionShape2DEditorPlugin::make_visible(bool visible) {

	if (!visible) {
		edit(NULL);
	}
}

CollisionShape2DEditorPlugin::CollisionShape2DEditorPlugin(EditorNode *p_node) {

	editor = p_node;

	collision_shape_2d_editor = memnew(CollisionShape2DEditor(p_node));
	p_node->get_gui_base()->add_child(collision_shape_2d_editor);
}

CollisionShape2DEditorPlugin::~CollisionShape2DEditorPlugin() {
}
