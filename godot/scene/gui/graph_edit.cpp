/*************************************************************************/
/*  graph_edit.cpp                                                       */
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
#include "graph_edit.h"
#include "os/input.h"
#include "os/keyboard.h"
#include "scene/gui/box_container.h"

#define ZOOM_SCALE 1.2

#define MIN_ZOOM (((1 / ZOOM_SCALE) / ZOOM_SCALE) / ZOOM_SCALE)
#define MAX_ZOOM (1 * ZOOM_SCALE * ZOOM_SCALE * ZOOM_SCALE)

bool GraphEditFilter::has_point(const Point2 &p_point) const {

	return ge->_filter_input(p_point);
}

GraphEditFilter::GraphEditFilter(GraphEdit *p_edit) {

	ge = p_edit;
}

Error GraphEdit::connect_node(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

	if (is_node_connected(p_from, p_from_port, p_to, p_to_port))
		return OK;
	Connection c;
	c.from = p_from;
	c.from_port = p_from_port;
	c.to = p_to;
	c.to_port = p_to_port;
	connections.push_back(c);
	top_layer->update();

	return OK;
}

bool GraphEdit::is_node_connected(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

	for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {

		if (E->get().from == p_from && E->get().from_port == p_from_port && E->get().to == p_to && E->get().to_port == p_to_port)
			return true;
	}

	return false;
}

void GraphEdit::disconnect_node(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {

	for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {

		if (E->get().from == p_from && E->get().from_port == p_from_port && E->get().to == p_to && E->get().to_port == p_to_port) {

			connections.erase(E);
			top_layer->update();
			return;
		}
	}
}

bool GraphEdit::clips_input() const {

	return true;
}

void GraphEdit::get_connection_list(List<Connection> *r_connections) const {

	*r_connections = connections;
}

Vector2 GraphEdit::get_scroll_ofs() const {

	return Vector2(h_scroll->get_val(), v_scroll->get_val());
}

void GraphEdit::_scroll_moved(double) {

	_update_scroll_offset();
	top_layer->update();
}

void GraphEdit::_update_scroll_offset() {

	for (int i = 0; i < get_child_count(); i++) {

		GraphNode *gn = get_child(i)->cast_to<GraphNode>();
		if (!gn)
			continue;

		Point2 pos = gn->get_offset() * zoom;
		pos -= Point2(h_scroll->get_val(), v_scroll->get_val());
		gn->set_pos(pos);
		gn->set_scale(Vector2(zoom, zoom));
	}
}

void GraphEdit::_update_scroll() {

	if (updating)
		return;

	updating = true;
	Rect2 screen;
	for (int i = 0; i < get_child_count(); i++) {

		GraphNode *gn = get_child(i)->cast_to<GraphNode>();
		if (!gn)
			continue;

		Rect2 r;
		r.pos = gn->get_offset() * zoom;
		r.size = gn->get_size() * zoom;
		screen = screen.merge(r);
	}

	screen.pos -= get_size();
	screen.size += get_size() * 2.0;

	h_scroll->set_min(screen.pos.x);
	h_scroll->set_max(screen.pos.x + screen.size.x);
	h_scroll->set_page(get_size().x);
	if (h_scroll->get_max() - h_scroll->get_min() <= h_scroll->get_page())
		h_scroll->hide();
	else
		h_scroll->show();

	v_scroll->set_min(screen.pos.y);
	v_scroll->set_max(screen.pos.y + screen.size.y);
	v_scroll->set_page(get_size().y);

	if (v_scroll->get_max() - v_scroll->get_min() <= v_scroll->get_page())
		v_scroll->hide();
	else
		v_scroll->show();

	_update_scroll_offset();
	updating = false;
}

void GraphEdit::_graph_node_raised(Node *p_gn) {

	GraphNode *gn = p_gn->cast_to<GraphNode>();
	ERR_FAIL_COND(!gn);
	gn->raise();
	top_layer->raise();
}

void GraphEdit::_graph_node_moved(Node *p_gn) {

	GraphNode *gn = p_gn->cast_to<GraphNode>();
	ERR_FAIL_COND(!gn);
	top_layer->update();
}

void GraphEdit::add_child_notify(Node *p_child) {

	top_layer->call_deferred("raise"); //top layer always on top!
	GraphNode *gn = p_child->cast_to<GraphNode>();
	if (gn) {
		gn->set_scale(Vector2(zoom, zoom));
		gn->connect("offset_changed", this, "_graph_node_moved", varray(gn));
		gn->connect("raise_request", this, "_graph_node_raised", varray(gn));
		_graph_node_moved(gn);
		gn->set_stop_mouse(false);
	}
}

void GraphEdit::remove_child_notify(Node *p_child) {

	top_layer->call_deferred("raise"); //top layer always on top!
	GraphNode *gn = p_child->cast_to<GraphNode>();
	if (gn) {
		gn->disconnect("offset_changed", this, "_graph_node_moved");
		gn->disconnect("raise_request", this, "_graph_node_raised");
	}
}

void GraphEdit::_notification(int p_what) {

	if (p_what == NOTIFICATION_READY) {
		Size2 hmin = h_scroll->get_combined_minimum_size();
		Size2 vmin = v_scroll->get_combined_minimum_size();

		v_scroll->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, vmin.width);
		v_scroll->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0);
		v_scroll->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 0);
		v_scroll->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0);

		h_scroll->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 0);
		h_scroll->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0);
		h_scroll->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, hmin.height);
		h_scroll->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0);

		//		zoom_icon->set_texture( get_icon("Zoom", "EditorIcons"));
	}
	if (p_what == NOTIFICATION_DRAW) {
		draw_style_box(get_stylebox("bg"), Rect2(Point2(), get_size()));
		VS::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
	}

	if (p_what == NOTIFICATION_RESIZED) {
		_update_scroll();
		top_layer->update();
	}
}

bool GraphEdit::_filter_input(const Point2 &p_point) {

	Ref<Texture> port = get_icon("port", "GraphNode");

	float grab_r = port->get_width() * 0.5;
	for (int i = get_child_count() - 1; i >= 0; i--) {

		GraphNode *gn = get_child(i)->cast_to<GraphNode>();
		if (!gn)
			continue;

		for (int j = 0; j < gn->get_connection_output_count(); j++) {

			Vector2 pos = gn->get_connection_output_pos(j) + gn->get_pos();
			if (pos.distance_to(p_point) < grab_r)
				return true;
		}

		for (int j = 0; j < gn->get_connection_input_count(); j++) {

			Vector2 pos = gn->get_connection_input_pos(j) + gn->get_pos();
			if (pos.distance_to(p_point) < grab_r)
				return true;
		}
	}

	return false;
}

void GraphEdit::_top_layer_input(const InputEvent &p_ev) {

	if (p_ev.type == InputEvent::MOUSE_BUTTON && p_ev.mouse_button.button_index == BUTTON_LEFT && p_ev.mouse_button.pressed) {

		Ref<Texture> port = get_icon("port", "GraphNode");
		Vector2 mpos(p_ev.mouse_button.x, p_ev.mouse_button.y);
		float grab_r = port->get_width() * 0.5;
		for (int i = get_child_count() - 1; i >= 0; i--) {

			GraphNode *gn = get_child(i)->cast_to<GraphNode>();
			if (!gn)
				continue;

			for (int j = 0; j < gn->get_connection_output_count(); j++) {

				Vector2 pos = gn->get_connection_output_pos(j) + gn->get_pos();
				if (pos.distance_to(mpos) < grab_r) {

					connecting = true;
					connecting_from = gn->get_name();
					connecting_index = j;
					connecting_out = true;
					connecting_type = gn->get_connection_output_type(j);
					connecting_color = gn->get_connection_output_color(j);
					connecting_target = false;
					connecting_to = pos;
					return;
				}
			}

			for (int j = 0; j < gn->get_connection_input_count(); j++) {

				Vector2 pos = gn->get_connection_input_pos(j) + gn->get_pos();

				if (pos.distance_to(mpos) < grab_r) {

					if (right_disconnects) {
						//check disconnect
						for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {

							if (E->get().to == gn->get_name() && E->get().to_port == j) {

								Node *fr = get_node(String(E->get().from));
								if (fr && fr->cast_to<GraphNode>()) {

									connecting_from = E->get().from;
									connecting_index = E->get().from_port;
									connecting_out = true;
									connecting_type = fr->cast_to<GraphNode>()->get_connection_output_type(E->get().from_port);
									connecting_color = fr->cast_to<GraphNode>()->get_connection_output_color(E->get().from_port);
									connecting_target = false;
									connecting_to = pos;

									emit_signal("disconnection_request", E->get().from, E->get().from_port, E->get().to, E->get().to_port);
									fr = get_node(String(connecting_from)); //maybe it was erased
									if (fr && fr->cast_to<GraphNode>()) {
										connecting = true;
									}
									return;
								}
							}
						}
					}

					connecting = true;
					connecting_from = gn->get_name();
					connecting_index = j;
					connecting_out = false;
					connecting_type = gn->get_connection_input_type(j);
					connecting_color = gn->get_connection_input_color(j);
					connecting_target = false;
					connecting_to = pos;
					return;
				}
			}
		}
	}

	if (p_ev.type == InputEvent::MOUSE_MOTION && connecting) {

		connecting_to = Vector2(p_ev.mouse_motion.x, p_ev.mouse_motion.y);
		connecting_target = false;
		top_layer->update();

		Ref<Texture> port = get_icon("port", "GraphNode");
		Vector2 mpos(p_ev.mouse_button.x, p_ev.mouse_button.y);
		float grab_r = port->get_width() * 0.5;
		for (int i = get_child_count() - 1; i >= 0; i--) {

			GraphNode *gn = get_child(i)->cast_to<GraphNode>();
			if (!gn)
				continue;

			if (!connecting_out) {
				for (int j = 0; j < gn->get_connection_output_count(); j++) {

					Vector2 pos = gn->get_connection_output_pos(j) + gn->get_pos();
					int type = gn->get_connection_output_type(j);
					if (type == connecting_type && pos.distance_to(mpos) < grab_r) {

						connecting_target = true;
						connecting_to = pos;
						connecting_target_to = gn->get_name();
						connecting_target_index = j;
						return;
					}
				}
			} else {

				for (int j = 0; j < gn->get_connection_input_count(); j++) {

					Vector2 pos = gn->get_connection_input_pos(j) + gn->get_pos();
					int type = gn->get_connection_input_type(j);
					if (type == connecting_type && pos.distance_to(mpos) < grab_r) {
						connecting_target = true;
						connecting_to = pos;
						connecting_target_to = gn->get_name();
						connecting_target_index = j;
						return;
					}
				}
			}
		}
	}

	if (p_ev.type == InputEvent::MOUSE_BUTTON && p_ev.mouse_button.button_index == BUTTON_LEFT && !p_ev.mouse_button.pressed) {

		if (connecting && connecting_target) {

			String from = connecting_from;
			int from_slot = connecting_index;
			String to = connecting_target_to;
			int to_slot = connecting_target_index;

			if (!connecting_out) {
				SWAP(from, to);
				SWAP(from_slot, to_slot);
			}
			emit_signal("connection_request", from, from_slot, to, to_slot);
		}
		connecting = false;
		top_layer->update();
	}
}

void GraphEdit::_draw_cos_line(const Vector2 &p_from, const Vector2 &p_to, const Color &p_color) {

	static const int steps = 20;

	Rect2 r;
	r.pos = p_from;
	r.expand_to(p_to);
	Vector2 sign = Vector2((p_from.x < p_to.x) ? 1 : -1, (p_from.y < p_to.y) ? 1 : -1);
	bool flip = sign.x * sign.y < 0;

	Vector2 prev;
	for (int i = 0; i <= steps; i++) {

		float d = i / float(steps);
		float c = -Math::cos(d * Math_PI) * 0.5 + 0.5;
		if (flip)
			c = 1.0 - c;
		Vector2 p = r.pos + Vector2(d * r.size.width, c * r.size.height);

		if (i > 0) {

			top_layer->draw_line(prev, p, p_color, 2);
		}

		prev = p;
	}
}

void GraphEdit::_top_layer_draw() {

	_update_scroll();

	if (connecting) {

		Node *fromn = get_node(connecting_from);
		ERR_FAIL_COND(!fromn);
		GraphNode *from = fromn->cast_to<GraphNode>();
		ERR_FAIL_COND(!from);
		Vector2 pos;
		if (connecting_out)
			pos = from->get_connection_output_pos(connecting_index);
		else
			pos = from->get_connection_input_pos(connecting_index);
		pos += from->get_pos();

		Vector2 topos;
		topos = connecting_to;

		Color col = connecting_color;

		if (connecting_target) {
			col.r += 0.4;
			col.g += 0.4;
			col.b += 0.4;
		}
		_draw_cos_line(pos, topos, col);
	}

	List<List<Connection>::Element *> to_erase;
	for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {

		NodePath fromnp(E->get().from);

		Node *from = get_node(fromnp);
		if (!from) {
			to_erase.push_back(E);
			continue;
		}

		GraphNode *gfrom = from->cast_to<GraphNode>();

		if (!gfrom) {
			to_erase.push_back(E);
			continue;
		}

		NodePath tonp(E->get().to);
		Node *to = get_node(tonp);
		if (!to) {
			to_erase.push_back(E);
			continue;
		}

		GraphNode *gto = to->cast_to<GraphNode>();

		if (!gto) {
			to_erase.push_back(E);
			continue;
		}

		Vector2 frompos = gfrom->get_connection_output_pos(E->get().from_port) + gfrom->get_pos();
		Color color = gfrom->get_connection_output_color(E->get().from_port);
		Vector2 topos = gto->get_connection_input_pos(E->get().to_port) + gto->get_pos();
		_draw_cos_line(frompos, topos, color);
	}

	while (to_erase.size()) {
		connections.erase(to_erase.front()->get());
		to_erase.pop_front();
	}
	if (box_selecting)
		top_layer->draw_rect(box_selecting_rect, Color(0.7, 0.7, 1.0, 0.3));
}

void GraphEdit::_input_event(const InputEvent &p_ev) {

	if (p_ev.type == InputEvent::MOUSE_MOTION && (p_ev.mouse_motion.button_mask & BUTTON_MASK_MIDDLE || (p_ev.mouse_motion.button_mask & BUTTON_MASK_LEFT && Input::get_singleton()->is_key_pressed(KEY_SPACE)))) {
		h_scroll->set_val(h_scroll->get_val() - p_ev.mouse_motion.relative_x);
		v_scroll->set_val(v_scroll->get_val() - p_ev.mouse_motion.relative_y);
	}

	if (p_ev.type == InputEvent::MOUSE_MOTION && dragging) {

		just_selected = true;
		// TODO: Remove local mouse pos hack if/when InputEventMouseMotion is fixed to support floats
		//drag_accum+=Vector2(p_ev.mouse_motion.relative_x,p_ev.mouse_motion.relative_y);
		drag_accum = get_local_mouse_pos() - drag_origin;
		for (int i = get_child_count() - 1; i >= 0; i--) {
			GraphNode *gn = get_child(i)->cast_to<GraphNode>();
			if (gn && gn->is_selected())
				gn->set_offset((gn->get_drag_from() * zoom + drag_accum) / zoom);
		}
	}

	if (p_ev.type == InputEvent::MOUSE_MOTION && box_selecting) {
		box_selecting_to = get_local_mouse_pos();

		box_selecting_rect = Rect2(MIN(box_selecting_from.x, box_selecting_to.x),
				MIN(box_selecting_from.y, box_selecting_to.y),
				ABS(box_selecting_from.x - box_selecting_to.x),
				ABS(box_selecting_from.y - box_selecting_to.y));

		for (int i = get_child_count() - 1; i >= 0; i--) {

			GraphNode *gn = get_child(i)->cast_to<GraphNode>();
			if (!gn)
				continue;

			Rect2 r = gn->get_rect();
			r.size *= zoom;
			bool in_box = r.intersects(box_selecting_rect);

			if (in_box)
				gn->set_selected(box_selection_mode_aditive);
			else
				gn->set_selected(previus_selected.find(gn) != NULL);
		}

		top_layer->update();
	}

	if (p_ev.type == InputEvent::MOUSE_BUTTON) {

		const InputEventMouseButton &b = p_ev.mouse_button;

		if (b.button_index == BUTTON_RIGHT && b.pressed) {
			if (box_selecting) {
				box_selecting = false;
				for (int i = get_child_count() - 1; i >= 0; i--) {

					GraphNode *gn = get_child(i)->cast_to<GraphNode>();
					if (!gn)
						continue;

					gn->set_selected(previus_selected.find(gn) != NULL);
				}
				top_layer->update();
			} else {
				if (connecting) {
					connecting = false;
					top_layer->update();
				} else {
					emit_signal("popup_request", Vector2(b.global_x, b.global_y));
				}
			}
		}

		if (b.button_index == BUTTON_LEFT && !b.pressed && dragging) {
			if (!just_selected && drag_accum == Vector2() && Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
				//deselect current node
				for (int i = get_child_count() - 1; i >= 0; i--) {
					GraphNode *gn = get_child(i)->cast_to<GraphNode>();

					if (gn) {
						Rect2 r = gn->get_rect();
						r.size *= zoom;
						if (r.has_point(get_local_mouse_pos()))
							gn->set_selected(false);
					}
				}
			}

			if (drag_accum != Vector2()) {

				emit_signal("_begin_node_move");

				for (int i = get_child_count() - 1; i >= 0; i--) {
					GraphNode *gn = get_child(i)->cast_to<GraphNode>();
					if (gn && gn->is_selected())
						gn->set_drag(false);
				}

				emit_signal("_end_node_move");
			}

			dragging = false;

			top_layer->update();
		}

		if (b.button_index == BUTTON_LEFT && b.pressed) {

			GraphNode *gn = NULL;
			for (int i = get_child_count() - 1; i >= 0; i--) {

				gn = get_child(i)->cast_to<GraphNode>();

				if (gn) {
					Rect2 r = gn->get_rect();
					r.size *= zoom;
					if (r.has_point(get_local_mouse_pos()))
						break;
				}
			}

			if (gn) {

				if (_filter_input(Vector2(b.x, b.y)))
					return;

				dragging = true;
				drag_accum = Vector2();
				drag_origin = get_local_mouse_pos();
				just_selected = !gn->is_selected();
				if (!gn->is_selected() && !Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
					for (int i = 0; i < get_child_count(); i++) {
						GraphNode *o_gn = get_child(i)->cast_to<GraphNode>();
						if (o_gn)
							o_gn->set_selected(o_gn == gn);
					}
				}

				gn->set_selected(true);
				for (int i = 0; i < get_child_count(); i++) {
					GraphNode *o_gn = get_child(i)->cast_to<GraphNode>();
					if (!o_gn)
						continue;
					if (o_gn->is_selected())
						o_gn->set_drag(true);
				}

			} else {
				if (_filter_input(Vector2(b.x, b.y)))
					return;
				if (Input::get_singleton()->is_key_pressed(KEY_SPACE))
					return;

				box_selecting = true;
				box_selecting_from = get_local_mouse_pos();
				if (b.mod.control) {
					box_selection_mode_aditive = true;
					previus_selected.clear();
					for (int i = get_child_count() - 1; i >= 0; i--) {

						GraphNode *gn = get_child(i)->cast_to<GraphNode>();
						if (!gn || !gn->is_selected())
							continue;

						previus_selected.push_back(gn);
					}
				} else if (b.mod.shift) {
					box_selection_mode_aditive = false;
					previus_selected.clear();
					for (int i = get_child_count() - 1; i >= 0; i--) {

						GraphNode *gn = get_child(i)->cast_to<GraphNode>();
						if (!gn || !gn->is_selected())
							continue;

						previus_selected.push_back(gn);
					}
				} else {
					box_selection_mode_aditive = true;
					previus_selected.clear();
					for (int i = get_child_count() - 1; i >= 0; i--) {

						GraphNode *gn = get_child(i)->cast_to<GraphNode>();
						if (!gn)
							continue;

						gn->set_selected(false);
					}
				}
			}
		}

		if (b.button_index == BUTTON_LEFT && !b.pressed && box_selecting) {
			box_selecting = false;
			previus_selected.clear();
			top_layer->update();
		}

		if (b.button_index == BUTTON_WHEEL_UP && b.pressed) {
			//too difficult to get right
			//set_zoom(zoom*ZOOM_SCALE);
		}

		if (b.button_index == BUTTON_WHEEL_DOWN && b.pressed) {
			//too difficult to get right
			//set_zoom(zoom/ZOOM_SCALE);
		}
		if (b.button_index == BUTTON_WHEEL_UP) {
			h_scroll->set_val(h_scroll->get_val() - h_scroll->get_page() * b.factor / 8);
		}
		if (b.button_index == BUTTON_WHEEL_DOWN) {
			h_scroll->set_val(h_scroll->get_val() + h_scroll->get_page() * b.factor / 8);
		}
		if (b.button_index == BUTTON_WHEEL_RIGHT) {
			v_scroll->set_val(v_scroll->get_val() + v_scroll->get_page() * b.factor / 8);
		}
		if (b.button_index == BUTTON_WHEEL_LEFT) {
			v_scroll->set_val(v_scroll->get_val() - v_scroll->get_page() * b.factor / 8);
		}
	}

	if (p_ev.type == InputEvent::KEY && p_ev.key.scancode == KEY_D && p_ev.key.pressed && p_ev.key.mod.command) {
		emit_signal("duplicate_nodes_request");
		accept_event();
	}

	if (p_ev.type == InputEvent::KEY && p_ev.key.scancode == KEY_DELETE && p_ev.key.pressed) {
		emit_signal("delete_nodes_request");
		accept_event();
	}
}

void GraphEdit::clear_connections() {

	connections.clear();
	update();
}

void GraphEdit::set_zoom(float p_zoom) {

	p_zoom = CLAMP(p_zoom, MIN_ZOOM, MAX_ZOOM);
	if (zoom == p_zoom)
		return;

	zoom_minus->set_disabled(zoom == MIN_ZOOM);
	zoom_plus->set_disabled(zoom == MAX_ZOOM);

	Vector2 sbofs = (Vector2(h_scroll->get_val(), v_scroll->get_val()) + get_size() / 2) / zoom;

	zoom = p_zoom;
	top_layer->update();

	_update_scroll();

	if (is_visible()) {

		Vector2 ofs = sbofs * zoom - get_size() / 2;
		h_scroll->set_val(ofs.x);
		v_scroll->set_val(ofs.y);
	}

	update();
}

float GraphEdit::get_zoom() const {
	return zoom;
}

void GraphEdit::set_right_disconnects(bool p_enable) {

	right_disconnects = p_enable;
}

bool GraphEdit::is_right_disconnects_enabled() const {

	return right_disconnects;
}

Array GraphEdit::_get_connection_list() const {

	List<Connection> conns;
	get_connection_list(&conns);
	Array arr;
	for (List<Connection>::Element *E = conns.front(); E; E = E->next()) {
		Dictionary d;
		d["from"] = E->get().from;
		d["from_port"] = E->get().from_port;
		d["to"] = E->get().to;
		d["to_port"] = E->get().to_port;
		arr.push_back(d);
	}
	return arr;
}

void GraphEdit::_zoom_minus() {

	set_zoom(zoom / ZOOM_SCALE);
}
void GraphEdit::_zoom_reset() {

	set_zoom(1);
}

void GraphEdit::_zoom_plus() {

	set_zoom(zoom * ZOOM_SCALE);
}

void GraphEdit::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("connect_node:Error", "from", "from_port", "to", "to_port"), &GraphEdit::connect_node);
	ObjectTypeDB::bind_method(_MD("is_node_connected", "from", "from_port", "to", "to_port"), &GraphEdit::is_node_connected);
	ObjectTypeDB::bind_method(_MD("disconnect_node", "from", "from_port", "to", "to_port"), &GraphEdit::disconnect_node);
	ObjectTypeDB::bind_method(_MD("get_connection_list"), &GraphEdit::_get_connection_list);
	ObjectTypeDB::bind_method(_MD("get_scroll_ofs"), &GraphEdit::get_scroll_ofs);

	ObjectTypeDB::bind_method(_MD("set_zoom", "p_zoom"), &GraphEdit::set_zoom);
	ObjectTypeDB::bind_method(_MD("get_zoom"), &GraphEdit::get_zoom);

	ObjectTypeDB::bind_method(_MD("set_right_disconnects", "enable"), &GraphEdit::set_right_disconnects);
	ObjectTypeDB::bind_method(_MD("is_right_disconnects_enabled"), &GraphEdit::is_right_disconnects_enabled);

	ObjectTypeDB::bind_method(_MD("_graph_node_moved"), &GraphEdit::_graph_node_moved);
	ObjectTypeDB::bind_method(_MD("_graph_node_raised"), &GraphEdit::_graph_node_raised);

	ObjectTypeDB::bind_method(_MD("_top_layer_input"), &GraphEdit::_top_layer_input);
	ObjectTypeDB::bind_method(_MD("_top_layer_draw"), &GraphEdit::_top_layer_draw);
	ObjectTypeDB::bind_method(_MD("_scroll_moved"), &GraphEdit::_scroll_moved);
	ObjectTypeDB::bind_method(_MD("_zoom_minus"), &GraphEdit::_zoom_minus);
	ObjectTypeDB::bind_method(_MD("_zoom_reset"), &GraphEdit::_zoom_reset);
	ObjectTypeDB::bind_method(_MD("_zoom_plus"), &GraphEdit::_zoom_plus);

	ObjectTypeDB::bind_method(_MD("_input_event"), &GraphEdit::_input_event);

	ADD_SIGNAL(MethodInfo("connection_request", PropertyInfo(Variant::STRING, "from"), PropertyInfo(Variant::INT, "from_slot"), PropertyInfo(Variant::STRING, "to"), PropertyInfo(Variant::INT, "to_slot")));
	ADD_SIGNAL(MethodInfo("disconnection_request", PropertyInfo(Variant::STRING, "from"), PropertyInfo(Variant::INT, "from_slot"), PropertyInfo(Variant::STRING, "to"), PropertyInfo(Variant::INT, "to_slot")));
	ADD_SIGNAL(MethodInfo("popup_request", PropertyInfo(Variant::VECTOR2, "p_position")));
	ADD_SIGNAL(MethodInfo("duplicate_nodes_request"));
	ADD_SIGNAL(MethodInfo("delete_nodes_request"));
	ADD_SIGNAL(MethodInfo("_begin_node_move"));
	ADD_SIGNAL(MethodInfo("_end_node_move"));
}

GraphEdit::GraphEdit() {
	set_focus_mode(FOCUS_ALL);

	top_layer = NULL;
	top_layer = memnew(GraphEditFilter(this));
	add_child(top_layer);
	top_layer->set_stop_mouse(false);
	top_layer->set_area_as_parent_rect();
	top_layer->connect("draw", this, "_top_layer_draw");
	top_layer->set_stop_mouse(false);
	top_layer->connect("input_event", this, "_top_layer_input");

	h_scroll = memnew(HScrollBar);
	h_scroll->set_name("_h_scroll");
	top_layer->add_child(h_scroll);

	v_scroll = memnew(VScrollBar);
	v_scroll->set_name("_v_scroll");
	top_layer->add_child(v_scroll);
	updating = false;
	connecting = false;
	right_disconnects = false;

	box_selecting = false;
	dragging = false;

	h_scroll->connect("value_changed", this, "_scroll_moved");
	v_scroll->connect("value_changed", this, "_scroll_moved");

	zoom = 1;

	HBoxContainer *zoom_hb = memnew(HBoxContainer);
	top_layer->add_child(zoom_hb);
	zoom_hb->set_pos(Vector2(10, 10));

	zoom_minus = memnew(ToolButton);
	zoom_hb->add_child(zoom_minus);
	zoom_minus->connect("pressed", this, "_zoom_minus");
	zoom_minus->set_icon(get_icon("minus"));

	zoom_reset = memnew(ToolButton);
	zoom_hb->add_child(zoom_reset);
	zoom_reset->connect("pressed", this, "_zoom_reset");
	zoom_reset->set_icon(get_icon("reset"));

	zoom_plus = memnew(ToolButton);
	zoom_hb->add_child(zoom_plus);
	zoom_plus->connect("pressed", this, "_zoom_plus");
	zoom_plus->set_icon(get_icon("more"));
}
