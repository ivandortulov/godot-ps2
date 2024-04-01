/*************************************************************************/
/*  body_sw.h                                                            */
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
#ifndef BODY_SW_H
#define BODY_SW_H

#include "area_sw.h"
#include "collision_object_sw.h"
#include "vset.h"

class ConstraintSW;

class BodySW : public CollisionObjectSW {

	PhysicsServer::BodyMode mode;

	Vector3 linear_velocity;
	Vector3 angular_velocity;

	Vector3 biased_linear_velocity;
	Vector3 biased_angular_velocity;
	real_t mass;
	real_t bounce;
	real_t friction;

	real_t linear_damp;
	real_t angular_damp;
	real_t gravity_scale;

	PhysicsServer::BodyAxisLock axis_lock;

	real_t _inv_mass;
	Vector3 _inv_inertia;
	Matrix3 _inv_inertia_tensor;

	Vector3 gravity;

	real_t still_time;

	Vector3 applied_force;
	Vector3 applied_torque;

	float area_angular_damp;
	float area_linear_damp;

	SelfList<BodySW> active_list;
	SelfList<BodySW> inertia_update_list;
	SelfList<BodySW> direct_state_query_list;

	VSet<RID> exceptions;
	bool omit_force_integration;
	bool active;

	bool first_integration;

	bool continuous_cd;
	bool can_sleep;
	bool first_time_kinematic;
	void _update_inertia();
	virtual void _shapes_changed();
	Transform new_transform;

	Map<ConstraintSW *, int> constraint_map;

	struct AreaCMP {

		AreaSW *area;
		int refCount;
		_FORCE_INLINE_ bool operator==(const AreaCMP &p_cmp) const { return area->get_self() == p_cmp.area->get_self(); }
		_FORCE_INLINE_ bool operator<(const AreaCMP &p_cmp) const { return area->get_priority() < p_cmp.area->get_priority(); }
		_FORCE_INLINE_ AreaCMP() {}
		_FORCE_INLINE_ AreaCMP(AreaSW *p_area) {
			area = p_area;
			refCount = 1;
		}
	};

	Vector<AreaCMP> areas;

	struct Contact {

		Vector3 local_pos;
		Vector3 local_normal;
		float depth;
		int local_shape;
		Vector3 collider_pos;
		int collider_shape;
		ObjectID collider_instance_id;
		RID collider;
		Vector3 collider_velocity_at_pos;
	};

	Vector<Contact> contacts; //no contacts by default
	int contact_count;

	struct ForceIntegrationCallback {

		ObjectID id;
		StringName method;
		Variant udata;
	};

	ForceIntegrationCallback *fi_callback;

	uint64_t island_step;
	BodySW *island_next;
	BodySW *island_list_next;

	_FORCE_INLINE_ void _compute_area_gravity_and_dampenings(const AreaSW *p_area);

	_FORCE_INLINE_ void _update_inertia_tensor();

	friend class PhysicsDirectBodyStateSW; // i give up, too many functions to expose

protected:
	virtual void _shape_index_removed(int p_index);

public:
	void set_force_integration_callback(ObjectID p_id, const StringName &p_method, const Variant &p_udata = Variant());

	_FORCE_INLINE_ void add_area(AreaSW *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas[index].refCount += 1;
		} else {
			areas.ordered_insert(AreaCMP(p_area));
		}
	}

	_FORCE_INLINE_ void remove_area(AreaSW *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas[index].refCount -= 1;
			if (areas[index].refCount < 1)
				areas.remove(index);
		}
	}

	_FORCE_INLINE_ void set_max_contacts_reported(int p_size) {
		contacts.resize(p_size);
		contact_count = 0;
		if (mode == PhysicsServer::BODY_MODE_KINEMATIC && p_size) set_active(true);
	}
	_FORCE_INLINE_ int get_max_contacts_reported() const { return contacts.size(); }

	_FORCE_INLINE_ bool can_report_contacts() const { return !contacts.empty(); }
	_FORCE_INLINE_ void add_contact(const Vector3 &p_local_pos, const Vector3 &p_local_normal, float p_depth, int p_local_shape, const Vector3 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector3 &p_collider_velocity_at_pos);

	_FORCE_INLINE_ void add_exception(const RID &p_exception) { exceptions.insert(p_exception); }
	_FORCE_INLINE_ void remove_exception(const RID &p_exception) { exceptions.erase(p_exception); }
	_FORCE_INLINE_ bool has_exception(const RID &p_exception) const { return exceptions.has(p_exception); }
	_FORCE_INLINE_ const VSet<RID> &get_exceptions() const { return exceptions; }

	_FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
	_FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

	_FORCE_INLINE_ BodySW *get_island_next() const { return island_next; }
	_FORCE_INLINE_ void set_island_next(BodySW *p_next) { island_next = p_next; }

	_FORCE_INLINE_ BodySW *get_island_list_next() const { return island_list_next; }
	_FORCE_INLINE_ void set_island_list_next(BodySW *p_next) { island_list_next = p_next; }

	_FORCE_INLINE_ void add_constraint(ConstraintSW *p_constraint, int p_pos) { constraint_map[p_constraint] = p_pos; }
	_FORCE_INLINE_ void remove_constraint(ConstraintSW *p_constraint) { constraint_map.erase(p_constraint); }
	const Map<ConstraintSW *, int> &get_constraint_map() const { return constraint_map; }
	_FORCE_INLINE_ void clear_constraint_map() { constraint_map.clear(); }

	_FORCE_INLINE_ void set_omit_force_integration(bool p_omit_force_integration) { omit_force_integration = p_omit_force_integration; }
	_FORCE_INLINE_ bool get_omit_force_integration() const { return omit_force_integration; }

	_FORCE_INLINE_ void set_linear_velocity(const Vector3 &p_velocity) { linear_velocity = p_velocity; }
	_FORCE_INLINE_ Vector3 get_linear_velocity() const { return linear_velocity; }

	_FORCE_INLINE_ void set_angular_velocity(const Vector3 &p_velocity) { angular_velocity = p_velocity; }
	_FORCE_INLINE_ Vector3 get_angular_velocity() const { return angular_velocity; }

	_FORCE_INLINE_ const Vector3 &get_biased_linear_velocity() const { return biased_linear_velocity; }
	_FORCE_INLINE_ const Vector3 &get_biased_angular_velocity() const { return biased_angular_velocity; }

	_FORCE_INLINE_ void apply_impulse(const Vector3 &p_pos, const Vector3 &p_j) {

		linear_velocity += p_j * _inv_mass;
		angular_velocity += _inv_inertia_tensor.xform(p_pos.cross(p_j));
	}

	_FORCE_INLINE_ void apply_bias_impulse(const Vector3 &p_pos, const Vector3 &p_j) {

		biased_linear_velocity += p_j * _inv_mass;
		biased_angular_velocity += _inv_inertia_tensor.xform(p_pos.cross(p_j));
	}

	_FORCE_INLINE_ void apply_torque_impulse(const Vector3 &p_j) {

		angular_velocity += _inv_inertia_tensor.xform(p_j);
	}

	_FORCE_INLINE_ void add_force(const Vector3 &p_force, const Vector3 &p_pos) {

		applied_force += p_force;
		applied_torque += p_pos.cross(p_force);
	}

	void set_active(bool p_active);
	_FORCE_INLINE_ bool is_active() const { return active; }

	_FORCE_INLINE_ void wakeup() {
		if ((!get_space()) || mode == PhysicsServer::BODY_MODE_STATIC || mode == PhysicsServer::BODY_MODE_KINEMATIC)
			return;
		set_active(true);
	}

	void set_param(PhysicsServer::BodyParameter p_param, float);
	float get_param(PhysicsServer::BodyParameter p_param) const;

	void set_mode(PhysicsServer::BodyMode p_mode);
	PhysicsServer::BodyMode get_mode() const;

	void set_state(PhysicsServer::BodyState p_state, const Variant &p_variant);
	Variant get_state(PhysicsServer::BodyState p_state) const;

	void set_applied_force(const Vector3 &p_force) { applied_force = p_force; }
	Vector3 get_applied_force() const { return applied_force; }

	void set_applied_torque(const Vector3 &p_torque) { applied_torque = p_torque; }
	Vector3 get_applied_torque() const { return applied_torque; }

	_FORCE_INLINE_ void set_continuous_collision_detection(bool p_enable) { continuous_cd = p_enable; }
	_FORCE_INLINE_ bool is_continuous_collision_detection_enabled() const { return continuous_cd; }

	void set_space(SpaceSW *p_space);

	void update_inertias();

	_FORCE_INLINE_ real_t get_inv_mass() const { return _inv_mass; }
	_FORCE_INLINE_ Vector3 get_inv_inertia() const { return _inv_inertia; }
	_FORCE_INLINE_ Matrix3 get_inv_inertia_tensor() const { return _inv_inertia_tensor; }
	_FORCE_INLINE_ real_t get_friction() const { return friction; }
	_FORCE_INLINE_ Vector3 get_gravity() const { return gravity; }
	_FORCE_INLINE_ real_t get_bounce() const { return bounce; }

	_FORCE_INLINE_ void set_axis_lock(PhysicsServer::BodyAxisLock p_lock) { axis_lock = p_lock; }
	_FORCE_INLINE_ PhysicsServer::BodyAxisLock get_axis_lock() const { return axis_lock; }

	void integrate_forces(real_t p_step);
	void integrate_velocities(real_t p_step);

	_FORCE_INLINE_ Vector3 get_velocity_in_local_point(const Vector3 &rel_pos) const {

		return linear_velocity + angular_velocity.cross(rel_pos);
	}

	_FORCE_INLINE_ real_t compute_impulse_denominator(const Vector3 &p_pos, const Vector3 &p_normal) const {

		Vector3 r0 = p_pos - get_transform().origin;

		Vector3 c0 = (r0).cross(p_normal);

		Vector3 vec = (_inv_inertia_tensor.xform_inv(c0)).cross(r0);

		return _inv_mass + p_normal.dot(vec);
	}

	_FORCE_INLINE_ real_t compute_angular_impulse_denominator(const Vector3 &p_axis) const {

		return p_axis.dot(_inv_inertia_tensor.xform_inv(p_axis));
	}

	//void simulate_motion(const Transform& p_xform,real_t p_step);
	void call_queries();
	void wakeup_neighbours();

	bool sleep_test(real_t p_step);

	BodySW();
	~BodySW();
};

//add contact inline

void BodySW::add_contact(const Vector3 &p_local_pos, const Vector3 &p_local_normal, float p_depth, int p_local_shape, const Vector3 &p_collider_pos, int p_collider_shape, ObjectID p_collider_instance_id, const RID &p_collider, const Vector3 &p_collider_velocity_at_pos) {

	int c_max = contacts.size();

	if (c_max == 0)
		return;

	Contact *c = &contacts[0];

	int idx = -1;

	if (contact_count < c_max) {
		idx = contact_count++;
	} else {

		float least_depth = 1e20;
		int least_deep = -1;
		for (int i = 0; i < c_max; i++) {

			if (i == 0 || c[i].depth < least_depth) {
				least_deep = i;
				least_depth = c[i].depth;
			}
		}

		if (least_deep >= 0 && least_depth < p_depth) {

			idx = least_deep;
		}
		if (idx == -1)
			return; //none least deepe than this
	}

	c[idx].local_pos = p_local_pos;
	c[idx].local_normal = p_local_normal;
	c[idx].depth = p_depth;
	c[idx].local_shape = p_local_shape;
	c[idx].collider_pos = p_collider_pos;
	c[idx].collider_shape = p_collider_shape;
	c[idx].collider_instance_id = p_collider_instance_id;
	c[idx].collider = p_collider;
	c[idx].collider_velocity_at_pos = p_collider_velocity_at_pos;
}

class PhysicsDirectBodyStateSW : public PhysicsDirectBodyState {

	OBJ_TYPE(PhysicsDirectBodyStateSW, PhysicsDirectBodyState);

public:
	static PhysicsDirectBodyStateSW *singleton;
	BodySW *body;
	real_t step;

	virtual Vector3 get_total_gravity() const { return body->gravity; } // get gravity vector working on this body space/area
	virtual float get_total_angular_damp() const { return body->area_angular_damp; } // get density of this body space/area
	virtual float get_total_linear_damp() const { return body->area_linear_damp; } // get density of this body space/area

	virtual float get_inverse_mass() const { return body->get_inv_mass(); } // get the mass
	virtual Vector3 get_inverse_inertia() const { return body->get_inv_inertia(); } // get density of this body space
	virtual Matrix3 get_inverse_inertia_tensor() const { return body->get_inv_inertia_tensor(); } // get density of this body space

	virtual void set_linear_velocity(const Vector3 &p_velocity) { body->set_linear_velocity(p_velocity); }
	virtual Vector3 get_linear_velocity() const { return body->get_linear_velocity(); }

	virtual void set_angular_velocity(const Vector3 &p_velocity) { body->set_angular_velocity(p_velocity); }
	virtual Vector3 get_angular_velocity() const { return body->get_angular_velocity(); }

	virtual void set_transform(const Transform &p_transform) { body->set_state(PhysicsServer::BODY_STATE_TRANSFORM, p_transform); }
	virtual Transform get_transform() const { return body->get_transform(); }

	virtual void add_force(const Vector3 &p_force, const Vector3 &p_pos) { body->add_force(p_force, p_pos); }
	virtual void apply_impulse(const Vector3 &p_pos, const Vector3 &p_j) { body->apply_impulse(p_pos, p_j); }

	virtual void set_sleep_state(bool p_enable) { body->set_active(!p_enable); }
	virtual bool is_sleeping() const { return !body->is_active(); }

	virtual int get_contact_count() const { return body->contact_count; }

	virtual Vector3 get_contact_local_pos(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector3());
		return body->contacts[p_contact_idx].local_pos;
	}
	virtual Vector3 get_contact_local_normal(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector3());
		return body->contacts[p_contact_idx].local_normal;
	}
	virtual int get_contact_local_shape(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, -1);
		return body->contacts[p_contact_idx].local_shape;
	}

	virtual RID get_contact_collider(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, RID());
		return body->contacts[p_contact_idx].collider;
	}
	virtual Vector3 get_contact_collider_pos(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector3());
		return body->contacts[p_contact_idx].collider_pos;
	}
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, 0);
		return body->contacts[p_contact_idx].collider_instance_id;
	}
	virtual int get_contact_collider_shape(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, 0);
		return body->contacts[p_contact_idx].collider_shape;
	}
	virtual Vector3 get_contact_collider_velocity_at_pos(int p_contact_idx) const {
		ERR_FAIL_INDEX_V(p_contact_idx, body->contact_count, Vector3());
		return body->contacts[p_contact_idx].collider_velocity_at_pos;
	}

	virtual PhysicsDirectSpaceState *get_space_state();

	virtual real_t get_step() const { return step; }
	PhysicsDirectBodyStateSW() {
		singleton = this;
		body = NULL;
	}
};

#endif // BODY__SW_H
