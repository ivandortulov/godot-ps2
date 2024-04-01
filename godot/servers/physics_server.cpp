/*************************************************************************/
/*  physics_server.cpp                                                   */
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
#include "physics_server.h"
#include "print_string.h"
PhysicsServer *PhysicsServer::singleton = NULL;

void PhysicsDirectBodyState::integrate_forces() {

	real_t step = get_step();
	Vector3 lv = get_linear_velocity();
	lv += get_total_gravity() * step;

	Vector3 av = get_angular_velocity();

	float linear_damp = 1.0 - step * get_total_linear_damp();

	if (linear_damp < 0) // reached zero in the given time
		linear_damp = 0;

	float angular_damp = 1.0 - step * get_total_angular_damp();

	if (angular_damp < 0) // reached zero in the given time
		angular_damp = 0;

	lv *= linear_damp;
	av *= angular_damp;

	set_linear_velocity(lv);
	set_angular_velocity(av);
}

Object *PhysicsDirectBodyState::get_contact_collider_object(int p_contact_idx) const {

	ObjectID objid = get_contact_collider_id(p_contact_idx);
	Object *obj = ObjectDB::get_instance(objid);
	return obj;
}

PhysicsServer *PhysicsServer::get_singleton() {

	return singleton;
}

void PhysicsDirectBodyState::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_total_gravity"), &PhysicsDirectBodyState::get_total_gravity);
	ObjectTypeDB::bind_method(_MD("get_total_linear_damp"), &PhysicsDirectBodyState::get_total_linear_damp);
	ObjectTypeDB::bind_method(_MD("get_total_angular_damp"), &PhysicsDirectBodyState::get_total_angular_damp);

	ObjectTypeDB::bind_method(_MD("get_inverse_mass"), &PhysicsDirectBodyState::get_inverse_mass);
	ObjectTypeDB::bind_method(_MD("get_inverse_inertia"), &PhysicsDirectBodyState::get_inverse_inertia);

	ObjectTypeDB::bind_method(_MD("set_linear_velocity", "velocity"), &PhysicsDirectBodyState::set_linear_velocity);
	ObjectTypeDB::bind_method(_MD("get_linear_velocity"), &PhysicsDirectBodyState::get_linear_velocity);

	ObjectTypeDB::bind_method(_MD("set_angular_velocity", "velocity"), &PhysicsDirectBodyState::set_angular_velocity);
	ObjectTypeDB::bind_method(_MD("get_angular_velocity"), &PhysicsDirectBodyState::get_angular_velocity);

	ObjectTypeDB::bind_method(_MD("set_transform", "transform"), &PhysicsDirectBodyState::set_transform);
	ObjectTypeDB::bind_method(_MD("get_transform"), &PhysicsDirectBodyState::get_transform);

	ObjectTypeDB::bind_method(_MD("add_force", "force", "pos"), &PhysicsDirectBodyState::add_force);
	ObjectTypeDB::bind_method(_MD("apply_impulse", "pos", "j"), &PhysicsDirectBodyState::apply_impulse);

	ObjectTypeDB::bind_method(_MD("set_sleep_state", "enabled"), &PhysicsDirectBodyState::set_sleep_state);
	ObjectTypeDB::bind_method(_MD("is_sleeping"), &PhysicsDirectBodyState::is_sleeping);

	ObjectTypeDB::bind_method(_MD("get_contact_count"), &PhysicsDirectBodyState::get_contact_count);

	ObjectTypeDB::bind_method(_MD("get_contact_local_pos", "contact_idx"), &PhysicsDirectBodyState::get_contact_local_pos);
	ObjectTypeDB::bind_method(_MD("get_contact_local_normal", "contact_idx"), &PhysicsDirectBodyState::get_contact_local_normal);
	ObjectTypeDB::bind_method(_MD("get_contact_local_shape", "contact_idx"), &PhysicsDirectBodyState::get_contact_local_shape);
	ObjectTypeDB::bind_method(_MD("get_contact_collider", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider);
	ObjectTypeDB::bind_method(_MD("get_contact_collider_pos", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider_pos);
	ObjectTypeDB::bind_method(_MD("get_contact_collider_id", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider_id);
	ObjectTypeDB::bind_method(_MD("get_contact_collider_object", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider_object);
	ObjectTypeDB::bind_method(_MD("get_contact_collider_shape", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider_shape);
	ObjectTypeDB::bind_method(_MD("get_contact_collider_velocity_at_pos", "contact_idx"), &PhysicsDirectBodyState::get_contact_collider_velocity_at_pos);
	ObjectTypeDB::bind_method(_MD("get_step"), &PhysicsDirectBodyState::get_step);
	ObjectTypeDB::bind_method(_MD("integrate_forces"), &PhysicsDirectBodyState::integrate_forces);
	ObjectTypeDB::bind_method(_MD("get_space_state:PhysicsDirectSpaceState"), &PhysicsDirectBodyState::get_space_state);
}

PhysicsDirectBodyState::PhysicsDirectBodyState() {}

///////////////////////////////////////////////////////

void PhysicsShapeQueryParameters::set_shape(const RES &p_shape) {

	ERR_FAIL_COND(p_shape.is_null());
	shape = p_shape->get_rid();
}

void PhysicsShapeQueryParameters::set_shape_rid(const RID &p_shape) {

	shape = p_shape;
}

RID PhysicsShapeQueryParameters::get_shape_rid() const {

	return shape;
}

void PhysicsShapeQueryParameters::set_transform(const Transform &p_transform) {

	transform = p_transform;
}
Transform PhysicsShapeQueryParameters::get_transform() const {

	return transform;
}

void PhysicsShapeQueryParameters::set_margin(float p_margin) {

	margin = p_margin;
}

float PhysicsShapeQueryParameters::get_margin() const {

	return margin;
}

void PhysicsShapeQueryParameters::set_layer_mask(int p_layer_mask) {

	layer_mask = p_layer_mask;
}
int PhysicsShapeQueryParameters::get_layer_mask() const {

	return layer_mask;
}

void PhysicsShapeQueryParameters::set_object_type_mask(int p_object_type_mask) {

	object_type_mask = p_object_type_mask;
}
int PhysicsShapeQueryParameters::get_object_type_mask() const {

	return object_type_mask;
}
void PhysicsShapeQueryParameters::set_exclude(const Vector<RID> &p_exclude) {

	exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++)
		exclude.insert(p_exclude[i]);
}

Vector<RID> PhysicsShapeQueryParameters::get_exclude() const {

	Vector<RID> ret;
	ret.resize(exclude.size());
	int idx = 0;
	for (Set<RID>::Element *E = exclude.front(); E; E = E->next()) {
		ret[idx] = E->get();
	}
	return ret;
}

void PhysicsShapeQueryParameters::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_shape", "shape:Shape"), &PhysicsShapeQueryParameters::set_shape);
	ObjectTypeDB::bind_method(_MD("set_shape_rid", "shape"), &PhysicsShapeQueryParameters::set_shape_rid);
	ObjectTypeDB::bind_method(_MD("get_shape_rid"), &PhysicsShapeQueryParameters::get_shape_rid);

	ObjectTypeDB::bind_method(_MD("set_transform", "transform"), &PhysicsShapeQueryParameters::set_transform);
	ObjectTypeDB::bind_method(_MD("get_transform"), &PhysicsShapeQueryParameters::get_transform);

	ObjectTypeDB::bind_method(_MD("set_margin", "margin"), &PhysicsShapeQueryParameters::set_margin);
	ObjectTypeDB::bind_method(_MD("get_margin"), &PhysicsShapeQueryParameters::get_margin);

	ObjectTypeDB::bind_method(_MD("set_layer_mask", "layer_mask"), &PhysicsShapeQueryParameters::set_layer_mask);
	ObjectTypeDB::bind_method(_MD("get_layer_mask"), &PhysicsShapeQueryParameters::get_layer_mask);

	ObjectTypeDB::bind_method(_MD("set_object_type_mask", "object_type_mask"), &PhysicsShapeQueryParameters::set_object_type_mask);
	ObjectTypeDB::bind_method(_MD("get_object_type_mask"), &PhysicsShapeQueryParameters::get_object_type_mask);

	ObjectTypeDB::bind_method(_MD("set_exclude", "exclude"), &PhysicsShapeQueryParameters::set_exclude);
	ObjectTypeDB::bind_method(_MD("get_exclude"), &PhysicsShapeQueryParameters::get_exclude);
}

PhysicsShapeQueryParameters::PhysicsShapeQueryParameters() {

	margin = 0;
	layer_mask = 0x7FFFFFFF;
	object_type_mask = PhysicsDirectSpaceState::TYPE_MASK_COLLISION;
}

/////////////////////////////////////

/*
Variant PhysicsDirectSpaceState::_intersect_shape(const RID& p_shape, const Transform& p_xform,int p_result_max,const Vector<RID>& p_exclude,uint32_t p_collision_mask) {



	ERR_FAIL_INDEX_V(p_result_max,4096,Variant());
	if (p_result_max<=0)
		return Variant();

	Set<RID> exclude;
	for(int i=0;i<p_exclude.size();i++)
		exclude.insert(p_exclude[i]);

	ShapeResult *res=(ShapeResult*)alloca(p_result_max*sizeof(ShapeResult));

	int rc = intersect_shape(p_shape,p_xform,0,res,p_result_max,exclude);

	if (rc==0)
		return Variant();

	Ref<PhysicsShapeQueryResult>  result = memnew( PhysicsShapeQueryResult );
	result->result.resize(rc);
	for(int i=0;i<rc;i++)
		result->result[i]=res[i];

	return result;

}
*/

Dictionary PhysicsDirectSpaceState::_intersect_ray(const Vector3 &p_from, const Vector3 &p_to, const Vector<RID> &p_exclude, uint32_t p_layers, uint32_t p_object_type_mask) {

	RayResult inters;
	Set<RID> exclude;
	for (int i = 0; i < p_exclude.size(); i++)
		exclude.insert(p_exclude[i]);

	bool res = intersect_ray(p_from, p_to, inters, exclude, p_layers, p_object_type_mask);

	if (!res)
		return Dictionary(true);

	Dictionary d(true);
	d["position"] = inters.position;
	d["normal"] = inters.normal;
	d["collider_id"] = inters.collider_id;
	d["collider"] = inters.collider;
	d["shape"] = inters.shape;
	d["rid"] = inters.rid;

	return d;
}

Array PhysicsDirectSpaceState::_intersect_shape(const Ref<PhysicsShapeQueryParameters> &psq, int p_max_results) {

	Vector<ShapeResult> sr;
	sr.resize(p_max_results);
	int rc = intersect_shape(psq->shape, psq->transform, psq->margin, sr.ptr(), sr.size(), psq->exclude, psq->layer_mask, psq->object_type_mask);
	Array ret;
	ret.resize(rc);
	for (int i = 0; i < rc; i++) {

		Dictionary d;
		d["rid"] = sr[i].rid;
		d["collider_id"] = sr[i].collider_id;
		d["collider"] = sr[i].collider;
		d["shape"] = sr[i].shape;
		ret[i] = d;
	}

	return ret;
}

Array PhysicsDirectSpaceState::_cast_motion(const Ref<PhysicsShapeQueryParameters> &psq, const Vector3 &p_motion) {

	float closest_safe, closest_unsafe;
	bool res = cast_motion(psq->shape, psq->transform, p_motion, psq->margin, closest_safe, closest_unsafe, psq->exclude, psq->layer_mask, psq->object_type_mask);
	if (!res)
		return Array();
	Array ret(true);
	ret.resize(2);
	ret[0] = closest_safe;
	ret[1] = closest_unsafe;
	return ret;
}
Array PhysicsDirectSpaceState::_collide_shape(const Ref<PhysicsShapeQueryParameters> &psq, int p_max_results) {

	Vector<Vector3> ret;
	ret.resize(p_max_results * 2);
	int rc = 0;
	bool res = collide_shape(psq->shape, psq->transform, psq->margin, ret.ptr(), p_max_results, rc, psq->exclude, psq->layer_mask, psq->object_type_mask);
	if (!res)
		return Array();
	Array r;
	r.resize(rc * 2);
	for (int i = 0; i < rc * 2; i++)
		r[i] = ret[i];
	return r;
}
Dictionary PhysicsDirectSpaceState::_get_rest_info(const Ref<PhysicsShapeQueryParameters> &psq) {

	ShapeRestInfo sri;

	bool res = rest_info(psq->shape, psq->transform, psq->margin, &sri, psq->exclude, psq->layer_mask, psq->object_type_mask);
	Dictionary r(true);
	if (!res)
		return r;

	r["point"] = sri.point;
	r["normal"] = sri.normal;
	r["rid"] = sri.rid;
	r["collider_id"] = sri.collider_id;
	r["shape"] = sri.shape;
	r["linear_velocity"] = sri.linear_velocity;

	return r;
}

PhysicsDirectSpaceState::PhysicsDirectSpaceState() {
}

void PhysicsDirectSpaceState::_bind_methods() {

	//	ObjectTypeDB::bind_method(_MD("intersect_ray","from","to","exclude","umask"),&PhysicsDirectSpaceState::_intersect_ray,DEFVAL(Array()),DEFVAL(0));
	//	ObjectTypeDB::bind_method(_MD("intersect_shape:PhysicsShapeQueryResult","shape","xform","result_max","exclude","umask"),&PhysicsDirectSpaceState::_intersect_shape,DEFVAL(Array()),DEFVAL(0));

	ObjectTypeDB::bind_method(_MD("intersect_ray:Dictionary", "from", "to", "exclude", "layer_mask", "type_mask"), &PhysicsDirectSpaceState::_intersect_ray, DEFVAL(Array()), DEFVAL(0x7FFFFFFF), DEFVAL(TYPE_MASK_COLLISION));
	ObjectTypeDB::bind_method(_MD("intersect_shape", "shape:PhysicsShapeQueryParameters", "max_results"), &PhysicsDirectSpaceState::_intersect_shape, DEFVAL(32));
	ObjectTypeDB::bind_method(_MD("cast_motion", "shape:PhysicsShapeQueryParameters", "motion"), &PhysicsDirectSpaceState::_cast_motion);
	ObjectTypeDB::bind_method(_MD("collide_shape", "shape:PhysicsShapeQueryParameters", "max_results"), &PhysicsDirectSpaceState::_collide_shape, DEFVAL(32));
	ObjectTypeDB::bind_method(_MD("get_rest_info", "shape:PhysicsShapeQueryParameters"), &PhysicsDirectSpaceState::_get_rest_info);

	BIND_CONSTANT(TYPE_MASK_STATIC_BODY);
	BIND_CONSTANT(TYPE_MASK_KINEMATIC_BODY);
	BIND_CONSTANT(TYPE_MASK_RIGID_BODY);
	BIND_CONSTANT(TYPE_MASK_CHARACTER_BODY);
	BIND_CONSTANT(TYPE_MASK_AREA);
	BIND_CONSTANT(TYPE_MASK_COLLISION);
}

int PhysicsShapeQueryResult::get_result_count() const {

	return result.size();
}
RID PhysicsShapeQueryResult::get_result_rid(int p_idx) const {

	return result[p_idx].rid;
}
ObjectID PhysicsShapeQueryResult::get_result_object_id(int p_idx) const {

	return result[p_idx].collider_id;
}
Object *PhysicsShapeQueryResult::get_result_object(int p_idx) const {

	return result[p_idx].collider;
}
int PhysicsShapeQueryResult::get_result_object_shape(int p_idx) const {

	return result[p_idx].shape;
}

PhysicsShapeQueryResult::PhysicsShapeQueryResult() {
}

void PhysicsShapeQueryResult::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_result_count"), &PhysicsShapeQueryResult::get_result_count);
	ObjectTypeDB::bind_method(_MD("get_result_rid", "idx"), &PhysicsShapeQueryResult::get_result_rid);
	ObjectTypeDB::bind_method(_MD("get_result_object_id", "idx"), &PhysicsShapeQueryResult::get_result_object_id);
	ObjectTypeDB::bind_method(_MD("get_result_object", "idx"), &PhysicsShapeQueryResult::get_result_object);
	ObjectTypeDB::bind_method(_MD("get_result_object_shape", "idx"), &PhysicsShapeQueryResult::get_result_object_shape);
}

///////////////////////////////////////

void PhysicsServer::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("shape_create", "type"), &PhysicsServer::shape_create);
	ObjectTypeDB::bind_method(_MD("shape_set_data", "shape", "data"), &PhysicsServer::shape_set_data);

	ObjectTypeDB::bind_method(_MD("shape_get_type", "shape"), &PhysicsServer::shape_get_type);
	ObjectTypeDB::bind_method(_MD("shape_get_data", "shape"), &PhysicsServer::shape_get_data);

	ObjectTypeDB::bind_method(_MD("space_create"), &PhysicsServer::space_create);
	ObjectTypeDB::bind_method(_MD("space_set_active", "space", "active"), &PhysicsServer::space_set_active);
	ObjectTypeDB::bind_method(_MD("space_is_active", "space"), &PhysicsServer::space_is_active);
	ObjectTypeDB::bind_method(_MD("space_set_param", "space", "param", "value"), &PhysicsServer::space_set_param);
	ObjectTypeDB::bind_method(_MD("space_get_param", "space", "param"), &PhysicsServer::space_get_param);
	ObjectTypeDB::bind_method(_MD("space_get_direct_state:PhysicsDirectSpaceState", "space"), &PhysicsServer::space_get_direct_state);

	ObjectTypeDB::bind_method(_MD("area_create"), &PhysicsServer::area_create);
	ObjectTypeDB::bind_method(_MD("area_set_space", "area", "space"), &PhysicsServer::area_set_space);
	ObjectTypeDB::bind_method(_MD("area_get_space", "area"), &PhysicsServer::area_get_space);

	ObjectTypeDB::bind_method(_MD("area_set_space_override_mode", "area", "mode"), &PhysicsServer::area_set_space_override_mode);
	ObjectTypeDB::bind_method(_MD("area_get_space_override_mode", "area"), &PhysicsServer::area_get_space_override_mode);

	ObjectTypeDB::bind_method(_MD("area_add_shape", "area", "shape", "transform"), &PhysicsServer::area_add_shape, DEFVAL(Transform()));
	ObjectTypeDB::bind_method(_MD("area_set_shape", "area", "shape_idx", "shape"), &PhysicsServer::area_set_shape);
	ObjectTypeDB::bind_method(_MD("area_set_shape_transform", "area", "shape_idx", "transform"), &PhysicsServer::area_set_shape_transform);

	ObjectTypeDB::bind_method(_MD("area_get_shape_count", "area"), &PhysicsServer::area_get_shape_count);
	ObjectTypeDB::bind_method(_MD("area_get_shape", "area", "shape_idx"), &PhysicsServer::area_get_shape);
	ObjectTypeDB::bind_method(_MD("area_get_shape_transform", "area", "shape_idx"), &PhysicsServer::area_get_shape_transform);

	ObjectTypeDB::bind_method(_MD("area_remove_shape", "area", "shape_idx"), &PhysicsServer::area_remove_shape);
	ObjectTypeDB::bind_method(_MD("area_clear_shapes", "area"), &PhysicsServer::area_clear_shapes);

	ObjectTypeDB::bind_method(_MD("area_set_layer_mask", "area", "mask"), &PhysicsServer::area_set_layer_mask);
	ObjectTypeDB::bind_method(_MD("area_set_collision_mask", "area", "mask"), &PhysicsServer::area_set_collision_mask);

	ObjectTypeDB::bind_method(_MD("area_set_param", "area", "param", "value"), &PhysicsServer::area_set_param);
	ObjectTypeDB::bind_method(_MD("area_set_transform", "area", "transform"), &PhysicsServer::area_set_transform);

	ObjectTypeDB::bind_method(_MD("area_get_param", "area", "param"), &PhysicsServer::area_get_param);
	ObjectTypeDB::bind_method(_MD("area_get_transform", "area"), &PhysicsServer::area_get_transform);

	ObjectTypeDB::bind_method(_MD("area_attach_object_instance_ID", "area", "id"), &PhysicsServer::area_attach_object_instance_ID);
	ObjectTypeDB::bind_method(_MD("area_get_object_instance_ID", "area"), &PhysicsServer::area_get_object_instance_ID);

	ObjectTypeDB::bind_method(_MD("area_set_monitor_callback", "area", "receiver", "method"), &PhysicsServer::area_set_monitor_callback);

	ObjectTypeDB::bind_method(_MD("area_set_ray_pickable", "area", "enable"), &PhysicsServer::area_set_ray_pickable);
	ObjectTypeDB::bind_method(_MD("area_is_ray_pickable", "area"), &PhysicsServer::area_is_ray_pickable);

	ObjectTypeDB::bind_method(_MD("body_create", "mode", "init_sleeping"), &PhysicsServer::body_create, DEFVAL(BODY_MODE_RIGID), DEFVAL(false));

	ObjectTypeDB::bind_method(_MD("body_set_space", "body", "space"), &PhysicsServer::body_set_space);
	ObjectTypeDB::bind_method(_MD("body_get_space", "body"), &PhysicsServer::body_get_space);

	ObjectTypeDB::bind_method(_MD("body_set_mode", "body", "mode"), &PhysicsServer::body_set_mode);
	ObjectTypeDB::bind_method(_MD("body_get_mode", "body"), &PhysicsServer::body_get_mode);

	ObjectTypeDB::bind_method(_MD("body_set_layer_mask", "body", "mask"), &PhysicsServer::body_set_layer_mask);
	ObjectTypeDB::bind_method(_MD("body_get_layer_mask", "body"), &PhysicsServer::body_get_layer_mask);

	ObjectTypeDB::bind_method(_MD("body_set_collision_mask", "body", "mask"), &PhysicsServer::body_set_collision_mask);
	ObjectTypeDB::bind_method(_MD("body_get_collision_mask", "body"), &PhysicsServer::body_get_collision_mask);

	ObjectTypeDB::bind_method(_MD("body_add_shape", "body", "shape", "transform"), &PhysicsServer::body_add_shape, DEFVAL(Transform()));
	ObjectTypeDB::bind_method(_MD("body_set_shape", "body", "shape_idx", "shape"), &PhysicsServer::body_set_shape);
	ObjectTypeDB::bind_method(_MD("body_set_shape_transform", "body", "shape_idx", "transform"), &PhysicsServer::body_set_shape_transform);

	ObjectTypeDB::bind_method(_MD("body_get_shape_count", "body"), &PhysicsServer::body_get_shape_count);
	ObjectTypeDB::bind_method(_MD("body_get_shape", "body", "shape_idx"), &PhysicsServer::body_get_shape);
	ObjectTypeDB::bind_method(_MD("body_get_shape_transform", "body", "shape_idx"), &PhysicsServer::body_get_shape_transform);

	ObjectTypeDB::bind_method(_MD("body_remove_shape", "body", "shape_idx"), &PhysicsServer::body_remove_shape);
	ObjectTypeDB::bind_method(_MD("body_clear_shapes", "body"), &PhysicsServer::body_clear_shapes);

	ObjectTypeDB::bind_method(_MD("body_attach_object_instance_ID", "body", "id"), &PhysicsServer::body_attach_object_instance_ID);
	ObjectTypeDB::bind_method(_MD("body_get_object_instance_ID", "body"), &PhysicsServer::body_get_object_instance_ID);

	ObjectTypeDB::bind_method(_MD("body_set_enable_continuous_collision_detection", "body", "enable"), &PhysicsServer::body_set_enable_continuous_collision_detection);
	ObjectTypeDB::bind_method(_MD("body_is_continuous_collision_detection_enabled", "body"), &PhysicsServer::body_is_continuous_collision_detection_enabled);

	//ObjectTypeDB::bind_method(_MD("body_set_user_flags","flags""),&PhysicsServer::body_set_shape,DEFVAL(Transform));
	//ObjectTypeDB::bind_method(_MD("body_get_user_flags","body","shape_idx","shape"),&PhysicsServer::body_get_shape);

	ObjectTypeDB::bind_method(_MD("body_set_param", "body", "param", "value"), &PhysicsServer::body_set_param);
	ObjectTypeDB::bind_method(_MD("body_get_param", "body", "param"), &PhysicsServer::body_get_param);

	ObjectTypeDB::bind_method(_MD("body_set_state", "body", "state", "value"), &PhysicsServer::body_set_state);
	ObjectTypeDB::bind_method(_MD("body_get_state", "body", "state"), &PhysicsServer::body_get_state);

	ObjectTypeDB::bind_method(_MD("body_apply_impulse", "body", "pos", "impulse"), &PhysicsServer::body_apply_impulse);
	ObjectTypeDB::bind_method(_MD("body_set_axis_velocity", "body", "axis_velocity"), &PhysicsServer::body_set_axis_velocity);

	ObjectTypeDB::bind_method(_MD("body_set_axis_lock", "body", "axis"), &PhysicsServer::body_set_axis_lock);
	ObjectTypeDB::bind_method(_MD("body_get_axis_lock", "body"), &PhysicsServer::body_get_axis_lock);

	ObjectTypeDB::bind_method(_MD("body_add_collision_exception", "body", "excepted_body"), &PhysicsServer::body_add_collision_exception);
	ObjectTypeDB::bind_method(_MD("body_remove_collision_exception", "body", "excepted_body"), &PhysicsServer::body_remove_collision_exception);
	//	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions)=0;

	ObjectTypeDB::bind_method(_MD("body_set_max_contacts_reported", "body", "amount"), &PhysicsServer::body_set_max_contacts_reported);
	ObjectTypeDB::bind_method(_MD("body_get_max_contacts_reported", "body"), &PhysicsServer::body_get_max_contacts_reported);

	ObjectTypeDB::bind_method(_MD("body_set_omit_force_integration", "body", "enable"), &PhysicsServer::body_set_omit_force_integration);
	ObjectTypeDB::bind_method(_MD("body_is_omitting_force_integration", "body"), &PhysicsServer::body_is_omitting_force_integration);

	ObjectTypeDB::bind_method(_MD("body_set_force_integration_callback", "body", "receiver", "method", "userdata"), &PhysicsServer::body_set_force_integration_callback, DEFVAL(Variant()));

	ObjectTypeDB::bind_method(_MD("body_set_ray_pickable", "body", "enable"), &PhysicsServer::body_set_ray_pickable);
	ObjectTypeDB::bind_method(_MD("body_is_ray_pickable", "body"), &PhysicsServer::body_is_ray_pickable);

	/* JOINT API */

	BIND_CONSTANT(JOINT_PIN);
	BIND_CONSTANT(JOINT_HINGE);
	BIND_CONSTANT(JOINT_SLIDER);
	BIND_CONSTANT(JOINT_CONE_TWIST);
	BIND_CONSTANT(JOINT_6DOF);

	ObjectTypeDB::bind_method(_MD("joint_create_pin", "body_A", "local_A", "body_B", "local_B"), &PhysicsServer::joint_create_pin);
	ObjectTypeDB::bind_method(_MD("pin_joint_set_param", "joint", "param", "value"), &PhysicsServer::pin_joint_set_param);
	ObjectTypeDB::bind_method(_MD("pin_joint_get_param", "joint", "param"), &PhysicsServer::pin_joint_get_param);

	ObjectTypeDB::bind_method(_MD("pin_joint_set_local_A", "joint", "local_A"), &PhysicsServer::pin_joint_set_local_A);
	ObjectTypeDB::bind_method(_MD("pin_joint_get_local_A", "joint"), &PhysicsServer::pin_joint_get_local_A);

	ObjectTypeDB::bind_method(_MD("pin_joint_set_local_B", "joint", "local_B"), &PhysicsServer::pin_joint_set_local_B);
	ObjectTypeDB::bind_method(_MD("pin_joint_get_local_B", "joint"), &PhysicsServer::pin_joint_get_local_B);

	BIND_CONSTANT(PIN_JOINT_BIAS);
	BIND_CONSTANT(PIN_JOINT_DAMPING);
	BIND_CONSTANT(PIN_JOINT_IMPULSE_CLAMP);

	BIND_CONSTANT(HINGE_JOINT_BIAS);
	BIND_CONSTANT(HINGE_JOINT_LIMIT_UPPER);
	BIND_CONSTANT(HINGE_JOINT_LIMIT_LOWER);
	BIND_CONSTANT(HINGE_JOINT_LIMIT_BIAS);
	BIND_CONSTANT(HINGE_JOINT_LIMIT_SOFTNESS);
	BIND_CONSTANT(HINGE_JOINT_LIMIT_RELAXATION);
	BIND_CONSTANT(HINGE_JOINT_MOTOR_TARGET_VELOCITY);
	BIND_CONSTANT(HINGE_JOINT_MOTOR_MAX_IMPULSE);
	BIND_CONSTANT(HINGE_JOINT_FLAG_USE_LIMIT);
	BIND_CONSTANT(HINGE_JOINT_FLAG_ENABLE_MOTOR);

	ObjectTypeDB::bind_method(_MD("joint_create_hinge", "body_A", "hinge_A", "body_B", "hinge_B"), &PhysicsServer::joint_create_hinge);

	ObjectTypeDB::bind_method(_MD("hinge_joint_set_param", "joint", "param", "value"), &PhysicsServer::hinge_joint_set_param);
	ObjectTypeDB::bind_method(_MD("hinge_joint_get_param", "joint", "param"), &PhysicsServer::hinge_joint_get_param);

	ObjectTypeDB::bind_method(_MD("hinge_joint_set_flag", "joint", "flag", "enabled"), &PhysicsServer::hinge_joint_set_flag);
	ObjectTypeDB::bind_method(_MD("hinge_joint_get_flag", "joint", "flag"), &PhysicsServer::hinge_joint_get_flag);

	ObjectTypeDB::bind_method(_MD("joint_create_slider", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer::joint_create_slider);

	ObjectTypeDB::bind_method(_MD("slider_joint_set_param", "joint", "param", "value"), &PhysicsServer::slider_joint_set_param);
	ObjectTypeDB::bind_method(_MD("slider_joint_get_param", "joint", "param"), &PhysicsServer::slider_joint_get_param);

	BIND_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_UPPER);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_LOWER);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_DAMPING);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_DAMPING);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING);

	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_UPPER);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_LOWER);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_DAMPING);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_DAMPING);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION);
	BIND_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING);
	BIND_CONSTANT(SLIDER_JOINT_MAX);

	ObjectTypeDB::bind_method(_MD("joint_create_cone_twist", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer::joint_create_cone_twist);

	ObjectTypeDB::bind_method(_MD("cone_twist_joint_set_param", "joint", "param", "value"), &PhysicsServer::cone_twist_joint_set_param);
	ObjectTypeDB::bind_method(_MD("cone_twist_joint_get_param", "joint", "param"), &PhysicsServer::cone_twist_joint_get_param);

	BIND_CONSTANT(CONE_TWIST_JOINT_SWING_SPAN);
	BIND_CONSTANT(CONE_TWIST_JOINT_TWIST_SPAN);
	BIND_CONSTANT(CONE_TWIST_JOINT_BIAS);
	BIND_CONSTANT(CONE_TWIST_JOINT_SOFTNESS);
	BIND_CONSTANT(CONE_TWIST_JOINT_RELAXATION);

	BIND_CONSTANT(G6DOF_JOINT_LINEAR_LOWER_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_LINEAR_UPPER_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS);
	BIND_CONSTANT(G6DOF_JOINT_LINEAR_RESTITUTION);
	BIND_CONSTANT(G6DOF_JOINT_LINEAR_DAMPING);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_LOWER_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_UPPER_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_DAMPING);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_RESTITUTION);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_FORCE_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_ERP);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY);
	BIND_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT);

	BIND_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT);
	BIND_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_MOTOR);

	ObjectTypeDB::bind_method(_MD("joint_get_type", "joint"), &PhysicsServer::joint_get_type);

	ObjectTypeDB::bind_method(_MD("joint_set_solver_priority", "joint", "priority"), &PhysicsServer::joint_set_solver_priority);
	ObjectTypeDB::bind_method(_MD("joint_get_solver_priority", "joint"), &PhysicsServer::joint_get_solver_priority);

	ObjectTypeDB::bind_method(_MD("joint_create_generic_6dof", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer::joint_create_generic_6dof);

	ObjectTypeDB::bind_method(_MD("generic_6dof_joint_set_param", "joint", "axis", "param", "value"), &PhysicsServer::generic_6dof_joint_set_param);
	ObjectTypeDB::bind_method(_MD("generic_6dof_joint_get_param", "joint", "axis", "param"), &PhysicsServer::generic_6dof_joint_get_param);

	ObjectTypeDB::bind_method(_MD("generic_6dof_joint_set_flag", "joint", "axis", "flag", "enable"), &PhysicsServer::generic_6dof_joint_set_flag);
	ObjectTypeDB::bind_method(_MD("generic_6dof_joint_get_flag", "joint", "axis", "flag"), &PhysicsServer::generic_6dof_joint_get_flag);

	/*
	ObjectTypeDB::bind_method(_MD("joint_set_param","joint","param","value"),&PhysicsServer::joint_set_param);
	ObjectTypeDB::bind_method(_MD("joint_get_param","joint","param"),&PhysicsServer::joint_get_param);

	ObjectTypeDB::bind_method(_MD("pin_joint_create","anchor","body_a","body_b"),&PhysicsServer::pin_joint_create,DEFVAL(RID()));
	ObjectTypeDB::bind_method(_MD("groove_joint_create","groove1_a","groove2_a","anchor_b","body_a","body_b"),&PhysicsServer::groove_joint_create,DEFVAL(RID()),DEFVAL(RID()));
	ObjectTypeDB::bind_method(_MD("damped_spring_joint_create","anchor_a","anchor_b","body_a","body_b"),&PhysicsServer::damped_spring_joint_create,DEFVAL(RID()));

	ObjectTypeDB::bind_method(_MD("damped_string_joint_set_param","joint","param","value"),&PhysicsServer::damped_string_joint_set_param,DEFVAL(RID()));
	ObjectTypeDB::bind_method(_MD("damped_string_joint_get_param","joint","param"),&PhysicsServer::damped_string_joint_get_param);

	ObjectTypeDB::bind_method(_MD("joint_get_type","joint"),&PhysicsServer::joint_get_type);
*/
	ObjectTypeDB::bind_method(_MD("free_rid", "rid"), &PhysicsServer::free);

	ObjectTypeDB::bind_method(_MD("set_active", "active"), &PhysicsServer::set_active);

	//	ObjectTypeDB::bind_method(_MD("init"),&PhysicsServer::init);
	//	ObjectTypeDB::bind_method(_MD("step"),&PhysicsServer::step);
	//	ObjectTypeDB::bind_method(_MD("sync"),&PhysicsServer::sync);
	//ObjectTypeDB::bind_method(_MD("flush_queries"),&PhysicsServer::flush_queries);

	ObjectTypeDB::bind_method(_MD("get_process_info", "process_info"), &PhysicsServer::get_process_info);

	BIND_CONSTANT(SHAPE_PLANE);
	BIND_CONSTANT(SHAPE_RAY);
	BIND_CONSTANT(SHAPE_SPHERE);
	BIND_CONSTANT(SHAPE_BOX);
	BIND_CONSTANT(SHAPE_CAPSULE);
	BIND_CONSTANT(SHAPE_CONVEX_POLYGON);
	BIND_CONSTANT(SHAPE_CONCAVE_POLYGON);
	BIND_CONSTANT(SHAPE_HEIGHTMAP);
	BIND_CONSTANT(SHAPE_CUSTOM);

	BIND_CONSTANT(AREA_PARAM_GRAVITY);
	BIND_CONSTANT(AREA_PARAM_GRAVITY_VECTOR);
	BIND_CONSTANT(AREA_PARAM_GRAVITY_IS_POINT);
	BIND_CONSTANT(AREA_PARAM_GRAVITY_DISTANCE_SCALE);
	BIND_CONSTANT(AREA_PARAM_GRAVITY_POINT_ATTENUATION);
	BIND_CONSTANT(AREA_PARAM_LINEAR_DAMP);
	BIND_CONSTANT(AREA_PARAM_ANGULAR_DAMP);
	BIND_CONSTANT(AREA_PARAM_PRIORITY);

	BIND_CONSTANT(AREA_SPACE_OVERRIDE_DISABLED);
	BIND_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE);
	BIND_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE_REPLACE);
	BIND_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE);
	BIND_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE_COMBINE);

	BIND_CONSTANT(BODY_MODE_STATIC);
	BIND_CONSTANT(BODY_MODE_KINEMATIC);
	BIND_CONSTANT(BODY_MODE_RIGID);
	BIND_CONSTANT(BODY_MODE_CHARACTER);

	BIND_CONSTANT(BODY_PARAM_BOUNCE);
	BIND_CONSTANT(BODY_PARAM_FRICTION);
	BIND_CONSTANT(BODY_PARAM_MASS);
	BIND_CONSTANT(BODY_PARAM_GRAVITY_SCALE);
	BIND_CONSTANT(BODY_PARAM_ANGULAR_DAMP);
	BIND_CONSTANT(BODY_PARAM_LINEAR_DAMP);
	BIND_CONSTANT(BODY_PARAM_MAX);

	BIND_CONSTANT(BODY_STATE_TRANSFORM);
	BIND_CONSTANT(BODY_STATE_LINEAR_VELOCITY);
	BIND_CONSTANT(BODY_STATE_ANGULAR_VELOCITY);
	BIND_CONSTANT(BODY_STATE_SLEEPING);
	BIND_CONSTANT(BODY_STATE_CAN_SLEEP);
	/*
	BIND_CONSTANT( JOINT_PIN );
	BIND_CONSTANT( JOINT_GROOVE );
	BIND_CONSTANT( JOINT_DAMPED_SPRING );

	BIND_CONSTANT( DAMPED_STRING_REST_LENGTH );
	BIND_CONSTANT( DAMPED_STRING_STIFFNESS );
	BIND_CONSTANT( DAMPED_STRING_DAMPING );
*/
	//	BIND_CONSTANT( TYPE_BODY );
	//	BIND_CONSTANT( TYPE_AREA );

	BIND_CONSTANT(AREA_BODY_ADDED);
	BIND_CONSTANT(AREA_BODY_REMOVED);

	BIND_CONSTANT(INFO_ACTIVE_OBJECTS);
	BIND_CONSTANT(INFO_COLLISION_PAIRS);
	BIND_CONSTANT(INFO_ISLAND_COUNT);
}

PhysicsServer::PhysicsServer() {

	ERR_FAIL_COND(singleton != NULL);
	singleton = this;
}

PhysicsServer::~PhysicsServer() {

	singleton = NULL;
}
