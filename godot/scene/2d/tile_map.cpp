/*************************************************************************/
/*  tile_map.cpp                                                         */
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
#include "tile_map.h"
#include "io/marshalls.h"
#include "method_bind_ext.gen.inc"
#include "os/os.h"
#include "servers/physics_2d_server.h"

int TileMap::_get_quadrant_size() const {

	if (y_sort_mode)
		return 1;
	else
		return quadrant_size;
}

void TileMap::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE: {

			Node2D *c = this;
			while (c) {

				navigation = c->cast_to<Navigation2D>();
				if (navigation) {
					break;
				}

				c = c->get_parent()->cast_to<Node2D>();
			}

			pending_update = true;
			_update_dirty_quadrants();
			RID space = get_world_2d()->get_space();
			_update_quadrant_transform();
			_update_quadrant_space(space);

		} break;
		case NOTIFICATION_EXIT_TREE: {

			_update_quadrant_space(RID());
			for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

				Quadrant &q = E->get();
				if (navigation) {
					for (Map<PosKey, Quadrant::NavPoly>::Element *E = q.navpoly_ids.front(); E; E = E->next()) {

						navigation->navpoly_remove(E->get().id);
					}
					q.navpoly_ids.clear();
				}

				for (Map<PosKey, Quadrant::Occluder>::Element *E = q.occluder_instances.front(); E; E = E->next()) {
					VS::get_singleton()->free(E->get().id);
				}
				q.occluder_instances.clear();
			}

			navigation = NULL;

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {

			//move stuff
			_update_quadrant_transform();

		} break;
	}
}

void TileMap::_update_quadrant_space(const RID &p_space) {

	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_space(q.bodies[i], p_space);
		}
	}
}

void TileMap::_update_quadrant_transform() {

	if (!is_inside_tree())
		return;

	Matrix32 global_transform = get_global_transform();

	Matrix32 nav_rel;
	if (navigation)
		nav_rel = get_relative_transform_to_parent(navigation);

	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		Matrix32 xform;
		xform.set_origin(q.pos);
		xform = global_transform * xform;
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_state(q.bodies[i], Physics2DServer::BODY_STATE_TRANSFORM, xform);
		}

		if (navigation) {
			for (Map<PosKey, Quadrant::NavPoly>::Element *E = q.navpoly_ids.front(); E; E = E->next()) {

				navigation->navpoly_set_transform(E->get().id, nav_rel * E->get().xform);
			}
		}

		for (Map<PosKey, Quadrant::Occluder>::Element *E = q.occluder_instances.front(); E; E = E->next()) {
			VS::get_singleton()->canvas_light_occluder_set_transform(E->get().id, global_transform * E->get().xform);
		}
	}
}

void TileMap::set_tileset(const Ref<TileSet> &p_tileset) {

	if (tile_set.is_valid())
		tile_set->disconnect("changed", this, "_recreate_quadrants");

	_clear_quadrants();
	tile_set = p_tileset;

	if (tile_set.is_valid())
		tile_set->connect("changed", this, "_recreate_quadrants");
	else
		clear();

	_recreate_quadrants();
	emit_signal("settings_changed");
}

Ref<TileSet> TileMap::get_tileset() const {

	return tile_set;
}

void TileMap::set_cell_size(Size2 p_size) {

	ERR_FAIL_COND(p_size.x < 1 || p_size.y < 1);

	_clear_quadrants();
	cell_size = p_size;
	_recreate_quadrants();
	emit_signal("settings_changed");
}
Size2 TileMap::get_cell_size() const {

	return cell_size;
}
void TileMap::set_quadrant_size(int p_size) {

	ERR_FAIL_COND(p_size < 1);

	_clear_quadrants();
	quadrant_size = p_size;
	_recreate_quadrants();
	emit_signal("settings_changed");
}
int TileMap::get_quadrant_size() const {

	return quadrant_size;
}

void TileMap::set_center_x(bool p_enable) {

	center_x = p_enable;
	_recreate_quadrants();
	emit_signal("settings_changed");
}
bool TileMap::get_center_x() const {

	return center_x;
}
void TileMap::set_center_y(bool p_enable) {

	center_y = p_enable;
	_recreate_quadrants();
	emit_signal("settings_changed");
}
bool TileMap::get_center_y() const {

	return center_y;
}

void TileMap::_fix_cell_transform(Matrix32 &xform, const Cell &p_cell, const Vector2 &p_offset, const Size2 &p_sc) {

	Size2 s = p_sc;
	Vector2 offset = p_offset;

	if (tile_origin == TILE_ORIGIN_BOTTOM_LEFT)
		offset.y += cell_size.y;

	if (s.y > s.x) {
		if ((p_cell.flip_h && (p_cell.flip_v || p_cell.transpose)) || (p_cell.flip_v && !p_cell.transpose))
			offset.y += s.y - s.x;
	} else if (s.y < s.x) {
		if ((p_cell.flip_v && (p_cell.flip_h || p_cell.transpose)) || (p_cell.flip_h && !p_cell.transpose))
			offset.x += s.x - s.y;
	}

	if (p_cell.transpose) {
		SWAP(xform.elements[0].x, xform.elements[0].y);
		SWAP(xform.elements[1].x, xform.elements[1].y);
		SWAP(offset.x, offset.y);
		SWAP(s.x, s.y);
	}
	if (p_cell.flip_h) {
		xform.elements[0].x = -xform.elements[0].x;
		xform.elements[1].x = -xform.elements[1].x;
		if (tile_origin == TILE_ORIGIN_TOP_LEFT || tile_origin == TILE_ORIGIN_BOTTOM_LEFT)
			offset.x = s.x - offset.x;
	}
	if (p_cell.flip_v) {
		xform.elements[0].y = -xform.elements[0].y;
		xform.elements[1].y = -xform.elements[1].y;
		if (tile_origin == TILE_ORIGIN_TOP_LEFT)
			offset.y = s.y - offset.y;
		else if (tile_origin == TILE_ORIGIN_BOTTOM_LEFT) {
			if (p_cell.transpose)
				offset.y += s.y;
			else
				offset.y -= s.y;
		}
	}
	xform.elements[2].x += offset.x;
	xform.elements[2].y += offset.y;
}

namespace {

struct _BodyParams {
	Vector2 one_way_direction;
	float one_way_max_depth;

	bool operator<(const _BodyParams &p_rhs) const {
		return p_rhs.one_way_direction < one_way_direction || p_rhs.one_way_max_depth < one_way_max_depth;
	}

	bool operator==(const _BodyParams &p_rhs) const {
		return p_rhs.one_way_direction == one_way_direction && p_rhs.one_way_max_depth == one_way_max_depth;
	}
};
} // namespace

void TileMap::_update_dirty_quadrants() {

	if (!pending_update)
		return;
	if (!is_inside_tree() || !tile_set.is_valid()) {
		pending_update = false;
		return;
	}

	VisualServer *vs = VisualServer::get_singleton();
	Physics2DServer *ps = Physics2DServer::get_singleton();
	Vector2 tofs = get_cell_draw_offset();
	Vector2 tcenter = cell_size / 2;
	Matrix32 nav_rel;
	if (navigation)
		nav_rel = get_relative_transform_to_parent(navigation);

	Vector2 qofs;

	SceneTree *st = SceneTree::get_singleton();
	Color debug_collision_color;

	bool debug_shapes = st && st->is_debugging_collisions_hint();
	if (debug_shapes) {
		debug_collision_color = st->get_debug_collisions_color();
	}

	while (dirty_quadrant_list.first()) {

		Quadrant &q = *dirty_quadrant_list.first()->self();

		for (List<RID>::Element *E = q.canvas_items.front(); E; E = E->next()) {

			vs->free(E->get());
		}

		q.canvas_items.clear();

		Map<_BodyParams, int> params_bodies;
		int num_bodies_used = 0;

		if (navigation) {
			for (Map<PosKey, Quadrant::NavPoly>::Element *E = q.navpoly_ids.front(); E; E = E->next()) {

				navigation->navpoly_remove(E->get().id);
			}
			q.navpoly_ids.clear();
		}

		for (Map<PosKey, Quadrant::Occluder>::Element *E = q.occluder_instances.front(); E; E = E->next()) {
			VS::get_singleton()->free(E->get().id);
		}
		q.occluder_instances.clear();
		Ref<CanvasItemMaterial> prev_material;
		RID prev_canvas_item;
		RID prev_debug_canvas_item;

		for (int i = 0; i < q.cells.size(); i++) {

			Map<PosKey, Cell>::Element *E = tile_map.find(q.cells[i]);
			Cell &c = E->get();
			//moment of truth
			if (!tile_set->has_tile(c.id))
				continue;
			Ref<Texture> tex = tile_set->tile_get_texture(c.id);
			Vector2 tile_ofs = tile_set->tile_get_texture_offset(c.id);

			Vector2 wofs = _map_to_world(E->key().x, E->key().y);
			Vector2 offset = wofs - q.pos + tofs;

			if (!tex.is_valid())
				continue;

			Ref<CanvasItemMaterial> mat = tile_set->tile_get_material(c.id);

			RID canvas_item;
			RID debug_canvas_item;

			if (prev_canvas_item == RID() || prev_material != mat) {

				canvas_item = vs->canvas_item_create();
				if (mat.is_valid())
					vs->canvas_item_set_material(canvas_item, mat->get_rid());
				vs->canvas_item_set_parent(canvas_item, get_canvas_item());
				_update_item_material_state(canvas_item);
				Matrix32 xform;
				xform.set_origin(q.pos);
				vs->canvas_item_set_transform(canvas_item, xform);
				vs->canvas_item_set_light_mask(canvas_item, get_light_mask());

				q.canvas_items.push_back(canvas_item);

				if (debug_shapes) {

					debug_canvas_item = vs->canvas_item_create();
					vs->canvas_item_set_parent(debug_canvas_item, canvas_item);
					vs->canvas_item_set_z_as_relative_to_parent(debug_canvas_item, false);
					vs->canvas_item_set_z(debug_canvas_item, VS::CANVAS_ITEM_Z_MAX - 1);
					q.canvas_items.push_back(debug_canvas_item);
					prev_debug_canvas_item = debug_canvas_item;
				}

				prev_canvas_item = canvas_item;
				prev_material = mat;

			} else {
				canvas_item = prev_canvas_item;
				if (debug_shapes) {
					debug_canvas_item = prev_debug_canvas_item;
				}
			}

			Rect2 r = tile_set->tile_get_region(c.id);
			Size2 s = tex->get_size();

			if (r == Rect2())
				s = tex->get_size();
			else {
				s = r.size;
				r.pos.x += fp_adjust;
				r.pos.y += fp_adjust;
				r.size.x -= fp_adjust * 2.0;
				r.size.y -= fp_adjust * 2.0;
			}

			Rect2 rect;
			rect.pos = offset.floor();
			rect.size = s;

			if (rect.size.y > rect.size.x) {
				if ((c.flip_h && (c.flip_v || c.transpose)) || (c.flip_v && !c.transpose))
					tile_ofs.y += rect.size.y - rect.size.x;
			} else if (rect.size.y < rect.size.x) {
				if ((c.flip_v && (c.flip_h || c.transpose)) || (c.flip_h && !c.transpose))
					tile_ofs.x += rect.size.x - rect.size.y;
			}

			/*	rect.size.x+=fp_adjust;
			rect.size.y+=fp_adjust;*/

			if (c.transpose)
				SWAP(tile_ofs.x, tile_ofs.y);

			if (c.flip_h) {
				rect.size.x = -rect.size.x;
				tile_ofs.x = -tile_ofs.x;
			}
			if (c.flip_v) {
				rect.size.y = -rect.size.y;
				tile_ofs.y = -tile_ofs.y;
			}

			Vector2 center_ofs;

			if (tile_origin == TILE_ORIGIN_TOP_LEFT) {
				rect.pos += tile_ofs;

			} else if (tile_origin == TILE_ORIGIN_BOTTOM_LEFT) {

				rect.pos += tile_ofs;

				if (c.transpose) {
					if (c.flip_h)
						rect.pos.x -= cell_size.x;
					else
						rect.pos.x += cell_size.x;
				} else {
					if (c.flip_v)
						rect.pos.y -= cell_size.y;
					else
						rect.pos.y += cell_size.y;
				}

			} else if (tile_origin == TILE_ORIGIN_CENTER) {
				rect.pos += tcenter;

				Vector2 center = (s / 2) - tile_ofs;
				center_ofs = tcenter - (s / 2);

				if (c.flip_h)
					rect.pos.x -= s.x - center.x;
				else
					rect.pos.x -= center.x;

				if (c.flip_v)
					rect.pos.y -= s.y - center.y;
				else
					rect.pos.y -= center.y;
			}

			Color modulate = tile_set->tile_get_modulate(c.id);
			if (r == Rect2()) {
				tex->draw_rect(canvas_item, rect, false, modulate, c.transpose);
			} else {
				tex->draw_rect_region(canvas_item, rect, r, modulate, c.transpose);
			}

			RID body;
			_BodyParams params = { tile_set->tile_get_one_way_collision_direction(c.id), tile_set->tile_get_one_way_collision_max_depth(c.id) };
			Map<_BodyParams, int>::Element *B = params_bodies.find(params);
			if (!B) {
				if (q.bodies.size() > num_bodies_used) {
					// recycle one already existent
					body = q.bodies[num_bodies_used];
					// reset it
					ps->body_clear_shapes(body);
				} else {
					// create a new one
					body = Physics2DServer::get_singleton()->body_create(use_kinematic ? Physics2DServer::BODY_MODE_KINEMATIC : Physics2DServer::BODY_MODE_STATIC);
					Physics2DServer::get_singleton()->body_attach_object_instance_ID(body, get_instance_ID());
					Physics2DServer::get_singleton()->body_set_layer_mask(body, collision_layer);
					Physics2DServer::get_singleton()->body_set_collision_mask(body, collision_mask);
					Physics2DServer::get_singleton()->body_set_param(body, Physics2DServer::BODY_PARAM_FRICTION, friction);
					Physics2DServer::get_singleton()->body_set_param(body, Physics2DServer::BODY_PARAM_BOUNCE, bounce);
					q.bodies.push_back(body);
				}

				// initialize to match current quadrant
				Matrix32 xform;
				xform.set_origin(q.pos);
				if (is_inside_tree()) {
					xform = get_global_transform() * xform;
					RID space = get_world_2d()->get_space();
					Physics2DServer::get_singleton()->body_set_space(body, space);
				}
				Physics2DServer::get_singleton()->body_set_state(body, Physics2DServer::BODY_STATE_TRANSFORM, xform);

				// bookkeep
				params_bodies[params] = num_bodies_used;
				num_bodies_used++;
			} else {
				// take the one already set up for this tile's parameters
				body = q.bodies[B->get()];
			}

			Vector<Ref<Shape2D> > shapes = tile_set->tile_get_shapes(c.id);

			int shape_idx = ps->body_get_shape_count(body);
			for (int i = 0; i < shapes.size(); i++) {

				Ref<Shape2D> shape = shapes[i];
				if (shape.is_valid()) {

					Vector2 shape_ofs = tile_set->tile_get_shape_offset(c.id);
					Matrix32 xform;
					xform.set_origin(offset.floor());

					_fix_cell_transform(xform, c, shape_ofs + center_ofs, s);

					if (debug_canvas_item) {
						vs->canvas_item_add_set_transform(debug_canvas_item, xform);
						shape->draw(debug_canvas_item, debug_collision_color);
					}
					ps->body_add_shape(body, shape->get_rid(), xform);
					ps->body_set_shape_metadata(body, shape_idx++, Vector2(E->key().x, E->key().y));
					ps->body_set_one_way_collision_direction(body, params.one_way_direction);
					ps->body_set_one_way_collision_max_depth(body, params.one_way_max_depth);
				}
			}

			if (debug_canvas_item) {
				vs->canvas_item_add_set_transform(debug_canvas_item, Matrix32());
			}

			if (navigation) {
				Ref<NavigationPolygon> navpoly = tile_set->tile_get_navigation_polygon(c.id);
				if (navpoly.is_valid()) {
					Vector2 npoly_ofs = tile_set->tile_get_navigation_polygon_offset(c.id);
					Matrix32 xform;
					xform.set_origin(offset.floor() + q.pos);
					_fix_cell_transform(xform, c, npoly_ofs + center_ofs, s);

					int pid = navigation->navpoly_create(navpoly, nav_rel * xform);

					Quadrant::NavPoly np;
					np.id = pid;
					np.xform = xform;
					q.navpoly_ids[E->key()] = np;
				}
			}

			Ref<OccluderPolygon2D> occluder = tile_set->tile_get_light_occluder(c.id);
			if (occluder.is_valid()) {

				Vector2 occluder_ofs = tile_set->tile_get_occluder_offset(c.id);
				Matrix32 xform;
				xform.set_origin(offset.floor() + q.pos);
				_fix_cell_transform(xform, c, occluder_ofs + center_ofs, s);

				RID orid = VS::get_singleton()->canvas_light_occluder_create();
				VS::get_singleton()->canvas_light_occluder_set_transform(orid, get_global_transform() * xform);
				VS::get_singleton()->canvas_light_occluder_set_polygon(orid, occluder->get_rid());
				VS::get_singleton()->canvas_light_occluder_attach_to_canvas(orid, get_canvas());
				VS::get_singleton()->canvas_light_occluder_set_light_mask(orid, occluder_light_mask);
				Quadrant::Occluder oc;
				oc.xform = xform;
				oc.id = orid;
				q.occluder_instances[E->key()] = oc;
			}
		}

		// keep just as many bodies as needed
		for (int i = num_bodies_used; i < q.bodies.size(); i++) {
			ps->free(q.bodies[i]);
		}
		q.bodies.resize(params_bodies.size());

		//OS::get_singleton()->print("body count: %d (%d)\n", q.bodies.size(), params_bodies.size());

		dirty_quadrant_list.remove(dirty_quadrant_list.first());
		quadrant_order_dirty = true;
	}

	pending_update = false;

	if (quadrant_order_dirty) {

		for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

			Quadrant &q = E->get();
			for (List<RID>::Element *E = q.canvas_items.front(); E; E = E->next()) {

				VS::get_singleton()->canvas_item_raise(E->get());
			}
		}

		quadrant_order_dirty = false;
	}

	for (int i = 0; i < get_child_count(); i++) {

		CanvasItem *c = get_child(i)->cast_to<CanvasItem>();

		if (c)
			VS::get_singleton()->canvas_item_raise(c->get_canvas_item());
	}

	_recompute_rect_cache();
}

void TileMap::_recompute_rect_cache() {

#ifdef DEBUG_ENABLED

	if (!rect_cache_dirty)
		return;

	Rect2 r_total;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Rect2 r;
		r.pos = _map_to_world(E->key().x * _get_quadrant_size(), E->key().y * _get_quadrant_size());
		r.expand_to(_map_to_world(E->key().x * _get_quadrant_size() + _get_quadrant_size(), E->key().y * _get_quadrant_size()));
		r.expand_to(_map_to_world(E->key().x * _get_quadrant_size() + _get_quadrant_size(), E->key().y * _get_quadrant_size() + _get_quadrant_size()));
		r.expand_to(_map_to_world(E->key().x * _get_quadrant_size(), E->key().y * _get_quadrant_size() + _get_quadrant_size()));
		if (E == quadrant_map.front())
			r_total = r;
		else
			r_total = r_total.merge(r);
	}

	if (r_total == Rect2()) {
		rect_cache = Rect2(-10, -10, 20, 20);
	} else {
		rect_cache = r_total.grow(MAX(cell_size.x, cell_size.y) * _get_quadrant_size());
	}

	item_rect_changed();

	rect_cache_dirty = false;
#endif
}

Map<TileMap::PosKey, TileMap::Quadrant>::Element *TileMap::_create_quadrant(const PosKey &p_qk) {

	Quadrant q;
	q.pos = _map_to_world(p_qk.x * _get_quadrant_size(), p_qk.y * _get_quadrant_size());
	q.pos += get_cell_draw_offset();
	if (tile_origin == TILE_ORIGIN_CENTER)
		q.pos += cell_size / 2;
	else if (tile_origin == TILE_ORIGIN_BOTTOM_LEFT)
		q.pos.y += cell_size.y;

	//	q.canvas_item = VisualServer::get_singleton()->canvas_item_create();

	rect_cache_dirty = true;
	quadrant_order_dirty = true;
	return quadrant_map.insert(p_qk, q);
}

void TileMap::_erase_quadrant(Map<PosKey, Quadrant>::Element *Q) {

	Quadrant &q = Q->get();
	for (int i = 0; i < q.bodies.size(); i++) {
		Physics2DServer::get_singleton()->free(q.bodies[i]);
	}
	q.bodies.clear();

	for (List<RID>::Element *E = q.canvas_items.front(); E; E = E->next()) {

		VisualServer::get_singleton()->free(E->get());
	}
	q.canvas_items.clear();
	if (q.dirty_list.in_list())
		dirty_quadrant_list.remove(&q.dirty_list);

	if (navigation) {
		for (Map<PosKey, Quadrant::NavPoly>::Element *E = q.navpoly_ids.front(); E; E = E->next()) {

			navigation->navpoly_remove(E->get().id);
		}
		q.navpoly_ids.clear();
	}

	for (Map<PosKey, Quadrant::Occluder>::Element *E = q.occluder_instances.front(); E; E = E->next()) {
		VS::get_singleton()->free(E->get().id);
	}
	q.occluder_instances.clear();

	quadrant_map.erase(Q);
	rect_cache_dirty = true;
}

void TileMap::_make_quadrant_dirty(Map<PosKey, Quadrant>::Element *Q) {

	Quadrant &q = Q->get();
	if (!q.dirty_list.in_list())
		dirty_quadrant_list.add(&q.dirty_list);

	if (pending_update)
		return;
	pending_update = true;
	if (!is_inside_tree())
		return;
	call_deferred("_update_dirty_quadrants");
}

void TileMap::set_cellv(const Vector2 &p_pos, int p_tile, bool p_flip_x, bool p_flip_y, bool p_transpose) {

	set_cell(p_pos.x, p_pos.y, p_tile, p_flip_x, p_flip_y, p_transpose);
}

void TileMap::set_cell(int p_x, int p_y, int p_tile, bool p_flip_x, bool p_flip_y, bool p_transpose) {

	PosKey pk(p_x, p_y);

	Map<PosKey, Cell>::Element *E = tile_map.find(pk);
	if (!E && p_tile == INVALID_CELL)
		return; //nothing to do

	PosKey qk(p_x / _get_quadrant_size(), p_y / _get_quadrant_size());
	if (p_tile == INVALID_CELL) {
		//erase existing
		tile_map.erase(pk);
		Map<PosKey, Quadrant>::Element *Q = quadrant_map.find(qk);
		ERR_FAIL_COND(!Q);
		Quadrant &q = Q->get();
		q.cells.erase(pk);
		if (q.cells.size() == 0)
			_erase_quadrant(Q);
		else
			_make_quadrant_dirty(Q);

		return;
	}

	Map<PosKey, Quadrant>::Element *Q = quadrant_map.find(qk);

	if (!E) {
		E = tile_map.insert(pk, Cell());
		if (!Q) {
			Q = _create_quadrant(qk);
		}
		Quadrant &q = Q->get();
		q.cells.insert(pk);
	} else {
		ERR_FAIL_COND(!Q); // quadrant should exist...

		if (E->get().id == p_tile && E->get().flip_h == p_flip_x && E->get().flip_v == p_flip_y && E->get().transpose == p_transpose)
			return; //nothing changed
	}

	Cell &c = E->get();

	c.id = p_tile;
	c.flip_h = p_flip_x;
	c.flip_v = p_flip_y;
	c.transpose = p_transpose;

	_make_quadrant_dirty(Q);
	used_size_cache_dirty = true;
}

int TileMap::get_cellv(const Vector2 &p_pos) const {
	return get_cell(p_pos.x, p_pos.y);
}

int TileMap::get_cell(int p_x, int p_y) const {

	PosKey pk(p_x, p_y);

	const Map<PosKey, Cell>::Element *E = tile_map.find(pk);

	if (!E)
		return INVALID_CELL;

	return E->get().id;
}
bool TileMap::is_cell_x_flipped(int p_x, int p_y) const {

	PosKey pk(p_x, p_y);

	const Map<PosKey, Cell>::Element *E = tile_map.find(pk);

	if (!E)
		return false;

	return E->get().flip_h;
}
bool TileMap::is_cell_y_flipped(int p_x, int p_y) const {

	PosKey pk(p_x, p_y);

	const Map<PosKey, Cell>::Element *E = tile_map.find(pk);

	if (!E)
		return false;

	return E->get().flip_v;
}
bool TileMap::is_cell_transposed(int p_x, int p_y) const {

	PosKey pk(p_x, p_y);

	const Map<PosKey, Cell>::Element *E = tile_map.find(pk);

	if (!E)
		return false;

	return E->get().transpose;
}

void TileMap::_recreate_quadrants() {

	_clear_quadrants();

	for (Map<PosKey, Cell>::Element *E = tile_map.front(); E; E = E->next()) {

		PosKey qk(E->key().x / _get_quadrant_size(), E->key().y / _get_quadrant_size());

		Map<PosKey, Quadrant>::Element *Q = quadrant_map.find(qk);
		if (!Q) {
			Q = _create_quadrant(qk);
			dirty_quadrant_list.add(&Q->get().dirty_list);
		}

		Q->get().cells.insert(E->key());
		_make_quadrant_dirty(Q);
	}
}

void TileMap::_clear_quadrants() {

	while (quadrant_map.size()) {
		_erase_quadrant(quadrant_map.front());
	}
}

void TileMap::set_material(const Ref<CanvasItemMaterial> &p_material) {

	CanvasItem::set_material(p_material);
	_update_all_items_material_state();
}

void TileMap::set_use_parent_material(bool p_use_parent_material) {

	CanvasItem::set_use_parent_material(p_use_parent_material);
	_update_all_items_material_state();
}

void TileMap::_update_all_items_material_state() {

	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (List<RID>::Element *E = q.canvas_items.front(); E; E = E->next()) {

			_update_item_material_state(E->get());
		}
	}
}

void TileMap::_update_item_material_state(const RID &p_canvas_item) {

	VS::get_singleton()->canvas_item_set_use_parent_material(p_canvas_item, get_use_parent_material() || get_material().is_valid());
}

void TileMap::clear() {

	_clear_quadrants();
	tile_map.clear();
	used_size_cache_dirty = true;
}

void TileMap::_set_tile_data(const DVector<int> &p_data) {

	int c = p_data.size();
	DVector<int>::Read r = p_data.read();

	for (int i = 0; i < c; i += 2) {

		const uint8_t *ptr = (const uint8_t *)&r[i];
		uint8_t local[8];
		for (int j = 0; j < 8; j++)
			local[j] = ptr[j];

#ifdef BIG_ENDIAN_ENABLED

		SWAP(local[0], local[3]);
		SWAP(local[1], local[2]);
		SWAP(local[4], local[7]);
		SWAP(local[5], local[6]);
#endif

		int16_t x = decode_uint16(&local[0]);
		int16_t y = decode_uint16(&local[2]);
		uint32_t v = decode_uint32(&local[4]);
		bool flip_h = v & (1 << 29);
		bool flip_v = v & (1 << 30);
		bool transpose = v & (1 << 31);
		v &= (1 << 29) - 1;

		//		if (x<-20 || y <-20 || x>4000 || y>4000)
		//			continue;
		set_cell(x, y, v, flip_h, flip_v, transpose);
	}
}

DVector<int> TileMap::_get_tile_data() const {

	DVector<int> data;
	data.resize(tile_map.size() * 2);
	DVector<int>::Write w = data.write();

	int idx = 0;
	for (const Map<PosKey, Cell>::Element *E = tile_map.front(); E; E = E->next()) {

		uint8_t *ptr = (uint8_t *)&w[idx];
		encode_uint16(E->key().x, &ptr[0]);
		encode_uint16(E->key().y, &ptr[2]);
		uint32_t val = E->get().id;
		if (E->get().flip_h)
			val |= (1 << 29);
		if (E->get().flip_v)
			val |= (1 << 30);
		if (E->get().transpose)
			val |= (1 << 31);

		encode_uint32(val, &ptr[4]);
		idx += 2;
	}

	w = DVector<int>::Write();

	return data;
}

Rect2 TileMap::get_item_rect() const {

	const_cast<TileMap *>(this)->_update_dirty_quadrants();
	return rect_cache;
}

void TileMap::set_collision_layer(uint32_t p_layer) {

	collision_layer = p_layer;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_layer_mask(q.bodies[i], collision_layer);
		}
	}
}

void TileMap::set_collision_mask(uint32_t p_mask) {

	collision_mask = p_mask;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_collision_mask(q.bodies[i], collision_mask);
		}
	}
}

void TileMap::set_collision_layer_bit(int p_bit, bool p_value) {

	uint32_t layer = get_collision_layer();
	if (p_value)
		layer |= 1 << p_bit;
	else
		layer &= ~(1 << p_bit);
	set_collision_layer(layer);
}

void TileMap::set_collision_mask_bit(int p_bit, bool p_value) {

	uint32_t mask = get_collision_mask();
	if (p_value)
		mask |= 1 << p_bit;
	else
		mask &= ~(1 << p_bit);
	set_collision_mask(mask);
}

bool TileMap::get_collision_use_kinematic() const {

	return use_kinematic;
}

void TileMap::set_collision_use_kinematic(bool p_use_kinematic) {

	_clear_quadrants();
	use_kinematic = p_use_kinematic;
	_recreate_quadrants();
}

void TileMap::set_collision_friction(float p_friction) {

	friction = p_friction;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_param(q.bodies[i], Physics2DServer::BODY_PARAM_FRICTION, p_friction);
		}
	}
}

float TileMap::get_collision_friction() const {

	return friction;
}

void TileMap::set_collision_bounce(float p_bounce) {

	bounce = p_bounce;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		Quadrant &q = E->get();
		for (int i = q.bodies.size() - 1; i >= 0; i--) {
			Physics2DServer::get_singleton()->body_set_param(q.bodies[i], Physics2DServer::BODY_PARAM_BOUNCE, p_bounce);
		}
	}
}
float TileMap::get_collision_bounce() const {

	return bounce;
}

uint32_t TileMap::get_collision_layer() const {

	return collision_layer;
}

uint32_t TileMap::get_collision_mask() const {

	return collision_mask;
}

bool TileMap::get_collision_layer_bit(int p_bit) const {

	return get_collision_layer() & (1 << p_bit);
}

bool TileMap::get_collision_mask_bit(int p_bit) const {

	return get_collision_mask() & (1 << p_bit);
}

void TileMap::set_mode(Mode p_mode) {

	_clear_quadrants();
	mode = p_mode;
	_recreate_quadrants();
	emit_signal("settings_changed");
}

TileMap::Mode TileMap::get_mode() const {
	return mode;
}

void TileMap::set_half_offset(HalfOffset p_half_offset) {

	_clear_quadrants();
	half_offset = p_half_offset;
	_recreate_quadrants();
	emit_signal("settings_changed");
}

void TileMap::set_tile_origin(TileOrigin p_tile_origin) {

	_clear_quadrants();
	tile_origin = p_tile_origin;
	_recreate_quadrants();
	emit_signal("settings_changed");
}

TileMap::TileOrigin TileMap::get_tile_origin() const {

	return tile_origin;
}

Vector2 TileMap::get_cell_draw_offset() const {

	switch (mode) {

		case MODE_SQUARE: {

			return Vector2();
		} break;
		case MODE_ISOMETRIC: {

			return Vector2(-cell_size.x * 0.5, 0);

		} break;
		case MODE_CUSTOM: {

			Vector2 min;
			min.x = MIN(custom_transform[0].x, min.x);
			min.y = MIN(custom_transform[0].y, min.y);
			min.x = MIN(custom_transform[1].x, min.x);
			min.y = MIN(custom_transform[1].y, min.y);
			return min;
		} break;
	}

	return Vector2();
}

TileMap::HalfOffset TileMap::get_half_offset() const {
	return half_offset;
}

Matrix32 TileMap::get_cell_transform() const {

	switch (mode) {

		case MODE_SQUARE: {

			Matrix32 m;
			m[0] *= cell_size.x;
			m[1] *= cell_size.y;
			return m;
		} break;
		case MODE_ISOMETRIC: {

			//isometric only makes sense when y is positive in both x and y vectors, otherwise
			//the drawing of tiles will overlap
			Matrix32 m;
			m[0] = Vector2(cell_size.x * 0.5, cell_size.y * 0.5);
			m[1] = Vector2(-cell_size.x * 0.5, cell_size.y * 0.5);
			return m;

		} break;
		case MODE_CUSTOM: {

			return custom_transform;
		} break;
	}

	return Matrix32();
}

void TileMap::set_custom_transform(const Matrix32 &p_xform) {

	_clear_quadrants();
	custom_transform = p_xform;
	_recreate_quadrants();
	emit_signal("settings_changed");
}

Matrix32 TileMap::get_custom_transform() const {

	return custom_transform;
}

Vector2 TileMap::_map_to_world(int x, int y, bool p_ignore_ofs) const {

	Vector2 ret = get_cell_transform().xform(Vector2(x, y));
	if (!p_ignore_ofs) {
		switch (half_offset) {

			case HALF_OFFSET_X: {
				if (ABS(y) & 1) {

					ret += get_cell_transform()[0] * 0.5;
				}
			} break;
			case HALF_OFFSET_Y: {
				if (ABS(x) & 1) {
					ret += get_cell_transform()[1] * 0.5;
				}
			} break;
			default: {}
		}
	}
	return ret;
}
Vector2 TileMap::map_to_world(const Vector2 &p_pos, bool p_ignore_ofs) const {

	return _map_to_world(p_pos.x, p_pos.y, p_ignore_ofs);
}
Vector2 TileMap::world_to_map(const Vector2 &p_pos) const {

	Vector2 ret = get_cell_transform().affine_inverse().xform(p_pos);

	switch (half_offset) {

		case HALF_OFFSET_X: {
			if (ret.y > 0 ? int(ret.y) & 1 : (int(ret.y) - 1) & 1) {
				ret.x -= 0.5;
			}
		} break;
		case HALF_OFFSET_Y: {
			if (ret.x > 0 ? int(ret.x) & 1 : (int(ret.x) - 1) & 1) {
				ret.y -= 0.5;
			}
		} break;
		default: {}
	}

	return ret.floor();
}

void TileMap::set_y_sort_mode(bool p_enable) {

	_clear_quadrants();
	y_sort_mode = p_enable;
	VS::get_singleton()->canvas_item_set_sort_children_by_y(get_canvas_item(), y_sort_mode);
	_recreate_quadrants();
	emit_signal("settings_changed");
}

bool TileMap::is_y_sort_mode_enabled() const {

	return y_sort_mode;
}

Array TileMap::get_used_cells() const {

	Array a;
	a.resize(tile_map.size());
	int i = 0;
	for (Map<PosKey, Cell>::Element *E = tile_map.front(); E; E = E->next()) {

		Vector2 p(E->key().x, E->key().y);
		a[i++] = p;
	}

	return a;
}

Array TileMap::get_used_cells_by_id(int p_id) const {

	Array a;
	a.clear();
	for (Map<PosKey, Cell>::Element *E = tile_map.front(); E; E = E->next()) {

		if (E->value().id == p_id) {
			Vector2 p(E->key().x, E->key().y);
			a.push_back(p);
		}
	}

	return a;
}

Rect2 TileMap::get_used_rect() { // Not const because of cache

	if (used_size_cache_dirty) {
		if (tile_map.size() > 0) {
			used_size_cache = Rect2(tile_map.front()->key().x, tile_map.front()->key().y, 0, 0);

			for (Map<PosKey, Cell>::Element *E = tile_map.front(); E; E = E->next()) {
				used_size_cache.expand_to(Vector2(E->key().x, E->key().y));
			}

			used_size_cache.size += Vector2(1, 1);
		} else {
			used_size_cache = Rect2();
		}

		used_size_cache_dirty = false;
	}

	return used_size_cache;
}

void TileMap::set_occluder_light_mask(int p_mask) {

	occluder_light_mask = p_mask;
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		for (Map<PosKey, Quadrant::Occluder>::Element *F = E->get().occluder_instances.front(); F; F = F->next()) {
			VisualServer::get_singleton()->canvas_light_occluder_set_light_mask(F->get().id, occluder_light_mask);
		}
	}
}

int TileMap::get_occluder_light_mask() const {

	return occluder_light_mask;
}

void TileMap::set_light_mask(int p_light_mask) {

	CanvasItem::set_light_mask(p_light_mask);
	for (Map<PosKey, Quadrant>::Element *E = quadrant_map.front(); E; E = E->next()) {

		for (List<RID>::Element *F = E->get().canvas_items.front(); F; F = F->next()) {
			VisualServer::get_singleton()->canvas_item_set_light_mask(F->get(), get_light_mask());
		}
	}
}

void TileMap::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_tileset", "tileset:TileSet"), &TileMap::set_tileset);
	ObjectTypeDB::bind_method(_MD("get_tileset:TileSet"), &TileMap::get_tileset);

	ObjectTypeDB::bind_method(_MD("set_mode", "mode"), &TileMap::set_mode);
	ObjectTypeDB::bind_method(_MD("get_mode"), &TileMap::get_mode);

	ObjectTypeDB::bind_method(_MD("set_half_offset", "half_offset"), &TileMap::set_half_offset);
	ObjectTypeDB::bind_method(_MD("get_half_offset"), &TileMap::get_half_offset);

	ObjectTypeDB::bind_method(_MD("set_custom_transform", "custom_transform"), &TileMap::set_custom_transform);
	ObjectTypeDB::bind_method(_MD("get_custom_transform"), &TileMap::get_custom_transform);

	ObjectTypeDB::bind_method(_MD("set_cell_size", "size"), &TileMap::set_cell_size);
	ObjectTypeDB::bind_method(_MD("get_cell_size"), &TileMap::get_cell_size);

	ObjectTypeDB::bind_method(_MD("_set_old_cell_size", "size"), &TileMap::_set_old_cell_size);
	ObjectTypeDB::bind_method(_MD("_get_old_cell_size"), &TileMap::_get_old_cell_size);

	ObjectTypeDB::bind_method(_MD("set_quadrant_size", "size"), &TileMap::set_quadrant_size);
	ObjectTypeDB::bind_method(_MD("get_quadrant_size"), &TileMap::get_quadrant_size);

	ObjectTypeDB::bind_method(_MD("set_tile_origin", "origin"), &TileMap::set_tile_origin);
	ObjectTypeDB::bind_method(_MD("get_tile_origin"), &TileMap::get_tile_origin);

	ObjectTypeDB::bind_method(_MD("set_center_x", "enable"), &TileMap::set_center_x);
	ObjectTypeDB::bind_method(_MD("get_center_x"), &TileMap::get_center_x);

	ObjectTypeDB::bind_method(_MD("set_center_y", "enable"), &TileMap::set_center_y);
	ObjectTypeDB::bind_method(_MD("get_center_y"), &TileMap::get_center_y);

	ObjectTypeDB::bind_method(_MD("set_y_sort_mode", "enable"), &TileMap::set_y_sort_mode);
	ObjectTypeDB::bind_method(_MD("is_y_sort_mode_enabled"), &TileMap::is_y_sort_mode_enabled);

	ObjectTypeDB::bind_method(_MD("set_collision_use_kinematic", "use_kinematic"), &TileMap::set_collision_use_kinematic);
	ObjectTypeDB::bind_method(_MD("get_collision_use_kinematic"), &TileMap::get_collision_use_kinematic);

	ObjectTypeDB::bind_method(_MD("set_collision_layer", "layer"), &TileMap::set_collision_layer);
	ObjectTypeDB::bind_method(_MD("get_collision_layer"), &TileMap::get_collision_layer);

	ObjectTypeDB::bind_method(_MD("set_collision_mask", "mask"), &TileMap::set_collision_mask);
	ObjectTypeDB::bind_method(_MD("get_collision_mask"), &TileMap::get_collision_mask);

	ObjectTypeDB::bind_method(_MD("set_collision_layer_bit", "bit", "value"), &TileMap::set_collision_layer_bit);
	ObjectTypeDB::bind_method(_MD("get_collision_layer_bit", "bit"), &TileMap::get_collision_layer_bit);

	ObjectTypeDB::bind_method(_MD("set_collision_mask_bit", "bit", "value"), &TileMap::set_collision_mask_bit);
	ObjectTypeDB::bind_method(_MD("get_collision_mask_bit", "bit"), &TileMap::get_collision_mask_bit);

	ObjectTypeDB::bind_method(_MD("set_collision_friction", "value"), &TileMap::set_collision_friction);
	ObjectTypeDB::bind_method(_MD("get_collision_friction"), &TileMap::get_collision_friction);

	ObjectTypeDB::bind_method(_MD("set_collision_bounce", "value"), &TileMap::set_collision_bounce);
	ObjectTypeDB::bind_method(_MD("get_collision_bounce"), &TileMap::get_collision_bounce);

	ObjectTypeDB::bind_method(_MD("set_occluder_light_mask", "mask"), &TileMap::set_occluder_light_mask);
	ObjectTypeDB::bind_method(_MD("get_occluder_light_mask"), &TileMap::get_occluder_light_mask);

	ObjectTypeDB::bind_method(_MD("set_cell", "x", "y", "tile", "flip_x", "flip_y", "transpose"), &TileMap::set_cell, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("set_cellv", "pos", "tile", "flip_x", "flip_y", "transpose"), &TileMap::set_cellv, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("get_cell", "x", "y"), &TileMap::get_cell);
	ObjectTypeDB::bind_method(_MD("get_cellv", "pos"), &TileMap::get_cellv);
	ObjectTypeDB::bind_method(_MD("is_cell_x_flipped", "x", "y"), &TileMap::is_cell_x_flipped);
	ObjectTypeDB::bind_method(_MD("is_cell_y_flipped", "x", "y"), &TileMap::is_cell_y_flipped);
	ObjectTypeDB::bind_method(_MD("is_cell_transposed", "x", "y"), &TileMap::is_cell_transposed);

	ObjectTypeDB::bind_method(_MD("clear"), &TileMap::clear);

	ObjectTypeDB::bind_method(_MD("get_used_cells"), &TileMap::get_used_cells);
	ObjectTypeDB::bind_method(_MD("get_used_cells_by_id", "id"), &TileMap::get_used_cells_by_id);
	ObjectTypeDB::bind_method(_MD("get_used_rect"), &TileMap::get_used_rect);

	ObjectTypeDB::bind_method(_MD("map_to_world", "mappos", "ignore_half_ofs"), &TileMap::map_to_world, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("world_to_map", "worldpos"), &TileMap::world_to_map);

	ObjectTypeDB::bind_method(_MD("_clear_quadrants"), &TileMap::_clear_quadrants);
	ObjectTypeDB::bind_method(_MD("_recreate_quadrants"), &TileMap::_recreate_quadrants);
	ObjectTypeDB::bind_method(_MD("_update_dirty_quadrants"), &TileMap::_update_dirty_quadrants);

	ObjectTypeDB::bind_method(_MD("_set_tile_data"), &TileMap::_set_tile_data);
	ObjectTypeDB::bind_method(_MD("_get_tile_data"), &TileMap::_get_tile_data);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Square,Isometric,Custom"), _SCS("set_mode"), _SCS("get_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_set", PROPERTY_HINT_RESOURCE_TYPE, "TileSet"), _SCS("set_tileset"), _SCS("get_tileset"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell_size", PROPERTY_HINT_RANGE, "1,8192,1", 0), _SCS("_set_old_cell_size"), _SCS("_get_old_cell_size"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "cell/size", PROPERTY_HINT_RANGE, "1,8192,1"), _SCS("set_cell_size"), _SCS("get_cell_size"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell/quadrant_size", PROPERTY_HINT_RANGE, "1,128,1"), _SCS("set_quadrant_size"), _SCS("get_quadrant_size"));
	ADD_PROPERTY(PropertyInfo(Variant::MATRIX32, "cell/custom_transform"), _SCS("set_custom_transform"), _SCS("get_custom_transform"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell/half_offset", PROPERTY_HINT_ENUM, "Offset X,Offset Y,Disabled"), _SCS("set_half_offset"), _SCS("get_half_offset"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell/tile_origin", PROPERTY_HINT_ENUM, "Top Left,Center,Bottom Left"), _SCS("set_tile_origin"), _SCS("get_tile_origin"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cell/y_sort"), _SCS("set_y_sort_mode"), _SCS("is_y_sort_mode_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision/use_kinematic", PROPERTY_HINT_NONE, ""), _SCS("set_collision_use_kinematic"), _SCS("get_collision_use_kinematic"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "collision/friction", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_collision_friction"), _SCS("get_collision_friction"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "collision/bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_collision_bounce"), _SCS("get_collision_bounce"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/layers", PROPERTY_HINT_ALL_FLAGS), _SCS("set_collision_layer"), _SCS("get_collision_layer"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/mask", PROPERTY_HINT_ALL_FLAGS), _SCS("set_collision_mask"), _SCS("get_collision_mask"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "occluder/light_mask", PROPERTY_HINT_ALL_FLAGS), _SCS("set_occluder_light_mask"), _SCS("get_occluder_light_mask"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), _SCS("_set_tile_data"), _SCS("_get_tile_data"));

	ADD_SIGNAL(MethodInfo("settings_changed"));

	BIND_CONSTANT(INVALID_CELL);
	BIND_CONSTANT(MODE_SQUARE);
	BIND_CONSTANT(MODE_ISOMETRIC);
	BIND_CONSTANT(MODE_CUSTOM);
	BIND_CONSTANT(HALF_OFFSET_X);
	BIND_CONSTANT(HALF_OFFSET_Y);
	BIND_CONSTANT(HALF_OFFSET_DISABLED);
	BIND_CONSTANT(TILE_ORIGIN_TOP_LEFT);
	BIND_CONSTANT(TILE_ORIGIN_CENTER);
	BIND_CONSTANT(TILE_ORIGIN_BOTTOM_LEFT);
}

TileMap::TileMap() {

	rect_cache_dirty = true;
	used_size_cache_dirty = true;
	pending_update = false;
	quadrant_order_dirty = false;
	quadrant_size = 16;
	cell_size = Size2(64, 64);
	center_x = false;
	center_y = false;
	collision_layer = 1;
	collision_mask = 1;
	friction = 1;
	bounce = 0;
	mode = MODE_SQUARE;
	half_offset = HALF_OFFSET_DISABLED;
	use_kinematic = false;
	navigation = NULL;
	y_sort_mode = false;
	occluder_light_mask = 1;

	fp_adjust = 0.00001;
	tile_origin = TILE_ORIGIN_TOP_LEFT;
}

TileMap::~TileMap() {

	clear();
}
