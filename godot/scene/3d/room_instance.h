/*************************************************************************/
/*  room_instance.h                                                      */
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
#ifndef ROOM_INSTANCE_H
#define ROOM_INSTANCE_H

#include "scene/3d/visual_instance.h"
#include "scene/resources/room.h"
#include "servers/spatial_sound_server.h"
/**
	@author Juan Linietsky <reduzio@gmail.com>
*/

/* RoomInstance Logic:
   a) Instances that belong to the room are drawn only if the room is visible (seen through portal, or player inside)
   b) Instances that don't belong to any room are considered to belong to the root room (RID empty)
   c) "dynamic" Instances are assigned to the rooms their AABB touch

*/

class Room : public VisualInstance {

	OBJ_TYPE(Room, VisualInstance);

public:
private:
	Ref<RoomBounds> room;

	RID sound_room;

	bool sound_enabled;

	int level;
	void _parse_node_faces(DVector<Face3> &all_faces, const Node *p_node) const;

	void _bounds_changed();
	virtual RES _get_gizmo_geometry() const;

protected:
	void _notification(int p_what);

	static void _bind_methods();

public:
	enum {
		// used to notify portals that the room in which they are has changed.
		NOTIFICATION_AREA_CHANGED = 60
	};

	virtual AABB get_aabb() const;
	virtual DVector<Face3> get_faces(uint32_t p_usage_flags) const;

	void set_room(const Ref<RoomBounds> &p_room);
	Ref<RoomBounds> get_room() const;

	void set_simulate_acoustics(bool p_enable);
	bool is_simulating_acoustics() const;

	void compute_room_from_subtree();

	RID get_sound_room() const;

	Room();
	~Room();
};

#endif // ROOM_INSTANCE_H
