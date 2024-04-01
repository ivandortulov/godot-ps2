/*************************************************************************/
/*  area.cpp                                                             */
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
#include "area.h"
#include "scene/scene_string_names.h"
#include "servers/physics_server.h"
void Area::set_space_override_mode(SpaceOverride p_mode) {

	space_override = p_mode;
	PhysicsServer::get_singleton()->area_set_space_override_mode(get_rid(), PhysicsServer::AreaSpaceOverrideMode(p_mode));
}
Area::SpaceOverride Area::get_space_override_mode() const {

	return space_override;
}

void Area::set_gravity_is_point(bool p_enabled) {

	gravity_is_point = p_enabled;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_GRAVITY_IS_POINT, p_enabled);
}
bool Area::is_gravity_a_point() const {

	return gravity_is_point;
}

void Area::set_gravity_distance_scale(real_t p_scale) {

	gravity_distance_scale = p_scale;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_GRAVITY_DISTANCE_SCALE, p_scale);
}

real_t Area::get_gravity_distance_scale() const {
	return gravity_distance_scale;
}

void Area::set_gravity_vector(const Vector3 &p_vec) {

	gravity_vec = p_vec;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_GRAVITY_VECTOR, p_vec);
}
Vector3 Area::get_gravity_vector() const {

	return gravity_vec;
}

void Area::set_gravity(real_t p_gravity) {

	gravity = p_gravity;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_GRAVITY, p_gravity);
}
real_t Area::get_gravity() const {

	return gravity;
}
void Area::set_linear_damp(real_t p_linear_damp) {

	linear_damp = p_linear_damp;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_LINEAR_DAMP, p_linear_damp);
}
real_t Area::get_linear_damp() const {

	return linear_damp;
}

void Area::set_angular_damp(real_t p_angular_damp) {

	angular_damp = p_angular_damp;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_ANGULAR_DAMP, p_angular_damp);
}

real_t Area::get_angular_damp() const {

	return angular_damp;
}

void Area::set_priority(real_t p_priority) {

	priority = p_priority;
	PhysicsServer::get_singleton()->area_set_param(get_rid(), PhysicsServer::AREA_PARAM_PRIORITY, p_priority);
}
real_t Area::get_priority() const {

	return priority;
}

void Area::_body_enter_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);

	Map<ObjectID, BodyState>::Element *E = body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->get().in_tree);

	E->get().in_tree = true;
	emit_signal(SceneStringNames::get_singleton()->body_enter, node);
	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->body_enter_shape, p_id, node, E->get().shapes[i].body_shape, E->get().shapes[i].area_shape);
	}
}

void Area::_body_exit_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);
	Map<ObjectID, BodyState>::Element *E = body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->get().in_tree);
	E->get().in_tree = false;
	emit_signal(SceneStringNames::get_singleton()->body_exit, node);
	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->body_exit_shape, p_id, node, E->get().shapes[i].body_shape, E->get().shapes[i].area_shape);
	}
}

void Area::_body_inout(int p_status, const RID &p_body, int p_instance, int p_body_shape, int p_area_shape) {

	bool body_in = p_status == PhysicsServer::AREA_BODY_ADDED;
	ObjectID objid = p_instance;

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = obj ? obj->cast_to<Node>() : NULL;

	Map<ObjectID, BodyState>::Element *E = body_map.find(objid);

	ERR_FAIL_COND(!body_in && !E);

	locked = true;

	if (body_in) {
		if (!E) {

			E = body_map.insert(objid, BodyState());
			E->get().rc = 0;
			E->get().in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_body_enter_tree, make_binds(objid));
				node->connect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_body_exit_tree, make_binds(objid));
				if (E->get().in_tree) {
					emit_signal(SceneStringNames::get_singleton()->body_enter, node);
				}
			}
		}
		E->get().rc++;
		if (node)
			E->get().shapes.insert(ShapePair(p_body_shape, p_area_shape));

		if (E->get().in_tree) {
			emit_signal(SceneStringNames::get_singleton()->body_enter_shape, objid, node, p_body_shape, p_area_shape);
		}

	} else {

		E->get().rc--;

		if (node)
			E->get().shapes.erase(ShapePair(p_body_shape, p_area_shape));

		bool eraseit = false;

		if (E->get().rc == 0) {

			if (node) {
				node->disconnect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_body_enter_tree);
				node->disconnect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_body_exit_tree);
				if (E->get().in_tree)
					emit_signal(SceneStringNames::get_singleton()->body_exit, obj);
			}

			eraseit = true;
		}
		if (node && E->get().in_tree) {
			emit_signal(SceneStringNames::get_singleton()->body_exit_shape, objid, obj, p_body_shape, p_area_shape);
		}

		if (eraseit)
			body_map.erase(E);
	}

	locked = false;
}

void Area::_clear_monitoring() {

	if (locked) {
		ERR_EXPLAIN("This function can't be used during the in/out signal.");
	}
	ERR_FAIL_COND(locked);

	{
		Map<ObjectID, BodyState> bmcopy = body_map;
		body_map.clear();
		//disconnect all monitored stuff

		for (Map<ObjectID, BodyState>::Element *E = bmcopy.front(); E; E = E->next()) {

			Object *obj = ObjectDB::get_instance(E->key());
			Node *node = obj ? obj->cast_to<Node>() : NULL;

			if (!node) //node may have been deleted in previous frame or at other legiminate point
				continue;
			//ERR_CONTINUE(!node);

			node->disconnect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_body_enter_tree);
			node->disconnect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_body_exit_tree);

			if (!E->get().in_tree)
				continue;

			for (int i = 0; i < E->get().shapes.size(); i++) {

				emit_signal(SceneStringNames::get_singleton()->body_exit_shape, E->key(), node, E->get().shapes[i].body_shape, E->get().shapes[i].area_shape);
			}

			emit_signal(SceneStringNames::get_singleton()->body_exit, obj);
		}
	}

	{

		Map<ObjectID, AreaState> bmcopy = area_map;
		area_map.clear();
		//disconnect all monitored stuff

		for (Map<ObjectID, AreaState>::Element *E = bmcopy.front(); E; E = E->next()) {

			Object *obj = ObjectDB::get_instance(E->key());
			Node *node = obj ? obj->cast_to<Node>() : NULL;

			if (!node) //node may have been deleted in previous frame or at other legiminate point
				continue;
			//ERR_CONTINUE(!node);

			node->disconnect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_area_enter_tree);
			node->disconnect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_area_exit_tree);

			if (!E->get().in_tree)
				continue;

			for (int i = 0; i < E->get().shapes.size(); i++) {

				emit_signal(SceneStringNames::get_singleton()->area_exit_shape, E->key(), node, E->get().shapes[i].area_shape, E->get().shapes[i].self_shape);
			}

			emit_signal(SceneStringNames::get_singleton()->area_exit, obj);
		}
	}
}
void Area::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_EXIT_WORLD: {

			monitoring_stored = monitoring;
			set_enable_monitoring(false);
			_clear_monitoring();
		} break;
		case NOTIFICATION_ENTER_WORLD: {

			if (monitoring_stored) {
				set_enable_monitoring(true);
				monitoring_stored = false;
			}
		} break;
	}
}

void Area::set_enable_monitoring(bool p_enable) {

	if (locked) {
		ERR_EXPLAIN("This function can't be used during the in/out signal.");
	}
	ERR_FAIL_COND(locked);

	if (!is_inside_tree()) {
		monitoring_stored = p_enable;
		return;
	}

	if (p_enable == monitoring)
		return;

	monitoring = p_enable;

	if (monitoring) {

		PhysicsServer::get_singleton()->area_set_monitor_callback(get_rid(), this, SceneStringNames::get_singleton()->_body_inout);
		PhysicsServer::get_singleton()->area_set_area_monitor_callback(get_rid(), this, SceneStringNames::get_singleton()->_area_inout);
	} else {
		PhysicsServer::get_singleton()->area_set_monitor_callback(get_rid(), NULL, StringName());
		PhysicsServer::get_singleton()->area_set_area_monitor_callback(get_rid(), NULL, StringName());
		_clear_monitoring();
	}
}

void Area::_area_enter_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);

	Map<ObjectID, AreaState>::Element *E = area_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->get().in_tree);

	E->get().in_tree = true;
	emit_signal(SceneStringNames::get_singleton()->area_enter, node);
	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->area_enter_shape, p_id, node, E->get().shapes[i].area_shape, E->get().shapes[i].self_shape);
	}
}

void Area::_area_exit_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);
	Map<ObjectID, AreaState>::Element *E = area_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->get().in_tree);
	E->get().in_tree = false;
	emit_signal(SceneStringNames::get_singleton()->area_exit, node);
	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->area_exit_shape, p_id, node, E->get().shapes[i].area_shape, E->get().shapes[i].self_shape);
	}
}

void Area::_area_inout(int p_status, const RID &p_area, int p_instance, int p_area_shape, int p_self_shape) {

	bool area_in = p_status == PhysicsServer::AREA_BODY_ADDED;
	ObjectID objid = p_instance;

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = obj ? obj->cast_to<Node>() : NULL;

	Map<ObjectID, AreaState>::Element *E = area_map.find(objid);

	ERR_FAIL_COND(!area_in && !E);

	locked = true;

	if (area_in) {
		if (!E) {

			E = area_map.insert(objid, AreaState());
			E->get().rc = 0;
			E->get().in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_area_enter_tree, make_binds(objid));
				node->connect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_area_exit_tree, make_binds(objid));
				if (E->get().in_tree) {
					emit_signal(SceneStringNames::get_singleton()->area_enter, node);
				}
			}
		}
		E->get().rc++;
		if (node)
			E->get().shapes.insert(AreaShapePair(p_area_shape, p_self_shape));

		if (!node || E->get().in_tree) {
			emit_signal(SceneStringNames::get_singleton()->area_enter_shape, objid, node, p_area_shape, p_self_shape);
		}

	} else {

		E->get().rc--;

		if (node)
			E->get().shapes.erase(AreaShapePair(p_area_shape, p_self_shape));

		bool eraseit = false;

		if (E->get().rc == 0) {

			if (node) {
				node->disconnect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_area_enter_tree);
				node->disconnect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_area_exit_tree);
				if (E->get().in_tree) {
					emit_signal(SceneStringNames::get_singleton()->area_exit, obj);
				}
			}

			eraseit = true;
		}
		if (!node || E->get().in_tree) {
			emit_signal(SceneStringNames::get_singleton()->area_exit_shape, objid, obj, p_area_shape, p_self_shape);
		}

		if (eraseit)
			area_map.erase(E);
	}

	locked = false;
}

bool Area::is_monitoring_enabled() const {

	return monitoring || monitoring_stored;
}

Array Area::get_overlapping_bodies() const {

	ERR_FAIL_COND_V(!monitoring, Array());
	Array ret;
	ret.resize(body_map.size());
	int idx = 0;
	for (const Map<ObjectID, BodyState>::Element *E = body_map.front(); E; E = E->next()) {
		Object *obj = ObjectDB::get_instance(E->key());
		if (!obj) {
			ret.resize(ret.size() - 1); //ops
		} else {
			ret[idx++] = obj;
		}
	}

	return ret;
}

void Area::set_monitorable(bool p_enable) {

	if (locked) {
		ERR_EXPLAIN("This function can't be used during the in/out signal.");
	}
	ERR_FAIL_COND(locked);

	if (p_enable == monitorable)
		return;

	monitorable = p_enable;

	PhysicsServer::get_singleton()->area_set_monitorable(get_rid(), monitorable);
}

bool Area::is_monitorable() const {

	return monitorable;
}

Array Area::get_overlapping_areas() const {

	ERR_FAIL_COND_V(!monitoring, Array());
	Array ret;
	ret.resize(area_map.size());
	int idx = 0;
	for (const Map<ObjectID, AreaState>::Element *E = area_map.front(); E; E = E->next()) {
		Object *obj = ObjectDB::get_instance(E->key());
		if (!obj) {
			ret.resize(ret.size() - 1); //ops
		} else {
			ret[idx++] = obj;
		}
	}

	return ret;
}

bool Area::overlaps_area(Node *p_area) const {

	ERR_FAIL_NULL_V(p_area, false);
	const Map<ObjectID, AreaState>::Element *E = area_map.find(p_area->get_instance_ID());
	if (!E)
		return false;
	return E->get().in_tree;
}

bool Area::overlaps_body(Node *p_body) const {

	ERR_FAIL_NULL_V(p_body, false);
	const Map<ObjectID, BodyState>::Element *E = body_map.find(p_body->get_instance_ID());
	if (!E)
		return false;
	return E->get().in_tree;
}
void Area::set_collision_mask(uint32_t p_mask) {

	collision_mask = p_mask;
	PhysicsServer::get_singleton()->area_set_collision_mask(get_rid(), p_mask);
}

uint32_t Area::get_collision_mask() const {

	return collision_mask;
}
void Area::set_layer_mask(uint32_t p_mask) {

	layer_mask = p_mask;
	PhysicsServer::get_singleton()->area_set_layer_mask(get_rid(), p_mask);
}

uint32_t Area::get_layer_mask() const {

	return layer_mask;
}

void Area::set_collision_mask_bit(int p_bit, bool p_value) {

	uint32_t mask = get_collision_mask();
	if (p_value)
		mask |= 1 << p_bit;
	else
		mask &= ~(1 << p_bit);
	set_collision_mask(mask);
}

bool Area::get_collision_mask_bit(int p_bit) const {

	return get_collision_mask() & (1 << p_bit);
}

void Area::set_layer_mask_bit(int p_bit, bool p_value) {

	uint32_t mask = get_layer_mask();
	if (p_value)
		mask |= 1 << p_bit;
	else
		mask &= ~(1 << p_bit);
	set_layer_mask(mask);
}

bool Area::get_layer_mask_bit(int p_bit) const {

	return get_layer_mask() & (1 << p_bit);
}

void Area::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_body_enter_tree", "id"), &Area::_body_enter_tree);
	ObjectTypeDB::bind_method(_MD("_body_exit_tree", "id"), &Area::_body_exit_tree);

	ObjectTypeDB::bind_method(_MD("_area_enter_tree", "id"), &Area::_area_enter_tree);
	ObjectTypeDB::bind_method(_MD("_area_exit_tree", "id"), &Area::_area_exit_tree);

	ObjectTypeDB::bind_method(_MD("set_space_override_mode", "enable"), &Area::set_space_override_mode);
	ObjectTypeDB::bind_method(_MD("get_space_override_mode"), &Area::get_space_override_mode);

	ObjectTypeDB::bind_method(_MD("set_gravity_is_point", "enable"), &Area::set_gravity_is_point);
	ObjectTypeDB::bind_method(_MD("is_gravity_a_point"), &Area::is_gravity_a_point);

	ObjectTypeDB::bind_method(_MD("set_gravity_distance_scale", "distance_scale"), &Area::set_gravity_distance_scale);
	ObjectTypeDB::bind_method(_MD("get_gravity_distance_scale"), &Area::get_gravity_distance_scale);

	ObjectTypeDB::bind_method(_MD("set_gravity_vector", "vector"), &Area::set_gravity_vector);
	ObjectTypeDB::bind_method(_MD("get_gravity_vector"), &Area::get_gravity_vector);

	ObjectTypeDB::bind_method(_MD("set_gravity", "gravity"), &Area::set_gravity);
	ObjectTypeDB::bind_method(_MD("get_gravity"), &Area::get_gravity);

	ObjectTypeDB::bind_method(_MD("set_angular_damp", "angular_damp"), &Area::set_angular_damp);
	ObjectTypeDB::bind_method(_MD("get_angular_damp"), &Area::get_angular_damp);

	ObjectTypeDB::bind_method(_MD("set_linear_damp", "linear_damp"), &Area::set_linear_damp);
	ObjectTypeDB::bind_method(_MD("get_linear_damp"), &Area::get_linear_damp);

	ObjectTypeDB::bind_method(_MD("set_priority", "priority"), &Area::set_priority);
	ObjectTypeDB::bind_method(_MD("get_priority"), &Area::get_priority);

	ObjectTypeDB::bind_method(_MD("set_collision_mask", "collision_mask"), &Area::set_collision_mask);
	ObjectTypeDB::bind_method(_MD("get_collision_mask"), &Area::get_collision_mask);

	ObjectTypeDB::bind_method(_MD("set_layer_mask", "layer_mask"), &Area::set_layer_mask);
	ObjectTypeDB::bind_method(_MD("get_layer_mask"), &Area::get_layer_mask);

	ObjectTypeDB::bind_method(_MD("set_collision_mask_bit", "bit", "value"), &Area::set_collision_mask_bit);
	ObjectTypeDB::bind_method(_MD("get_collision_mask_bit", "bit"), &Area::get_collision_mask_bit);

	ObjectTypeDB::bind_method(_MD("set_layer_mask_bit", "bit", "value"), &Area::set_layer_mask_bit);
	ObjectTypeDB::bind_method(_MD("get_layer_mask_bit", "bit"), &Area::get_layer_mask_bit);

	ObjectTypeDB::bind_method(_MD("set_monitorable", "enable"), &Area::set_monitorable);
	ObjectTypeDB::bind_method(_MD("is_monitorable"), &Area::is_monitorable);

	ObjectTypeDB::bind_method(_MD("set_enable_monitoring", "enable"), &Area::set_enable_monitoring);
	ObjectTypeDB::bind_method(_MD("is_monitoring_enabled"), &Area::is_monitoring_enabled);

	ObjectTypeDB::bind_method(_MD("get_overlapping_bodies"), &Area::get_overlapping_bodies);
	ObjectTypeDB::bind_method(_MD("get_overlapping_areas"), &Area::get_overlapping_areas);

	ObjectTypeDB::bind_method(_MD("overlaps_body", "body"), &Area::overlaps_body);
	ObjectTypeDB::bind_method(_MD("overlaps_area", "area"), &Area::overlaps_area);

	ObjectTypeDB::bind_method(_MD("_body_inout"), &Area::_body_inout);
	ObjectTypeDB::bind_method(_MD("_area_inout"), &Area::_area_inout);

	ADD_SIGNAL(MethodInfo("body_enter_shape", PropertyInfo(Variant::INT, "body_id"), PropertyInfo(Variant::OBJECT, "body"), PropertyInfo(Variant::INT, "body_shape"), PropertyInfo(Variant::INT, "area_shape")));
	ADD_SIGNAL(MethodInfo("body_exit_shape", PropertyInfo(Variant::INT, "body_id"), PropertyInfo(Variant::OBJECT, "body"), PropertyInfo(Variant::INT, "body_shape"), PropertyInfo(Variant::INT, "area_shape")));
	ADD_SIGNAL(MethodInfo("body_enter", PropertyInfo(Variant::OBJECT, "body")));
	ADD_SIGNAL(MethodInfo("body_exit", PropertyInfo(Variant::OBJECT, "body")));

	ADD_SIGNAL(MethodInfo("area_enter_shape", PropertyInfo(Variant::INT, "area_id"), PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area"), PropertyInfo(Variant::INT, "area_shape"), PropertyInfo(Variant::INT, "self_shape")));
	ADD_SIGNAL(MethodInfo("area_exit_shape", PropertyInfo(Variant::INT, "area_id"), PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area"), PropertyInfo(Variant::INT, "area_shape"), PropertyInfo(Variant::INT, "self_shape")));
	ADD_SIGNAL(MethodInfo("area_enter", PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area")));
	ADD_SIGNAL(MethodInfo("area_exit", PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "space_override", PROPERTY_HINT_ENUM, "Disabled,Combine,Combine-Replace,Replace,Replace-Combine"), _SCS("set_space_override_mode"), _SCS("get_space_override_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gravity_point"), _SCS("set_gravity_is_point"), _SCS("is_gravity_a_point"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "gravity_distance_scale", PROPERTY_HINT_RANGE, "0,1024,0.001"), _SCS("set_gravity_distance_scale"), _SCS("get_gravity_distance_scale"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity_vec"), _SCS("set_gravity_vector"), _SCS("get_gravity_vector"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "gravity", PROPERTY_HINT_RANGE, "-1024,1024,0.01"), _SCS("set_gravity"), _SCS("get_gravity"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "linear_damp", PROPERTY_HINT_RANGE, "0,1024,0.001"), _SCS("set_linear_damp"), _SCS("get_linear_damp"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_damp", PROPERTY_HINT_RANGE, "0,1024,0.001"), _SCS("set_angular_damp"), _SCS("get_angular_damp"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority", PROPERTY_HINT_RANGE, "0,128,1"), _SCS("set_priority"), _SCS("get_priority"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "monitoring"), _SCS("set_enable_monitoring"), _SCS("is_monitoring_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "monitorable"), _SCS("set_monitorable"), _SCS("is_monitorable"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/layers", PROPERTY_HINT_ALL_FLAGS), _SCS("set_layer_mask"), _SCS("get_layer_mask"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/mask", PROPERTY_HINT_ALL_FLAGS), _SCS("set_collision_mask"), _SCS("get_collision_mask"));
}

Area::Area() :
		CollisionObject(PhysicsServer::get_singleton()->area_create(), true) {

	space_override = SPACE_OVERRIDE_DISABLED;
	set_gravity(9.8);
	locked = false;
	set_gravity_vector(Vector3(0, -1, 0));
	gravity_is_point = false;
	gravity_distance_scale = 0;
	linear_damp = 0.1;
	angular_damp = 1;
	priority = 0;
	monitoring = false;
	monitorable = false;
	collision_mask = 1;
	layer_mask = 1;
	monitoring_stored = false;
	set_ray_pickable(false);
	set_enable_monitoring(true);
	set_monitorable(true);
}

Area::~Area() {
}
