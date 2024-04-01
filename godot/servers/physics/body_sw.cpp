/*************************************************************************/
/*  body_sw.cpp                                                          */
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
#include "body_sw.h"
#include "area_sw.h"
#include "space_sw.h"

void BodySW::_update_inertia() {

	if (get_space() && !inertia_update_list.in_list())
		get_space()->body_add_to_inertia_update_list(&inertia_update_list);
}

void BodySW::_update_inertia_tensor() {

	Matrix3 tb = get_transform().basis;
	tb.scale(_inv_inertia);
	_inv_inertia_tensor = tb * get_transform().basis.transposed();
}

void BodySW::update_inertias() {

	//update shapes and motions

	switch (mode) {

		case PhysicsServer::BODY_MODE_RIGID: {

			//update tensor for allshapes, not the best way but should be somehow OK. (inspired from bullet)
			float total_area = 0;

			for (int i = 0; i < get_shape_count(); i++) {

				total_area += get_shape_aabb(i).get_area();
			}

			Vector3 _inertia;

			for (int i = 0; i < get_shape_count(); i++) {

				const ShapeSW *shape = get_shape(i);

				float area = get_shape_aabb(i).get_area();

				float mass = area * this->mass / total_area;

				_inertia += shape->get_moment_of_inertia(mass) + mass * get_shape_transform(i).get_origin();
			}

			if (_inertia != Vector3())
				_inv_inertia = _inertia.inverse();
			else
				_inv_inertia = Vector3();

			if (mass)
				_inv_mass = 1.0 / mass;
			else
				_inv_mass = 0;

		} break;

		case PhysicsServer::BODY_MODE_KINEMATIC:
		case PhysicsServer::BODY_MODE_STATIC: {

			_inv_inertia = Vector3();
			_inv_mass = 0;
		} break;
		case PhysicsServer::BODY_MODE_CHARACTER: {

			_inv_inertia = Vector3();
			_inv_mass = 1.0 / mass;

		} break;
	}
	_update_inertia_tensor();

	//_update_shapes();
}

void BodySW::set_active(bool p_active) {

	if (active == p_active)
		return;

	active = p_active;
	if (!p_active) {
		if (get_space())
			get_space()->body_remove_from_active_list(&active_list);
	} else {
		if (mode == PhysicsServer::BODY_MODE_STATIC)
			return; //static bodies can't become active
		if (get_space())
			get_space()->body_add_to_active_list(&active_list);

		//still_time=0;
	}
	/*
	if (!space)
		return;

	for(int i=0;i<get_shape_count();i++) {
		Shape &s=shapes[i];
		if (s.bpid>0) {
			get_space()->get_broadphase()->set_active(s.bpid,active);
		}
	}
*/
}

void BodySW::set_param(PhysicsServer::BodyParameter p_param, float p_value) {

	switch (p_param) {
		case PhysicsServer::BODY_PARAM_BOUNCE: {

			bounce = p_value;
		} break;
		case PhysicsServer::BODY_PARAM_FRICTION: {

			friction = p_value;
		} break;
		case PhysicsServer::BODY_PARAM_MASS: {
			ERR_FAIL_COND(p_value <= 0);
			mass = p_value;
			_update_inertia();

		} break;
		case PhysicsServer::BODY_PARAM_GRAVITY_SCALE: {
			gravity_scale = p_value;
		} break;
		case PhysicsServer::BODY_PARAM_LINEAR_DAMP: {

			linear_damp = p_value;
		} break;
		case PhysicsServer::BODY_PARAM_ANGULAR_DAMP: {

			angular_damp = p_value;
		} break;
		default: {}
	}
}

float BodySW::get_param(PhysicsServer::BodyParameter p_param) const {

	switch (p_param) {
		case PhysicsServer::BODY_PARAM_BOUNCE: {

			return bounce;
		} break;
		case PhysicsServer::BODY_PARAM_FRICTION: {

			return friction;
		} break;
		case PhysicsServer::BODY_PARAM_MASS: {
			return mass;
		} break;
		case PhysicsServer::BODY_PARAM_GRAVITY_SCALE: {
			return gravity_scale;
		} break;
		case PhysicsServer::BODY_PARAM_LINEAR_DAMP: {

			return linear_damp;
		} break;
		case PhysicsServer::BODY_PARAM_ANGULAR_DAMP: {

			return angular_damp;
		} break;

		default: {}
	}

	return 0;
}

void BodySW::set_mode(PhysicsServer::BodyMode p_mode) {

	PhysicsServer::BodyMode prev = mode;
	mode = p_mode;

	switch (p_mode) {
		//CLEAR UP EVERYTHING IN CASE IT NOT WORKS!
		case PhysicsServer::BODY_MODE_STATIC:
		case PhysicsServer::BODY_MODE_KINEMATIC: {

			_set_inv_transform(get_transform().affine_inverse());
			_inv_mass = 0;
			_set_static(p_mode == PhysicsServer::BODY_MODE_STATIC);
			//set_active(p_mode==PhysicsServer::BODY_MODE_KINEMATIC);
			set_active(p_mode == PhysicsServer::BODY_MODE_KINEMATIC && contacts.size());
			linear_velocity = Vector3();
			angular_velocity = Vector3();
			if (mode == PhysicsServer::BODY_MODE_KINEMATIC && prev != mode) {
				first_time_kinematic = true;
			}

		} break;
		case PhysicsServer::BODY_MODE_RIGID: {

			_inv_mass = mass > 0 ? (1.0 / mass) : 0;
			_set_static(false);

		} break;
		case PhysicsServer::BODY_MODE_CHARACTER: {

			_inv_mass = mass > 0 ? (1.0 / mass) : 0;
			_set_static(false);
		} break;
	}

	_update_inertia();
	//if (get_space())
	//		_update_queries();
}
PhysicsServer::BodyMode BodySW::get_mode() const {

	return mode;
}

void BodySW::_shapes_changed() {

	_update_inertia();
}

void BodySW::_shape_index_removed(int p_index) {

	for (Map<ConstraintSW *, int>::Element *E = constraint_map.front(); E; E = E->next()) {
		E->key()->shift_shape_indices(this, p_index);
	}
}

void BodySW::set_state(PhysicsServer::BodyState p_state, const Variant &p_variant) {

	switch (p_state) {
		case PhysicsServer::BODY_STATE_TRANSFORM: {

			if (mode == PhysicsServer::BODY_MODE_KINEMATIC) {
				new_transform = p_variant;
				//wakeup_neighbours();
				set_active(true);
				if (first_time_kinematic) {
					_set_transform(p_variant);
					_set_inv_transform(get_transform().affine_inverse());
					first_time_kinematic = false;
				}

			} else if (mode == PhysicsServer::BODY_MODE_STATIC) {
				_set_transform(p_variant);
				_set_inv_transform(get_transform().affine_inverse());
				wakeup_neighbours();
			} else {
				Transform t = p_variant;
				t.orthonormalize();
				new_transform = get_transform(); //used as old to compute motion
				if (new_transform == t)
					break;
				_set_transform(t);
				_set_inv_transform(get_transform().inverse());
			}
			wakeup();

		} break;
		case PhysicsServer::BODY_STATE_LINEAR_VELOCITY: {

			//if (mode==PhysicsServer::BODY_MODE_STATIC)
			//	break;
			linear_velocity = p_variant;
			wakeup();
		} break;
		case PhysicsServer::BODY_STATE_ANGULAR_VELOCITY: {
			//if (mode!=PhysicsServer::BODY_MODE_RIGID)
			//	break;
			angular_velocity = p_variant;
			wakeup();

		} break;
		case PhysicsServer::BODY_STATE_SLEEPING: {
			//?
			if (mode == PhysicsServer::BODY_MODE_STATIC || mode == PhysicsServer::BODY_MODE_KINEMATIC)
				break;
			bool do_sleep = p_variant;
			if (do_sleep) {
				linear_velocity = Vector3();
				//biased_linear_velocity=Vector3();
				angular_velocity = Vector3();
				//biased_angular_velocity=Vector3();
				set_active(false);
			} else {
				if (mode != PhysicsServer::BODY_MODE_STATIC)
					set_active(true);
			}
		} break;
		case PhysicsServer::BODY_STATE_CAN_SLEEP: {
			can_sleep = p_variant;
			if (mode == PhysicsServer::BODY_MODE_RIGID && !active && !can_sleep)
				set_active(true);

		} break;
	}
}
Variant BodySW::get_state(PhysicsServer::BodyState p_state) const {

	switch (p_state) {
		case PhysicsServer::BODY_STATE_TRANSFORM: {
			return get_transform();
		} break;
		case PhysicsServer::BODY_STATE_LINEAR_VELOCITY: {
			return linear_velocity;
		} break;
		case PhysicsServer::BODY_STATE_ANGULAR_VELOCITY: {
			return angular_velocity;
		} break;
		case PhysicsServer::BODY_STATE_SLEEPING: {
			return !is_active();
		} break;
		case PhysicsServer::BODY_STATE_CAN_SLEEP: {
			return can_sleep;
		} break;
	}

	return Variant();
}

void BodySW::set_space(SpaceSW *p_space) {

	if (get_space()) {

		if (inertia_update_list.in_list())
			get_space()->body_remove_from_inertia_update_list(&inertia_update_list);
		if (active_list.in_list())
			get_space()->body_remove_from_active_list(&active_list);
		if (direct_state_query_list.in_list())
			get_space()->body_remove_from_state_query_list(&direct_state_query_list);
	}

	_set_space(p_space);

	if (get_space()) {

		_update_inertia();
		if (active)
			get_space()->body_add_to_active_list(&active_list);
		//		_update_queries();
		//if (is_active()) {
		//	active=false;
		//	set_active(true);
		//}
	}

	first_integration = true;
}

void BodySW::_compute_area_gravity_and_dampenings(const AreaSW *p_area) {

	if (p_area->is_gravity_point()) {
		if (p_area->get_gravity_distance_scale() > 0) {
			Vector3 v = p_area->get_transform().xform(p_area->get_gravity_vector()) - get_transform().get_origin();
			gravity += v.normalized() * (p_area->get_gravity() / Math::pow(v.length() * p_area->get_gravity_distance_scale() + 1, 2));
		} else {
			gravity += (p_area->get_transform().xform(p_area->get_gravity_vector()) - get_transform().get_origin()).normalized() * p_area->get_gravity();
		}
	} else {
		gravity += p_area->get_gravity_vector() * p_area->get_gravity();
	}

	area_linear_damp += p_area->get_linear_damp();
	area_angular_damp += p_area->get_angular_damp();
}

void BodySW::integrate_forces(real_t p_step) {

	if (mode == PhysicsServer::BODY_MODE_STATIC)
		return;

	AreaSW *def_area = get_space()->get_default_area();
	// AreaSW *damp_area = def_area;

	ERR_FAIL_COND(!def_area);

	int ac = areas.size();
	bool stopped = false;
	gravity = Vector3(0, 0, 0);
	area_linear_damp = 0;
	area_angular_damp = 0;
	if (ac) {
		areas.sort();
		const AreaCMP *aa = &areas[0];
		// damp_area = aa[ac-1].area;
		for (int i = ac - 1; i >= 0 && !stopped; i--) {
			PhysicsServer::AreaSpaceOverrideMode mode = aa[i].area->get_space_override_mode();
			switch (mode) {
				case PhysicsServer::AREA_SPACE_OVERRIDE_COMBINE:
				case PhysicsServer::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
					_compute_area_gravity_and_dampenings(aa[i].area);
					stopped = mode == PhysicsServer::AREA_SPACE_OVERRIDE_COMBINE_REPLACE;
				} break;
				case PhysicsServer::AREA_SPACE_OVERRIDE_REPLACE:
				case PhysicsServer::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
					gravity = Vector3(0, 0, 0);
					area_angular_damp = 0;
					area_linear_damp = 0;
					_compute_area_gravity_and_dampenings(aa[i].area);
					stopped = mode == PhysicsServer::AREA_SPACE_OVERRIDE_REPLACE;
				} break;
				default: {}
			}
		}
	}

	if (!stopped) {
		_compute_area_gravity_and_dampenings(def_area);
	}

	gravity *= gravity_scale;

	// If less than 0, override dampenings with that of the Body
	if (angular_damp >= 0)
		area_angular_damp = angular_damp;
	//else
	//	area_angular_damp=damp_area->get_angular_damp();

	if (linear_damp >= 0)
		area_linear_damp = linear_damp;
	//else
	//	area_linear_damp=damp_area->get_linear_damp();

	Vector3 motion;
	bool do_motion = false;

	if (mode == PhysicsServer::BODY_MODE_KINEMATIC) {

		//compute motion, angular and etc. velocities from prev transform
		linear_velocity = (new_transform.origin - get_transform().origin) / p_step;

		//compute a FAKE angular velocity, not so easy
		Matrix3 rot = new_transform.basis.orthonormalized().transposed() * get_transform().basis.orthonormalized();
		Vector3 axis;
		float angle;

		rot.get_axis_and_angle(axis, angle);
		axis.normalize();
		angular_velocity = axis.normalized() * (angle / p_step);

		motion = new_transform.origin - get_transform().origin;
		do_motion = true;

	} else {
		if (!omit_force_integration && !first_integration) {
			//overriden by direct state query

			Vector3 force = gravity * mass;
			force += applied_force;
			Vector3 torque = applied_torque;

			real_t damp = 1.0 - p_step * area_linear_damp;

			if (damp < 0) // reached zero in the given time
				damp = 0;

			real_t angular_damp = 1.0 - p_step * area_angular_damp;

			if (angular_damp < 0) // reached zero in the given time
				angular_damp = 0;

			linear_velocity *= damp;
			angular_velocity *= angular_damp;

			linear_velocity += _inv_mass * force * p_step;
			angular_velocity += _inv_inertia_tensor.xform(torque) * p_step;
		}

		if (continuous_cd) {
			motion = linear_velocity * p_step;
			do_motion = true;
		}
	}

	applied_force = Vector3();
	applied_torque = Vector3();
	first_integration = false;

	//motion=linear_velocity*p_step;

	biased_angular_velocity = Vector3();
	biased_linear_velocity = Vector3();

	if (do_motion) { //shapes temporarily extend for raycast
		_update_shapes_with_motion(motion);
	}

	def_area = NULL; // clear the area, so it is set in the next frame
	contact_count = 0;
}

void BodySW::integrate_velocities(real_t p_step) {

	if (mode == PhysicsServer::BODY_MODE_STATIC)
		return;

	if (fi_callback)
		get_space()->body_add_to_state_query_list(&direct_state_query_list);

	if (mode == PhysicsServer::BODY_MODE_KINEMATIC) {

		_set_transform(new_transform, false);
		_set_inv_transform(new_transform.affine_inverse());
		if (contacts.size() == 0 && linear_velocity == Vector3() && angular_velocity == Vector3())
			set_active(false); //stopped moving, deactivate

		return;
	}

	//apply axis lock
	if (axis_lock != PhysicsServer::BODY_AXIS_LOCK_DISABLED) {

		int axis = axis_lock - 1;
		for (int i = 0; i < 3; i++) {
			if (i == axis) {
				linear_velocity[i] = 0;
				biased_linear_velocity[i] = 0;
			} else {

				angular_velocity[i] = 0;
				biased_angular_velocity[i] = 0;
			}
		}
	}

	Vector3 total_angular_velocity = angular_velocity + biased_angular_velocity;

	float ang_vel = total_angular_velocity.length();
	Transform transform = get_transform();

	if (ang_vel != 0.0) {
		Vector3 ang_vel_axis = total_angular_velocity / ang_vel;
		Matrix3 rot(ang_vel_axis, -ang_vel * p_step);
		transform.basis = rot * transform.basis;
		transform.orthonormalize();
	}

	Vector3 total_linear_velocity = linear_velocity + biased_linear_velocity;
	/*for(int i=0;i<3;i++) {
		if (axis_lock&(1<<i)) {
			transform.origin[i]=0.0;
		}
	}*/

	transform.origin += total_linear_velocity * p_step;

	_set_transform(transform);
	_set_inv_transform(get_transform().inverse());

	_update_inertia_tensor();

	//if (fi_callback) {

	//	get_space()->body_add_to_state_query_list(&direct_state_query_list);
	//
}

/*
void BodySW::simulate_motion(const Transform& p_xform,real_t p_step) {

	Transform inv_xform = p_xform.affine_inverse();
	if (!get_space()) {
		_set_transform(p_xform);
		_set_inv_transform(inv_xform);

		return;
	}

	//compute a FAKE linear velocity - this is easy

	linear_velocity=(p_xform.origin - get_transform().origin)/p_step;

	//compute a FAKE angular velocity, not so easy
	Matrix3 rot=get_transform().basis.orthonormalized().transposed() * p_xform.basis.orthonormalized();
	Vector3 axis;
	float angle;

	rot.get_axis_and_angle(axis,angle);
	axis.normalize();
	angular_velocity=axis.normalized() * (angle/p_step);
	linear_velocity = (p_xform.origin - get_transform().origin)/p_step;

	if (!direct_state_query_list.in_list())// - callalways, so lv and av are cleared && (state_query || direct_state_query))
		get_space()->body_add_to_state_query_list(&direct_state_query_list);
	simulated_motion=true;
	_set_transform(p_xform);


}
*/

void BodySW::wakeup_neighbours() {

	for (Map<ConstraintSW *, int>::Element *E = constraint_map.front(); E; E = E->next()) {

		const ConstraintSW *c = E->key();
		BodySW **n = c->get_body_ptr();
		int bc = c->get_body_count();

		for (int i = 0; i < bc; i++) {

			if (i == E->get())
				continue;
			BodySW *b = n[i];
			if (b->mode != PhysicsServer::BODY_MODE_RIGID)
				continue;

			if (!b->is_active())
				b->set_active(true);
		}
	}
}

void BodySW::call_queries() {

	if (fi_callback) {

		PhysicsDirectBodyStateSW *dbs = PhysicsDirectBodyStateSW::singleton;
		dbs->body = this;

		Variant v = dbs;

		Object *obj = ObjectDB::get_instance(fi_callback->id);
		if (!obj) {

			set_force_integration_callback(0, StringName());
		} else {
			const Variant *vp[2] = { &v, &fi_callback->udata };

			Variant::CallError ce;
			int argc = (fi_callback->udata.get_type() == Variant::NIL) ? 1 : 2;
			obj->call(fi_callback->method, vp, argc, ce);
		}
	}
}

bool BodySW::sleep_test(real_t p_step) {

	if (mode == PhysicsServer::BODY_MODE_STATIC || mode == PhysicsServer::BODY_MODE_KINEMATIC)
		return true; //
	else if (mode == PhysicsServer::BODY_MODE_CHARACTER)
		return !active; // characters don't sleep unless asked to sleep
	else if (!can_sleep)
		return false;

	if (Math::abs(angular_velocity.length()) < get_space()->get_body_angular_velocity_sleep_treshold() && Math::abs(linear_velocity.length_squared()) < get_space()->get_body_linear_velocity_sleep_treshold() * get_space()->get_body_linear_velocity_sleep_treshold()) {

		still_time += p_step;

		return still_time > get_space()->get_body_time_to_sleep();
	} else {

		still_time = 0; //maybe this should be set to 0 on set_active?
		return false;
	}
}

void BodySW::set_force_integration_callback(ObjectID p_id, const StringName &p_method, const Variant &p_udata) {

	if (fi_callback) {

		memdelete(fi_callback);
		fi_callback = NULL;
	}

	if (p_id != 0) {

		fi_callback = memnew(ForceIntegrationCallback);
		fi_callback->id = p_id;
		fi_callback->method = p_method;
		fi_callback->udata = p_udata;
	}
}

BodySW::BodySW() :
		CollisionObjectSW(TYPE_BODY),
		active_list(this),
		inertia_update_list(this),
		direct_state_query_list(this) {

	mode = PhysicsServer::BODY_MODE_RIGID;
	active = true;

	mass = 1;
	//	_inv_inertia=Transform();
	_inv_mass = 1;
	bounce = 0;
	friction = 1;
	omit_force_integration = false;
	//	applied_torque=0;
	island_step = 0;
	island_next = NULL;
	island_list_next = NULL;
	first_time_kinematic = false;
	first_integration = false;
	_set_static(false);

	contact_count = 0;
	gravity_scale = 1.0;

	linear_damp = -1;
	angular_damp = -1;
	area_angular_damp = 0;
	area_linear_damp = 0;

	still_time = 0;
	continuous_cd = false;
	can_sleep = false;
	fi_callback = NULL;
	axis_lock = PhysicsServer::BODY_AXIS_LOCK_DISABLED;
}

BodySW::~BodySW() {

	if (fi_callback)
		memdelete(fi_callback);
}

PhysicsDirectBodyStateSW *PhysicsDirectBodyStateSW::singleton = NULL;

PhysicsDirectSpaceState *PhysicsDirectBodyStateSW::get_space_state() {

	return body->get_space()->get_direct_state();
}
