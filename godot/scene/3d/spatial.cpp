/*************************************************************************/
/*  spatial.cpp                                                          */
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
#include "spatial.h"

#include "message_queue.h"
#include "scene/main/viewport.h"
#include "scene/scene_string_names.h"

/*

 possible algorithms:

 Algorithm 1: (current)

 definition of invalidation: global is invalid

 1) If a node sets a LOCAL, it produces an invalidation of everything above
    a) If above is invalid, don't keep invalidating upwards
 2) If a node sets a GLOBAL, it is converted to LOCAL (and forces validation of everything pending below)

 drawback: setting/reading globals is useful and used very very often, and using affine inverses is slow

---

 Algorithm 2: (no longer current)

 definition of invalidation: NONE dirty, LOCAL dirty, GLOBAL dirty

 1) If a node sets a LOCAL, it must climb the tree and set it as GLOBAL dirty
    a) marking GLOBALs as dirty up all the tree must be done always
 2) If a node sets a GLOBAL, it marks local as dirty, and that's all?

 //is clearing the dirty state correct in this case?

 drawback: setting a local down the tree forces many tree walks often

--

future: no idea

 */

SpatialGizmo::SpatialGizmo() {
}

void Spatial::_notify_dirty() {

	if (!data.ignore_notification && !xform_change.in_list()) {

		get_tree()->xform_change_list.add(&xform_change);
	}
}

void Spatial::_update_local_transform() const {

	data.local_transform.basis.set_euler(data.rotation);
	data.local_transform.basis.scale(data.scale);

	data.dirty &= ~DIRTY_LOCAL;
}
void Spatial::_propagate_transform_changed(Spatial *p_origin) {

	if (!is_inside_tree()) {
		return;
	}

	//	if (data.dirty&DIRTY_GLOBAL)
	//		return; //already dirty

	data.children_lock++;

	for (List<Spatial *>::Element *E = data.children.front(); E; E = E->next()) {

		if (E->get()->data.toplevel_active)
			continue; //don't propagate to a toplevel
		E->get()->_propagate_transform_changed(p_origin);
	}

	if (!data.ignore_notification && !xform_change.in_list()) {

		get_tree()->xform_change_list.add(&xform_change);
	}
	data.dirty |= DIRTY_GLOBAL;

	data.children_lock--;
}

void Spatial::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {

			Node *p = get_parent();
			if (p)
				data.parent = p->cast_to<Spatial>();

			if (data.parent)
				data.C = data.parent->data.children.push_back(this);
			else
				data.C = NULL;

			if (data.toplevel && !get_tree()->is_editor_hint()) {

				if (data.parent) {
					data.local_transform = data.parent->get_global_transform() * get_transform();
					data.dirty = DIRTY_VECTORS; //global is always dirty upon entering a scene
				}
				data.toplevel_active = true;
			}

			data.dirty |= DIRTY_GLOBAL; //global is always dirty upon entering a scene
			_notify_dirty();

			notification(NOTIFICATION_ENTER_WORLD);

		} break;
		case NOTIFICATION_EXIT_TREE: {

			notification(NOTIFICATION_EXIT_WORLD, true);
			if (xform_change.in_list())
				get_tree()->xform_change_list.remove(&xform_change);
			if (data.C)
				data.parent->data.children.erase(data.C);
			data.parent = NULL;
			data.C = NULL;
			data.toplevel_active = false;
		} break;
		case NOTIFICATION_ENTER_WORLD: {

			data.inside_world = true;
			data.viewport = NULL;
			Node *parent = get_parent();
			while (parent && !data.viewport) {
				data.viewport = parent->cast_to<Viewport>();
				parent = parent->get_parent();
			}

			ERR_FAIL_COND(!data.viewport);

			if (get_script_instance()) {

				Variant::CallError err;
				get_script_instance()->call_multilevel(SceneStringNames::get_singleton()->_enter_world, NULL, 0);
			}
#ifdef TOOLS_ENABLED
			if (get_tree()->is_editor_hint()) {

				//				get_scene()->call_group(SceneMainLoop::GROUP_CALL_REALTIME,SceneStringNames::get_singleton()->_spatial_editor_group,SceneStringNames::get_singleton()->_request_gizmo,this);
				get_tree()->call_group(0, SceneStringNames::get_singleton()->_spatial_editor_group, SceneStringNames::get_singleton()->_request_gizmo, this);
				if (!data.gizmo_disabled) {

					if (data.gizmo.is_valid())
						data.gizmo->create();
				}
			}
#endif

		} break;
		case NOTIFICATION_EXIT_WORLD: {

#ifdef TOOLS_ENABLED
			if (data.gizmo.is_valid()) {
				data.gizmo->free();
			}
#endif

			if (get_script_instance()) {

				Variant::CallError err;
				get_script_instance()->call_multilevel(SceneStringNames::get_singleton()->_exit_world, NULL, 0);
			}

			data.viewport = NULL;
			data.inside_world = false;

		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {

#ifdef TOOLS_ENABLED
			if (data.gizmo.is_valid()) {
				data.gizmo->transform();
			}
#endif
		} break;

		default: {}
	}
}

void Spatial::set_transform(const Transform &p_transform) {

	data.local_transform = p_transform;
	data.dirty |= DIRTY_VECTORS;
	_change_notify("transform/translation");
	_change_notify("transform/rotation");
	_change_notify("transform/scale");
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

void Spatial::set_global_transform(const Transform &p_transform) {

	Transform xform =
			(data.parent && !data.toplevel_active) ?
					data.parent->get_global_transform().affine_inverse() * p_transform :
					p_transform;

	set_transform(xform);
}

Transform Spatial::get_transform() const {

	if (data.dirty & DIRTY_LOCAL) {

		_update_local_transform();
	}

	return data.local_transform;
}
Transform Spatial::get_global_transform() const {

	ERR_FAIL_COND_V(!is_inside_tree(), Transform());

	if (data.dirty & DIRTY_GLOBAL) {

		if (data.dirty & DIRTY_LOCAL) {

			_update_local_transform();
		}

		if (data.parent && !data.toplevel_active) {

			data.global_transform = data.parent->get_global_transform() * data.local_transform;
		} else {

			data.global_transform = data.local_transform;
		}

		data.dirty &= ~DIRTY_GLOBAL;
	}

	return data.global_transform;
}
#if 0
void Spatial::add_child_notify(Node *p_child) {
/*
	Spatial *s=p_child->cast_to<Spatial>();
	if (!s)
		return;

	ERR_FAIL_COND(data.children_lock>0);

	s->data.dirty=DIRTY_GLOBAL; // don't allow global transform to be valid
	s->data.parent=this;
	data.children.push_back(s);
	s->data.C=data.children.back();
*/
}

void Spatial::remove_child_notify(Node *p_child) {
/*
	Spatial *s=p_child->cast_to<Spatial>();
	if (!s)
		return;

	ERR_FAIL_COND(data.children_lock>0);

	if (s->data.C)
		data.children.erase(s->data.C);
	s->data.parent=NULL;
	s->data.C=NULL;
*/
}
#endif

Spatial *Spatial::get_parent_spatial() const {

	return data.parent;
}

Transform Spatial::get_relative_transform(const Node *p_parent) const {

	if (p_parent == this)
		return Transform();

	ERR_FAIL_COND_V(!data.parent, Transform());

	if (p_parent == data.parent)
		return get_transform();
	else
		return data.parent->get_relative_transform(p_parent) * get_transform();
}

void Spatial::set_translation(const Vector3 &p_translation) {

	data.local_transform.origin = p_translation;
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

void Spatial::set_rotation(const Vector3 &p_euler_rad) {

	if (data.dirty & DIRTY_VECTORS) {
		data.scale = data.local_transform.basis.get_scale();
		data.dirty &= ~DIRTY_VECTORS;
	}

	data.rotation = p_euler_rad;
	data.dirty |= DIRTY_LOCAL;
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

void Spatial::set_rotation_deg(const Vector3 &p_euler_deg) {

	set_rotation(p_euler_deg * Math_PI / 180.0);
}

void Spatial::_set_rotation_deg(const Vector3 &p_euler_deg) {

	WARN_PRINT("Deprecated method Spatial._set_rotation_deg(): This method was renamed to set_rotation_deg. Please adapt your code accordingly, as the old method will be obsoleted.");
	set_rotation_deg(p_euler_deg);
}

void Spatial::set_scale(const Vector3 &p_scale) {

	if (data.dirty & DIRTY_VECTORS) {
		data.rotation = data.local_transform.basis.get_euler();
		data.dirty &= ~DIRTY_VECTORS;
	}

	data.scale = p_scale;
	data.dirty |= DIRTY_LOCAL;
	_propagate_transform_changed(this);
	if (data.notify_local_transform) {
		notification(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	}
}

Vector3 Spatial::get_translation() const {

	return data.local_transform.origin;
}

Vector3 Spatial::get_rotation() const {

	if (data.dirty & DIRTY_VECTORS) {
		data.scale = data.local_transform.basis.get_scale();
		data.rotation = data.local_transform.basis.get_euler();
		data.dirty &= ~DIRTY_VECTORS;
	}

	return data.rotation;
}

Vector3 Spatial::get_rotation_deg() const {

	return get_rotation() * 180.0 / Math_PI;
}

// Kept for compatibility after rename to set_rotd.
// Could be removed after a couple releases.
Vector3 Spatial::_get_rotation_deg() const {

	WARN_PRINT("Deprecated method Spatial._get_rotation_deg(): This method was renamed to get_rotation_deg. Please adapt your code accordingly, as the old method will be obsoleted.");
	return get_rotation_deg();
}

Vector3 Spatial::get_scale() const {

	if (data.dirty & DIRTY_VECTORS) {
		data.scale = data.local_transform.basis.get_scale();
		data.rotation = data.local_transform.basis.get_euler();
		data.dirty &= ~DIRTY_VECTORS;
	}

	return data.scale;
}

void Spatial::update_gizmo() {

#ifdef TOOLS_ENABLED
	if (!is_inside_world())
		return;
	if (!data.gizmo.is_valid())
		return;
	if (data.gizmo_dirty)
		return;
	data.gizmo_dirty = true;
	MessageQueue::get_singleton()->push_call(this, "_update_gizmo");
#endif
}

void Spatial::set_gizmo(const Ref<SpatialGizmo> &p_gizmo) {

#ifdef TOOLS_ENABLED

	if (data.gizmo_disabled)
		return;
	if (data.gizmo.is_valid() && is_inside_world())
		data.gizmo->free();
	data.gizmo = p_gizmo;
	if (data.gizmo.is_valid() && is_inside_world()) {

		data.gizmo->create();
		data.gizmo->redraw();
		data.gizmo->transform();
	}

#endif
}

Ref<SpatialGizmo> Spatial::get_gizmo() const {

#ifdef TOOLS_ENABLED

	return data.gizmo;
#else

	return Ref<SpatialGizmo>();
#endif
}

#ifdef TOOLS_ENABLED

void Spatial::_update_gizmo() {

	data.gizmo_dirty = false;
	if (data.gizmo.is_valid()) {
		if (is_visible())
			data.gizmo->redraw();
		else
			data.gizmo->clear();
	}
}

void Spatial::set_disable_gizmo(bool p_enabled) {

	data.gizmo_disabled = p_enabled;
	if (!p_enabled && data.gizmo.is_valid())
		data.gizmo = Ref<SpatialGizmo>();
}

#endif

void Spatial::set_as_toplevel(bool p_enabled) {

	if (data.toplevel == p_enabled)
		return;
	if (is_inside_tree() && !get_tree()->is_editor_hint()) {

		if (p_enabled)
			set_transform(get_global_transform());
		else if (data.parent)
			set_transform(data.parent->get_global_transform().affine_inverse() * get_global_transform());

		data.toplevel = p_enabled;
		data.toplevel_active = p_enabled;

	} else {
		data.toplevel = p_enabled;
	}
}

bool Spatial::is_set_as_toplevel() const {

	return data.toplevel;
}

Ref<World> Spatial::get_world() const {

	ERR_FAIL_COND_V(!is_inside_world(), Ref<World>());
	return data.viewport->find_world();
}

#ifdef TOOLS_ENABLED
void Spatial::set_import_transform(const Transform &p_transform) {
	data.import_transform = p_transform;
}

Transform Spatial::get_import_transform() const {

	return data.import_transform;
}
#endif

void Spatial::_propagate_visibility_changed() {

	notification(NOTIFICATION_VISIBILITY_CHANGED);
	emit_signal(SceneStringNames::get_singleton()->visibility_changed);
	_change_notify("visibility/visible");
#ifdef TOOLS_ENABLED
	if (data.gizmo.is_valid())
		_update_gizmo();
#endif

	for (List<Spatial *>::Element *E = data.children.front(); E; E = E->next()) {

		Spatial *c = E->get();
		if (!c || !c->data.visible)
			continue;
		c->_propagate_visibility_changed();
	}
}

void Spatial::show() {

	if (data.visible)
		return;

	data.visible = true;

	if (!is_inside_tree())
		return;

	_propagate_visibility_changed();
}

void Spatial::hide() {

	if (!data.visible)
		return;

	data.visible = false;

	if (!is_inside_tree())
		return;

	_propagate_visibility_changed();
}

bool Spatial::is_visible() const {

	const Spatial *s = this;

	while (s) {
		if (!s->data.visible) {
			return false;
		}
		s = s->data.parent;
	}

	return true;
}

bool Spatial::is_hidden() const {

	return !data.visible;
}

void Spatial::set_hidden(bool p_hidden) {

	if (data.visible != p_hidden) {
		return;
	}

	_set_visible_(!p_hidden);
}

void Spatial::_set_visible_(bool p_visible) {

	if (p_visible)
		show();
	else
		hide();
}

bool Spatial::_is_visible_() const {

	return !is_hidden();
}

void Spatial::rotate(const Vector3 &p_normal, float p_radians) {

	Transform t = get_transform();
	t.basis.rotate(p_normal, p_radians);
	set_transform(t);
}

void Spatial::rotate_x(float p_radians) {

	Transform t = get_transform();
	t.basis.rotate(Vector3(1, 0, 0), p_radians);
	set_transform(t);
}

void Spatial::rotate_y(float p_radians) {

	Transform t = get_transform();
	t.basis.rotate(Vector3(0, 1, 0), p_radians);
	set_transform(t);
}
void Spatial::rotate_z(float p_radians) {

	Transform t = get_transform();
	t.basis.rotate(Vector3(0, 0, 1), p_radians);
	set_transform(t);
}

void Spatial::translate(const Vector3 &p_offset) {

	Transform t = get_transform();
	t.translate(p_offset);
	set_transform(t);
}

void Spatial::scale(const Vector3 &p_ratio) {

	Transform t = get_transform();
	t.basis.scale(p_ratio);
	set_transform(t);
}
void Spatial::global_rotate(const Vector3 &p_normal, float p_radians) {

	Matrix3 rotation(p_normal, p_radians);
	Transform t = get_global_transform();
	t.basis = rotation * t.basis;
	set_global_transform(t);
}
void Spatial::global_translate(const Vector3 &p_offset) {
	Transform t = get_global_transform();
	t.origin += p_offset;
	set_global_transform(t);
}

void Spatial::orthonormalize() {

	Transform t = get_transform();
	t.orthonormalize();
	set_transform(t);
}

void Spatial::set_identity() {

	set_transform(Transform());
}

void Spatial::look_at(const Vector3 &p_target, const Vector3 &p_up_normal) {

	Transform lookat;
	lookat.origin = get_global_transform().origin;
	if (lookat.origin == p_target) {
		ERR_EXPLAIN("Node origin and target are in the same position, look_at() failed");
		ERR_FAIL();
	}

	if (p_up_normal.cross(p_target - lookat.origin) == Vector3()) {
		ERR_EXPLAIN("Up vector and direction between node origin and target are aligned, look_at() failed");
		ERR_FAIL();
	}
	lookat = lookat.looking_at(p_target, p_up_normal);
	set_global_transform(lookat);
}

void Spatial::look_at_from_pos(const Vector3 &p_pos, const Vector3 &p_target, const Vector3 &p_up_normal) {

	Transform lookat;
	lookat.origin = p_pos;
	lookat = lookat.looking_at(p_target, p_up_normal);
	set_global_transform(lookat);
}

void Spatial::set_notify_local_transform(bool p_enable) {
	data.notify_local_transform = p_enable;
}

bool Spatial::is_local_transform_notification_enabled() const {
	return data.notify_local_transform;
}

void Spatial::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_transform", "local"), &Spatial::set_transform);
	ObjectTypeDB::bind_method(_MD("get_transform"), &Spatial::get_transform);
	ObjectTypeDB::bind_method(_MD("set_translation", "translation"), &Spatial::set_translation);
	ObjectTypeDB::bind_method(_MD("get_translation"), &Spatial::get_translation);
	ObjectTypeDB::bind_method(_MD("set_rotation", "rotation_rad"), &Spatial::set_rotation);
	ObjectTypeDB::bind_method(_MD("get_rotation"), &Spatial::get_rotation);
	ObjectTypeDB::bind_method(_MD("set_rotation_deg", "rotation_deg"), &Spatial::set_rotation_deg);
	ObjectTypeDB::bind_method(_MD("get_rotation_deg"), &Spatial::get_rotation_deg);
	ObjectTypeDB::bind_method(_MD("set_scale", "scale"), &Spatial::set_scale);
	ObjectTypeDB::bind_method(_MD("get_scale"), &Spatial::get_scale);
	ObjectTypeDB::bind_method(_MD("set_global_transform", "global"), &Spatial::set_global_transform);
	ObjectTypeDB::bind_method(_MD("get_global_transform"), &Spatial::get_global_transform);
	ObjectTypeDB::bind_method(_MD("get_parent_spatial"), &Spatial::get_parent_spatial);
	ObjectTypeDB::bind_method(_MD("set_ignore_transform_notification", "enabled"), &Spatial::set_ignore_transform_notification);
	ObjectTypeDB::bind_method(_MD("set_as_toplevel", "enable"), &Spatial::set_as_toplevel);
	ObjectTypeDB::bind_method(_MD("is_set_as_toplevel"), &Spatial::is_set_as_toplevel);
	ObjectTypeDB::bind_method(_MD("get_world:World"), &Spatial::get_world);

	// TODO: Obsolete those two methods (old name) properly (GH-4397)
	ObjectTypeDB::bind_method(_MD("_set_rotation_deg", "rotation_deg"), &Spatial::_set_rotation_deg);
	ObjectTypeDB::bind_method(_MD("_get_rotation_deg"), &Spatial::_get_rotation_deg);

#ifdef TOOLS_ENABLED
	ObjectTypeDB::bind_method(_MD("_update_gizmo"), &Spatial::_update_gizmo);
	ObjectTypeDB::bind_method(_MD("_set_import_transform"), &Spatial::set_import_transform);
	ObjectTypeDB::bind_method(_MD("_get_import_transform"), &Spatial::get_import_transform);
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM, "_import_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), _SCS("_set_import_transform"), _SCS("_get_import_transform"));
#endif

	ObjectTypeDB::bind_method(_MD("update_gizmo"), &Spatial::update_gizmo);
	ObjectTypeDB::bind_method(_MD("set_gizmo", "gizmo:SpatialGizmo"), &Spatial::set_gizmo);
	ObjectTypeDB::bind_method(_MD("get_gizmo:SpatialGizmo"), &Spatial::get_gizmo);

	ObjectTypeDB::bind_method(_MD("show"), &Spatial::show);
	ObjectTypeDB::bind_method(_MD("hide"), &Spatial::hide);
	ObjectTypeDB::bind_method(_MD("is_visible"), &Spatial::is_visible);
	ObjectTypeDB::bind_method(_MD("is_hidden"), &Spatial::is_hidden);
	ObjectTypeDB::bind_method(_MD("set_hidden", "hidden"), &Spatial::set_hidden);

	ObjectTypeDB::bind_method(_MD("_set_visible_"), &Spatial::_set_visible_);
	ObjectTypeDB::bind_method(_MD("_is_visible_"), &Spatial::_is_visible_);

	ObjectTypeDB::bind_method(_MD("set_notify_local_transform", "enable"), &Spatial::set_notify_local_transform);
	ObjectTypeDB::bind_method(_MD("is_local_transform_notification_enabled"), &Spatial::is_local_transform_notification_enabled);

	void rotate(const Vector3 &p_normal, float p_radians);
	void rotate_x(float p_radians);
	void rotate_y(float p_radians);
	void rotate_z(float p_radians);
	void translate(const Vector3 &p_offset);
	void scale(const Vector3 &p_ratio);
	void global_rotate(const Vector3 &p_normal, float p_radians);
	void global_translate(const Vector3 &p_offset);

	ObjectTypeDB::bind_method(_MD("rotate", "normal", "radians"), &Spatial::rotate);
	ObjectTypeDB::bind_method(_MD("global_rotate", "normal", "radians"), &Spatial::global_rotate);
	ObjectTypeDB::bind_method(_MD("rotate_x", "radians"), &Spatial::rotate_x);
	ObjectTypeDB::bind_method(_MD("rotate_y", "radians"), &Spatial::rotate_y);
	ObjectTypeDB::bind_method(_MD("rotate_z", "radians"), &Spatial::rotate_z);
	ObjectTypeDB::bind_method(_MD("translate", "offset"), &Spatial::translate);
	ObjectTypeDB::bind_method(_MD("global_translate", "offset"), &Spatial::global_translate);
	ObjectTypeDB::bind_method(_MD("orthonormalize"), &Spatial::orthonormalize);
	ObjectTypeDB::bind_method(_MD("set_identity"), &Spatial::set_identity);

	ObjectTypeDB::bind_method(_MD("look_at", "target", "up"), &Spatial::look_at);
	ObjectTypeDB::bind_method(_MD("look_at_from_pos", "pos", "target", "up"), &Spatial::look_at_from_pos);

	BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
	BIND_CONSTANT(NOTIFICATION_ENTER_WORLD);
	BIND_CONSTANT(NOTIFICATION_EXIT_WORLD);
	BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);

	//ADD_PROPERTY( PropertyInfo(Variant::TRANSFORM,"transform/global",PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR ), _SCS("set_global_transform"), _SCS("get_global_transform") );
	ADD_PROPERTYNZ(PropertyInfo(Variant::TRANSFORM, "transform/local", PROPERTY_HINT_NONE, ""), _SCS("set_transform"), _SCS("get_transform"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "transform/translation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), _SCS("set_translation"), _SCS("get_translation"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "transform/rotation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), _SCS("set_rotation_deg"), _SCS("get_rotation_deg"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "transform/rotation_rad", PROPERTY_HINT_NONE, "", 0), _SCS("set_rotation"), _SCS("get_rotation"));
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "transform/scale", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), _SCS("set_scale"), _SCS("get_scale"));
	ADD_PROPERTYNO(PropertyInfo(Variant::BOOL, "visibility/visible"), _SCS("_set_visible_"), _SCS("_is_visible_"));
	//ADD_PROPERTY( PropertyInfo(Variant::TRANSFORM,"transform/local"), _SCS("set_transform"), _SCS("get_transform") );

	ADD_SIGNAL(MethodInfo("visibility_changed"));
}

Spatial::Spatial() :
		xform_change(this) {

	data.dirty = DIRTY_NONE;
	data.children_lock = 0;

	data.ignore_notification = false;
	data.toplevel = false;
	data.toplevel_active = false;
	data.scale = Vector3(1, 1, 1);
	data.viewport = NULL;
	data.inside_world = false;
	data.visible = true;
#ifdef TOOLS_ENABLED
	data.gizmo_disabled = false;
	data.gizmo_dirty = false;
#endif
	data.notify_local_transform = false;
	data.parent = NULL;
	data.C = NULL;
}

Spatial::~Spatial() {
}
