/*************************************************************************/
/*  canvas_item.cpp                                                      */
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
#include "canvas_item.h"
#include "message_queue.h"
#include "os/input.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"
#include "scene/resources/texture.h"
#include "scene/scene_string_names.h"
#include "servers/visual_server.h"

bool CanvasItemMaterial::_set(const StringName &p_name, const Variant &p_value) {

	if (p_name == SceneStringNames::get_singleton()->shader_shader) {
		set_shader(p_value);
		return true;
	} else if (p_name == SceneStringNames::get_singleton()->shading_mode) {
		set_shading_mode(ShadingMode(p_value.operator int()));
		return true;
	} else {

		if (shader.is_valid()) {

			StringName pr = shader->remap_param(p_name);
			if (!pr) {
				String n = p_name;
				if (n.find("param/") == 0) { //backwards compatibility
					pr = n.substr(6, n.length());
				}
			}
			if (pr) {
				VisualServer::get_singleton()->canvas_item_material_set_shader_param(material, pr, p_value);
				return true;
			}
		}
	}

	return false;
}

bool CanvasItemMaterial::_get(const StringName &p_name, Variant &r_ret) const {

	if (p_name == SceneStringNames::get_singleton()->shader_shader) {

		r_ret = get_shader();
		return true;
	} else if (p_name == SceneStringNames::get_singleton()->shading_mode) {

		r_ret = shading_mode;
		return true;
	} else {

		if (shader.is_valid()) {

			StringName pr = shader->remap_param(p_name);
			if (pr) {
				r_ret = VisualServer::get_singleton()->canvas_item_material_get_shader_param(material, pr);
				return true;
			}
		}
	}

	return false;
}

void CanvasItemMaterial::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::OBJECT, "shader/shader", PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemShader,CanvasItemShaderGraph"));
	p_list->push_back(PropertyInfo(Variant::INT, "shader/shading_mode", PROPERTY_HINT_ENUM, "Normal,Unshaded,Light Only"));

	if (!shader.is_null()) {

		shader->get_param_list(p_list);
	}
}

void CanvasItemMaterial::set_shader(const Ref<Shader> &p_shader) {

	ERR_FAIL_COND(p_shader.is_valid() && p_shader->get_mode() != Shader::MODE_CANVAS_ITEM);

	shader = p_shader;

	RID rid;
	if (shader.is_valid())
		rid = shader->get_rid();

	VS::get_singleton()->canvas_item_material_set_shader(material, rid);
	_change_notify(); //properties for shader exposed
	emit_changed();
}

Ref<Shader> CanvasItemMaterial::get_shader() const {

	return shader;
}

void CanvasItemMaterial::set_shader_param(const StringName &p_param, const Variant &p_value) {

	VS::get_singleton()->canvas_item_material_set_shader_param(material, p_param, p_value);
}

Variant CanvasItemMaterial::get_shader_param(const StringName &p_param) const {

	return VS::get_singleton()->canvas_item_material_get_shader_param(material, p_param);
}

RID CanvasItemMaterial::get_rid() const {

	return material;
}

void CanvasItemMaterial::set_shading_mode(ShadingMode p_mode) {

	shading_mode = p_mode;
	VS::get_singleton()->canvas_item_material_set_shading_mode(material, VS::CanvasItemShadingMode(p_mode));
}

CanvasItemMaterial::ShadingMode CanvasItemMaterial::get_shading_mode() const {
	return shading_mode;
}

void CanvasItemMaterial::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_shader", "shader:Shader"), &CanvasItemMaterial::set_shader);
	ObjectTypeDB::bind_method(_MD("get_shader:Shader"), &CanvasItemMaterial::get_shader);
	ObjectTypeDB::bind_method(_MD("set_shader_param", "param", "value"), &CanvasItemMaterial::set_shader_param);
	ObjectTypeDB::bind_method(_MD("get_shader_param", "param"), &CanvasItemMaterial::get_shader_param);
	ObjectTypeDB::bind_method(_MD("set_shading_mode", "mode"), &CanvasItemMaterial::set_shading_mode);
	ObjectTypeDB::bind_method(_MD("get_shading_mode"), &CanvasItemMaterial::get_shading_mode);

	BIND_CONSTANT(SHADING_NORMAL);
	BIND_CONSTANT(SHADING_UNSHADED);
	BIND_CONSTANT(SHADING_ONLY_LIGHT);
}

void CanvasItemMaterial::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {

	String f = p_function.operator String();
	if ((f == "get_shader_param" || f == "set_shader_param") && p_idx == 0) {

		if (shader.is_valid()) {
			List<PropertyInfo> pl;
			shader->get_param_list(&pl);
			for (List<PropertyInfo>::Element *E = pl.front(); E; E = E->next()) {
				r_options->push_back("\"" + E->get().name.replace_first("shader_param/", "") + "\"");
			}
		}
	}
	Resource::get_argument_options(p_function, p_idx, r_options);
}

CanvasItemMaterial::CanvasItemMaterial() {

	material = VS::get_singleton()->canvas_item_material_create();
	shading_mode = SHADING_NORMAL;
}

CanvasItemMaterial::~CanvasItemMaterial() {

	VS::get_singleton()->free(material);
}

///////////////////////////////////////////////////////////////////

bool CanvasItem::is_visible() const {

	if (!is_inside_tree())
		return false;

	const CanvasItem *p = this;

	while (p) {
		if (p->hidden)
			return false;
		p = p->get_parent_item();
	}

	return true;
}

bool CanvasItem::is_hidden() const {

	/*if (!is_inside_scene())
		return false;*/

	return hidden;
}

void CanvasItem::_propagate_visibility_changed(bool p_visible) {

	notification(NOTIFICATION_VISIBILITY_CHANGED);

	if (p_visible)
		update(); //todo optimize
	else
		emit_signal(SceneStringNames::get_singleton()->hide);
	_block();

	for (int i = 0; i < get_child_count(); i++) {

		CanvasItem *c = get_child(i)->cast_to<CanvasItem>();

		if (c && !c->hidden) //should the toplevels stop propagation? i think so but..
			c->_propagate_visibility_changed(p_visible);
	}

	_unblock();
}

void CanvasItem::show() {

	if (!hidden)
		return;

	hidden = false;
	VisualServer::get_singleton()->canvas_item_set_visible(canvas_item, true);

	if (!is_inside_tree())
		return;

	_propagate_visibility_changed(true);
	_change_notify("visibility/visible");
}

void CanvasItem::hide() {

	if (hidden)
		return;

	hidden = true;
	VisualServer::get_singleton()->canvas_item_set_visible(canvas_item, false);

	if (!is_inside_tree())
		return;

	_propagate_visibility_changed(false);
	_change_notify("visibility/visible");
}

void CanvasItem::set_hidden(bool p_hidden) {

	if (hidden == p_hidden) {
		return;
	}

	_set_visible_(!p_hidden);
}

Variant CanvasItem::edit_get_state() const {

	return Variant();
}
void CanvasItem::edit_set_state(const Variant &p_state) {
}

void CanvasItem::edit_set_rect(const Rect2 &p_edit_rect) {

	//used by editors, implement at will
}

void CanvasItem::edit_rotate(float p_rot) {
}

Size2 CanvasItem::edit_get_minimum_size() const {

	return Size2(-1, -1); //no limit
}

void CanvasItem::_update_callback() {

	if (!is_inside_tree()) {
		pending_update = false;
		return;
	}

	VisualServer::get_singleton()->canvas_item_clear(get_canvas_item());
	//todo updating = true - only allow drawing here
	if (is_visible()) { //todo optimize this!!
		if (first_draw) {
			notification(NOTIFICATION_VISIBILITY_CHANGED);
			first_draw = false;
		}
		drawing = true;
		notification(NOTIFICATION_DRAW);
		emit_signal(SceneStringNames::get_singleton()->draw);
		if (get_script_instance()) {
			Variant::CallError err;
			get_script_instance()->call_multilevel_reversed(SceneStringNames::get_singleton()->_draw, NULL, 0);
		}
		drawing = false;
	}
	//todo updating = false
	pending_update = false; // don't change to false until finished drawing (avoid recursive update)
}

Matrix32 CanvasItem::get_global_transform_with_canvas() const {

	const CanvasItem *ci = this;
	Matrix32 xform;
	const CanvasItem *last_valid = NULL;

	while (ci) {

		last_valid = ci;
		xform = ci->get_transform() * xform;
		ci = ci->get_parent_item();
	}

	if (last_valid->canvas_layer)
		return last_valid->canvas_layer->get_transform() * xform;
	else if (is_inside_tree())
		return get_viewport()->get_canvas_transform() * xform;

	return xform;
}

Matrix32 CanvasItem::get_global_transform() const {

	if (global_invalid) {

		const CanvasItem *pi = get_parent_item();
		if (pi)
			global_transform = pi->get_global_transform() * get_transform();
		else
			global_transform = get_transform();

		global_invalid = false;
	}

	return global_transform;
}

void CanvasItem::_queue_sort_children() {

	if (pending_children_sort)
		return;

	pending_children_sort = true;
	MessageQueue::get_singleton()->push_call(this, "_sort_children");
}

void CanvasItem::_sort_children() {

	pending_children_sort = false;

	if (!is_inside_tree())
		return;

	for (int i = 0; i < get_child_count(); i++) {

		Node *n = get_child(i);
		CanvasItem *ci = n->cast_to<CanvasItem>();

		if (ci) {
			if (ci->toplevel || ci->group != "")
				continue;
			VisualServer::get_singleton()->canvas_item_raise(n->cast_to<CanvasItem>()->canvas_item);
		}
	}
}

void CanvasItem::_raise_self() {

	if (!is_inside_tree())
		return;

	VisualServer::get_singleton()->canvas_item_raise(canvas_item);
}

void CanvasItem::_enter_canvas() {

	if ((!get_parent() || !get_parent()->cast_to<CanvasItem>()) || toplevel) {

		Node *n = this;

		canvas_layer = NULL;

		while (n) {

			canvas_layer = n->cast_to<CanvasLayer>();
			if (canvas_layer) {
				break;
			}
			n = n->get_parent();
		}

		RID canvas;
		if (canvas_layer)
			canvas = canvas_layer->get_world_2d()->get_canvas();
		else
			canvas = get_viewport()->find_world_2d()->get_canvas();

		VisualServer::get_singleton()->canvas_item_set_parent(canvas_item, canvas);

		group = "root_canvas" + itos(canvas.get_id());

		add_to_group(group);
		get_tree()->call_group(SceneTree::GROUP_CALL_UNIQUE, group, "_raise_self");

	} else {

		CanvasItem *parent = get_parent_item();
		canvas_layer = parent->canvas_layer;
		VisualServer::get_singleton()->canvas_item_set_parent(canvas_item, parent->get_canvas_item());
		parent->_queue_sort_children();
	}

	pending_update = false;
	update();

	notification(NOTIFICATION_ENTER_CANVAS);
}

void CanvasItem::_exit_canvas() {

	notification(NOTIFICATION_EXIT_CANVAS, true); //reverse the notification
	VisualServer::get_singleton()->canvas_item_set_parent(canvas_item, RID());
	canvas_layer = NULL;
	group = "";
}

void CanvasItem::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {

			first_draw = true;
			pending_children_sort = false;
			if (get_parent()) {
				CanvasItem *ci = get_parent()->cast_to<CanvasItem>();
				if (ci)
					C = ci->children_items.push_back(this);
			}
			_enter_canvas();
			if (!block_transform_notify && !xform_change.in_list()) {
				get_tree()->xform_change_list.add(&xform_change);
			}
		} break;
		case NOTIFICATION_MOVED_IN_PARENT: {

			if (group != "") {
				get_tree()->call_group(SceneTree::GROUP_CALL_UNIQUE, group, "_raise_self");
			} else {
				CanvasItem *p = get_parent_item();
				ERR_FAIL_COND(!p);
				p->_queue_sort_children();
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (xform_change.in_list())
				get_tree()->xform_change_list.remove(&xform_change);
			_exit_canvas();
			if (C) {
				get_parent()->cast_to<CanvasItem>()->children_items.erase(C);
				C = NULL;
			}
			global_invalid = true;
		} break;
		case NOTIFICATION_DRAW: {

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {

		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {

			emit_signal(SceneStringNames::get_singleton()->visibility_changed);
		} break;
	}
}

void CanvasItem::_set_visible_(bool p_visible) {

	if (p_visible)
		show();
	else
		hide();
}
bool CanvasItem::_is_visible_() const {

	return !is_hidden();
}

void CanvasItem::update() {

	if (!is_inside_tree())
		return;
	if (pending_update)
		return;

	pending_update = true;

	MessageQueue::get_singleton()->push_call(this, "_update_callback");
}

void CanvasItem::set_opacity(float p_opacity) {

	opacity = p_opacity;
	VisualServer::get_singleton()->canvas_item_set_opacity(canvas_item, opacity);
}
float CanvasItem::get_opacity() const {

	return opacity;
}

void CanvasItem::set_as_toplevel(bool p_toplevel) {

	if (toplevel == p_toplevel)
		return;

	if (!is_inside_tree()) {
		toplevel = p_toplevel;
		return;
	}

	_exit_canvas();
	toplevel = p_toplevel;
	_enter_canvas();
}

bool CanvasItem::is_set_as_toplevel() const {

	return toplevel;
}

CanvasItem *CanvasItem::get_parent_item() const {

	if (toplevel)
		return NULL;

	Node *parent = get_parent();
	if (!parent)
		return NULL;

	return parent->cast_to<CanvasItem>();
}

void CanvasItem::set_self_opacity(float p_self_opacity) {

	self_opacity = p_self_opacity;
	VisualServer::get_singleton()->canvas_item_set_self_opacity(canvas_item, self_opacity);
}
float CanvasItem::get_self_opacity() const {

	return self_opacity;
}

void CanvasItem::set_blend_mode(BlendMode p_blend_mode) {

	ERR_FAIL_INDEX(p_blend_mode, 5);
	blend_mode = p_blend_mode;
	VisualServer::get_singleton()->canvas_item_set_blend_mode(canvas_item, VS::MaterialBlendMode(blend_mode));
}

CanvasItem::BlendMode CanvasItem::get_blend_mode() const {

	return blend_mode;
}

void CanvasItem::set_light_mask(int p_light_mask) {

	light_mask = p_light_mask;
	VS::get_singleton()->canvas_item_set_light_mask(canvas_item, p_light_mask);
}

int CanvasItem::get_light_mask() const {

	return light_mask;
}

void CanvasItem::item_rect_changed() {

	update();
	emit_signal(SceneStringNames::get_singleton()->item_rect_changed);
}

void CanvasItem::draw_line(const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	VisualServer::get_singleton()->canvas_item_add_line(canvas_item, p_from, p_to, p_color, p_width);
}

void CanvasItem::draw_rect(const Rect2 &p_rect, const Color &p_color) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	VisualServer::get_singleton()->canvas_item_add_rect(canvas_item, p_rect, p_color);
}

void CanvasItem::draw_circle(const Point2 &p_pos, float p_radius, const Color &p_color) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	VisualServer::get_singleton()->canvas_item_add_circle(canvas_item, p_pos, p_radius, p_color);
}

void CanvasItem::draw_texture(const Ref<Texture> &p_texture, const Point2 &p_pos, const Color &p_modulate) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	ERR_FAIL_COND(p_texture.is_null());

	p_texture->draw(canvas_item, p_pos, p_modulate);
}

void CanvasItem::draw_texture_rect(const Ref<Texture> &p_texture, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	ERR_FAIL_COND(p_texture.is_null());
	p_texture->draw_rect(canvas_item, p_rect, p_tile, p_modulate, p_transpose);
}
void CanvasItem::draw_texture_rect_region(const Ref<Texture> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}
	ERR_FAIL_COND(p_texture.is_null());
	p_texture->draw_rect_region(canvas_item, p_rect, p_src_rect, p_modulate, p_transpose);
}

void CanvasItem::draw_style_box(const Ref<StyleBox> &p_style_box, const Rect2 &p_rect) {
	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	ERR_FAIL_COND(p_style_box.is_null());

	p_style_box->draw(canvas_item, p_rect);
}
void CanvasItem::draw_primitive(const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, Ref<Texture> p_texture, float p_width) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	RID rid = p_texture.is_valid() ? p_texture->get_rid() : RID();

	VisualServer::get_singleton()->canvas_item_add_primitive(canvas_item, p_points, p_colors, p_uvs, rid, p_width);
}
void CanvasItem::draw_set_transform(const Point2 &p_offset, float p_rot, const Size2 &p_scale) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	Matrix32 xform(p_rot, p_offset);
	xform.scale_basis(p_scale);
	VisualServer::get_singleton()->canvas_item_add_set_transform(canvas_item, xform);
}

void CanvasItem::draw_set_transform_matrix(const Matrix32 &p_matrix) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	VisualServer::get_singleton()->canvas_item_add_set_transform(canvas_item, p_matrix);
}

void CanvasItem::draw_polygon(const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, Ref<Texture> p_texture) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	RID rid = p_texture.is_valid() ? p_texture->get_rid() : RID();

	VisualServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, p_colors, p_uvs, rid);
}

void CanvasItem::draw_colored_polygon(const Vector<Point2> &p_points, const Color &p_color, const Vector<Point2> &p_uvs, Ref<Texture> p_texture) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	Vector<Color> colors;
	colors.push_back(p_color);
	RID rid = p_texture.is_valid() ? p_texture->get_rid() : RID();

	VisualServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, colors, p_uvs, rid);
}

void CanvasItem::draw_string(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_text, const Color &p_modulate, int p_clip_w) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL();
	}

	ERR_FAIL_COND(p_font.is_null());
	p_font->draw(canvas_item, p_pos, p_text, p_modulate, p_clip_w);
}

float CanvasItem::draw_char(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_char, const String &p_next, const Color &p_modulate) {

	if (!drawing) {
		ERR_EXPLAIN("Drawing is only allowed inside NOTIFICATION_DRAW, _draw() function or 'draw' signal.");
		ERR_FAIL_V(0);
	}

	ERR_FAIL_COND_V(p_char.length() != 1, 0);
	ERR_FAIL_COND_V(p_font.is_null(), 0);

	return p_font->draw_char(canvas_item, p_pos, p_char[0], p_next.c_str()[0], p_modulate);
}

void CanvasItem::_notify_transform(CanvasItem *p_node) {

	if (p_node->xform_change.in_list() && p_node->global_invalid)
		return; //nothing to do

	p_node->global_invalid = true;

	if (!p_node->xform_change.in_list()) {
		if (!p_node->block_transform_notify) {
			if (p_node->is_inside_tree())
				get_tree()->xform_change_list.add(&p_node->xform_change);
		}
	}

	for (List<CanvasItem *>::Element *E = p_node->children_items.front(); E; E = E->next()) {

		CanvasItem *ci = E->get();
		if (ci->toplevel)
			continue;
		_notify_transform(ci);
	}
}

Rect2 CanvasItem::get_viewport_rect() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
	return get_viewport()->get_visible_rect();
}

RID CanvasItem::get_canvas() const {

	ERR_FAIL_COND_V(!is_inside_tree(), RID());

	if (canvas_layer)
		return canvas_layer->get_world_2d()->get_canvas();
	else
		return get_viewport()->find_world_2d()->get_canvas();
}

CanvasItem *CanvasItem::get_toplevel() const {

	CanvasItem *ci = const_cast<CanvasItem *>(this);
	while (!ci->toplevel && ci->get_parent() && ci->get_parent()->cast_to<CanvasItem>()) {
		ci = ci->get_parent()->cast_to<CanvasItem>();
	}

	return ci;
}

Ref<World2D> CanvasItem::get_world_2d() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Ref<World2D>());

	CanvasItem *tl = get_toplevel();

	if (tl->canvas_layer) {
		return tl->canvas_layer->get_world_2d();
	} else if (tl->get_viewport()) {
		return tl->get_viewport()->find_world_2d();
	} else {
		return Ref<World2D>();
	}
}

RID CanvasItem::get_viewport_rid() const {

	ERR_FAIL_COND_V(!is_inside_tree(), RID());
	return get_viewport()->get_viewport();
}

void CanvasItem::set_block_transform_notify(bool p_enable) {
	block_transform_notify = p_enable;
}

bool CanvasItem::is_block_transform_notify_enabled() const {

	return block_transform_notify;
}

void CanvasItem::set_draw_behind_parent(bool p_enable) {

	if (behind == p_enable)
		return;
	behind = p_enable;
	VisualServer::get_singleton()->canvas_item_set_on_top(canvas_item, !behind);
}

bool CanvasItem::is_draw_behind_parent_enabled() const {

	return behind;
}

void CanvasItem::set_material(const Ref<CanvasItemMaterial> &p_material) {

	material = p_material;
	RID rid;
	if (material.is_valid())
		rid = material->get_rid();
	VS::get_singleton()->canvas_item_set_material(canvas_item, rid);
	_change_notify(); //properties for material exposed
}

void CanvasItem::set_use_parent_material(bool p_use_parent_material) {

	use_parent_material = p_use_parent_material;
	VS::get_singleton()->canvas_item_set_use_parent_material(canvas_item, p_use_parent_material);
}

bool CanvasItem::get_use_parent_material() const {

	return use_parent_material;
}

Ref<CanvasItemMaterial> CanvasItem::get_material() const {

	return material;
}

Vector2 CanvasItem::make_canvas_pos_local(const Vector2 &screen_point) const {

	ERR_FAIL_COND_V(!is_inside_tree(), screen_point);

	Matrix32 local_matrix = (get_canvas_transform() *
							 get_global_transform())
									.affine_inverse();

	return local_matrix.xform(screen_point);
}

InputEvent CanvasItem::make_input_local(const InputEvent &p_event) const {

	ERR_FAIL_COND_V(!is_inside_tree(), p_event);

	return p_event.xform_by((get_canvas_transform() * get_global_transform()).affine_inverse());
}

Vector2 CanvasItem::get_global_mouse_pos() const {

	ERR_FAIL_COND_V(!get_viewport(), Vector2());
	return get_canvas_transform().affine_inverse().xform(get_viewport()->get_mouse_pos());
}
Vector2 CanvasItem::get_local_mouse_pos() const {

	ERR_FAIL_COND_V(!get_viewport(), Vector2());

	return get_global_transform().affine_inverse().xform(get_global_mouse_pos());
}

void CanvasItem::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_sort_children"), &CanvasItem::_sort_children);
	ObjectTypeDB::bind_method(_MD("_raise_self"), &CanvasItem::_raise_self);
	ObjectTypeDB::bind_method(_MD("_update_callback"), &CanvasItem::_update_callback);
	ObjectTypeDB::bind_method(_MD("_set_visible_"), &CanvasItem::_set_visible_);
	ObjectTypeDB::bind_method(_MD("_is_visible_"), &CanvasItem::_is_visible_);

	ObjectTypeDB::bind_method(_MD("edit_set_state", "state"), &CanvasItem::edit_set_state);
	ObjectTypeDB::bind_method(_MD("edit_get_state:Variant"), &CanvasItem::edit_get_state);
	ObjectTypeDB::bind_method(_MD("edit_set_rect", "rect"), &CanvasItem::edit_set_rect);
	ObjectTypeDB::bind_method(_MD("edit_rotate", "degrees"), &CanvasItem::edit_rotate);

	ObjectTypeDB::bind_method(_MD("get_item_rect"), &CanvasItem::get_item_rect);
	ObjectTypeDB::bind_method(_MD("get_item_and_children_rect"), &CanvasItem::get_item_and_children_rect);
	//ObjectTypeDB::bind_method(_MD("get_transform"),&CanvasItem::get_transform);

	ObjectTypeDB::bind_method(_MD("get_canvas_item"), &CanvasItem::get_canvas_item);

	ObjectTypeDB::bind_method(_MD("is_visible"), &CanvasItem::is_visible);
	ObjectTypeDB::bind_method(_MD("is_hidden"), &CanvasItem::is_hidden);
	ObjectTypeDB::bind_method(_MD("show"), &CanvasItem::show);
	ObjectTypeDB::bind_method(_MD("hide"), &CanvasItem::hide);
	ObjectTypeDB::bind_method(_MD("set_hidden", "hidden"), &CanvasItem::set_hidden);

	ObjectTypeDB::bind_method(_MD("update"), &CanvasItem::update);

	ObjectTypeDB::bind_method(_MD("set_as_toplevel", "enable"), &CanvasItem::set_as_toplevel);
	ObjectTypeDB::bind_method(_MD("is_set_as_toplevel"), &CanvasItem::is_set_as_toplevel);

	ObjectTypeDB::bind_method(_MD("set_blend_mode", "blend_mode"), &CanvasItem::set_blend_mode);
	ObjectTypeDB::bind_method(_MD("get_blend_mode"), &CanvasItem::get_blend_mode);

	ObjectTypeDB::bind_method(_MD("set_light_mask", "light_mask"), &CanvasItem::set_light_mask);
	ObjectTypeDB::bind_method(_MD("get_light_mask"), &CanvasItem::get_light_mask);

	ObjectTypeDB::bind_method(_MD("set_opacity", "opacity"), &CanvasItem::set_opacity);
	ObjectTypeDB::bind_method(_MD("get_opacity"), &CanvasItem::get_opacity);
	ObjectTypeDB::bind_method(_MD("set_self_opacity", "self_opacity"), &CanvasItem::set_self_opacity);
	ObjectTypeDB::bind_method(_MD("get_self_opacity"), &CanvasItem::get_self_opacity);

	ObjectTypeDB::bind_method(_MD("set_draw_behind_parent", "enable"), &CanvasItem::set_draw_behind_parent);
	ObjectTypeDB::bind_method(_MD("is_draw_behind_parent_enabled"), &CanvasItem::is_draw_behind_parent_enabled);

	ObjectTypeDB::bind_method(_MD("_set_on_top", "on_top"), &CanvasItem::_set_on_top);
	ObjectTypeDB::bind_method(_MD("_is_on_top"), &CanvasItem::_is_on_top);
	//ObjectTypeDB::bind_method(_MD("get_transform"),&CanvasItem::get_transform);

	ObjectTypeDB::bind_method(_MD("draw_line", "from", "to", "color", "width"), &CanvasItem::draw_line, DEFVAL(1.0));
	ObjectTypeDB::bind_method(_MD("draw_rect", "rect", "color"), &CanvasItem::draw_rect);
	ObjectTypeDB::bind_method(_MD("draw_circle", "pos", "radius", "color"), &CanvasItem::draw_circle);
	ObjectTypeDB::bind_method(_MD("draw_texture", "texture:Texture", "pos", "modulate"), &CanvasItem::draw_texture, DEFVAL(Color(1, 1, 1, 1)));
	ObjectTypeDB::bind_method(_MD("draw_texture_rect", "texture:Texture", "rect", "tile", "modulate", "transpose"), &CanvasItem::draw_texture_rect, DEFVAL(Color(1, 1, 1)), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("draw_texture_rect_region", "texture:Texture", "rect", "src_rect", "modulate", "transpose"), &CanvasItem::draw_texture_rect_region, DEFVAL(Color(1, 1, 1)), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("draw_style_box", "style_box:StyleBox", "rect"), &CanvasItem::draw_style_box);
	ObjectTypeDB::bind_method(_MD("draw_primitive", "points", "colors", "uvs", "texture:Texture", "width"), &CanvasItem::draw_primitive, DEFVAL(Variant()), DEFVAL(1.0));
	ObjectTypeDB::bind_method(_MD("draw_polygon", "points", "colors", "uvs", "texture:Texture"), &CanvasItem::draw_polygon, DEFVAL(Vector2Array()), DEFVAL(Variant()));
	ObjectTypeDB::bind_method(_MD("draw_colored_polygon", "points", "color", "uvs", "texture:Texture"), &CanvasItem::draw_colored_polygon, DEFVAL(Vector2Array()), DEFVAL(Variant()));
	ObjectTypeDB::bind_method(_MD("draw_string", "font:Font", "pos", "text", "modulate", "clip_w"), &CanvasItem::draw_string, DEFVAL(Color(1, 1, 1)), DEFVAL(-1));
	ObjectTypeDB::bind_method(_MD("draw_char", "font:Font", "pos", "char", "next", "modulate"), &CanvasItem::draw_char, DEFVAL(Color(1, 1, 1)));

	ObjectTypeDB::bind_method(_MD("draw_set_transform", "pos", "rot", "scale"), &CanvasItem::draw_set_transform);
	ObjectTypeDB::bind_method(_MD("draw_set_transform_matrix", "xform"), &CanvasItem::draw_set_transform_matrix);
	ObjectTypeDB::bind_method(_MD("get_transform"), &CanvasItem::get_transform);
	ObjectTypeDB::bind_method(_MD("get_global_transform"), &CanvasItem::get_global_transform);
	ObjectTypeDB::bind_method(_MD("get_global_transform_with_canvas"), &CanvasItem::get_global_transform_with_canvas);
	ObjectTypeDB::bind_method(_MD("get_viewport_transform"), &CanvasItem::get_viewport_transform);
	ObjectTypeDB::bind_method(_MD("get_viewport_rect"), &CanvasItem::get_viewport_rect);
	ObjectTypeDB::bind_method(_MD("get_canvas_transform"), &CanvasItem::get_canvas_transform);
	ObjectTypeDB::bind_method(_MD("get_local_mouse_pos"), &CanvasItem::get_local_mouse_pos);
	ObjectTypeDB::bind_method(_MD("get_global_mouse_pos"), &CanvasItem::get_global_mouse_pos);
	ObjectTypeDB::bind_method(_MD("get_canvas"), &CanvasItem::get_canvas);
	ObjectTypeDB::bind_method(_MD("get_world_2d"), &CanvasItem::get_world_2d);
	//ObjectTypeDB::bind_method(_MD("get_viewport"),&CanvasItem::get_viewport);

	ObjectTypeDB::bind_method(_MD("set_material", "material:CanvasItemMaterial"), &CanvasItem::set_material);
	ObjectTypeDB::bind_method(_MD("get_material:CanvasItemMaterial"), &CanvasItem::get_material);

	ObjectTypeDB::bind_method(_MD("set_use_parent_material", "enable"), &CanvasItem::set_use_parent_material);
	ObjectTypeDB::bind_method(_MD("get_use_parent_material"), &CanvasItem::get_use_parent_material);

	ObjectTypeDB::bind_method(_MD("make_canvas_pos_local", "screen_point"),
			&CanvasItem::make_canvas_pos_local);
	ObjectTypeDB::bind_method(_MD("make_input_local", "event"), &CanvasItem::make_input_local);

	BIND_VMETHOD(MethodInfo("_draw"));

	ADD_PROPERTYNO(PropertyInfo(Variant::BOOL, "visibility/visible"), _SCS("_set_visible_"), _SCS("_is_visible_"));
	ADD_PROPERTYNO(PropertyInfo(Variant::REAL, "visibility/opacity", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_opacity"), _SCS("get_opacity"));
	ADD_PROPERTYNO(PropertyInfo(Variant::REAL, "visibility/self_opacity", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_self_opacity"), _SCS("get_self_opacity"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "visibility/behind_parent"), _SCS("set_draw_behind_parent"), _SCS("is_draw_behind_parent_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visibility/on_top", PROPERTY_HINT_NONE, "", 0), _SCS("_set_on_top"), _SCS("_is_on_top")); //compatibility

	ADD_PROPERTYNZ(PropertyInfo(Variant::INT, "visibility/blend_mode", PROPERTY_HINT_ENUM, "Mix,Add,Sub,Mul,PMAlpha"), _SCS("set_blend_mode"), _SCS("get_blend_mode"));
	ADD_PROPERTYNO(PropertyInfo(Variant::INT, "visibility/light_mask", PROPERTY_HINT_ALL_FLAGS), _SCS("set_light_mask"), _SCS("get_light_mask"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "material/material", PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemMaterial"), _SCS("set_material"), _SCS("get_material"));
	ADD_PROPERTYNZ(PropertyInfo(Variant::BOOL, "material/use_parent"), _SCS("set_use_parent_material"), _SCS("get_use_parent_material"));
	//exporting these two things doesn't really make much sense i think
	//ADD_PROPERTY( PropertyInfo(Variant::BOOL,"transform/toplevel"), _SCS("set_as_toplevel"),_SCS("is_set_as_toplevel") );
	//ADD_PROPERTY(PropertyInfo(Variant::BOOL,"transform/notify"),_SCS("set_transform_notify"),_SCS("is_transform_notify_enabled"));

	ADD_SIGNAL(MethodInfo("draw"));
	ADD_SIGNAL(MethodInfo("visibility_changed"));
	ADD_SIGNAL(MethodInfo("hide"));
	ADD_SIGNAL(MethodInfo("item_rect_changed"));

	BIND_CONSTANT(BLEND_MODE_MIX);
	BIND_CONSTANT(BLEND_MODE_ADD);
	BIND_CONSTANT(BLEND_MODE_SUB);
	BIND_CONSTANT(BLEND_MODE_MUL);
	BIND_CONSTANT(BLEND_MODE_PREMULT_ALPHA);

	BIND_CONSTANT(NOTIFICATION_DRAW);
	BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
	BIND_CONSTANT(NOTIFICATION_ENTER_CANVAS);
	BIND_CONSTANT(NOTIFICATION_EXIT_CANVAS);
	BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
}

Matrix32 CanvasItem::get_canvas_transform() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Matrix32());

	if (canvas_layer)
		return canvas_layer->get_transform();
	else if (get_parent()->cast_to<CanvasItem>())
		return get_parent()->cast_to<CanvasItem>()->get_canvas_transform();
	else
		return get_viewport()->get_canvas_transform();
}

Matrix32 CanvasItem::get_viewport_transform() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Matrix32());

	if (canvas_layer) {

		if (get_viewport()) {
			return get_viewport()->get_final_transform() * canvas_layer->get_transform();
		} else {
			return canvas_layer->get_transform();
		}

	} else {
		return get_viewport()->get_final_transform() * get_viewport()->get_canvas_transform();
	}
}

void CanvasItem::set_notify_local_transform(bool p_enable) {
	notify_local_transform = p_enable;
}

bool CanvasItem::is_local_transform_notification_enabled() const {
	return notify_local_transform;
}

int CanvasItem::get_canvas_layer() const {

	if (canvas_layer)
		return canvas_layer->get_layer();
	else
		return 0;
}

Rect2 CanvasItem::get_item_and_children_rect() const {

	Rect2 rect = get_item_rect();

	for (int i = 0; i < get_child_count(); i++) {
		CanvasItem *c = get_child(i)->cast_to<CanvasItem>();
		if (c) {
			Rect2 sir = c->get_transform().xform(c->get_item_and_children_rect());
			rect = rect.merge(sir);
		}
	}

	return rect;
}

CanvasItem::CanvasItem() :
		xform_change(this) {

	canvas_item = VisualServer::get_singleton()->canvas_item_create();
	hidden = false;
	pending_update = false;
	opacity = 1;
	self_opacity = 1;
	toplevel = false;
	pending_children_sort = false;
	first_draw = false;
	blend_mode = BLEND_MODE_MIX;
	drawing = false;
	behind = false;
	block_transform_notify = false;
	//	viewport=NULL;
	canvas_layer = NULL;
	use_parent_material = false;
	global_invalid = true;
	notify_local_transform = false;
	light_mask = 1;

	C = NULL;
}

CanvasItem::~CanvasItem() {

	VisualServer::get_singleton()->free(canvas_item);
}
