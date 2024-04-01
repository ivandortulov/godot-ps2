/*************************************************************************/
/*  physics_body.cpp                                                     */
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
#include "physics_body.h"
#include "scene/scene_string_names.h"

void PhysicsBody::_notification(int p_what) {

	/*
	switch(p_what) {

		case NOTIFICATION_TRANSFORM_CHANGED: {

			PhysicsServer::get_singleton()->body_set_state(get_rid(),PhysicsServer::BODY_STATE_TRANSFORM,get_global_transform());

		} break;
	}
	*/
}

Vector3 PhysicsBody::get_linear_velocity() const {

	return Vector3();
}
Vector3 PhysicsBody::get_angular_velocity() const {

	return Vector3();
}

float PhysicsBody::get_inverse_mass() const {

	return 0;
}

void PhysicsBody::set_layer_mask(uint32_t p_mask) {

	layer_mask = p_mask;
	PhysicsServer::get_singleton()->body_set_layer_mask(get_rid(), p_mask);
}

uint32_t PhysicsBody::get_layer_mask() const {

	return layer_mask;
}

void PhysicsBody::set_collision_mask(uint32_t p_mask) {

	collision_mask = p_mask;
	PhysicsServer::get_singleton()->body_set_collision_mask(get_rid(), p_mask);
}

uint32_t PhysicsBody::get_collision_mask() const {

	return collision_mask;
}

void PhysicsBody::set_collision_mask_bit(int p_bit, bool p_value) {

	uint32_t mask = get_collision_mask();
	if (p_value)
		mask |= 1 << p_bit;
	else
		mask &= ~(1 << p_bit);
	set_collision_mask(mask);
}

bool PhysicsBody::get_collision_mask_bit(int p_bit) const {

	return get_collision_mask() & (1 << p_bit);
}

void PhysicsBody::set_layer_mask_bit(int p_bit, bool p_value) {

	uint32_t mask = get_layer_mask();
	if (p_value)
		mask |= 1 << p_bit;
	else
		mask &= ~(1 << p_bit);
	set_layer_mask(mask);
}

bool PhysicsBody::get_layer_mask_bit(int p_bit) const {

	return get_layer_mask() & (1 << p_bit);
}

void PhysicsBody::add_collision_exception_with(Node *p_node) {

	ERR_FAIL_NULL(p_node);
	PhysicsBody *physics_body = p_node->cast_to<PhysicsBody>();
	if (!physics_body) {
		ERR_EXPLAIN("Collision exception only works between two objects of PhysicsBody type");
	}
	ERR_FAIL_COND(!physics_body);
	PhysicsServer::get_singleton()->body_add_collision_exception(get_rid(), physics_body->get_rid());
}

void PhysicsBody::remove_collision_exception_with(Node *p_node) {

	ERR_FAIL_NULL(p_node);
	PhysicsBody *physics_body = p_node->cast_to<PhysicsBody>();
	if (!physics_body) {
		ERR_EXPLAIN("Collision exception only works between two objects of PhysicsBody type");
	}
	ERR_FAIL_COND(!physics_body);
	PhysicsServer::get_singleton()->body_remove_collision_exception(get_rid(), physics_body->get_rid());
}

void PhysicsBody::_set_layers(uint32_t p_mask) {
	set_layer_mask(p_mask);
	set_collision_mask(p_mask);
}

uint32_t PhysicsBody::_get_layers() const {

	return get_layer_mask();
}

void PhysicsBody::_bind_methods() {
	ObjectTypeDB::bind_method(_MD("set_layer_mask", "mask"), &PhysicsBody::set_layer_mask);
	ObjectTypeDB::bind_method(_MD("get_layer_mask"), &PhysicsBody::get_layer_mask);

	ObjectTypeDB::bind_method(_MD("set_collision_mask", "mask"), &PhysicsBody::set_collision_mask);
	ObjectTypeDB::bind_method(_MD("get_collision_mask"), &PhysicsBody::get_collision_mask);

	ObjectTypeDB::bind_method(_MD("set_collision_mask_bit", "bit", "value"), &PhysicsBody::set_collision_mask_bit);
	ObjectTypeDB::bind_method(_MD("get_collision_mask_bit", "bit"), &PhysicsBody::get_collision_mask_bit);

	ObjectTypeDB::bind_method(_MD("set_layer_mask_bit", "bit", "value"), &PhysicsBody::set_layer_mask_bit);
	ObjectTypeDB::bind_method(_MD("get_layer_mask_bit", "bit"), &PhysicsBody::get_layer_mask_bit);

	ObjectTypeDB::bind_method(_MD("_set_layers", "mask"), &PhysicsBody::_set_layers);
	ObjectTypeDB::bind_method(_MD("_get_layers"), &PhysicsBody::_get_layers);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "layers", PROPERTY_HINT_ALL_FLAGS, "", 0), _SCS("_set_layers"), _SCS("_get_layers")); //for backwards compat
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/layers", PROPERTY_HINT_ALL_FLAGS), _SCS("set_layer_mask"), _SCS("get_layer_mask"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision/mask", PROPERTY_HINT_ALL_FLAGS), _SCS("set_collision_mask"), _SCS("get_collision_mask"));
}

PhysicsBody::PhysicsBody(PhysicsServer::BodyMode p_mode) :
		CollisionObject(PhysicsServer::get_singleton()->body_create(p_mode), false) {

	layer_mask = 1;
	collision_mask = 1;
}

void StaticBody::set_friction(real_t p_friction) {

	ERR_FAIL_COND(p_friction < 0 || p_friction > 1);

	friction = p_friction;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_FRICTION, friction);
}
real_t StaticBody::get_friction() const {

	return friction;
}

void StaticBody::set_bounce(real_t p_bounce) {

	ERR_FAIL_COND(p_bounce < 0 || p_bounce > 1);

	bounce = p_bounce;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_BOUNCE, bounce);
}
real_t StaticBody::get_bounce() const {

	return bounce;
}

void StaticBody::set_constant_linear_velocity(const Vector3 &p_vel) {

	constant_linear_velocity = p_vel;
	PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_LINEAR_VELOCITY, constant_linear_velocity);
}

void StaticBody::set_constant_angular_velocity(const Vector3 &p_vel) {

	constant_angular_velocity = p_vel;
	PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_ANGULAR_VELOCITY, constant_angular_velocity);
}

Vector3 StaticBody::get_constant_linear_velocity() const {

	return constant_linear_velocity;
}
Vector3 StaticBody::get_constant_angular_velocity() const {

	return constant_angular_velocity;
}

void StaticBody::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_constant_linear_velocity", "vel"), &StaticBody::set_constant_linear_velocity);
	ObjectTypeDB::bind_method(_MD("set_constant_angular_velocity", "vel"), &StaticBody::set_constant_angular_velocity);
	ObjectTypeDB::bind_method(_MD("get_constant_linear_velocity"), &StaticBody::get_constant_linear_velocity);
	ObjectTypeDB::bind_method(_MD("get_constant_angular_velocity"), &StaticBody::get_constant_angular_velocity);

	ObjectTypeDB::bind_method(_MD("set_friction", "friction"), &StaticBody::set_friction);
	ObjectTypeDB::bind_method(_MD("get_friction"), &StaticBody::get_friction);

	ObjectTypeDB::bind_method(_MD("set_bounce", "bounce"), &StaticBody::set_bounce);
	ObjectTypeDB::bind_method(_MD("get_bounce"), &StaticBody::get_bounce);

	ObjectTypeDB::bind_method(_MD("add_collision_exception_with", "body:PhysicsBody"), &PhysicsBody::add_collision_exception_with);
	ObjectTypeDB::bind_method(_MD("remove_collision_exception_with", "body:PhysicsBody"), &PhysicsBody::remove_collision_exception_with);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "friction", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_friction"), _SCS("get_friction"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_bounce"), _SCS("get_bounce"));

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "constant_linear_velocity"), _SCS("set_constant_linear_velocity"), _SCS("get_constant_linear_velocity"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "constant_angular_velocity"), _SCS("set_constant_angular_velocity"), _SCS("get_constant_angular_velocity"));
}

StaticBody::StaticBody() :
		PhysicsBody(PhysicsServer::BODY_MODE_STATIC) {

	bounce = 0;
	friction = 1;
}

StaticBody::~StaticBody() {
}

void RigidBody::_body_enter_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);

	Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->get().in_tree);

	E->get().in_tree = true;

	contact_monitor->locked = true;

	emit_signal(SceneStringNames::get_singleton()->body_enter, node);

	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->body_enter_shape, p_id, node, E->get().shapes[i].body_shape, E->get().shapes[i].local_shape);
	}

	contact_monitor->locked = false;
}

void RigidBody::_body_exit_tree(ObjectID p_id) {

	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = obj ? obj->cast_to<Node>() : NULL;
	ERR_FAIL_COND(!node);
	Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->get().in_tree);
	E->get().in_tree = false;

	contact_monitor->locked = true;

	emit_signal(SceneStringNames::get_singleton()->body_exit, node);

	for (int i = 0; i < E->get().shapes.size(); i++) {

		emit_signal(SceneStringNames::get_singleton()->body_exit_shape, p_id, node, E->get().shapes[i].body_shape, E->get().shapes[i].local_shape);
	}

	contact_monitor->locked = false;
}

void RigidBody::_body_inout(int p_status, ObjectID p_instance, int p_body_shape, int p_local_shape) {

	bool body_in = p_status == 1;
	ObjectID objid = p_instance;

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = obj ? obj->cast_to<Node>() : NULL;

	Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.find(objid);

	ERR_FAIL_COND(!body_in && !E);

	if (body_in) {
		if (!E) {

			E = contact_monitor->body_map.insert(objid, BodyState());
			//E->get().rc=0;
			E->get().in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_body_enter_tree, make_binds(objid));
				node->connect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_body_exit_tree, make_binds(objid));
				if (E->get().in_tree) {
					emit_signal(SceneStringNames::get_singleton()->body_enter, node);
				}
			}
		}
		//E->get().rc++;
		if (node)
			E->get().shapes.insert(ShapePair(p_body_shape, p_local_shape));

		if (E->get().in_tree) {
			emit_signal(SceneStringNames::get_singleton()->body_enter_shape, objid, node, p_body_shape, p_local_shape);
		}

	} else {

		//E->get().rc--;

		if (node)
			E->get().shapes.erase(ShapePair(p_body_shape, p_local_shape));

		bool in_tree = E->get().in_tree;

		if (E->get().shapes.empty()) {

			if (node) {
				node->disconnect(SceneStringNames::get_singleton()->enter_tree, this, SceneStringNames::get_singleton()->_body_enter_tree);
				node->disconnect(SceneStringNames::get_singleton()->exit_tree, this, SceneStringNames::get_singleton()->_body_exit_tree);
				if (in_tree)
					emit_signal(SceneStringNames::get_singleton()->body_exit, obj);
			}

			contact_monitor->body_map.erase(E);
		}
		if (node && in_tree) {
			emit_signal(SceneStringNames::get_singleton()->body_exit_shape, objid, obj, p_body_shape, p_local_shape);
		}
	}
}

struct _RigidBodyInOut {

	ObjectID id;
	int shape;
	int local_shape;
};

void RigidBody::_direct_state_changed(Object *p_state) {

//eh.. fuck
#ifdef DEBUG_ENABLED

	state = p_state->cast_to<PhysicsDirectBodyState>();
#else
	state = (PhysicsDirectBodyState *)p_state; //trust it
#endif

	set_ignore_transform_notification(true);
	set_global_transform(state->get_transform());
	linear_velocity = state->get_linear_velocity();
	angular_velocity = state->get_angular_velocity();
	if (sleeping != state->is_sleeping()) {
		sleeping = state->is_sleeping();
		emit_signal(SceneStringNames::get_singleton()->sleeping_state_changed);
	}
	if (get_script_instance())
		get_script_instance()->call("_integrate_forces", state);
	set_ignore_transform_notification(false);

	if (contact_monitor) {

		contact_monitor->locked = true;

		//untag all
		int rc = 0;
		for (Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.front(); E; E = E->next()) {

			for (int i = 0; i < E->get().shapes.size(); i++) {

				E->get().shapes[i].tagged = false;
				rc++;
			}
		}

		_RigidBodyInOut *toadd = (_RigidBodyInOut *)alloca(state->get_contact_count() * sizeof(_RigidBodyInOut));
		int toadd_count = 0; //state->get_contact_count();
		RigidBody_RemoveAction *toremove = (RigidBody_RemoveAction *)alloca(rc * sizeof(RigidBody_RemoveAction));
		int toremove_count = 0;

		//put the ones to add

		for (int i = 0; i < state->get_contact_count(); i++) {

			ObjectID obj = state->get_contact_collider_id(i);
			int local_shape = state->get_contact_local_shape(i);
			int shape = state->get_contact_collider_shape(i);

			//			bool found=false;

			Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.find(obj);
			if (!E) {
				toadd[toadd_count].local_shape = local_shape;
				toadd[toadd_count].id = obj;
				toadd[toadd_count].shape = shape;
				toadd_count++;
				continue;
			}

			ShapePair sp(shape, local_shape);
			int idx = E->get().shapes.find(sp);
			if (idx == -1) {

				toadd[toadd_count].local_shape = local_shape;
				toadd[toadd_count].id = obj;
				toadd[toadd_count].shape = shape;
				toadd_count++;
				continue;
			}

			E->get().shapes[idx].tagged = true;
		}

		//put the ones to remove

		for (Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.front(); E; E = E->next()) {

			for (int i = 0; i < E->get().shapes.size(); i++) {

				if (!E->get().shapes[i].tagged) {

					toremove[toremove_count].body_id = E->key();
					toremove[toremove_count].pair = E->get().shapes[i];
					toremove_count++;
				}
			}
		}

		//process remotions

		for (int i = 0; i < toremove_count; i++) {

			_body_inout(0, toremove[i].body_id, toremove[i].pair.body_shape, toremove[i].pair.local_shape);
		}

		//process aditions

		for (int i = 0; i < toadd_count; i++) {

			_body_inout(1, toadd[i].id, toadd[i].shape, toadd[i].local_shape);
		}

		contact_monitor->locked = false;
	}

	state = NULL;
}

void RigidBody::_notification(int p_what) {

#ifdef TOOLS_ENABLED
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (get_tree()->is_editor_hint()) {
			set_notify_local_transform(true); //used for warnings and only in editor
		}
	}

	if (p_what == NOTIFICATION_LOCAL_TRANSFORM_CHANGED) {
		if (get_tree()->is_editor_hint()) {
			update_configuration_warning();
		}
	}

#endif
}

void RigidBody::set_mode(Mode p_mode) {

	mode = p_mode;
	switch (p_mode) {

		case MODE_RIGID: {

			PhysicsServer::get_singleton()->body_set_mode(get_rid(), PhysicsServer::BODY_MODE_RIGID);
		} break;
		case MODE_STATIC: {

			PhysicsServer::get_singleton()->body_set_mode(get_rid(), PhysicsServer::BODY_MODE_STATIC);

		} break;
		case MODE_CHARACTER: {
			PhysicsServer::get_singleton()->body_set_mode(get_rid(), PhysicsServer::BODY_MODE_CHARACTER);

		} break;
		case MODE_KINEMATIC: {

			PhysicsServer::get_singleton()->body_set_mode(get_rid(), PhysicsServer::BODY_MODE_KINEMATIC);
		} break;
	}
}

RigidBody::Mode RigidBody::get_mode() const {

	return mode;
}

void RigidBody::set_mass(real_t p_mass) {

	ERR_FAIL_COND(p_mass <= 0);
	mass = p_mass;
	_change_notify("mass");
	_change_notify("weight");
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_MASS, mass);
}
real_t RigidBody::get_mass() const {

	return mass;
}

void RigidBody::set_weight(real_t p_weight) {

	set_mass(p_weight / 9.8);
}
real_t RigidBody::get_weight() const {

	return mass * 9.8;
}

void RigidBody::set_friction(real_t p_friction) {

	ERR_FAIL_COND(p_friction < 0 || p_friction > 1);

	friction = p_friction;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_FRICTION, friction);
}
real_t RigidBody::get_friction() const {

	return friction;
}

void RigidBody::set_bounce(real_t p_bounce) {

	ERR_FAIL_COND(p_bounce < 0 || p_bounce > 1);

	bounce = p_bounce;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_BOUNCE, bounce);
}
real_t RigidBody::get_bounce() const {

	return bounce;
}

void RigidBody::set_gravity_scale(real_t p_gravity_scale) {

	gravity_scale = p_gravity_scale;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_GRAVITY_SCALE, gravity_scale);
}
real_t RigidBody::get_gravity_scale() const {

	return gravity_scale;
}

void RigidBody::set_linear_damp(real_t p_linear_damp) {

	ERR_FAIL_COND(p_linear_damp < -1);
	linear_damp = p_linear_damp;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_LINEAR_DAMP, linear_damp);
}
real_t RigidBody::get_linear_damp() const {

	return linear_damp;
}

void RigidBody::set_angular_damp(real_t p_angular_damp) {

	ERR_FAIL_COND(p_angular_damp < -1);
	angular_damp = p_angular_damp;
	PhysicsServer::get_singleton()->body_set_param(get_rid(), PhysicsServer::BODY_PARAM_ANGULAR_DAMP, angular_damp);
}
real_t RigidBody::get_angular_damp() const {

	return angular_damp;
}

void RigidBody::set_axis_velocity(const Vector3 &p_axis) {

	Vector3 v = state ? state->get_linear_velocity() : linear_velocity;
	Vector3 axis = p_axis.normalized();
	v -= axis * axis.dot(v);
	v += p_axis;
	if (state) {
		set_linear_velocity(v);
	} else {
		PhysicsServer::get_singleton()->body_set_axis_velocity(get_rid(), p_axis);
		linear_velocity = v;
	}
}

void RigidBody::set_linear_velocity(const Vector3 &p_velocity) {

	linear_velocity = p_velocity;
	if (state)
		state->set_linear_velocity(linear_velocity);
	else
		PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_LINEAR_VELOCITY, linear_velocity);
}

Vector3 RigidBody::get_linear_velocity() const {

	return linear_velocity;
}

void RigidBody::set_angular_velocity(const Vector3 &p_velocity) {

	angular_velocity = p_velocity;
	if (state)
		state->set_angular_velocity(angular_velocity);
	else
		PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_ANGULAR_VELOCITY, angular_velocity);
}
Vector3 RigidBody::get_angular_velocity() const {

	return angular_velocity;
}

void RigidBody::set_use_custom_integrator(bool p_enable) {

	if (custom_integrator == p_enable)
		return;

	custom_integrator = p_enable;
	PhysicsServer::get_singleton()->body_set_omit_force_integration(get_rid(), p_enable);
}
bool RigidBody::is_using_custom_integrator() {

	return custom_integrator;
}

void RigidBody::set_sleeping(bool p_sleeping) {

	sleeping = p_sleeping;
	PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_SLEEPING, sleeping);
}

void RigidBody::set_can_sleep(bool p_active) {

	can_sleep = p_active;
	PhysicsServer::get_singleton()->body_set_state(get_rid(), PhysicsServer::BODY_STATE_CAN_SLEEP, p_active);
}

bool RigidBody::is_able_to_sleep() const {

	return can_sleep;
}

bool RigidBody::is_sleeping() const {

	return sleeping;
}

void RigidBody::set_max_contacts_reported(int p_amount) {

	max_contacts_reported = p_amount;
	PhysicsServer::get_singleton()->body_set_max_contacts_reported(get_rid(), p_amount);
}

int RigidBody::get_max_contacts_reported() const {

	return max_contacts_reported;
}

void RigidBody::apply_impulse(const Vector3 &p_pos, const Vector3 &p_impulse) {

	PhysicsServer::get_singleton()->body_apply_impulse(get_rid(), p_pos, p_impulse);
}

void RigidBody::set_use_continuous_collision_detection(bool p_enable) {

	ccd = p_enable;
	PhysicsServer::get_singleton()->body_set_enable_continuous_collision_detection(get_rid(), p_enable);
}

bool RigidBody::is_using_continuous_collision_detection() const {

	return ccd;
}

void RigidBody::set_contact_monitor(bool p_enabled) {

	if (p_enabled == is_contact_monitor_enabled())
		return;

	if (!p_enabled) {

		if (contact_monitor->locked) {
			ERR_EXPLAIN("Can't disable contact monitoring during in/out callback. Use call_deferred(\"set_contact_monitor\",false) instead");
		}
		ERR_FAIL_COND(contact_monitor->locked);

		for (Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.front(); E; E = E->next()) {

			//clean up mess
		}

		memdelete(contact_monitor);
		contact_monitor = NULL;
	} else {

		contact_monitor = memnew(ContactMonitor);
		contact_monitor->locked = false;
	}
}

bool RigidBody::is_contact_monitor_enabled() const {

	return contact_monitor != NULL;
}

void RigidBody::set_axis_lock(AxisLock p_lock) {

	axis_lock = p_lock;
	PhysicsServer::get_singleton()->body_set_axis_lock(get_rid(), PhysicsServer::BodyAxisLock(axis_lock));
}

RigidBody::AxisLock RigidBody::get_axis_lock() const {

	return axis_lock;
}

Array RigidBody::get_colliding_bodies() const {

	ERR_FAIL_COND_V(!contact_monitor, Array());

	Array ret;
	ret.resize(contact_monitor->body_map.size());
	int idx = 0;
	for (const Map<ObjectID, BodyState>::Element *E = contact_monitor->body_map.front(); E; E = E->next()) {
		Object *obj = ObjectDB::get_instance(E->key());
		if (!obj) {
			ret.resize(ret.size() - 1); //ops
		} else {
			ret[idx++] = obj;
		}
	}

	return ret;
}

String RigidBody::get_configuration_warning() const {

	Transform t = get_transform();

	String warning = CollisionObject::get_configuration_warning();

	if ((get_mode() == MODE_RIGID || get_mode() == MODE_CHARACTER) && (ABS(t.basis.get_axis(0).length() - 1.0) > 0.05 || ABS(t.basis.get_axis(1).length() - 1.0) > 0.05 || ABS(t.basis.get_axis(0).length() - 1.0) > 0.05)) {
		if (warning != String()) {
			warning += "\n";
		}
		warning += TTR("Size changes to RigidBody (in character or rigid modes) will be overriden by the physics engine when running.\nChange the size in children collision shapes instead.");
	}

	return warning;
}

void RigidBody::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_mode", "mode"), &RigidBody::set_mode);
	ObjectTypeDB::bind_method(_MD("get_mode"), &RigidBody::get_mode);

	ObjectTypeDB::bind_method(_MD("set_mass", "mass"), &RigidBody::set_mass);
	ObjectTypeDB::bind_method(_MD("get_mass"), &RigidBody::get_mass);

	ObjectTypeDB::bind_method(_MD("set_weight", "weight"), &RigidBody::set_weight);
	ObjectTypeDB::bind_method(_MD("get_weight"), &RigidBody::get_weight);

	ObjectTypeDB::bind_method(_MD("set_friction", "friction"), &RigidBody::set_friction);
	ObjectTypeDB::bind_method(_MD("get_friction"), &RigidBody::get_friction);

	ObjectTypeDB::bind_method(_MD("set_bounce", "bounce"), &RigidBody::set_bounce);
	ObjectTypeDB::bind_method(_MD("get_bounce"), &RigidBody::get_bounce);

	ObjectTypeDB::bind_method(_MD("set_linear_velocity", "linear_velocity"), &RigidBody::set_linear_velocity);
	ObjectTypeDB::bind_method(_MD("get_linear_velocity"), &RigidBody::get_linear_velocity);

	ObjectTypeDB::bind_method(_MD("set_angular_velocity", "angular_velocity"), &RigidBody::set_angular_velocity);
	ObjectTypeDB::bind_method(_MD("get_angular_velocity"), &RigidBody::get_angular_velocity);

	ObjectTypeDB::bind_method(_MD("set_gravity_scale", "gravity_scale"), &RigidBody::set_gravity_scale);
	ObjectTypeDB::bind_method(_MD("get_gravity_scale"), &RigidBody::get_gravity_scale);

	ObjectTypeDB::bind_method(_MD("set_linear_damp", "linear_damp"), &RigidBody::set_linear_damp);
	ObjectTypeDB::bind_method(_MD("get_linear_damp"), &RigidBody::get_linear_damp);

	ObjectTypeDB::bind_method(_MD("set_angular_damp", "angular_damp"), &RigidBody::set_angular_damp);
	ObjectTypeDB::bind_method(_MD("get_angular_damp"), &RigidBody::get_angular_damp);

	ObjectTypeDB::bind_method(_MD("set_max_contacts_reported", "amount"), &RigidBody::set_max_contacts_reported);
	ObjectTypeDB::bind_method(_MD("get_max_contacts_reported"), &RigidBody::get_max_contacts_reported);

	ObjectTypeDB::bind_method(_MD("set_use_custom_integrator", "enable"), &RigidBody::set_use_custom_integrator);
	ObjectTypeDB::bind_method(_MD("is_using_custom_integrator"), &RigidBody::is_using_custom_integrator);

	ObjectTypeDB::bind_method(_MD("set_contact_monitor", "enabled"), &RigidBody::set_contact_monitor);
	ObjectTypeDB::bind_method(_MD("is_contact_monitor_enabled"), &RigidBody::is_contact_monitor_enabled);

	ObjectTypeDB::bind_method(_MD("set_use_continuous_collision_detection", "enable"), &RigidBody::set_use_continuous_collision_detection);
	ObjectTypeDB::bind_method(_MD("is_using_continuous_collision_detection"), &RigidBody::is_using_continuous_collision_detection);

	ObjectTypeDB::bind_method(_MD("set_axis_velocity", "axis_velocity"), &RigidBody::set_axis_velocity);
	ObjectTypeDB::bind_method(_MD("apply_impulse", "pos", "impulse"), &RigidBody::apply_impulse);

	ObjectTypeDB::bind_method(_MD("set_sleeping", "sleeping"), &RigidBody::set_sleeping);
	ObjectTypeDB::bind_method(_MD("is_sleeping"), &RigidBody::is_sleeping);

	ObjectTypeDB::bind_method(_MD("set_can_sleep", "able_to_sleep"), &RigidBody::set_can_sleep);
	ObjectTypeDB::bind_method(_MD("is_able_to_sleep"), &RigidBody::is_able_to_sleep);

	ObjectTypeDB::bind_method(_MD("_direct_state_changed"), &RigidBody::_direct_state_changed);
	ObjectTypeDB::bind_method(_MD("_body_enter_tree"), &RigidBody::_body_enter_tree);
	ObjectTypeDB::bind_method(_MD("_body_exit_tree"), &RigidBody::_body_exit_tree);

	ObjectTypeDB::bind_method(_MD("set_axis_lock", "axis_lock"), &RigidBody::set_axis_lock);
	ObjectTypeDB::bind_method(_MD("get_axis_lock"), &RigidBody::get_axis_lock);

	ObjectTypeDB::bind_method(_MD("get_colliding_bodies"), &RigidBody::get_colliding_bodies);

	BIND_VMETHOD(MethodInfo("_integrate_forces", PropertyInfo(Variant::OBJECT, "state:PhysicsDirectBodyState")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Rigid,Static,Character,Kinematic"), _SCS("set_mode"), _SCS("get_mode"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "mass", PROPERTY_HINT_EXP_RANGE, "0.01,65535,0.01"), _SCS("set_mass"), _SCS("get_mass"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "weight", PROPERTY_HINT_EXP_RANGE, "0.01,65535,0.01", PROPERTY_USAGE_EDITOR), _SCS("set_weight"), _SCS("get_weight"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "friction", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_friction"), _SCS("get_friction"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_bounce"), _SCS("get_bounce"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "gravity_scale", PROPERTY_HINT_RANGE, "-128,128,0.01"), _SCS("set_gravity_scale"), _SCS("get_gravity_scale"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "custom_integrator"), _SCS("set_use_custom_integrator"), _SCS("is_using_custom_integrator"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "continuous_cd"), _SCS("set_use_continuous_collision_detection"), _SCS("is_using_continuous_collision_detection"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "contacts_reported"), _SCS("set_max_contacts_reported"), _SCS("get_max_contacts_reported"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "contact_monitor"), _SCS("set_contact_monitor"), _SCS("is_contact_monitor_enabled"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sleeping"), _SCS("set_sleeping"), _SCS("is_sleeping"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), _SCS("set_can_sleep"), _SCS("is_able_to_sleep"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis_lock", PROPERTY_HINT_ENUM, "Disabled,Lock X,Lock Y,Lock Z"), _SCS("set_axis_lock"), _SCS("get_axis_lock"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity/linear"), _SCS("set_linear_velocity"), _SCS("get_linear_velocity"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity/angular"), _SCS("set_angular_velocity"), _SCS("get_angular_velocity"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "damp_override/linear", PROPERTY_HINT_RANGE, "-1,128,0.01"), _SCS("set_linear_damp"), _SCS("get_linear_damp"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "damp_override/angular", PROPERTY_HINT_RANGE, "-1,128,0.01"), _SCS("set_angular_damp"), _SCS("get_angular_damp"));

	ADD_SIGNAL(MethodInfo("body_enter_shape", PropertyInfo(Variant::INT, "body_id"), PropertyInfo(Variant::OBJECT, "body"), PropertyInfo(Variant::INT, "body_shape"), PropertyInfo(Variant::INT, "local_shape")));
	ADD_SIGNAL(MethodInfo("body_exit_shape", PropertyInfo(Variant::INT, "body_id"), PropertyInfo(Variant::OBJECT, "body"), PropertyInfo(Variant::INT, "body_shape"), PropertyInfo(Variant::INT, "local_shape")));
	ADD_SIGNAL(MethodInfo("body_enter", PropertyInfo(Variant::OBJECT, "body")));
	ADD_SIGNAL(MethodInfo("body_exit", PropertyInfo(Variant::OBJECT, "body")));
	ADD_SIGNAL(MethodInfo("sleeping_state_changed"));

	BIND_CONSTANT(MODE_STATIC);
	BIND_CONSTANT(MODE_KINEMATIC);
	BIND_CONSTANT(MODE_RIGID);
	BIND_CONSTANT(MODE_CHARACTER);
}

RigidBody::RigidBody() :
		PhysicsBody(PhysicsServer::BODY_MODE_RIGID) {

	mode = MODE_RIGID;

	bounce = 0;
	mass = 1;
	friction = 1;
	max_contacts_reported = 0;
	state = NULL;

	gravity_scale = 1;
	linear_damp = -1;
	angular_damp = -1;

	//angular_velocity=0;
	sleeping = false;
	ccd = false;

	custom_integrator = false;
	contact_monitor = NULL;
	can_sleep = true;

	axis_lock = AXIS_LOCK_DISABLED;

	PhysicsServer::get_singleton()->body_set_force_integration_callback(get_rid(), this, "_direct_state_changed");
}

RigidBody::~RigidBody() {

	if (contact_monitor)
		memdelete(contact_monitor);
}
//////////////////////////////////////////////////////
//////////////////////////

Variant KinematicBody::_get_collider() const {

	ObjectID oid = get_collider();
	if (oid == 0)
		return Variant();
	Object *obj = ObjectDB::get_instance(oid);
	if (!obj)
		return Variant();

	Reference *ref = obj->cast_to<Reference>();
	if (ref) {
		return Ref<Reference>(ref);
	}

	return obj;
}

bool KinematicBody::_ignores_mode(PhysicsServer::BodyMode p_mode) const {

	switch (p_mode) {
		case PhysicsServer::BODY_MODE_STATIC: return !collide_static;
		case PhysicsServer::BODY_MODE_KINEMATIC: return !collide_kinematic;
		case PhysicsServer::BODY_MODE_RIGID: return !collide_rigid;
		case PhysicsServer::BODY_MODE_CHARACTER: return !collide_character;
	}

	return true;
}

Vector3 KinematicBody::move(const Vector3 &p_motion) {

	//give me back regular physics engine logic
	//this is madness
	//and most people using this function will think
	//what it does is simpler than using physics
	//this took about a week to get right..
	//but is it right? who knows at this point..

	colliding = false;
	ERR_FAIL_COND_V(!is_inside_tree(), Vector3());
	PhysicsDirectSpaceState *dss = PhysicsServer::get_singleton()->space_get_direct_state(get_world()->get_space());
	ERR_FAIL_COND_V(!dss, Vector3());
	const int max_shapes = 32;
	Vector3 sr[max_shapes * 2];
	int res_shapes;

	Set<RID> exclude;
	exclude.insert(get_rid());

	//recover first
	int recover_attempts = 4;

	bool collided = false;
	uint32_t mask = 0;
	if (collide_static)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_STATIC_BODY;
	if (collide_kinematic)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_KINEMATIC_BODY;
	if (collide_rigid)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_RIGID_BODY;
	if (collide_character)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_CHARACTER_BODY;

	//	print_line("motion: "+p_motion+" margin: "+rtos(margin));

	//print_line("margin: "+rtos(margin));

	float m = margin;
	//m=0.001;

	do {

		//motion recover
		for (int i = 0; i < get_shape_count(); i++) {

			if (is_shape_set_as_trigger(i))
				continue;

			if (dss->collide_shape(get_shape(i)->get_rid(), get_global_transform() * get_shape_transform(i), m, sr, max_shapes, res_shapes, exclude, get_layer_mask(), mask)) {
				collided = true;
			}
		}

		if (!collided)
			break;

		//print_line("have to recover");
		Vector3 recover_motion;
		bool all_outside = true;
		for (int j = 0; j < 8; j++) {
			for (int i = 0; i < res_shapes; i++) {

				Vector3 a = sr[i * 2 + 0];
				Vector3 b = sr[i * 2 + 1];
//print_line(String()+a+" -> "+b);
#if 0
				float d = a.distance_to(b);

				//if (d<margin)
				///	continue;
	   ///
	   ///
				recover_motion+=(b-a)*0.2;
#else
				float dist = a.distance_to(b);
				if (dist > CMP_EPSILON) {
					Vector3 norm = (b - a).normalized();
					if (dist > margin * 0.5)
						all_outside = false;
					float adv = norm.dot(recover_motion);
					//print_line(itos(i)+" dist: "+rtos(dist)+" adv: "+rtos(adv));
					recover_motion += norm * MAX(dist - adv, 0) * 0.4;
				}
#endif
			}
		}

		if (recover_motion == Vector3()) {
			collided = false;
			break;
		}

		//print_line("**** RECOVER: "+recover_motion);

		Transform gt = get_global_transform();
		gt.origin += recover_motion;
		set_global_transform(gt);

		recover_attempts--;

		if (all_outside)
			break;

	} while (recover_attempts);

	//move second
	float safe = 1.0;
	float unsafe = 1.0;
	int best_shape = -1;

	PhysicsDirectSpaceState::ShapeRestInfo rest;

	//print_line("pos: "+get_global_transform().origin);
	//print_line("motion: "+p_motion);

	for (int i = 0; i < get_shape_count(); i++) {

		if (is_shape_set_as_trigger(i))
			continue;

		float lsafe, lunsafe;
		PhysicsDirectSpaceState::ShapeRestInfo lrest;
		bool valid = dss->cast_motion(get_shape(i)->get_rid(), get_global_transform() * get_shape_transform(i), p_motion, 0, lsafe, lunsafe, exclude, get_layer_mask(), mask, &lrest);
		//print_line("shape: "+itos(i)+" travel:"+rtos(ltravel));
		if (!valid) {
			safe = 0;
			unsafe = 0;
			best_shape = i; //sadly it's the best
			//print_line("initial stuck");

			break;
		}
		if (lsafe == 1.0) {
			//print_line("initial free");
			continue;
		}
		if (lsafe < safe) {

			//print_line("initial at "+rtos(lsafe));
			safe = lsafe;
			safe = MAX(0, lsafe - 0.01);
			unsafe = lunsafe;
			best_shape = i;
			rest = lrest;
		}
	}

	//print_line("best shape: "+itos(best_shape)+" motion "+p_motion);

	if (safe >= 1) {
		//not collided
		colliding = false;
	} else {

		colliding = true;

		if (true || (safe == 0 && unsafe == 0)) { //use it always because it's more precise than GJK
			//no advance, use rest info from collision
			Transform ugt = get_global_transform();
			ugt.origin += p_motion * unsafe;

			PhysicsDirectSpaceState::ShapeRestInfo rest_info;
			bool c2 = dss->rest_info(get_shape(best_shape)->get_rid(), ugt * get_shape_transform(best_shape), m, &rest, exclude, get_layer_mask(), mask);
			if (!c2) {
				//should not happen, but floating point precision is so weird..
				colliding = false;
			}

			//	print_line("Rest Travel: "+rest.normal);
		}

		if (colliding) {

			collision = rest.point;
			normal = rest.normal;
			collider = rest.collider_id;
			collider_vel = rest.linear_velocity;
			collider_shape = rest.shape;
		}
	}

	Vector3 motion = p_motion * safe;
	//if (colliding)
	//	motion+=normal*0.001;
	Transform gt = get_global_transform();
	gt.origin += motion;
	set_global_transform(gt);

	return p_motion - motion;
}

Vector3 KinematicBody::move_to(const Vector3 &p_position) {

	return move(p_position - get_global_transform().origin);
}

bool KinematicBody::can_teleport_to(const Vector3 &p_position) {

	ERR_FAIL_COND_V(!is_inside_tree(), false);
	PhysicsDirectSpaceState *dss = PhysicsServer::get_singleton()->space_get_direct_state(get_world()->get_space());
	ERR_FAIL_COND_V(!dss, false);

	uint32_t mask = 0;
	if (collide_static)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_STATIC_BODY;
	if (collide_kinematic)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_KINEMATIC_BODY;
	if (collide_rigid)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_RIGID_BODY;
	if (collide_character)
		mask |= PhysicsDirectSpaceState::TYPE_MASK_CHARACTER_BODY;

	Transform xform = get_global_transform();
	xform.origin = p_position;

	Set<RID> exclude;
	exclude.insert(get_rid());

	for (int i = 0; i < get_shape_count(); i++) {

		if (is_shape_set_as_trigger(i))
			continue;

		bool col = dss->intersect_shape(get_shape(i)->get_rid(), xform * get_shape_transform(i), 0, NULL, 1, exclude, get_layer_mask(), mask);
		if (col)
			return false;
	}

	return true;
}

bool KinematicBody::is_colliding() const {

	ERR_FAIL_COND_V(!is_inside_tree(), false);

	return colliding;
}
Vector3 KinematicBody::get_collision_pos() const {

	ERR_FAIL_COND_V(!colliding, Vector3());
	return collision;
}
Vector3 KinematicBody::get_collision_normal() const {

	ERR_FAIL_COND_V(!colliding, Vector3());
	return normal;
}

Vector3 KinematicBody::get_collider_velocity() const {

	return collider_vel;
}

ObjectID KinematicBody::get_collider() const {

	ERR_FAIL_COND_V(!colliding, 0);
	return collider;
}
int KinematicBody::get_collider_shape() const {

	ERR_FAIL_COND_V(!colliding, -1);
	return collider_shape;
}
void KinematicBody::set_collide_with_static_bodies(bool p_enable) {

	collide_static = p_enable;
}
bool KinematicBody::can_collide_with_static_bodies() const {

	return collide_static;
}

void KinematicBody::set_collide_with_rigid_bodies(bool p_enable) {

	collide_rigid = p_enable;
}
bool KinematicBody::can_collide_with_rigid_bodies() const {

	return collide_rigid;
}

void KinematicBody::set_collide_with_kinematic_bodies(bool p_enable) {

	collide_kinematic = p_enable;
}
bool KinematicBody::can_collide_with_kinematic_bodies() const {

	return collide_kinematic;
}

void KinematicBody::set_collide_with_character_bodies(bool p_enable) {

	collide_character = p_enable;
}
bool KinematicBody::can_collide_with_character_bodies() const {

	return collide_character;
}

void KinematicBody::set_collision_margin(float p_margin) {

	margin = p_margin;
}

float KinematicBody::get_collision_margin() const {

	return margin;
}

void KinematicBody::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("move", "rel_vec"), &KinematicBody::move);
	ObjectTypeDB::bind_method(_MD("move_to", "position"), &KinematicBody::move_to);

	ObjectTypeDB::bind_method(_MD("can_teleport_to", "position"), &KinematicBody::can_teleport_to);

	ObjectTypeDB::bind_method(_MD("is_colliding"), &KinematicBody::is_colliding);

	ObjectTypeDB::bind_method(_MD("get_collision_pos"), &KinematicBody::get_collision_pos);
	ObjectTypeDB::bind_method(_MD("get_collision_normal"), &KinematicBody::get_collision_normal);
	ObjectTypeDB::bind_method(_MD("get_collider_velocity"), &KinematicBody::get_collider_velocity);
	ObjectTypeDB::bind_method(_MD("get_collider:Variant"), &KinematicBody::_get_collider);
	ObjectTypeDB::bind_method(_MD("get_collider_shape"), &KinematicBody::get_collider_shape);

	ObjectTypeDB::bind_method(_MD("set_collide_with_static_bodies", "enable"), &KinematicBody::set_collide_with_static_bodies);
	ObjectTypeDB::bind_method(_MD("can_collide_with_static_bodies"), &KinematicBody::can_collide_with_static_bodies);

	ObjectTypeDB::bind_method(_MD("set_collide_with_kinematic_bodies", "enable"), &KinematicBody::set_collide_with_kinematic_bodies);
	ObjectTypeDB::bind_method(_MD("can_collide_with_kinematic_bodies"), &KinematicBody::can_collide_with_kinematic_bodies);

	ObjectTypeDB::bind_method(_MD("set_collide_with_rigid_bodies", "enable"), &KinematicBody::set_collide_with_rigid_bodies);
	ObjectTypeDB::bind_method(_MD("can_collide_with_rigid_bodies"), &KinematicBody::can_collide_with_rigid_bodies);

	ObjectTypeDB::bind_method(_MD("set_collide_with_character_bodies", "enable"), &KinematicBody::set_collide_with_character_bodies);
	ObjectTypeDB::bind_method(_MD("can_collide_with_character_bodies"), &KinematicBody::can_collide_with_character_bodies);

	ObjectTypeDB::bind_method(_MD("set_collision_margin", "pixels"), &KinematicBody::set_collision_margin);
	ObjectTypeDB::bind_method(_MD("get_collision_margin", "pixels"), &KinematicBody::get_collision_margin);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with/static"), _SCS("set_collide_with_static_bodies"), _SCS("can_collide_with_static_bodies"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with/kinematic"), _SCS("set_collide_with_kinematic_bodies"), _SCS("can_collide_with_kinematic_bodies"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with/rigid"), _SCS("set_collide_with_rigid_bodies"), _SCS("can_collide_with_rigid_bodies"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with/character"), _SCS("set_collide_with_character_bodies"), _SCS("can_collide_with_character_bodies"));
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "collision/margin", PROPERTY_HINT_RANGE, "0.001,256,0.001"), _SCS("set_collision_margin"), _SCS("get_collision_margin"));
}

KinematicBody::KinematicBody() :
		PhysicsBody(PhysicsServer::BODY_MODE_KINEMATIC) {

	collide_static = true;
	collide_rigid = true;
	collide_kinematic = true;
	collide_character = true;

	colliding = false;
	collider = 0;
	margin = 0.001;
	collider_shape = 0;
}
KinematicBody::~KinematicBody() {
}
