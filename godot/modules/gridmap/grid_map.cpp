/*************************************************************************/
/*  grid_map.cpp                                                         */
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
#include "grid_map.h"
#include "io/marshalls.h"
#include "message_queue.h"
#include "os/os.h"
#include "scene/3d/baked_light_instance.h"
#include "scene/3d/light.h"
#include "scene/resources/mesh_library.h"
#include "scene/resources/surface_tool.h"
#include "scene/scene_string_names.h"
#include "servers/visual_server.h"

bool GridMap::_set(const StringName &p_name, const Variant &p_value) {

	String name = p_name;

	if (name == "theme/theme") {

		set_theme(p_value);
	} else if (name == "cell/size") {
		set_cell_size(p_value);
	} else if (name == "cell/octant_size") {
		set_octant_size(p_value);
	} else if (name == "cell/center_x") {
		set_center_x(p_value);
	} else if (name == "cell/center_y") {
		set_center_y(p_value);
	} else if (name == "cell/center_z") {
		set_center_z(p_value);
	} else if (name == "cell/scale") {
		set_cell_scale(p_value);
	} else if (name == "lighting/bake") {
		set_use_baked_light(p_value);
	} else if (name == "theme/bake") {
		set_bake(p_value);
		/*	} else if (name=="cells") {
		DVector<int> cells = p_value;
		int amount=cells.size();
		DVector<int>::Read r = cells.read();
		ERR_FAIL_COND_V(amount&1,false); // not even
		cell_map.clear();
		for(int i=0;i<amount/3;i++) {


			IndexKey ik;
			ik.key=decode_uint64(&r[i*3]);
			Cell cell;
			cell.cell=uint32_t(r[i*+1]);
			cell_map[ik]=cell;

		}
		_recreate_octant_data();*/
	} else if (name == "data") {

		Dictionary d = p_value;

		Dictionary baked;
		if (d.has("baked"))
			baked = d["baked"];
		if (d.has("cells")) {

			DVector<int> cells = d["cells"];
			int amount = cells.size();
			DVector<int>::Read r = cells.read();
			ERR_FAIL_COND_V(amount % 3, false); // not even
			cell_map.clear();
			for (int i = 0; i < amount / 3; i++) {

				IndexKey ik;
				ik.key = decode_uint64((const uint8_t *)&r[i * 3]);
				Cell cell;
				cell.cell = decode_uint32((const uint8_t *)&r[i * 3 + 2]);
				cell_map[ik] = cell;
			}
		}
		baked_lock = baked.size() != 0;
		_recreate_octant_data();
		baked_lock = false;
		if (!baked.empty()) {
			List<Variant> kl;
			baked.get_key_list(&kl);
			for (List<Variant>::Element *E = kl.front(); E; E = E->next()) {

				Plane ikv = E->get();
				Ref<Mesh> b = baked[ikv];
				ERR_CONTINUE(!b.is_valid());
				OctantKey ok;
				ok.x = ikv.normal.x;
				ok.y = ikv.normal.y;
				ok.z = ikv.normal.z;
				ok.area = ikv.d;

				ERR_CONTINUE(!octant_map.has(ok));

				Octant &g = *octant_map[ok];

				g.baked = b;
				g.bake_instance = VS::get_singleton()->instance_create();
				VS::get_singleton()->instance_set_base(g.bake_instance, g.baked->get_rid());
				VS::get_singleton()->instance_geometry_set_baked_light(g.bake_instance, baked_light_instance ? baked_light_instance->get_baked_light_instance() : RID());
			}
		}

	} else if (name.begins_with("areas/")) {
		int which = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "bounds") {
			ERR_FAIL_COND_V(area_map.has(which), false);
			create_area(which, p_value);
			return true;
		}

		ERR_FAIL_COND_V(!area_map.has(which), false);

		if (what == "name")
			area_set_name(which, p_value);
		else if (what == "disable_distance")
			area_set_portal_disable_distance(which, p_value);
		else if (what == "exterior_portal")
			area_set_portal_disable_color(which, p_value);
		else
			return false;
	} else
		return false;

	return true;
}

bool GridMap::_get(const StringName &p_name, Variant &r_ret) const {

	String name = p_name;

	if (name == "theme/theme") {
		r_ret = get_theme();
	} else if (name == "cell/size") {
		r_ret = get_cell_size();
	} else if (name == "cell/octant_size") {
		r_ret = get_octant_size();
	} else if (name == "cell/center_x") {
		r_ret = get_center_x();
	} else if (name == "cell/center_y") {
		r_ret = get_center_y();
	} else if (name == "cell/center_z") {
		r_ret = get_center_z();
	} else if (name == "cell/scale") {
		r_ret = cell_scale;
	} else if (name == "lighting/bake") {
		r_ret = is_using_baked_light();
	} else if (name == "theme/bake") {
		r_ret = bake;
	} else if (name == "data") {

		Dictionary d;

		DVector<int> cells;
		cells.resize(cell_map.size() * 3);
		{
			DVector<int>::Write w = cells.write();
			int i = 0;
			for (Map<IndexKey, Cell>::Element *E = cell_map.front(); E; E = E->next(), i++) {

				encode_uint64(E->key().key, (uint8_t *)&w[i * 3]);
				encode_uint32(E->get().cell, (uint8_t *)&w[i * 3 + 2]);
			}
		}

		d["cells"] = cells;

		Dictionary baked;
		for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {

			Octant &g = *E->get();

			if (g.baked.is_valid()) {

				baked[Plane(E->key().x, E->key().y, E->key().z, E->key().area)] = g.baked;
			}
		}

		if (baked.size()) {
			d["baked"] = baked;
		}

		r_ret = d;
	} else if (name.begins_with("areas/")) {
		int which = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "bounds")
			r_ret = area_get_bounds(which);
		else if (what == "name")
			r_ret = area_get_name(which);
		else if (what == "disable_distance")
			r_ret = area_get_portal_disable_distance(which);
		else if (what == "exterior_portal")
			r_ret = area_is_exterior_portal(which);
		else
			return false;
	} else
		return false;

	return true;
}

void GridMap::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::OBJECT, "theme/theme", PROPERTY_HINT_RESOURCE_TYPE, "MeshLibrary"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "theme/bake"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "lighting/bake"));
	p_list->push_back(PropertyInfo(Variant::REAL, "cell/size", PROPERTY_HINT_RANGE, "0.01,16384,0.01"));
	p_list->push_back(PropertyInfo(Variant::INT, "cell/octant_size", PROPERTY_HINT_RANGE, "1,1024,1"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "cell/center_x"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "cell/center_y"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "cell/center_z"));
	p_list->push_back(PropertyInfo(Variant::REAL, "cell/scale"));

	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));

	for (const Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {

		String base = "areas/" + itos(E->key()) + "/";
		p_list->push_back(PropertyInfo(Variant::_AABB, base + "bounds", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		p_list->push_back(PropertyInfo(Variant::STRING, base + "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		p_list->push_back(PropertyInfo(Variant::REAL, base + "disable_distance", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		p_list->push_back(PropertyInfo(Variant::COLOR, base + "disable_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
		p_list->push_back(PropertyInfo(Variant::BOOL, base + "exterior_portal", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
	}
}

void GridMap::set_theme(const Ref<MeshLibrary> &p_theme) {

	if (!theme.is_null())
		theme->unregister_owner(this);
	theme = p_theme;
	if (!theme.is_null())
		theme->register_owner(this);

	_recreate_octant_data();
	_change_notify("theme");
}

Ref<MeshLibrary> GridMap::get_theme() const {

	return theme;
}

void GridMap::set_cell_size(float p_size) {

	cell_size = p_size;
	_recreate_octant_data();
}
float GridMap::get_cell_size() const {

	return cell_size;
}

void GridMap::set_octant_size(int p_size) {

	octant_size = p_size;
	_recreate_octant_data();
}
int GridMap::get_octant_size() const {

	return octant_size;
}

void GridMap::set_center_x(bool p_enable) {

	center_x = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_x() const {
	return center_x;
}

void GridMap::set_center_y(bool p_enable) {

	center_y = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_y() const {
	return center_y;
}

void GridMap::set_center_z(bool p_enable) {

	center_z = p_enable;
	_recreate_octant_data();
}

bool GridMap::get_center_z() const {
	return center_z;
}

int GridMap::_find_area(const IndexKey &p_pos) const {

	for (const Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {
		//this should somehow be faster...
		const Area &a = *E->get();
		if (p_pos.x >= a.from.x && p_pos.x < a.to.x &&
				p_pos.y >= a.from.y && p_pos.y < a.to.y &&
				p_pos.z >= a.from.z && p_pos.z < a.to.z) {
			return E->key();
		}
	}

	return 0;
}
void GridMap::set_cell_item(int p_x, int p_y, int p_z, int p_item, int p_rot) {

	ERR_FAIL_INDEX(ABS(p_x), 1 << 20);
	ERR_FAIL_INDEX(ABS(p_y), 1 << 20);
	ERR_FAIL_INDEX(ABS(p_z), 1 << 20);

	IndexKey key;
	key.x = p_x;
	key.y = p_y;
	key.z = p_z;

	OctantKey ok;
	ok.x = p_x / octant_size;
	ok.y = p_y / octant_size;
	ok.z = p_z / octant_size;
	ok.area = _find_area(key);

	if (cell_map.has(key)) {

		int prev_item = cell_map[key].item;

		OctantKey octantkey = ok;

		ERR_FAIL_COND(!octant_map.has(octantkey));
		Octant &g = *octant_map[octantkey];
		ERR_FAIL_COND(!g.items.has(prev_item));
		ERR_FAIL_COND(!g.items[prev_item].cells.has(key));

		g.items[prev_item].cells.erase(key);
		if (g.items[prev_item].cells.size() == 0) {
			VS::get_singleton()->free(g.items[prev_item].multimesh_instance);
			g.items.erase(prev_item);
		}

		if (g.items.empty() || !baked_lock) {
			//unbake just in case
			if (g.baked.is_valid()) {
				VS::get_singleton()->free(g.bake_instance);
				g.bake_instance = RID();
				g.baked = Ref<Mesh>();
			}
		}
		if (g.items.empty()) {

			PhysicsServer::get_singleton()->free(g.static_body);
			if (g.collision_debug.is_valid()) {
				PhysicsServer::get_singleton()->free(g.collision_debug);
				PhysicsServer::get_singleton()->free(g.collision_debug_instance);
			}

			memdelete(&g);
			octant_map.erase(octantkey);
		} else {

			g.dirty = true;
		}
		cell_map.erase(key);

		_queue_dirty_map();
	}

	if (p_item < 0)
		return;

	OctantKey octantkey = ok;

	//add later
	if (!octant_map.has(octantkey)) {

		Octant *g = memnew(Octant);
		g->dirty = true;
		g->static_body = PhysicsServer::get_singleton()->body_create(PhysicsServer::BODY_MODE_STATIC);
		PhysicsServer::get_singleton()->body_attach_object_instance_ID(g->static_body, get_instance_ID());
		if (is_inside_world())
			PhysicsServer::get_singleton()->body_set_space(g->static_body, get_world()->get_space());

		SceneTree *st = SceneTree::get_singleton();

		if (st && st->is_debugging_collisions_hint()) {

			g->collision_debug = VisualServer::get_singleton()->mesh_create();
			g->collision_debug_instance = VisualServer::get_singleton()->instance_create();
			VisualServer::get_singleton()->instance_set_base(g->collision_debug_instance, g->collision_debug);
			if (is_inside_world()) {
				VisualServer::get_singleton()->instance_set_scenario(g->collision_debug_instance, get_world()->get_scenario());
				VisualServer::get_singleton()->instance_set_transform(g->collision_debug_instance, get_global_transform());
			}
		}

		octant_map[octantkey] = g;
	}

	Octant &g = *octant_map[octantkey];
	if (!g.items.has(p_item)) {

		Octant::ItemInstances ii;
		if (theme.is_valid() && theme->has_item(p_item)) {
			ii.mesh = theme->get_item_mesh(p_item);
			ii.shape = theme->get_item_shape(p_item);
			ii.navmesh = theme->get_item_navmesh(p_item);
		}
		ii.multimesh = Ref<MultiMesh>(memnew(MultiMesh));
		ii.multimesh->set_mesh(ii.mesh);
		ii.multimesh_instance = VS::get_singleton()->instance_create();
		VS::get_singleton()->instance_set_base(ii.multimesh_instance, ii.multimesh->get_rid());
		VS::get_singleton()->instance_geometry_set_baked_light(ii.multimesh_instance, baked_light_instance ? baked_light_instance->get_baked_light_instance() : RID());

		if (!baked_lock) {

			//unbake just in case
			if (g.bake_instance.is_valid())
				VS::get_singleton()->free(g.bake_instance);
			g.baked = Ref<Mesh>();
			if (is_inside_world()) {
				VS::get_singleton()->instance_set_scenario(ii.multimesh_instance, get_world()->get_scenario());
				if (ok.area) {
					VS::get_singleton()->instance_set_room(ii.multimesh_instance, area_map[ok.area]->instance);
				}
			}
		}
		g.items[p_item] = ii;
	}

	Octant::ItemInstances &ii = g.items[p_item];
	ii.cells.insert(key);
	g.dirty = true;

	_queue_dirty_map();

	cell_map[key] = Cell();
	Cell &c = cell_map[key];
	c.item = p_item;
	c.rot = p_rot;
}

int GridMap::get_cell_item(int p_x, int p_y, int p_z) const {

	ERR_FAIL_INDEX_V(ABS(p_x), 1 << 20, INVALID_CELL_ITEM);
	ERR_FAIL_INDEX_V(ABS(p_y), 1 << 20, INVALID_CELL_ITEM);
	ERR_FAIL_INDEX_V(ABS(p_z), 1 << 20, INVALID_CELL_ITEM);

	IndexKey key;
	key.x = p_x;
	key.y = p_y;
	key.z = p_z;

	if (!cell_map.has(key))
		return INVALID_CELL_ITEM;
	return cell_map[key].item;
}

int GridMap::get_cell_item_orientation(int p_x, int p_y, int p_z) const {

	ERR_FAIL_INDEX_V(ABS(p_x), 1 << 20, -1);
	ERR_FAIL_INDEX_V(ABS(p_y), 1 << 20, -1);
	ERR_FAIL_INDEX_V(ABS(p_z), 1 << 20, -1);

	IndexKey key;
	key.x = p_x;
	key.y = p_y;
	key.z = p_z;

	if (!cell_map.has(key))
		return -1;
	return cell_map[key].rot;
}

void GridMap::_octant_enter_tree(const OctantKey &p_key) {
	ERR_FAIL_COND(!octant_map.has(p_key));
	if (navigation) {
		Octant &g = *octant_map[p_key];

		Vector3 ofs(cell_size * 0.5 * int(center_x), cell_size * 0.5 * int(center_y), cell_size * 0.5 * int(center_z));
		_octant_clear_navmesh(p_key);

		for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {
			Octant::ItemInstances &ii = E->get();

			for (Set<IndexKey>::Element *F = ii.cells.front(); F; F = F->next()) {

				IndexKey ik = F->get();
				Map<IndexKey, Cell>::Element *C = cell_map.find(ik);
				ERR_CONTINUE(!C);

				Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

				Transform xform;

				if (clip && ((clip_above && cellpos[clip_axis] > clip_floor) || (!clip_above && cellpos[clip_axis] < clip_floor))) {

					xform.basis.set_zero();

				} else {

					xform.basis.set_orthogonal_index(C->get().rot);
				}

				xform.set_origin(cellpos * cell_size + ofs);
				xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));
				// add the item's navmesh at given xform to GridMap's Navigation ancestor
				if (ii.navmesh.is_valid()) {
					int nm_id = navigation->navmesh_create(ii.navmesh, xform, this);
					Octant::NavMesh nm;
					nm.id = nm_id;
					nm.xform = xform;
					g.navmesh_ids[ik] = nm;
				}
			}
		}
	}
}

void GridMap::_octant_enter_world(const OctantKey &p_key) {

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer::get_singleton()->body_set_state(g.static_body, PhysicsServer::BODY_STATE_TRANSFORM, get_global_transform());
	PhysicsServer::get_singleton()->body_set_space(g.static_body, get_world()->get_space());
	//print_line("BODYPOS: "+get_global_transform());

	if (g.collision_debug_instance.is_valid()) {
		VS::get_singleton()->instance_set_scenario(g.collision_debug_instance, get_world()->get_scenario());
		VS::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
		if (area_map.has(p_key.area)) {
			VS::get_singleton()->instance_set_room(g.collision_debug_instance, area_map[p_key.area]->instance);
		}
	}
	if (g.baked.is_valid()) {

		Transform xf = get_global_transform();
		xf.translate(_octant_get_offset(p_key));

		VS::get_singleton()->instance_set_transform(g.bake_instance, xf);
		VS::get_singleton()->instance_set_scenario(g.bake_instance, get_world()->get_scenario());
		if (area_map.has(p_key.area)) {
			VS::get_singleton()->instance_set_room(g.bake_instance, area_map[p_key.area]->instance);
		}
	} else {
		for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {

			VS::get_singleton()->instance_set_scenario(E->get().multimesh_instance, get_world()->get_scenario());
			VS::get_singleton()->instance_set_transform(E->get().multimesh_instance, get_global_transform());
			//print_line("INSTANCEPOS: "+get_global_transform());

			if (area_map.has(p_key.area)) {
				VS::get_singleton()->instance_set_room(E->get().multimesh_instance, area_map[p_key.area]->instance);
			}
		}
	}
}

void GridMap::_octant_transform(const OctantKey &p_key) {

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer::get_singleton()->body_set_state(g.static_body, PhysicsServer::BODY_STATE_TRANSFORM, get_global_transform());

	if (g.collision_debug_instance.is_valid()) {
		VS::get_singleton()->instance_set_transform(g.collision_debug_instance, get_global_transform());
	}

	if (g.baked.is_valid()) {

		Transform xf = get_global_transform();
		xf.origin += _octant_get_offset(p_key);
		VS::get_singleton()->instance_set_transform(g.bake_instance, xf);
	} else {
		for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {

			VS::get_singleton()->instance_set_transform(E->get().multimesh_instance, get_global_transform());
			//print_line("UPDATEPOS: "+get_global_transform());
		}
	}
}

void GridMap::_octant_clear_navmesh(const OctantKey &p_key) {
	Octant &g = *octant_map[p_key];
	if (navigation) {
		for (Map<IndexKey, Octant::NavMesh>::Element *E = g.navmesh_ids.front(); E; E = E->next()) {
			Octant::NavMesh *nvm = &E->get();
			if (nvm && nvm->id) {
				navigation->navmesh_remove(E->get().id);
			}
		}
		g.navmesh_ids.clear();
	}
}

void GridMap::_octant_update(const OctantKey &p_key) {
	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	if (!g.dirty)
		return;

	Ref<Mesh> mesh;

	_octant_clear_navmesh(p_key);
	PhysicsServer::get_singleton()->body_clear_shapes(g.static_body);

	if (g.collision_debug.is_valid()) {

		VS::get_singleton()->mesh_clear(g.collision_debug);
	}

	DVector<Vector3> col_debug;

	/*
	 * foreach item in this octant,
	 * set item's multimesh's instance count to number of cells which have this item
	 * and set said multimesh bounding box to one containing all cells which have this item
	 */
	for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {

		Octant::ItemInstances &ii = E->get();

		ii.multimesh->set_instance_count(ii.cells.size());

		AABB aabb;
		AABB mesh_aabb = ii.mesh.is_null() ? AABB() : ii.mesh->get_aabb();

		Vector3 ofs(cell_size * 0.5 * int(center_x), cell_size * 0.5 * int(center_y), cell_size * 0.5 * int(center_z));

		//print_line("OCTANT, CELLS: "+itos(ii.cells.size()));
		int idx = 0;
		// foreach cell containing this item type
		for (Set<IndexKey>::Element *F = ii.cells.front(); F; F = F->next()) {
			IndexKey ik = F->get();
			Map<IndexKey, Cell>::Element *C = cell_map.find(ik);
			ERR_CONTINUE(!C);

			Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

			Transform xform;

			if (clip && ((clip_above && cellpos[clip_axis] > clip_floor) || (!clip_above && cellpos[clip_axis] < clip_floor))) {

				xform.basis.set_zero();

			} else {

				xform.basis.set_orthogonal_index(C->get().rot);
			}

			xform.set_origin(cellpos * cell_size + ofs);
			xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

			ii.multimesh->set_instance_transform(idx, xform);
			//ii.multimesh->set_instance_transform(idx,Transform()	);
			ii.multimesh->set_instance_color(idx, Color(1, 1, 1, 1));
			//print_line("MMINST: "+xform);

			if (idx == 0) {

				aabb = xform.xform(mesh_aabb);
			} else {

				aabb.merge_with(xform.xform(mesh_aabb));
			}

			// add the item's shape at given xform to octant's static_body
			if (ii.shape.is_valid()) {
				// add the item's shape
				PhysicsServer::get_singleton()->body_add_shape(g.static_body, ii.shape->get_rid(), xform);
				if (g.collision_debug.is_valid()) {
					ii.shape->add_vertices_to_array(col_debug, xform);
				}

				//	print_line("PHIS x: "+xform);
			}

			// add the item's navmesh at given xform to GridMap's Navigation ancestor
			if (navigation) {
				if (ii.navmesh.is_valid()) {
					int nm_id = navigation->navmesh_create(ii.navmesh, xform, this);
					Octant::NavMesh nm;
					nm.id = nm_id;
					nm.xform = xform;
					g.navmesh_ids[ik] = nm;
				}
			}

			idx++;
		}

		ii.multimesh->set_aabb(aabb);
	}

	if (col_debug.size()) {

		Array arr;
		arr.resize(VS::ARRAY_MAX);
		arr[VS::ARRAY_VERTEX] = col_debug;

		VS::get_singleton()->mesh_add_surface(g.collision_debug, VS::PRIMITIVE_LINES, arr);
		SceneTree *st = SceneTree::get_singleton();
		if (st) {
			VS::get_singleton()->mesh_surface_set_material(g.collision_debug, 0, st->get_debug_collision_material()->get_rid());
		}
	}

	g.dirty = false;
}

void GridMap::_octant_exit_world(const OctantKey &p_key) {

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];
	PhysicsServer::get_singleton()->body_set_state(g.static_body, PhysicsServer::BODY_STATE_TRANSFORM, get_global_transform());
	PhysicsServer::get_singleton()->body_set_space(g.static_body, RID());

	if (g.baked.is_valid()) {

		VS::get_singleton()->instance_set_room(g.bake_instance, RID());
		VS::get_singleton()->instance_set_scenario(g.bake_instance, RID());
	}

	if (g.collision_debug_instance.is_valid()) {

		VS::get_singleton()->instance_set_room(g.collision_debug_instance, RID());
		VS::get_singleton()->instance_set_scenario(g.collision_debug_instance, RID());
	}

	for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {

		VS::get_singleton()->instance_set_scenario(E->get().multimesh_instance, RID());
		//	VS::get_singleton()->instance_set_transform(E->get().multimesh_instance,get_global_transform());
		VS::get_singleton()->instance_set_room(E->get().multimesh_instance, RID());
	}
}

void GridMap::_octant_clear_baked(const OctantKey &p_key) {

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];

	if (!g.baked.is_valid())
		return;

	VS::get_singleton()->free(g.bake_instance);
	g.bake_instance = RID();
	g.baked = Ref<Mesh>();

	if (is_inside_tree())
		_octant_enter_world(p_key);
	g.dirty = true;
	_queue_dirty_map();
}

void GridMap::_octant_bake(const OctantKey &p_key, const Ref<TriangleMesh> &p_tmesh, const Vector<BakeLight> &p_lights, List<Vector3> *p_prebake) {

	ERR_FAIL_COND(!octant_map.has(p_key));
	Octant &g = *octant_map[p_key];

	Ref<TriangleMesh> tm = p_tmesh;
	if (!p_prebake && is_inside_world())
		_octant_exit_world(p_key);

	Map<Ref<Material>, Ref<SurfaceTool> > surfaces;
	Vector3 ofs(cell_size * 0.5 * int(center_x), cell_size * 0.5 * int(center_y), cell_size * 0.5 * int(center_z));
	Vector3 octant_ofs = _octant_get_offset(p_key);

	for (Map<int, Octant::ItemInstances>::Element *E = g.items.front(); E; E = E->next()) {

		Octant::ItemInstances &ii = E->get();

		if (ii.mesh.is_null())
			continue;

		for (Set<IndexKey>::Element *F = ii.cells.front(); F; F = F->next()) {

			IndexKey ik = F->get();
			Map<IndexKey, Cell>::Element *C = cell_map.find(ik);
			ERR_CONTINUE(!C);
			Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

			Transform xform;
			xform.basis.set_orthogonal_index(C->get().rot);
			xform.set_origin(cellpos * cell_size + ofs);
			if (!p_prebake)
				xform.origin -= octant_ofs;

			for (int i = 0; i < ii.mesh->get_surface_count(); i++) {

				if (p_prebake) {

					if (ii.mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES)
						continue;
					Array a = ii.mesh->surface_get_arrays(i);
					DVector<Vector3> av = a[VS::ARRAY_VERTEX];
					int avs = av.size();
					DVector<Vector3>::Read vr = av.read();

					DVector<int> ai = a[VS::ARRAY_INDEX];
					int ais = ai.size();
					if (ais) {

						DVector<int>::Read ir = ai.read();
						for (int j = 0; j < ais; j++) {

							p_prebake->push_back(xform.xform(vr[ir[j]]));
							//print_line("V SET: "+xform.xform(vr[ir[j]]));
						}

					} else {

						for (int j = 0; j < avs; j++) {

							p_prebake->push_back(xform.xform(vr[j]));
						}
					}

				} else {

					Ref<Material> m = ii.mesh->surface_get_material(i);

					Map<Ref<Material>, Ref<SurfaceTool> >::Element *S = surfaces.find(m);

					if (!S) {

						S = surfaces.insert(m, Ref<SurfaceTool>(memnew(SurfaceTool)));
					}

					Ref<SurfaceTool> st = S->get();
					List<SurfaceTool::Vertex>::Element *V = st->get_vertex_array().back();
					st->append_from(ii.mesh, i, xform);
					st->set_material(m);

					if (tm.is_valid()) {

						if (V)
							V = V->next();
						else
							V = st->get_vertex_array().front();
						int lc = p_lights.size();
						const BakeLight *bl = p_lights.ptr();
						float ofs = cell_size * 0.02;

						for (; V; V = V->next()) {

							SurfaceTool::Vertex &v = V->get();

							Vector3 vertex = v.vertex + octant_ofs;
							//print_line("V GET: "+vertex);
							Vector3 normal = tm->get_area_normal(AABB(Vector3(-ofs, -ofs, -ofs) + vertex, Vector3(ofs, ofs, ofs) * 2.0));
							if (normal == Vector3()) {
								print_line("couldn't find for vertex: " + vertex);
							}
							ERR_CONTINUE(normal == Vector3());

							float max_l = 1.0;
							float max_dist = 1.0;

							if (lc) {

								for (int j = 0; j < lc; j++) {
									const BakeLight &l = bl[j];
									switch (l.type) {
										case VS::LIGHT_DIRECTIONAL: {

											Vector3 ray_from = vertex + normal * ofs;
											Vector3 ray_to = l.dir * 5000;
											Vector3 n;
											Vector3 p;
											if (tm->intersect_segment(ray_from, ray_to, p, n)) {

												float dist = 1.0 - l.param[VS::LIGHT_PARAM_SHADOW_DARKENING];
												if (dist <= max_dist) {
													max_dist = dist;
													max_l = 1.0 - dist;
												}
											}
										} break;
									}
								}
							}

							v.color = Color(max_l, max_l, max_l, 1.0);
						}

						st->add_to_format(VS::ARRAY_FORMAT_COLOR);
						if (m.is_valid()) {
							Ref<FixedMaterial> fm = m;
							if (fm.is_valid())
								fm->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
						}
					}
				}
			}
		}
	}

	if (p_prebake)
		return;

	g.baked = Ref<Mesh>(memnew(Mesh));

	for (Map<Ref<Material>, Ref<SurfaceTool> >::Element *E = surfaces.front(); E; E = E->next()) {

		Ref<SurfaceTool> st = E->get();
		st->commit(g.baked);
	}

	g.bake_instance = VS::get_singleton()->instance_create();
	VS::get_singleton()->instance_set_base(g.bake_instance, g.baked->get_rid());

	if (is_inside_world())
		_octant_enter_world(p_key);

	g.dirty = true;
	_queue_dirty_map();
}

void GridMap::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_WORLD: {

			_update_area_instances();

			for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
				//				IndexKey ik;
				//				ik.key = E->key().indexkey;
				_octant_enter_world(E->key());
				_octant_update(E->key());
			}

			awaiting_update = false;

			last_transform = get_global_transform();

			if (use_baked_light) {

				_find_baked_light();
			}

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {

			Transform new_xform = get_global_transform();
			if (new_xform == last_transform)
				break;
			//update run
			for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
				_octant_transform(E->key());
			}

			last_transform = new_xform;

		} break;
		case NOTIFICATION_EXIT_WORLD: {

			for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
				_octant_exit_world(E->key());
			}

			if (use_baked_light) {

				if (baked_light_instance) {
					baked_light_instance->disconnect(SceneStringNames::get_singleton()->baked_light_changed, this, SceneStringNames::get_singleton()->_baked_light_changed);
					baked_light_instance = NULL;
				}
				_baked_light_changed();
			}

			//_queue_dirty_map(MAP_DIRTY_INSTANCES|MAP_DIRTY_TRANSFORMS);
			//_update_dirty_map_callback();
			//_update_area_instances();

		} break;
		case NOTIFICATION_ENTER_TREE: {

			Spatial *c = this;
			while (c) {
				navigation = c->cast_to<Navigation>();
				if (navigation) {
					break;
				}

				c = c->get_parent()->cast_to<Spatial>();
			}

			if (navigation) {
				for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
					if (navigation) {
						_octant_enter_tree(E->key());
					}
				}
			}

			_queue_dirty_map();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
				if (navigation) {
					_octant_clear_navmesh(E->key());
				}
			}

			navigation = NULL;

		} break;
	}
}

void GridMap::_queue_dirty_map() {

	if (awaiting_update)
		return;

	if (is_inside_world()) {

		MessageQueue::get_singleton()->push_call(this, "_update_dirty_map_callback");
		awaiting_update = true;
	}
}

void GridMap::_recreate_octant_data() {

	Map<IndexKey, Cell> cell_copy = cell_map;
	_clear_internal(true);
	for (Map<IndexKey, Cell>::Element *E = cell_copy.front(); E; E = E->next()) {

		set_cell_item(E->key().x, E->key().y, E->key().z, E->get().item, E->get().rot);
	}
}

void GridMap::_clear_internal(bool p_keep_areas) {

	for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
		if (is_inside_world())
			_octant_exit_world(E->key());

		for (Map<int, Octant::ItemInstances>::Element *F = E->get()->items.front(); F; F = F->next()) {

			VS::get_singleton()->free(F->get().multimesh_instance);
		}

		//unbake just in case
		if (E->get()->bake_instance.is_valid())
			VS::get_singleton()->free(E->get()->bake_instance);

		if (E->get()->collision_debug.is_valid())
			VS::get_singleton()->free(E->get()->collision_debug);
		if (E->get()->collision_debug_instance.is_valid())
			VS::get_singleton()->free(E->get()->collision_debug_instance);

		PhysicsServer::get_singleton()->free(E->get()->static_body);
		memdelete(E->get());
	}

	octant_map.clear();
	cell_map.clear();

	if (p_keep_areas)
		return;

	for (Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {

		VS::get_singleton()->free(E->get()->base_portal);
		VS::get_singleton()->free(E->get()->instance);
		for (int i = 0; i < E->get()->portals.size(); i++) {
			VS::get_singleton()->free(E->get()->portals[i].instance);
		}

		memdelete(E->get());
	}
}

void GridMap::clear() {

	_clear_internal();
}

void GridMap::resource_changed(const RES &p_res) {

	_recreate_octant_data();
}

void GridMap::_update_dirty_map_callback() {

	if (!awaiting_update)
		return;

	for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
		_octant_update(E->key());
	}

	awaiting_update = false;
}

void GridMap::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_theme", "theme:MeshLibrary"), &GridMap::set_theme);
	ObjectTypeDB::bind_method(_MD("get_theme:MeshLibrary"), &GridMap::get_theme);

	ObjectTypeDB::bind_method(_MD("set_bake", "enable"), &GridMap::set_bake);
	ObjectTypeDB::bind_method(_MD("is_baking_enabled"), &GridMap::is_baking_enabled);

	ObjectTypeDB::bind_method(_MD("set_cell_size", "size"), &GridMap::set_cell_size);
	ObjectTypeDB::bind_method(_MD("get_cell_size"), &GridMap::get_cell_size);

	ObjectTypeDB::bind_method(_MD("set_octant_size", "size"), &GridMap::set_octant_size);
	ObjectTypeDB::bind_method(_MD("get_octant_size"), &GridMap::get_octant_size);

	ObjectTypeDB::bind_method(_MD("set_cell_item", "x", "y", "z", "item", "orientation"), &GridMap::set_cell_item, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("get_cell_item", "x", "y", "z"), &GridMap::get_cell_item);
	ObjectTypeDB::bind_method(_MD("get_cell_item_orientation", "x", "y", "z"), &GridMap::get_cell_item_orientation);

	//	ObjectTypeDB::bind_method(_MD("_recreate_octants"),&GridMap::_recreate_octants);
	ObjectTypeDB::bind_method(_MD("_update_dirty_map_callback"), &GridMap::_update_dirty_map_callback);
	ObjectTypeDB::bind_method(_MD("resource_changed", "resource"), &GridMap::resource_changed);

	ObjectTypeDB::bind_method(_MD("set_center_x", "enable"), &GridMap::set_center_x);
	ObjectTypeDB::bind_method(_MD("get_center_x"), &GridMap::get_center_x);
	ObjectTypeDB::bind_method(_MD("set_center_y", "enable"), &GridMap::set_center_y);
	ObjectTypeDB::bind_method(_MD("get_center_y"), &GridMap::get_center_y);
	ObjectTypeDB::bind_method(_MD("set_center_z", "enable"), &GridMap::set_center_z);
	ObjectTypeDB::bind_method(_MD("get_center_z"), &GridMap::get_center_z);

	ObjectTypeDB::bind_method(_MD("set_clip", "enabled", "clipabove", "floor", "axis"), &GridMap::set_clip, DEFVAL(true), DEFVAL(0), DEFVAL(Vector3::AXIS_X));

	ObjectTypeDB::bind_method(_MD("create_area", "id", "area"), &GridMap::create_area);
	ObjectTypeDB::bind_method(_MD("area_get_bounds", "area", "bounds"), &GridMap::area_get_bounds);
	ObjectTypeDB::bind_method(_MD("area_set_exterior_portal", "area", "enable"), &GridMap::area_set_exterior_portal);
	ObjectTypeDB::bind_method(_MD("area_set_name", "area", "name"), &GridMap::area_set_name);
	ObjectTypeDB::bind_method(_MD("area_get_name", "area"), &GridMap::area_get_name);
	ObjectTypeDB::bind_method(_MD("area_is_exterior_portal", "area"), &GridMap::area_is_exterior_portal);
	ObjectTypeDB::bind_method(_MD("area_set_portal_disable_distance", "area", "distance"), &GridMap::area_set_portal_disable_distance);
	ObjectTypeDB::bind_method(_MD("area_get_portal_disable_distance", "area"), &GridMap::area_get_portal_disable_distance);
	ObjectTypeDB::bind_method(_MD("area_set_portal_disable_color", "area", "color"), &GridMap::area_set_portal_disable_color);
	ObjectTypeDB::bind_method(_MD("area_get_portal_disable_color", "area"), &GridMap::area_get_portal_disable_color);
	ObjectTypeDB::bind_method(_MD("erase_area", "area"), &GridMap::erase_area);
	ObjectTypeDB::bind_method(_MD("get_unused_area_id", "area"), &GridMap::get_unused_area_id);
	ObjectTypeDB::bind_method(_MD("bake_geometry"), &GridMap::bake_geometry);

	ObjectTypeDB::bind_method(_MD("_baked_light_changed"), &GridMap::_baked_light_changed);
	ObjectTypeDB::bind_method(_MD("set_use_baked_light", "use"), &GridMap::set_use_baked_light);
	ObjectTypeDB::bind_method(_MD("is_using_baked_light", "use"), &GridMap::is_using_baked_light);

	ObjectTypeDB::bind_method(_MD("_get_baked_light_meshes"), &GridMap::_get_baked_light_meshes);

	ObjectTypeDB::set_method_flags("GridMap", "bake_geometry", METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

	ObjectTypeDB::bind_method(_MD("clear"), &GridMap::clear);

	BIND_CONSTANT(INVALID_CELL_ITEM);
}

void GridMap::set_clip(bool p_enabled, bool p_clip_above, int p_floor, Vector3::Axis p_axis) {

	if (!p_enabled && !clip)
		return;
	if (clip && p_enabled && clip_floor == p_floor && p_clip_above == clip_above && p_axis == clip_axis)
		return;

	clip = p_enabled;
	clip_floor = p_floor;
	clip_axis = p_axis;
	clip_above = p_clip_above;

	//make it all update
	for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {

		Octant *g = E->get();
		g->dirty = true;
	}
	awaiting_update = true;
	_update_dirty_map_callback();
}

void GridMap::_update_areas() {

	//clear the portals
	for (Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {
		//this should somehow be faster...
		Area &a = *E->get();
		a.portals.clear();
		if (a.instance.is_valid()) {
			VisualServer::get_singleton()->free(a.instance);
			a.instance = RID();
		}
	}

	//test all areas against all areas and create portals - this sucks (slow :( )
	for (Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {
		Area &a = *E->get();
		if (a.exterior_portal) //that's pretty much all it does... yes it is
			continue;
		Vector3 from_a(a.from.x, a.from.y, a.from.z);
		Vector3 to_a(a.to.x, a.to.y, a.to.z);

		for (Map<int, Area *>::Element *F = area_map.front(); F; F = F->next()) {

			Area &b = *F->get();
			Vector3 from_b(b.from.x, b.from.y, b.from.z);
			Vector3 to_b(b.to.x, b.to.y, b.to.z);

			// initially test intersection and discards
			int axis = -1;
			float sign = 0;
			bool valid = true;
			Vector3 axmin, axmax;

			for (int i = 0; i < 3; i++) {

				if (from_a[i] == to_b[i]) {

					if (axis != -1) {
						valid = false;
						break;
					}

					axis = i;
					sign = -1;
				} else if (from_b[i] == to_a[i]) {

					if (axis != -1) {
						valid = false;
						break;
					}
					axis = i;
					sign = +1;
				}

				if (from_a[i] > to_b[i] || to_a[i] < from_b[i]) {
					valid = false;
					break;
				} else {

					axmin[i] = (from_a[i] > from_b[i]) ? from_a[i] : from_b[i];
					axmax[i] = (to_a[i] < to_b[i]) ? to_a[i] : to_b[i];
				}
			}

			if (axis == -1 || !valid)
				continue;

			Transform xf;

			for (int i = 0; i < 3; i++) {

				int ax = (axis + i) % 3;
				Vector3 axis_vec;
				float scale = (i == 0) ? sign : ((axmax[ax] - axmin[ax]) * cell_size);
				axis_vec[ax] = scale;
				xf.basis.set_axis((2 + i) % 3, axis_vec);
				xf.origin[i] = axmin[i] * cell_size;
			}

			Area::Portal portal;
			portal.xform = xf;
			a.portals.push_back(portal);
		}
	}

	_update_area_instances();
}

void GridMap::_update_area_instances() {

	Transform base_xform;
	if (_in_tree)
		base_xform = get_global_transform();

	for (Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {
		//this should somehow be faster...
		Area &a = *E->get();
		if (a.instance.is_valid() != _in_tree) {

			if (!_in_tree) {

				for (int i = 0; i < a.portals.size(); i++) {

					Area::Portal &p = a.portals[i];
					ERR_CONTINUE(!p.instance.is_valid());
					VisualServer::get_singleton()->free(p.instance);
					p.instance = RID();
				}

				VisualServer::get_singleton()->free(a.instance);
				a.instance = RID();

			} else {

				//a.instance = VisualServer::get_singleton()->instance_create2(base_room,get_world()->get_scenario());
				for (int i = 0; i < a.portals.size(); i++) {

					Area::Portal &p = a.portals[i];
					ERR_CONTINUE(p.instance.is_valid());
					p.instance = VisualServer::get_singleton()->instance_create2(a.base_portal, get_world()->get_scenario());
					VisualServer::get_singleton()->instance_set_room(p.instance, a.instance);
				}
			}
		}

		if (a.instance.is_valid()) {
			Transform xform;

			Vector3 from_a(a.from.x, a.from.y, a.from.z);
			Vector3 to_a(a.to.x, a.to.y, a.to.z);

			for (int i = 0; i < 3; i++) {
				xform.origin[i] = from_a[i] * cell_size;
				Vector3 s;
				s[i] = (to_a[i] - from_a[i]) * cell_size;
				xform.basis.set_axis(i, s);
			}

			VisualServer::get_singleton()->instance_set_transform(a.instance, base_xform * xform);

			for (int i = 0; i < a.portals.size(); i++) {

				Area::Portal &p = a.portals[i];
				ERR_CONTINUE(!p.instance.is_valid());

				VisualServer::get_singleton()->instance_set_transform(p.instance, base_xform * xform);
			}
		}
	}
}

Error GridMap::create_area(int p_id, const AABB &p_bounds) {

	ERR_FAIL_COND_V(area_map.has(p_id), ERR_ALREADY_EXISTS);
	ERR_EXPLAIN("ID 0 is taken as global area, start from 1");
	ERR_FAIL_COND_V(p_id == 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_bounds.has_no_area(), ERR_INVALID_PARAMETER);

	// FIRST VALIDATE AREA
	IndexKey from, to;
	from.x = p_bounds.pos.x;
	from.y = p_bounds.pos.y;
	from.z = p_bounds.pos.z;
	to.x = p_bounds.pos.x + p_bounds.size.x;
	to.y = p_bounds.pos.y + p_bounds.size.y;
	to.z = p_bounds.pos.z + p_bounds.size.z;

	for (Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {
		//this should somehow be faster...
		Area &a = *E->get();

		//does it interset with anything else?

		if (from.x >= a.to.x ||
				to.x <= a.from.x ||
				from.y >= a.to.y ||
				to.y <= a.from.y ||
				from.z >= a.to.z ||
				to.z <= a.from.z) {

			// all good
		} else {

			return ERR_INVALID_PARAMETER;
		}
	}

	Area *area = memnew(Area);
	area->from = from;
	area->to = to;
	area->portal_disable_distance = 0;
	area->exterior_portal = false;
	area->name = "Area " + itos(p_id);
	area_map[p_id] = area;
	_recreate_octant_data();
	return OK;
}

AABB GridMap::area_get_bounds(int p_area) const {

	ERR_FAIL_COND_V(!area_map.has(p_area), AABB());

	const Area *a = area_map[p_area];
	AABB aabb;
	aabb.pos = Vector3(a->from.x, a->from.y, a->from.z);
	aabb.size = Vector3(a->to.x, a->to.y, a->to.z) - aabb.pos;

	return aabb;
}

void GridMap::area_set_name(int p_area, const String &p_name) {

	ERR_FAIL_COND(!area_map.has(p_area));

	Area *a = area_map[p_area];
	a->name = p_name;
}

String GridMap::area_get_name(int p_area) const {

	ERR_FAIL_COND_V(!area_map.has(p_area), "");

	const Area *a = area_map[p_area];
	return a->name;
}

void GridMap::area_set_exterior_portal(int p_area, bool p_enable) {

	ERR_FAIL_COND(!area_map.has(p_area));

	Area *a = area_map[p_area];
	if (a->exterior_portal == p_enable)
		return;
	a->exterior_portal = p_enable;

	_recreate_octant_data();
}

bool GridMap::area_is_exterior_portal(int p_area) const {

	ERR_FAIL_COND_V(!area_map.has(p_area), false);

	const Area *a = area_map[p_area];
	return a->exterior_portal;
}

void GridMap::area_set_portal_disable_distance(int p_area, float p_distance) {

	ERR_FAIL_COND(!area_map.has(p_area));

	Area *a = area_map[p_area];
	a->portal_disable_distance = p_distance;
}

float GridMap::area_get_portal_disable_distance(int p_area) const {

	ERR_FAIL_COND_V(!area_map.has(p_area), 0);

	const Area *a = area_map[p_area];
	return a->portal_disable_distance;
}

void GridMap::area_set_portal_disable_color(int p_area, Color p_color) {

	ERR_FAIL_COND(!area_map.has(p_area));

	Area *a = area_map[p_area];
	a->portal_disable_color = p_color;
}

Color GridMap::area_get_portal_disable_color(int p_area) const {

	ERR_FAIL_COND_V(!area_map.has(p_area), Color());

	const Area *a = area_map[p_area];
	return a->portal_disable_color;
}

void GridMap::get_area_list(List<int> *p_areas) const {

	for (const Map<int, Area *>::Element *E = area_map.front(); E; E = E->next()) {

		p_areas->push_back(E->key());
	}
}

GridMap::Area::Portal::~Portal() {

	if (instance.is_valid())
		VisualServer::get_singleton()->free(instance);
}

GridMap::Area::Area() {

	base_portal = VisualServer::get_singleton()->portal_create();
	Vector<Point2> points;
	points.push_back(Point2(0, 1));
	points.push_back(Point2(1, 1));
	points.push_back(Point2(1, 0));
	points.push_back(Point2(0, 0));
	VisualServer::get_singleton()->portal_set_shape(base_portal, points);
}

GridMap::Area::~Area() {

	if (instance.is_valid())
		VisualServer::get_singleton()->free(instance);
	VisualServer::get_singleton()->free(base_portal);
}

void GridMap::erase_area(int p_area) {

	ERR_FAIL_COND(!area_map.has(p_area));

	Area *a = area_map[p_area];
	memdelete(a);
	area_map.erase(p_area);
	_recreate_octant_data();
}

int GridMap::get_unused_area_id() const {

	if (area_map.empty())
		return 1;
	else
		return area_map.back()->key() + 1;
}

void GridMap::set_bake(bool p_bake) {

	bake = p_bake;
	if (bake == false) {
		for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {

			_octant_clear_baked(E->key());
		}
	}
}

bool GridMap::is_baking_enabled() const {

	return bake;
}

void GridMap::set_cell_scale(float p_scale) {

	cell_scale = p_scale;
	_queue_dirty_map();
}

float GridMap::get_cell_scale() const {

	return cell_scale;
}

void GridMap::bake_geometry() {

	//used to compute vertex occlusion
	Ref<TriangleMesh> tmesh;
	Vector<BakeLight> lights;

	if (true) {

		List<Vector3> vertices;

		for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
			_octant_bake(E->key(), tmesh, lights, &vertices);
		}

		DVector<Vector3> vv;
		vv.fill_with(vertices);
		//print_line("TOTAL VERTICES: "+itos(vv.size()));
		tmesh = Ref<TriangleMesh>(memnew(TriangleMesh));
		tmesh->create(vv);

		for (int i = 0; i < get_child_count(); i++) {

			if (get_child(i)->cast_to<Light>()) {
				Light *l = get_child(i)->cast_to<Light>();
				BakeLight bl;
				for (int i = 0; i < Light::PARAM_MAX; i++) {
					bl.param[i] = l->get_parameter(Light::Parameter(i));
				}
				Transform t = l->get_global_transform();
				bl.pos = t.origin;
				bl.dir = t.basis.get_axis(2);
				bl.type = l->get_light_type();
				lights.push_back(bl);
			}
		}
	}

	int idx = 0;
	for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {
		if (E->get()->baked.is_valid())
			_octant_clear_baked(E->key());

		_octant_bake(E->key(), tmesh, lights);
		print_line("baking " + itos(idx) + "/" + itos(octant_map.size()));
		idx++;
	}
}

void GridMap::_baked_light_changed() {

	//	if (!baked_light_instance)
	//		VS::get_singleton()->instance_geometry_set_baked_light(get_instance(),RID());
	//	else
	//		VS::get_singleton()->instance_geometry_set_baked_light(get_instance(),baked_light_instance->get_baked_light_instance());
	for (Map<OctantKey, Octant *>::Element *E = octant_map.front(); E; E = E->next()) {

		for (Map<int, Octant::ItemInstances>::Element *F = E->get()->items.front(); F; F = F->next()) {

			VS::get_singleton()->instance_geometry_set_baked_light(F->get().multimesh_instance, baked_light_instance ? baked_light_instance->get_baked_light_instance() : RID());
		}
	}
}

void GridMap::_find_baked_light() {

	Node *n = get_parent();
	while (n) {

		BakedLightInstance *bl = n->cast_to<BakedLightInstance>();
		if (bl) {

			baked_light_instance = bl;
			baked_light_instance->connect(SceneStringNames::get_singleton()->baked_light_changed, this, SceneStringNames::get_singleton()->_baked_light_changed);
			_baked_light_changed();

			return;
		}

		n = n->get_parent();
	}

	_baked_light_changed();
}

Array GridMap::_get_baked_light_meshes() {

	if (theme.is_null())
		return Array();

	Vector3 ofs(cell_size * 0.5 * int(center_x), cell_size * 0.5 * int(center_y), cell_size * 0.5 * int(center_z));
	Array meshes;

	for (Map<IndexKey, Cell>::Element *E = cell_map.front(); E; E = E->next()) {

		int id = E->get().item;
		if (!theme->has_item(id))
			continue;
		Ref<Mesh> mesh = theme->get_item_mesh(id);
		if (mesh.is_null())
			continue;

		IndexKey ik = E->key();

		Vector3 cellpos = Vector3(ik.x, ik.y, ik.z);

		Transform xform;

		xform.basis.set_orthogonal_index(E->get().rot);

		xform.set_origin(cellpos * cell_size + ofs);
		xform.basis.scale(Vector3(cell_scale, cell_scale, cell_scale));

		meshes.push_back(xform);
		meshes.push_back(mesh);
	}

	return meshes;
}

void GridMap::set_use_baked_light(bool p_use) {

	if (use_baked_light == p_use)
		return;

	use_baked_light = p_use;

	if (is_inside_world()) {
		if (!p_use) {
			if (baked_light_instance) {
				baked_light_instance->disconnect(SceneStringNames::get_singleton()->baked_light_changed, this, SceneStringNames::get_singleton()->_baked_light_changed);
				baked_light_instance = NULL;
			}
			_baked_light_changed();
		} else {
			_find_baked_light();
		}
	}
}

bool GridMap::is_using_baked_light() const {

	return use_baked_light;
}

GridMap::GridMap() {

	cell_size = 2;
	octant_size = 4;
	awaiting_update = false;
	_in_tree = false;
	center_x = true;
	center_y = true;
	center_z = true;

	clip = false;
	clip_floor = 0;
	clip_axis = Vector3::AXIS_Z;
	clip_above = true;
	baked_lock = false;
	bake = false;
	cell_scale = 1.0;

	baked_light_instance = NULL;
	use_baked_light = false;

	navigation = NULL;
}

GridMap::~GridMap() {

	if (!theme.is_null())
		theme->unregister_owner(this);

	clear();
}
