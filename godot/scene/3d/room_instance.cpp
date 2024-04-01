/*************************************************************************/
/*  room_instance.cpp                                                    */
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
#include "room_instance.h"

#include "servers/visual_server.h"

#include "geometry.h"
#include "globals.h"
#include "scene/resources/surface_tool.h"

void Room::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			// go find parent level
			Node *parent_room = get_parent();
			level = 0;

			while (parent_room) {

				Room *r = parent_room->cast_to<Room>();
				if (r) {

					level = r->level + 1;
					break;
				}

				parent_room = parent_room->get_parent();
			}

			if (sound_enabled)
				SpatialSoundServer::get_singleton()->room_set_space(sound_room, get_world()->get_sound_space());

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			SpatialSoundServer::get_singleton()->room_set_transform(sound_room, get_global_transform());
		} break;
		case NOTIFICATION_EXIT_WORLD: {

			if (sound_enabled)
				SpatialSoundServer::get_singleton()->room_set_space(sound_room, RID());

		} break;
	}
}

RES Room::_get_gizmo_geometry() const {

	DVector<Face3> faces;
	if (!room.is_null())
		faces = room->get_geometry_hint();

	int count = faces.size();
	if (count == 0)
		return RES();

	DVector<Face3>::Read facesr = faces.read();

	const Face3 *facesptr = facesr.ptr();

	DVector<Vector3> points;

	Ref<SurfaceTool> surface_tool(memnew(SurfaceTool));

	Ref<FixedMaterial> mat(memnew(FixedMaterial));

	mat->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(0.2, 0.8, 0.9, 0.3));
	mat->set_line_width(4);
	mat->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	mat->set_flag(Material::FLAG_UNSHADED, true);
	//	mat->set_hint(Material::HINT_NO_DEPTH_DRAW,true);

	surface_tool->begin(Mesh::PRIMITIVE_LINES);
	surface_tool->set_material(mat);

	for (int i = 0; i < count; i++) {

		surface_tool->add_vertex(facesptr[i].vertex[0]);
		surface_tool->add_vertex(facesptr[i].vertex[1]);

		surface_tool->add_vertex(facesptr[i].vertex[1]);
		surface_tool->add_vertex(facesptr[i].vertex[2]);

		surface_tool->add_vertex(facesptr[i].vertex[2]);
		surface_tool->add_vertex(facesptr[i].vertex[0]);
	}

	return surface_tool->commit();
}

AABB Room::get_aabb() const {

	if (room.is_null())
		return AABB();

	return room->get_bounds().get_aabb();
}
DVector<Face3> Room::get_faces(uint32_t p_usage_flags) const {

	return DVector<Face3>();
}

void Room::set_room(const Ref<RoomBounds> &p_room) {

	room = p_room;
	update_gizmo();

	if (room.is_valid()) {

		set_base(room->get_rid());
	} else {
		set_base(RID());
	}

	if (!is_inside_tree())
		return;

	propagate_notification(NOTIFICATION_AREA_CHANGED);
	update_gizmo();

	if (room.is_valid())
		SpatialSoundServer::get_singleton()->room_set_bounds(sound_room, room->get_bounds());
}

Ref<RoomBounds> Room::get_room() const {

	return room;
}

void Room::_parse_node_faces(DVector<Face3> &all_faces, const Node *p_node) const {

	const VisualInstance *vi = p_node->cast_to<VisualInstance>();

	if (vi) {
		DVector<Face3> faces = vi->get_faces(FACES_ENCLOSING);

		if (faces.size()) {
			int old_len = all_faces.size();
			all_faces.resize(all_faces.size() + faces.size());
			int new_len = all_faces.size();
			DVector<Face3>::Write all_facesw = all_faces.write();
			Face3 *all_facesptr = all_facesw.ptr();

			DVector<Face3>::Read facesr = faces.read();
			const Face3 *facesptr = facesr.ptr();

			Transform tr = vi->get_relative_transform(this);

			for (int i = old_len; i < new_len; i++) {

				Face3 f = facesptr[i - old_len];
				for (int j = 0; j < 3; j++)
					f.vertex[j] = tr.xform(f.vertex[j]);
				all_facesptr[i] = f;
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {

		_parse_node_faces(all_faces, p_node->get_child(i));
	}
}

void Room::compute_room_from_subtree() {

	DVector<Face3> all_faces;
	_parse_node_faces(all_faces, this);

	if (all_faces.size() == 0)
		return;
	float error;
	DVector<Face3> wrapped_faces = Geometry::wrap_geometry(all_faces, &error);

	if (wrapped_faces.size() == 0)
		return;

	BSP_Tree tree(wrapped_faces, error);

	Ref<RoomBounds> room(memnew(RoomBounds));
	room->set_bounds(tree);
	room->set_geometry_hint(wrapped_faces);

	set_room(room);
}

void Room::set_simulate_acoustics(bool p_enable) {

	if (sound_enabled == p_enable)
		return;

	sound_enabled = p_enable;
	if (!is_inside_world())
		return; //nothing to do

	if (sound_enabled)
		SpatialSoundServer::get_singleton()->room_set_space(sound_room, get_world()->get_sound_space());
	else
		SpatialSoundServer::get_singleton()->room_set_space(sound_room, RID());
}

void Room::_bounds_changed() {

	update_gizmo();
}

bool Room::is_simulating_acoustics() const {

	return sound_enabled;
}

RID Room::get_sound_room() const {

	return RID();
}

void Room::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_room", "room:Room"), &Room::set_room);
	ObjectTypeDB::bind_method(_MD("get_room:Room"), &Room::get_room);
	ObjectTypeDB::bind_method(_MD("compute_room_from_subtree"), &Room::compute_room_from_subtree);

	ObjectTypeDB::bind_method(_MD("set_simulate_acoustics", "enable"), &Room::set_simulate_acoustics);
	ObjectTypeDB::bind_method(_MD("is_simulating_acoustics"), &Room::is_simulating_acoustics);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "room/room", PROPERTY_HINT_RESOURCE_TYPE, "Area"), _SCS("set_room"), _SCS("get_room"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "room/simulate_acoustics"), _SCS("set_simulate_acoustics"), _SCS("is_simulating_acoustics"));
}

Room::Room() {

	sound_enabled = false;
	sound_room = SpatialSoundServer::get_singleton()->room_create();

	level = 0;
}

Room::~Room() {

	SpatialSoundServer::get_singleton()->free(sound_room);
}
