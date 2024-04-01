/*************************************************************************/
/*  spatial_editor_gizmos.cpp                                            */
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
#include "spatial_editor_gizmos.h"
#include "geometry.h"
#include "quick_hull.h"
#include "scene/3d/camera.h"
#include "scene/resources/box_shape.h"
#include "scene/resources/capsule_shape.h"
#include "scene/resources/convex_polygon_shape.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/ray_shape.h"
#include "scene/resources/sphere_shape.h"
#include "scene/resources/surface_tool.h"

// Keep small children away from this file.
// It's so ugly it will eat them alive

#define HANDLE_HALF_SIZE 0.05

void EditorSpatialGizmo::clear() {

	for (int i = 0; i < instances.size(); i++) {

		if (instances[i].instance.is_valid())
			VS::get_singleton()->free(instances[i].instance);
	}

	billboard_handle = false;
	collision_segments.clear();
	collision_mesh = Ref<TriangleMesh>();
	instances.clear();
	handles.clear();
	secondary_handles.clear();
}

void EditorSpatialGizmo::redraw() {

	if (get_script_instance() && get_script_instance()->has_method("redraw"))
		get_script_instance()->call("redraw");
}

void EditorSpatialGizmo::Instance::create_instance(Spatial *p_base) {

	instance = VS::get_singleton()->instance_create2(mesh->get_rid(), p_base->get_world()->get_scenario());
	VS::get_singleton()->instance_attach_object_instance_ID(instance, p_base->get_instance_ID());
	if (billboard)
		VS::get_singleton()->instance_geometry_set_flag(instance, VS::INSTANCE_FLAG_BILLBOARD, true);
	if (unscaled)
		VS::get_singleton()->instance_geometry_set_flag(instance, VS::INSTANCE_FLAG_DEPH_SCALE, true);
	if (skeleton.is_valid())
		VS::get_singleton()->instance_attach_skeleton(instance, skeleton);
	if (extra_margin)
		VS::get_singleton()->instance_set_extra_visibility_margin(instance, 1);
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(instance, VS::SHADOW_CASTING_SETTING_OFF);
	VS::get_singleton()->instance_geometry_set_flag(instance, VS::INSTANCE_FLAG_RECEIVE_SHADOWS, false);
	VS::get_singleton()->instance_set_layer_mask(instance, 1 << SpatialEditorViewport::GIZMO_EDIT_LAYER); //gizmos are 26
}

void EditorSpatialGizmo::add_mesh(const Ref<Mesh> &p_mesh, bool p_billboard, const RID &p_skeleton) {

	ERR_FAIL_COND(!spatial_node);
	Instance ins;

	ins.billboard = p_billboard;
	ins.mesh = p_mesh;
	ins.skeleton = p_skeleton;
	if (valid) {
		ins.create_instance(spatial_node);
		VS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}

	instances.push_back(ins);
}

void EditorSpatialGizmo::add_lines(const Vector<Vector3> &p_lines, const Ref<Material> &p_material, bool p_billboard) {

	ERR_FAIL_COND(!spatial_node);
	Instance ins;

	Ref<Mesh> mesh = memnew(Mesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);

	a[Mesh::ARRAY_VERTEX] = p_lines;

	DVector<Color> color;
	color.resize(p_lines.size());
	{
		DVector<Color>::Write w = color.write();
		for (int i = 0; i < p_lines.size(); i++) {
			if (is_selected())
				w[i] = Color(1, 1, 1, 0.6);
			else
				w[i] = Color(1, 1, 1, 0.25);
		}
	}

	a[Mesh::ARRAY_COLOR] = color;

	mesh->add_surface(Mesh::PRIMITIVE_LINES, a);
	mesh->surface_set_material(0, p_material);

	if (p_billboard) {
		float md = 0;
		for (int i = 0; i < p_lines.size(); i++) {

			md = MAX(0, p_lines[i].length());
		}
		if (md) {
			mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
		}
	}

	ins.billboard = p_billboard;
	ins.mesh = mesh;
	if (valid) {
		ins.create_instance(spatial_node);
		VS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}

	instances.push_back(ins);
}

void EditorSpatialGizmo::add_unscaled_billboard(const Ref<Material> &p_material, float p_scale) {

	ERR_FAIL_COND(!spatial_node);
	Instance ins;

	Vector<Vector3> vs;
	Vector<Vector2> uv;

	vs.push_back(Vector3(-p_scale, p_scale, 0));
	vs.push_back(Vector3(p_scale, p_scale, 0));
	vs.push_back(Vector3(p_scale, -p_scale, 0));
	vs.push_back(Vector3(-p_scale, -p_scale, 0));

	uv.push_back(Vector2(1, 0));
	uv.push_back(Vector2(0, 0));
	uv.push_back(Vector2(0, 1));
	uv.push_back(Vector2(1, 1));

	Ref<Mesh> mesh = memnew(Mesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[Mesh::ARRAY_VERTEX] = vs;
	a[Mesh::ARRAY_TEX_UV] = uv;
	mesh->add_surface(Mesh::PRIMITIVE_TRIANGLE_FAN, a);
	mesh->surface_set_material(0, p_material);

	if (true) {
		float md = 0;
		for (int i = 0; i < vs.size(); i++) {

			md = MAX(0, vs[i].length());
		}
		if (md) {
			mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
		}
	}

	ins.mesh = mesh;
	ins.unscaled = true;
	ins.billboard = true;
	if (valid) {
		ins.create_instance(spatial_node);
		VS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}

	instances.push_back(ins);
}

void EditorSpatialGizmo::add_collision_triangles(const Ref<TriangleMesh> &p_tmesh) {

	collision_mesh = p_tmesh;
}

void EditorSpatialGizmo::add_collision_segments(const Vector<Vector3> &p_lines) {

	int from = collision_segments.size();
	collision_segments.resize(from + p_lines.size());
	for (int i = 0; i < p_lines.size(); i++) {

		collision_segments[from + i] = p_lines[i];
	}
}

void EditorSpatialGizmo::add_handles(const Vector<Vector3> &p_handles, bool p_billboard, bool p_secondary) {

	billboard_handle = p_billboard;

	if (!is_selected())
		return;

	ERR_FAIL_COND(!spatial_node);

	ERR_FAIL_COND(!spatial_node);
	Instance ins;

	Ref<Mesh> mesh = memnew(Mesh);
#if 1

	Array a;
	a.resize(VS::ARRAY_MAX);
	a[VS::ARRAY_VERTEX] = p_handles;
	DVector<Color> colors;
	{
		colors.resize(p_handles.size());
		DVector<Color>::Write w = colors.write();
		for (int i = 0; i < p_handles.size(); i++) {

			Color col(1, 1, 1, 1);
			if (SpatialEditor::get_singleton()->get_over_gizmo_handle() != i)
				col = Color(0.9, 0.9, 0.9, 0.9);
			w[i] = col;
		}
	}
	a[VS::ARRAY_COLOR] = colors;
	mesh->add_surface(Mesh::PRIMITIVE_POINTS, a);
	mesh->surface_set_material(0, SpatialEditorGizmos::singleton->handle2_material);

	if (p_billboard) {
		float md = 0;
		for (int i = 0; i < p_handles.size(); i++) {

			md = MAX(0, p_handles[i].length());
		}
		if (md) {
			mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
		}
	}

#else
	for (int ih = 0; ih < p_handles.size(); ih++) {

		Vector<Vector3> vertices;
		Vector<Vector3> normals;

		int vtx_idx = 0;

#define ADD_VTX(m_idx)                                                           \
	vertices.push_back((face_points[m_idx] * HANDLE_HALF_SIZE + p_handles[ih])); \
	normals.push_back(normal_points[m_idx]);                                     \
	vtx_idx++;

		for (int i = 0; i < 6; i++) {

			Vector3 face_points[4];
			Vector3 normal_points[4];
			float uv_points[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };

			for (int j = 0; j < 4; j++) {

				float v[3];
				v[0] = 1.0;
				v[1] = 1 - 2 * ((j >> 1) & 1);
				v[2] = v[1] * (1 - 2 * (j & 1));

				for (int k = 0; k < 3; k++) {

					if (i < 3)
						face_points[j][(i + k) % 3] = v[k] * (i >= 3 ? -1 : 1);
					else
						face_points[3 - j][(i + k) % 3] = v[k] * (i >= 3 ? -1 : 1);
				}
				normal_points[j] = Vector3();
				normal_points[j][i % 3] = (i >= 3 ? -1 : 1);
			}
			//tri 1
			ADD_VTX(0);
			ADD_VTX(1);
			ADD_VTX(2);
			//tri 2
			ADD_VTX(2);
			ADD_VTX(3);
			ADD_VTX(0);
		}

		Array d;
		d.resize(VS::ARRAY_MAX);
		d[VisualServer::ARRAY_NORMAL] = normals;
		d[VisualServer::ARRAY_VERTEX] = vertices;

		mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, d);
		mesh->surface_set_material(ih, SpatialEditorGizmos::singleton->handle_material);
	}
#endif
	ins.mesh = mesh;
	ins.billboard = p_billboard;
	ins.extra_margin = true;
	if (valid) {
		ins.create_instance(spatial_node);
		VS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}
	instances.push_back(ins);
	if (!p_secondary) {
		int chs = handles.size();
		handles.resize(chs + p_handles.size());
		for (int i = 0; i < p_handles.size(); i++) {
			handles[i + chs] = p_handles[i];
		}
	} else {

		int chs = secondary_handles.size();
		secondary_handles.resize(chs + p_handles.size());
		for (int i = 0; i < p_handles.size(); i++) {
			secondary_handles[i + chs] = p_handles[i];
		}
	}
}

void EditorSpatialGizmo::set_spatial_node(Spatial *p_node) {

	ERR_FAIL_NULL(p_node);
	spatial_node = p_node;
}

bool EditorSpatialGizmo::intersect_frustum(const Camera *p_camera, const Vector<Plane> &p_frustum) {

	ERR_FAIL_COND_V(!spatial_node, false);
	ERR_FAIL_COND_V(!valid, false);

	if (collision_segments.size()) {

		const Plane *p = p_frustum.ptr();
		int fc = p_frustum.size();

		int vc = collision_segments.size();
		const Vector3 *vptr = collision_segments.ptr();
		Transform t = spatial_node->get_global_transform();

		for (int i = 0; i < vc / 2; i++) {

			Vector3 a = t.xform(vptr[i * 2 + 0]);
			Vector3 b = t.xform(vptr[i * 2 + 1]);

			bool any_out = false;
			for (int j = 0; j < fc; j++) {

				if (p[j].distance_to(a) > 0 && p[j].distance_to(b) > 0) {

					any_out = true;
					break;
				}
			}

			if (!any_out)
				return true;
		}

		return false;
	}

	return false;
}

bool EditorSpatialGizmo::intersect_ray(const Camera *p_camera, const Point2 &p_point, Vector3 &r_pos, Vector3 &r_normal, int *r_gizmo_handle, bool p_sec_first) {

	ERR_FAIL_COND_V(!spatial_node, false);
	ERR_FAIL_COND_V(!valid, false);

	if (r_gizmo_handle) {

		Transform t = spatial_node->get_global_transform();
		t.orthonormalize();
		if (billboard_handle) {
			t.set_look_at(t.origin, t.origin + p_camera->get_transform().basis.get_axis(2), p_camera->get_transform().basis.get_axis(1));
		}
		Transform ti = t.affine_inverse();

		float min_d = 1e20;
		int idx = -1;

		for (int i = 0; i < secondary_handles.size(); i++) {

			Vector3 hpos = t.xform(secondary_handles[i]);
			Vector2 p = p_camera->unproject_position(hpos);
			if (p.distance_to(p_point) < SpatialEditorGizmos::singleton->handle_t->get_width() * 0.6) {

				real_t dp = p_camera->get_transform().origin.distance_to(hpos);
				if (dp < min_d) {

					r_pos = t.xform(hpos);
					r_normal = p_camera->get_transform().basis.get_axis(2);
					min_d = dp;
					idx = i + handles.size();
				}
			}
		}

		if (p_sec_first && idx != -1) {

			*r_gizmo_handle = idx;
			return true;
		}

		min_d = 1e20;

		for (int i = 0; i < handles.size(); i++) {

			Vector3 hpos = t.xform(handles[i]);
			Vector2 p = p_camera->unproject_position(hpos);
			if (p.distance_to(p_point) < SpatialEditorGizmos::singleton->handle_t->get_width() * 0.6) {

				real_t dp = p_camera->get_transform().origin.distance_to(hpos);
				if (dp < min_d) {

					r_pos = t.xform(hpos);
					r_normal = p_camera->get_transform().basis.get_axis(2);
					min_d = dp;
					idx = i;
				}
			}
		}

		if (idx >= 0) {
			*r_gizmo_handle = idx;
			return true;
		}
	}

	if (collision_segments.size()) {

		Plane camp(p_camera->get_transform().origin, (-p_camera->get_transform().basis.get_axis(2)).normalized());

		int vc = collision_segments.size();
		const Vector3 *vptr = collision_segments.ptr();
		Transform t = spatial_node->get_global_transform();
		if (billboard_handle) {
			t.set_look_at(t.origin, t.origin + p_camera->get_transform().basis.get_axis(2), p_camera->get_transform().basis.get_axis(1));
		}

		Vector3 cp;
		float cpd = 1e20;

		for (int i = 0; i < vc / 2; i++) {

			Vector3 a = t.xform(vptr[i * 2 + 0]);
			Vector3 b = t.xform(vptr[i * 2 + 1]);
			Vector2 s[2];
			s[0] = p_camera->unproject_position(a);
			s[1] = p_camera->unproject_position(b);

			Vector2 p = Geometry::get_closest_point_to_segment_2d(p_point, s);

			float pd = p.distance_to(p_point);

			if (pd < cpd) {

				float d = s[0].distance_to(s[1]);
				Vector3 tcp;
				if (d > 0) {

					float d2 = s[0].distance_to(p) / d;
					tcp = a + (b - a) * d2;

				} else {
					tcp = a;
				}

				if (camp.distance_to(tcp) < p_camera->get_znear())
					continue;
				cp = tcp;
				cpd = pd;
			}
		}

		if (cpd < 8) {

			r_pos = cp;
			r_normal = -p_camera->project_ray_normal(p_point);
			return true;
		}

		return false;
	}

	if (collision_mesh.is_valid()) {
		Transform gt = spatial_node->get_global_transform();

		if (billboard_handle) {
			gt.set_look_at(gt.origin, gt.origin + p_camera->get_transform().basis.get_axis(2), p_camera->get_transform().basis.get_axis(1));
		}

		Transform ai = gt.affine_inverse();
		Vector3 ray_from = ai.xform(p_camera->project_ray_origin(p_point));
		Vector3 ray_dir = ai.basis.xform(p_camera->project_ray_normal(p_point)).normalized();
		Vector3 rpos, rnorm;

		if (collision_mesh->intersect_ray(ray_from, ray_dir, rpos, rnorm)) {

			r_pos = gt.xform(rpos);
			r_normal = gt.basis.xform(rnorm).normalized();
			return true;
		}
	}

	return false;
}

void EditorSpatialGizmo::create() {

	ERR_FAIL_COND(!spatial_node);
	ERR_FAIL_COND(valid);
	valid = true;

	for (int i = 0; i < instances.size(); i++) {

		instances[i].create_instance(spatial_node);
	}

	transform();
}

void EditorSpatialGizmo::transform() {

	ERR_FAIL_COND(!spatial_node);
	ERR_FAIL_COND(!valid);
	for (int i = 0; i < instances.size(); i++) {
		VS::get_singleton()->instance_set_transform(instances[i].instance, spatial_node->get_global_transform());
	}
}

void EditorSpatialGizmo::free() {

	ERR_FAIL_COND(!spatial_node);
	ERR_FAIL_COND(!valid);

	for (int i = 0; i < instances.size(); i++) {

		if (instances[i].instance.is_valid())
			VS::get_singleton()->free(instances[i].instance);
		instances[i].instance = RID();
	}

	valid = false;
}

void EditorSpatialGizmo::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_lines", "lines", "material:Material", "billboard"), &EditorSpatialGizmo::add_lines, DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("add_mesh", "mesh:Mesh", "billboard", "skeleton"), &EditorSpatialGizmo::add_mesh, DEFVAL(false), DEFVAL(RID()));
	ObjectTypeDB::bind_method(_MD("add_collision_segments", "segments"), &EditorSpatialGizmo::add_collision_segments);
	ObjectTypeDB::bind_method(_MD("add_collision_triangles", "triangles:TriangleMesh"), &EditorSpatialGizmo::add_collision_triangles);
	ObjectTypeDB::bind_method(_MD("add_unscaled_billboard", "material:Material", "default_scale"), &EditorSpatialGizmo::add_unscaled_billboard, DEFVAL(1));
	ObjectTypeDB::bind_method(_MD("add_handles", "handles", "billboard", "secondary"), &EditorSpatialGizmo::add_handles, DEFVAL(false), DEFVAL(false));
	ObjectTypeDB::bind_method(_MD("set_spatial_node", "node:Spatial"), &EditorSpatialGizmo::_set_spatial_node);
	ObjectTypeDB::bind_method(_MD("clear"), &EditorSpatialGizmo::clear);

	BIND_VMETHOD(MethodInfo("redraw"));
	BIND_VMETHOD(MethodInfo(Variant::STRING, "get_handle_name", PropertyInfo(Variant::INT, "index")));
	BIND_VMETHOD(MethodInfo("get_handle_value:Variant", PropertyInfo(Variant::INT, "index")));
	BIND_VMETHOD(MethodInfo("set_handle", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::OBJECT, "camera:Camera"), PropertyInfo(Variant::VECTOR2, "point")));
	MethodInfo cm = MethodInfo("commit_handle", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::NIL, "restore:Variant"), PropertyInfo(Variant::BOOL, "cancel"));
	cm.default_arguments.push_back(false);
	BIND_VMETHOD(cm);
}

EditorSpatialGizmo::EditorSpatialGizmo() {
	valid = false;
	billboard_handle = false;
	base = NULL;
	spatial_node = NULL;
}

EditorSpatialGizmo::~EditorSpatialGizmo() {

	clear();
}

Vector3 EditorSpatialGizmo::get_handle_pos(int p_idx) const {

	ERR_FAIL_INDEX_V(p_idx, handles.size(), Vector3());

	return handles[p_idx];
}

//// light gizmo

String LightSpatialGizmo::get_handle_name(int p_idx) const {

	if (p_idx == 0)
		return "Radius";
	else
		return "Aperture";
}

Variant LightSpatialGizmo::get_handle_value(int p_idx) const {

	if (p_idx == 0)
		return light->get_parameter(Light::PARAM_RADIUS);
	if (p_idx == 1)
		return light->get_parameter(Light::PARAM_SPOT_ANGLE);

	return Variant();
}

static float _find_closest_angle_to_half_pi_arc(const Vector3 &p_from, const Vector3 &p_to, float p_arc_radius, const Transform &p_arc_xform) {

	//bleh, discrete is simpler
	static const int arc_test_points = 64;
	float min_d = 1e20;
	Vector3 min_p;

	for (int i = 0; i < arc_test_points; i++) {

		float a = i * Math_PI * 0.5 / arc_test_points;
		float an = (i + 1) * Math_PI * 0.5 / arc_test_points;
		Vector3 p = Vector3(Math::cos(a), 0, -Math::sin(a)) * p_arc_radius;
		Vector3 n = Vector3(Math::cos(an), 0, -Math::sin(an)) * p_arc_radius;

		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(p, n, p_from, p_to, ra, rb);

		float d = ra.distance_to(rb);
		if (d < min_d) {
			min_d = d;
			min_p = ra;
		}
	}

	//min_p = p_arc_xform.affine_inverse().xform(min_p);
	float a = Vector2(min_p.x, -min_p.z).angle();
	return a * 180.0 / Math_PI;
}

void LightSpatialGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {

	Transform gt = light->get_global_transform();
	gt.orthonormalize();
	Transform gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 s[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };
	if (p_idx == 0) {

		if (light->cast_to<SpotLight>()) {
			Vector3 ra, rb;
			Geometry::get_closest_points_between_segments(Vector3(), Vector3(0, 0, -4096), s[0], s[1], ra, rb);

			float d = -ra.z;
			if (d < 0)
				d = 0;

			light->set_parameter(Light::PARAM_RADIUS, d);
		} else if (light->cast_to<OmniLight>()) {

			Plane cp = Plane(gt.origin, p_camera->get_transform().basis.get_axis(2));

			Vector3 inters;
			if (cp.intersects_ray(ray_from, ray_dir, &inters)) {

				float r = inters.distance_to(gt.origin);
				light->set_parameter(Light::PARAM_RADIUS, r);
			}
		}

	} else if (p_idx == 1) {

		float a = _find_closest_angle_to_half_pi_arc(s[0], s[1], light->get_parameter(Light::PARAM_RADIUS), gt);
		light->set_parameter(Light::PARAM_SPOT_ANGLE, CLAMP(a, 0.01, 89.99));
	}
}

void LightSpatialGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {

	if (p_cancel) {

		light->set_parameter(p_idx == 0 ? Light::PARAM_RADIUS : Light::PARAM_SPOT_ANGLE, p_restore);

	} else if (p_idx == 0) {

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change Light Radius"));
		ur->add_do_method(light, "set_parameter", Light::PARAM_RADIUS, light->get_parameter(Light::PARAM_RADIUS));
		ur->add_undo_method(light, "set_parameter", Light::PARAM_RADIUS, p_restore);
		ur->commit_action();
	} else if (p_idx == 1) {

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change Light Radius"));
		ur->add_do_method(light, "set_parameter", Light::PARAM_SPOT_ANGLE, light->get_parameter(Light::PARAM_SPOT_ANGLE));
		ur->add_undo_method(light, "set_parameter", Light::PARAM_SPOT_ANGLE, p_restore);
		ur->commit_action();
	}
}

void LightSpatialGizmo::redraw() {

	if (light->cast_to<DirectionalLight>()) {

		const int arrow_points = 5;
		Vector3 arrow[arrow_points] = {
			Vector3(0, 0, 2),
			Vector3(1, 1, 2),
			Vector3(1, 1, -1),
			Vector3(2, 2, -1),
			Vector3(0, 0, -3)
		};

		int arrow_sides = 4;

		Vector<Vector3> lines;

		for (int i = 0; i < arrow_sides; i++) {

			Matrix3 ma(Vector3(0, 0, 1), Math_PI * 2 * float(i) / arrow_sides);
			Matrix3 mb(Vector3(0, 0, 1), Math_PI * 2 * float(i + 1) / arrow_sides);

			for (int j = 1; j < arrow_points - 1; j++) {

				if (j != 2) {
					lines.push_back(ma.xform(arrow[j]));
					lines.push_back(ma.xform(arrow[j + 1]));
				}
				if (j < arrow_points - 1) {
					lines.push_back(ma.xform(arrow[j]));
					lines.push_back(mb.xform(arrow[j]));
				}
			}
		}

		add_lines(lines, SpatialEditorGizmos::singleton->light_material);
		add_collision_segments(lines);
		add_unscaled_billboard(SpatialEditorGizmos::singleton->light_material_directional_icon, 0.05);
	}

	if (light->cast_to<OmniLight>()) {

		clear();

		OmniLight *on = light->cast_to<OmniLight>();

		float r = on->get_parameter(Light::PARAM_RADIUS);

		Vector<Vector3> points;

		for (int i = 0; i <= 360; i++) {

			float ra = Math::deg2rad(i);
			float rb = Math::deg2rad(i + 1);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

			/*points.push_back(Vector3(a.x,0,a.y));
			points.push_back(Vector3(b.x,0,b.y));
			points.push_back(Vector3(0,a.x,a.y));
			points.push_back(Vector3(0,b.x,b.y));*/
			points.push_back(Vector3(a.x, a.y, 0));
			points.push_back(Vector3(b.x, b.y, 0));
		}

		add_lines(points, SpatialEditorGizmos::singleton->light_material, true);
		add_collision_segments(points);

		add_unscaled_billboard(SpatialEditorGizmos::singleton->light_material_omni_icon, 0.05);

		Vector<Vector3> handles;
		handles.push_back(Vector3(r, 0, 0));
		add_handles(handles, true);
	}

	if (light->cast_to<SpotLight>()) {

		clear();

		Vector<Vector3> points;
		SpotLight *on = light->cast_to<SpotLight>();

		float r = on->get_parameter(Light::PARAM_RADIUS);
		float w = r * Math::sin(Math::deg2rad(on->get_parameter(Light::PARAM_SPOT_ANGLE)));
		float d = r * Math::cos(Math::deg2rad(on->get_parameter(Light::PARAM_SPOT_ANGLE)));

		for (int i = 0; i < 360; i++) {

			float ra = Math::deg2rad(i);
			float rb = Math::deg2rad(i + 1);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * w;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * w;

			/*points.push_back(Vector3(a.x,0,a.y));
			points.push_back(Vector3(b.x,0,b.y));
			points.push_back(Vector3(0,a.x,a.y));
			points.push_back(Vector3(0,b.x,b.y));*/
			points.push_back(Vector3(a.x, a.y, -d));
			points.push_back(Vector3(b.x, b.y, -d));

			if (i % 90 == 0) {

				points.push_back(Vector3(a.x, a.y, -d));
				points.push_back(Vector3());
			}
		}

		points.push_back(Vector3(0, 0, -r));
		points.push_back(Vector3());

		add_lines(points, SpatialEditorGizmos::singleton->light_material);

		Vector<Vector3> handles;
		handles.push_back(Vector3(0, 0, -r));

		Vector<Vector3> collision_segments;

		for (int i = 0; i < 64; i++) {

			float ra = i * Math_PI * 2.0 / 64.0;
			float rb = (i + 1) * Math_PI * 2.0 / 64.0;
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * w;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * w;

			collision_segments.push_back(Vector3(a.x, a.y, -d));
			collision_segments.push_back(Vector3(b.x, b.y, -d));

			if (i % 16 == 0) {

				collision_segments.push_back(Vector3(a.x, a.y, -d));
				collision_segments.push_back(Vector3());
			}

			if (i == 16) {

				handles.push_back(Vector3(a.x, a.y, -d));
			}
		}

		collision_segments.push_back(Vector3(0, 0, -r));
		collision_segments.push_back(Vector3());

		add_handles(handles);
		add_collision_segments(collision_segments);
		add_unscaled_billboard(SpatialEditorGizmos::singleton->light_material_omni_icon, 0.05);
	}
}

LightSpatialGizmo::LightSpatialGizmo(Light *p_light) {

	light = p_light;
	set_spatial_node(p_light);
}
//////

void ListenerSpatialGizmo::redraw() {

	clear();

	add_unscaled_billboard(SpatialEditorGizmos::singleton->listener_icon, 0.05);

	add_mesh(SpatialEditorGizmos::singleton->listener_line_mesh);
	Vector<Vector3> cursor_points;
	cursor_points.push_back(Vector3(0, 0, 0));
	cursor_points.push_back(Vector3(0, 0, -1.0));
	add_collision_segments(cursor_points);
}

ListenerSpatialGizmo::ListenerSpatialGizmo(Listener *p_listener) {

	set_spatial_node(p_listener);
	listener = p_listener;
}

//////

String CameraSpatialGizmo::get_handle_name(int p_idx) const {

	if (camera->get_projection() == Camera::PROJECTION_PERSPECTIVE) {
		return "FOV";
	} else {
		return "Size";
	}
}
Variant CameraSpatialGizmo::get_handle_value(int p_idx) const {

	if (camera->get_projection() == Camera::PROJECTION_PERSPECTIVE) {
		return camera->get_fov();
	} else {

		return camera->get_size();
	}
}
void CameraSpatialGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {

	Transform gt = camera->get_global_transform();
	gt.orthonormalize();
	Transform gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 s[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };

	if (camera->get_projection() == Camera::PROJECTION_PERSPECTIVE) {
		Transform gt = camera->get_global_transform();
		float a = _find_closest_angle_to_half_pi_arc(s[0], s[1], 1.0, gt);
		camera->set("fov", a);
	} else {

		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(Vector3(0, 0, -1), Vector3(4096, 0, -1), s[0], s[1], ra, rb);
		float d = ra.x * 2.0;
		if (d < 0)
			d = 0;

		camera->set("size", d);
	}
}
void CameraSpatialGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {

	if (camera->get_projection() == Camera::PROJECTION_PERSPECTIVE) {

		if (p_cancel) {

			camera->set("fov", p_restore);
		} else {
			UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
			ur->create_action(TTR("Change Camera FOV"));
			ur->add_do_property(camera, "fov", camera->get_fov());
			ur->add_undo_property(camera, "fov", p_restore);
			ur->commit_action();
		}

	} else {

		if (p_cancel) {

			camera->set("size", p_restore);
		} else {
			UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
			ur->create_action(TTR("Change Camera Size"));
			ur->add_do_property(camera, "size", camera->get_size());
			ur->add_undo_property(camera, "size", p_restore);
			ur->commit_action();
		}
	}
}

void CameraSpatialGizmo::redraw() {

	clear();

	Vector<Vector3> lines;
	Vector<Vector3> handles;

	switch (camera->get_projection()) {

		case Camera::PROJECTION_PERSPECTIVE: {

			float fov = camera->get_fov();

			Vector3 side = Vector3(Math::sin(Math::deg2rad(fov)), 0, -Math::cos(Math::deg2rad(fov)));
			Vector3 nside = side;
			nside.x = -nside.x;
			Vector3 up = Vector3(0, side.x, 0);

#define ADD_TRIANGLE(m_a, m_b, m_c) \
	{                               \
		lines.push_back(m_a);       \
		lines.push_back(m_b);       \
		lines.push_back(m_b);       \
		lines.push_back(m_c);       \
		lines.push_back(m_c);       \
		lines.push_back(m_a);       \
	}

			ADD_TRIANGLE(Vector3(), side + up, side - up);
			ADD_TRIANGLE(Vector3(), nside + up, nside - up);
			ADD_TRIANGLE(Vector3(), side + up, nside + up);
			ADD_TRIANGLE(Vector3(), side - up, nside - up);

			handles.push_back(side);
			side.x *= 0.25;
			nside.x *= 0.25;
			Vector3 tup(0, up.y * 3 / 2, side.z);
			ADD_TRIANGLE(tup, side + up, nside + up);

		} break;
		case Camera::PROJECTION_ORTHOGONAL: {

#define ADD_QUAD(m_a, m_b, m_c, m_d) \
	{                                \
		lines.push_back(m_a);        \
		lines.push_back(m_b);        \
		lines.push_back(m_b);        \
		lines.push_back(m_c);        \
		lines.push_back(m_c);        \
		lines.push_back(m_d);        \
		lines.push_back(m_d);        \
		lines.push_back(m_a);        \
	}
			float size = camera->get_size();

			float hsize = size * 0.5;
			Vector3 right(hsize, 0, 0);
			Vector3 up(0, hsize, 0);
			Vector3 back(0, 0, -1.0);
			Vector3 front(0, 0, 0);

			ADD_QUAD(-up - right, -up + right, up + right, up - right);
			ADD_QUAD(-up - right + back, -up + right + back, up + right + back, up - right + back);
			ADD_QUAD(up + right, up + right + back, up - right + back, up - right);
			ADD_QUAD(-up + right, -up + right + back, -up - right + back, -up - right);
			handles.push_back(right + back);

			right.x *= 0.25;
			Vector3 tup(0, up.y * 3 / 2, back.z);
			ADD_TRIANGLE(tup, right + up + back, -right + up + back);

		} break;
	}

	add_lines(lines, SpatialEditorGizmos::singleton->camera_material);
	add_collision_segments(lines);
	add_handles(handles);
}

CameraSpatialGizmo::CameraSpatialGizmo(Camera *p_camera) {

	camera = p_camera;
	set_spatial_node(camera);
}

//////

void MeshInstanceSpatialGizmo::redraw() {

	Ref<Mesh> m = mesh->get_mesh();
	if (!m.is_valid())
		return; //none

	Ref<TriangleMesh> tm = m->generate_triangle_mesh();
	if (tm.is_valid())
		add_collision_triangles(tm);
}

MeshInstanceSpatialGizmo::MeshInstanceSpatialGizmo(MeshInstance *p_mesh) {

	mesh = p_mesh;
	set_spatial_node(p_mesh);
}

/////

void Position3DSpatialGizmo::redraw() {

	clear();
	add_mesh(SpatialEditorGizmos::singleton->pos3d_mesh);
	Vector<Vector3> cursor_points;
	float cs = 0.25;
	cursor_points.push_back(Vector3(+cs, 0, 0));
	cursor_points.push_back(Vector3(-cs, 0, 0));
	cursor_points.push_back(Vector3(0, +cs, 0));
	cursor_points.push_back(Vector3(0, -cs, 0));
	cursor_points.push_back(Vector3(0, 0, +cs));
	cursor_points.push_back(Vector3(0, 0, -cs));
	add_collision_segments(cursor_points);
}

Position3DSpatialGizmo::Position3DSpatialGizmo(Position3D *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

/////

void SkeletonSpatialGizmo::redraw() {

	clear();

	Ref<SurfaceTool> surface_tool(memnew(SurfaceTool));

	surface_tool->begin(Mesh::PRIMITIVE_LINES);
	surface_tool->set_material(SpatialEditorGizmos::singleton->skeleton_material);
	Vector<Transform> grests;
	grests.resize(skel->get_bone_count());

	Vector<int> bones;
	Vector<float> weights;
	bones.resize(4);
	weights.resize(4);

	for (int i = 0; i < 4; i++) {
		bones[i] = 0;
		weights[i] = 0;
	}

	weights[0] = 1;

	AABB aabb;

	Color bonecolor = Color(1.0, 0.4, 0.4, 0.3);
	Color rootcolor = Color(0.4, 1.0, 0.4, 0.1);

	for (int i = 0; i < skel->get_bone_count(); i++) {

		int parent = skel->get_bone_parent(i);

		if (parent >= 0) {
			grests[i] = grests[parent] * skel->get_bone_rest(i);

			Vector3 v0 = grests[parent].origin;
			Vector3 v1 = grests[i].origin;
			Vector3 d = (v1 - v0).normalized();
			float dist = v0.distance_to(v1);

			//find closest axis
			int closest = -1;
			float closest_d = 0.0;

			for (int j = 0; j < 3; j++) {
				float dp = Math::abs(grests[parent].basis[j].normalized().dot(d));
				if (j == 0 || dp > closest_d)
					closest = j;
			}

			//find closest other
			Vector3 first;
			Vector3 points[4];
			int pointidx = 0;
			for (int j = 0; j < 3; j++) {

				bones[0] = parent;
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(rootcolor);
				surface_tool->add_vertex(v0 - grests[parent].basis[j].normalized() * dist * 0.05);
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(rootcolor);
				surface_tool->add_vertex(v0 + grests[parent].basis[j].normalized() * dist * 0.05);

				if (j == closest)
					continue;

				Vector3 axis;
				if (first == Vector3()) {
					axis = d.cross(d.cross(grests[parent].basis[j])).normalized();
					first = axis;
				} else {
					axis = d.cross(first).normalized();
				}

				for (int k = 0; k < 2; k++) {

					if (k == 1)
						axis = -axis;
					Vector3 point = v0 + d * dist * 0.2;
					point += axis * dist * 0.1;

					bones[0] = parent;
					surface_tool->add_bones(bones);
					surface_tool->add_weights(weights);
					surface_tool->add_color(bonecolor);
					surface_tool->add_vertex(v0);
					surface_tool->add_bones(bones);
					surface_tool->add_weights(weights);
					surface_tool->add_color(bonecolor);
					surface_tool->add_vertex(point);

					bones[0] = parent;
					surface_tool->add_bones(bones);
					surface_tool->add_weights(weights);
					surface_tool->add_color(bonecolor);
					surface_tool->add_vertex(point);
					bones[0] = i;
					surface_tool->add_bones(bones);
					surface_tool->add_weights(weights);
					surface_tool->add_color(bonecolor);
					surface_tool->add_vertex(v1);
					points[pointidx++] = point;
				}
			}

			SWAP(points[1], points[2]);
			for (int j = 0; j < 4; j++) {

				bones[0] = parent;
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(bonecolor);
				surface_tool->add_vertex(points[j]);
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(bonecolor);
				surface_tool->add_vertex(points[(j + 1) % 4]);
			}

			/*
			bones[0]=parent;
			surface_tool->add_bones(bones);
			surface_tool->add_weights(weights);
			surface_tool->add_color(Color(0.4,1,0.4,0.4));
			surface_tool->add_vertex(v0);
			bones[0]=i;
			surface_tool->add_bones(bones);
			surface_tool->add_weights(weights);
			surface_tool->add_color(Color(0.4,1,0.4,0.4));
			surface_tool->add_vertex(v1);
*/
		} else {

			grests[i] = skel->get_bone_rest(i);
			bones[0] = i;
		}
		/*
		Transform  t = grests[i];
		t.orthonormalize();

		for (int i=0;i<6;i++) {


			Vector3 face_points[4];

			for (int j=0;j<4;j++) {

				float v[3];
				v[0]=1.0;
				v[1]=1-2*((j>>1)&1);
				v[2]=v[1]*(1-2*(j&1));

				for (int k=0;k<3;k++) {

					if (i<3)
						face_points[j][(i+k)%3]=v[k]*(i>=3?-1:1);
					else
						face_points[3-j][(i+k)%3]=v[k]*(i>=3?-1:1);
				}
			}

			for(int j=0;j<4;j++) {
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(Color(1.0,0.4,0.4,0.4));
				surface_tool->add_vertex(t.xform(face_points[j]*0.04));
				surface_tool->add_bones(bones);
				surface_tool->add_weights(weights);
				surface_tool->add_color(Color(1.0,0.4,0.4,0.4));
				surface_tool->add_vertex(t.xform(face_points[(j+1)%4]*0.04));
			}

		}
		*/
	}

	Ref<Mesh> m = surface_tool->commit();
	add_mesh(m, false, skel->get_skeleton());
}

SkeletonSpatialGizmo::SkeletonSpatialGizmo(Skeleton *p_skel) {

	skel = p_skel;
	set_spatial_node(p_skel);
}

/////

void SpatialPlayerSpatialGizmo::redraw() {

	clear();
	if (splayer->cast_to<SpatialStreamPlayer>()) {

		add_unscaled_billboard(SpatialEditorGizmos::singleton->stream_player_icon, 0.05);

	} else if (splayer->cast_to<SpatialSamplePlayer>()) {

		add_unscaled_billboard(SpatialEditorGizmos::singleton->sample_player_icon, 0.05);
	}
}

SpatialPlayerSpatialGizmo::SpatialPlayerSpatialGizmo(SpatialPlayer *p_splayer) {

	set_spatial_node(p_splayer);
	splayer = p_splayer;
}

/////

void RoomSpatialGizmo::redraw() {

	clear();
	Ref<RoomBounds> roomie = room->get_room();
	if (roomie.is_null())
		return;
	DVector<Face3> faces = roomie->get_geometry_hint();

	Vector<Vector3> lines;
	int fc = faces.size();
	DVector<Face3>::Read r = faces.read();

	Map<_EdgeKey, Vector3> edge_map;

	for (int i = 0; i < fc; i++) {

		Vector3 fn = r[i].get_plane().normal;

		for (int j = 0; j < 3; j++) {

			_EdgeKey ek;
			ek.from = r[i].vertex[j].snapped(CMP_EPSILON);
			ek.to = r[i].vertex[(j + 1) % 3].snapped(CMP_EPSILON);
			if (ek.from < ek.to)
				SWAP(ek.from, ek.to);

			Map<_EdgeKey, Vector3>::Element *E = edge_map.find(ek);

			if (E) {

				if (E->get().dot(fn) > 0.9) {

					E->get() = Vector3();
				}

			} else {

				edge_map[ek] = fn;
			}
		}
	}

	for (Map<_EdgeKey, Vector3>::Element *E = edge_map.front(); E; E = E->next()) {

		if (E->get() != Vector3()) {
			lines.push_back(E->key().from);
			lines.push_back(E->key().to);
		}
	}

	add_lines(lines, SpatialEditorGizmos::singleton->room_material);
	add_collision_segments(lines);
}

RoomSpatialGizmo::RoomSpatialGizmo(Room *p_room) {

	set_spatial_node(p_room);
	room = p_room;
}

/////

void PortalSpatialGizmo::redraw() {

	clear();

	Vector<Point2> points = portal->get_shape();
	if (points.size() == 0) {
		return;
	}

	Vector<Vector3> lines;

	Vector3 center;
	for (int i = 0; i < points.size(); i++) {

		Vector3 f;
		f.x = points[i].x;
		f.y = points[i].y;
		Vector3 fn;
		fn.x = points[(i + 1) % points.size()].x;
		fn.y = points[(i + 1) % points.size()].y;
		center += f;

		lines.push_back(f);
		lines.push_back(fn);
	}

	center /= points.size();
	lines.push_back(center);
	lines.push_back(center + Vector3(0, 0, 1));

	add_lines(lines, SpatialEditorGizmos::singleton->portal_material);
	add_collision_segments(lines);
}

PortalSpatialGizmo::PortalSpatialGizmo(Portal *p_portal) {

	set_spatial_node(p_portal);
	portal = p_portal;
}

/////

void RayCastSpatialGizmo::redraw() {

	clear();

	Vector<Vector3> lines;

	lines.push_back(Vector3());
	lines.push_back(raycast->get_cast_to());

	add_lines(lines, SpatialEditorGizmos::singleton->raycast_material);
	add_collision_segments(lines);
}

RayCastSpatialGizmo::RayCastSpatialGizmo(RayCast *p_raycast) {

	set_spatial_node(p_raycast);
	raycast = p_raycast;
}

/////

void VehicleWheelSpatialGizmo::redraw() {

	clear();

	Vector<Vector3> points;

	float r = car_wheel->get_radius();
	const int skip = 10;
	for (int i = 0; i <= 360; i += skip) {

		float ra = Math::deg2rad(i);
		float rb = Math::deg2rad(i + skip);
		Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
		Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

		points.push_back(Vector3(0, a.x, a.y));
		points.push_back(Vector3(0, b.x, b.y));

		const int springsec = 4;

		for (int j = 0; j < springsec; j++) {
			float t = car_wheel->get_suspension_rest_length() * 5;
			points.push_back(Vector3(a.x, i / 360.0 * t / springsec + j * (t / springsec), a.y) * 0.2);
			points.push_back(Vector3(b.x, (i + skip) / 360.0 * t / springsec + j * (t / springsec), b.y) * 0.2);
		}
	}

	//travel
	points.push_back(Vector3(0, 0, 0));
	points.push_back(Vector3(0, car_wheel->get_suspension_rest_length(), 0));

	//axis
	points.push_back(Vector3(r * 0.2, car_wheel->get_suspension_rest_length(), 0));
	points.push_back(Vector3(-r * 0.2, car_wheel->get_suspension_rest_length(), 0));
	//axis
	points.push_back(Vector3(r * 0.2, 0, 0));
	points.push_back(Vector3(-r * 0.2, 0, 0));

	//forward line
	points.push_back(Vector3(0, -r, 0));
	points.push_back(Vector3(0, -r, r * 2));
	points.push_back(Vector3(0, -r, r * 2));
	points.push_back(Vector3(r * 2 * 0.2, -r, r * 2 * 0.8));
	points.push_back(Vector3(0, -r, r * 2));
	points.push_back(Vector3(-r * 2 * 0.2, -r, r * 2 * 0.8));

	add_lines(points, SpatialEditorGizmos::singleton->car_wheel_material);
	add_collision_segments(points);
}

VehicleWheelSpatialGizmo::VehicleWheelSpatialGizmo(VehicleWheel *p_car_wheel) {

	set_spatial_node(p_car_wheel);
	car_wheel = p_car_wheel;
}

///

void TestCubeSpatialGizmo::redraw() {

	clear();
	add_collision_triangles(SpatialEditorGizmos::singleton->test_cube_tm);
}

TestCubeSpatialGizmo::TestCubeSpatialGizmo(TestCube *p_tc) {

	tc = p_tc;
	set_spatial_node(p_tc);
}

///////////

String CollisionShapeSpatialGizmo::get_handle_name(int p_idx) const {

	Ref<Shape> s = cs->get_shape();
	if (s.is_null())
		return "";

	if (s->cast_to<SphereShape>()) {

		return "Radius";
	}

	if (s->cast_to<BoxShape>()) {

		return "Extents";
	}

	if (s->cast_to<CapsuleShape>()) {

		return p_idx == 0 ? "Radius" : "Height";
	}

	if (s->cast_to<RayShape>()) {

		return "Length";
	}

	return "";
}
Variant CollisionShapeSpatialGizmo::get_handle_value(int p_idx) const {

	Ref<Shape> s = cs->get_shape();
	if (s.is_null())
		return Variant();

	if (s->cast_to<SphereShape>()) {

		Ref<SphereShape> ss = s;
		return ss->get_radius();
	}

	if (s->cast_to<BoxShape>()) {

		Ref<BoxShape> bs = s;
		return bs->get_extents();
	}

	if (s->cast_to<CapsuleShape>()) {

		Ref<CapsuleShape> cs = s;
		return p_idx == 0 ? cs->get_radius() : cs->get_height();
	}

	if (s->cast_to<RayShape>()) {

		Ref<RayShape> cs = s;
		return cs->get_length();
	}

	return Variant();
}
void CollisionShapeSpatialGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {
	Ref<Shape> s = cs->get_shape();
	if (s.is_null())
		return;

	Transform gt = cs->get_global_transform();
	gt.orthonormalize();
	Transform gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 sg[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };

	if (s->cast_to<SphereShape>()) {

		Ref<SphereShape> ss = s;
		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(Vector3(), Vector3(4096, 0, 0), sg[0], sg[1], ra, rb);
		float d = ra.x;
		if (d < 0.001)
			d = 0.001;

		ss->set_radius(d);
	}

	if (s->cast_to<RayShape>()) {

		Ref<RayShape> rs = s;
		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(Vector3(), Vector3(0, 0, 4096), sg[0], sg[1], ra, rb);
		float d = ra.z;
		if (d < 0.001)
			d = 0.001;

		rs->set_length(d);
	}

	if (s->cast_to<BoxShape>()) {

		Vector3 axis;
		axis[p_idx] = 1.0;
		Ref<BoxShape> bs = s;
		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
		float d = ra[p_idx];
		if (d < 0.001)
			d = 0.001;

		Vector3 he = bs->get_extents();
		he[p_idx] = d;
		bs->set_extents(he);
	}

	if (s->cast_to<CapsuleShape>()) {

		Vector3 axis;
		axis[p_idx == 0 ? 0 : 2] = 1.0;
		Ref<CapsuleShape> cs = s;
		Vector3 ra, rb;
		Geometry::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
		float d = axis.dot(ra);
		if (p_idx == 1)
			d -= cs->get_radius();
		if (d < 0.001)
			d = 0.001;

		if (p_idx == 0)
			cs->set_radius(d);
		else if (p_idx == 1)
			cs->set_height(d * 2.0);
	}
}
void CollisionShapeSpatialGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {
	Ref<Shape> s = cs->get_shape();
	if (s.is_null())
		return;

	if (s->cast_to<SphereShape>()) {

		Ref<SphereShape> ss = s;
		if (p_cancel) {
			ss->set_radius(p_restore);
			return;
		}

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change Sphere Shape Radius"));
		ur->add_do_method(ss.ptr(), "set_radius", ss->get_radius());
		ur->add_undo_method(ss.ptr(), "set_radius", p_restore);
		ur->commit_action();
	}

	if (s->cast_to<BoxShape>()) {

		Ref<BoxShape> ss = s;
		if (p_cancel) {
			ss->set_extents(p_restore);
			return;
		}

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change Box Shape Extents"));
		ur->add_do_method(ss.ptr(), "set_extents", ss->get_extents());
		ur->add_undo_method(ss.ptr(), "set_extents", p_restore);
		ur->commit_action();
	}

	if (s->cast_to<CapsuleShape>()) {

		Ref<CapsuleShape> ss = s;
		if (p_cancel) {
			if (p_idx == 0)
				ss->set_radius(p_restore);
			else
				ss->set_height(p_restore);
			return;
		}

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		if (p_idx == 0) {
			ur->create_action(TTR("Change Capsule Shape Radius"));
			ur->add_do_method(ss.ptr(), "set_radius", ss->get_radius());
			ur->add_undo_method(ss.ptr(), "set_radius", p_restore);
		} else {
			ur->create_action(TTR("Change Capsule Shape Height"));
			ur->add_do_method(ss.ptr(), "set_height", ss->get_height());
			ur->add_undo_method(ss.ptr(), "set_height", p_restore);
		}

		ur->commit_action();
	}

	if (s->cast_to<RayShape>()) {

		Ref<RayShape> ss = s;
		if (p_cancel) {
			ss->set_length(p_restore);
			return;
		}

		UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
		ur->create_action(TTR("Change Ray Shape Length"));
		ur->add_do_method(ss.ptr(), "set_length", ss->get_length());
		ur->add_undo_method(ss.ptr(), "set_length", p_restore);
		ur->commit_action();
	}
}
void CollisionShapeSpatialGizmo::redraw() {

	clear();

	Ref<Shape> s = cs->get_shape();
	if (s.is_null())
		return;

	if (s->cast_to<SphereShape>()) {

		Ref<SphereShape> sp = s;
		float r = sp->get_radius();

		Vector<Vector3> points;

		for (int i = 0; i <= 360; i++) {

			float ra = Math::deg2rad(i);
			float rb = Math::deg2rad(i + 1);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

			points.push_back(Vector3(a.x, 0, a.y));
			points.push_back(Vector3(b.x, 0, b.y));
			points.push_back(Vector3(0, a.x, a.y));
			points.push_back(Vector3(0, b.x, b.y));
			points.push_back(Vector3(a.x, a.y, 0));
			points.push_back(Vector3(b.x, b.y, 0));
		}

		Vector<Vector3> collision_segments;

		for (int i = 0; i < 64; i++) {

			float ra = i * Math_PI * 2.0 / 64.0;
			float rb = (i + 1) * Math_PI * 2.0 / 64.0;
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

			collision_segments.push_back(Vector3(a.x, 0, a.y));
			collision_segments.push_back(Vector3(b.x, 0, b.y));
			collision_segments.push_back(Vector3(0, a.x, a.y));
			collision_segments.push_back(Vector3(0, b.x, b.y));
			collision_segments.push_back(Vector3(a.x, a.y, 0));
			collision_segments.push_back(Vector3(b.x, b.y, 0));
		}

		add_lines(points, SpatialEditorGizmos::singleton->shape_material);
		add_collision_segments(collision_segments);
		Vector<Vector3> handles;
		handles.push_back(Vector3(r, 0, 0));
		add_handles(handles);
	}

	if (s->cast_to<BoxShape>()) {

		Ref<BoxShape> bs = s;
		Vector<Vector3> lines;
		AABB aabb;
		aabb.pos = -bs->get_extents();
		aabb.size = aabb.pos * -2;

		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}

		Vector<Vector3> handles;

		for (int i = 0; i < 3; i++) {

			Vector3 ax;
			ax[i] = bs->get_extents()[i];
			handles.push_back(ax);
		}

		add_lines(lines, SpatialEditorGizmos::singleton->shape_material);
		add_collision_segments(lines);
		add_handles(handles);
	}

	if (s->cast_to<CapsuleShape>()) {

		Ref<CapsuleShape> cs = s;
		float radius = cs->get_radius();
		float height = cs->get_height();

		Vector<Vector3> points;

		Vector3 d(0, 0, height * 0.5);
		for (int i = 0; i < 360; i++) {

			float ra = Math::deg2rad(i);
			float rb = Math::deg2rad(i + 1);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * radius;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * radius;

			points.push_back(Vector3(a.x, a.y, 0) + d);
			points.push_back(Vector3(b.x, b.y, 0) + d);

			points.push_back(Vector3(a.x, a.y, 0) - d);
			points.push_back(Vector3(b.x, b.y, 0) - d);

			if (i % 90 == 0) {

				points.push_back(Vector3(a.x, a.y, 0) + d);
				points.push_back(Vector3(a.x, a.y, 0) - d);
			}

			Vector3 dud = i < 180 ? d : -d;

			points.push_back(Vector3(0, a.y, a.x) + dud);
			points.push_back(Vector3(0, b.y, b.x) + dud);
			points.push_back(Vector3(a.y, 0, a.x) + dud);
			points.push_back(Vector3(b.y, 0, b.x) + dud);
		}

		add_lines(points, SpatialEditorGizmos::singleton->shape_material);

		Vector<Vector3> collision_segments;

		for (int i = 0; i < 64; i++) {

			float ra = i * Math_PI * 2.0 / 64.0;
			float rb = (i + 1) * Math_PI * 2.0 / 64.0;
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * radius;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * radius;

			collision_segments.push_back(Vector3(a.x, a.y, 0) + d);
			collision_segments.push_back(Vector3(b.x, b.y, 0) + d);

			collision_segments.push_back(Vector3(a.x, a.y, 0) - d);
			collision_segments.push_back(Vector3(b.x, b.y, 0) - d);

			if (i % 16 == 0) {

				collision_segments.push_back(Vector3(a.x, a.y, 0) + d);
				collision_segments.push_back(Vector3(a.x, a.y, 0) - d);
			}

			Vector3 dud = i < 32 ? d : -d;

			collision_segments.push_back(Vector3(0, a.y, a.x) + dud);
			collision_segments.push_back(Vector3(0, b.y, b.x) + dud);
			collision_segments.push_back(Vector3(a.y, 0, a.x) + dud);
			collision_segments.push_back(Vector3(b.y, 0, b.x) + dud);
		}

		add_collision_segments(collision_segments);

		Vector<Vector3> handles;
		handles.push_back(Vector3(cs->get_radius(), 0, 0));
		handles.push_back(Vector3(0, 0, cs->get_height() * 0.5 + cs->get_radius()));
		add_handles(handles);
	}

	if (s->cast_to<PlaneShape>()) {

		Ref<PlaneShape> ps = s;
		Plane p = ps->get_plane();
		Vector<Vector3> points;

		Vector3 n1 = p.get_any_perpendicular_normal();
		Vector3 n2 = p.normal.cross(n1).normalized();

		Vector3 pface[4] = {
			p.normal * p.d + n1 * 10.0 + n2 * 10.0,
			p.normal * p.d + n1 * 10.0 + n2 * -10.0,
			p.normal * p.d + n1 * -10.0 + n2 * -10.0,
			p.normal * p.d + n1 * -10.0 + n2 * 10.0,
		};

		points.push_back(pface[0]);
		points.push_back(pface[1]);
		points.push_back(pface[1]);
		points.push_back(pface[2]);
		points.push_back(pface[2]);
		points.push_back(pface[3]);
		points.push_back(pface[3]);
		points.push_back(pface[0]);
		points.push_back(p.normal * p.d);
		points.push_back(p.normal * p.d + p.normal * 3);

		add_lines(points, SpatialEditorGizmos::singleton->shape_material);
		add_collision_segments(points);
	}

	if (s->cast_to<ConvexPolygonShape>()) {

		DVector<Vector3> points = s->cast_to<ConvexPolygonShape>()->get_points();

		if (points.size() > 3) {

			QuickHull qh;
			Vector<Vector3> varr = Variant(points);
			Geometry::MeshData md;
			Error err = qh.build(varr, md);
			if (err == OK) {
				Vector<Vector3> points;
				points.resize(md.edges.size() * 2);
				for (int i = 0; i < md.edges.size(); i++) {
					points[i * 2 + 0] = md.vertices[md.edges[i].a];
					points[i * 2 + 1] = md.vertices[md.edges[i].b];
				}

				add_lines(points, SpatialEditorGizmos::singleton->shape_material);
				add_collision_segments(points);
			}
		}
	}

	if (s->cast_to<RayShape>()) {

		Ref<RayShape> rs = s;

		Vector<Vector3> points;
		points.push_back(Vector3());
		points.push_back(Vector3(0, 0, rs->get_length()));
		add_lines(points, SpatialEditorGizmos::singleton->shape_material);
		add_collision_segments(points);
		Vector<Vector3> handles;
		handles.push_back(Vector3(0, 0, rs->get_length()));
		add_handles(handles);
	}
}
CollisionShapeSpatialGizmo::CollisionShapeSpatialGizmo(CollisionShape *p_cs) {

	cs = p_cs;
	set_spatial_node(p_cs);
}

/////

void CollisionPolygonSpatialGizmo::redraw() {

	clear();

	Vector<Vector2> points = polygon->get_polygon();
	float depth = polygon->get_depth() * 0.5;

	Vector<Vector3> lines;
	for (int i = 0; i < points.size(); i++) {

		int n = (i + 1) % points.size();
		lines.push_back(Vector3(points[i].x, points[i].y, depth));
		lines.push_back(Vector3(points[n].x, points[n].y, depth));
		lines.push_back(Vector3(points[i].x, points[i].y, -depth));
		lines.push_back(Vector3(points[n].x, points[n].y, -depth));
		lines.push_back(Vector3(points[i].x, points[i].y, depth));
		lines.push_back(Vector3(points[i].x, points[i].y, -depth));
	}

	add_lines(lines, SpatialEditorGizmos::singleton->shape_material);
	add_collision_segments(lines);
}

CollisionPolygonSpatialGizmo::CollisionPolygonSpatialGizmo(CollisionPolygon *p_polygon) {

	set_spatial_node(p_polygon);
	polygon = p_polygon;
}
///

String VisibilityNotifierGizmo::get_handle_name(int p_idx) const {

	switch (p_idx) {
		case 0: return "X";
		case 1: return "Y";
		case 2: return "Z";
	}

	return "";
}
Variant VisibilityNotifierGizmo::get_handle_value(int p_idx) const {

	return notifier->get_aabb();
}
void VisibilityNotifierGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {

	Transform gt = notifier->get_global_transform();
	//gt.orthonormalize();
	Transform gi = gt.affine_inverse();

	AABB aabb = notifier->get_aabb();
	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 sg[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };
	Vector3 ofs = aabb.pos + aabb.size * 0.5;

	Vector3 axis;
	axis[p_idx] = 1.0;

	Vector3 ra, rb;
	Geometry::get_closest_points_between_segments(ofs, ofs + axis * 4096, sg[0], sg[1], ra, rb);
	float d = ra[p_idx];
	if (d < 0.001)
		d = 0.001;

	aabb.pos[p_idx] = (aabb.pos[p_idx] + aabb.size[p_idx] * 0.5) - d;
	aabb.size[p_idx] = d * 2;
	notifier->set_aabb(aabb);
}

void VisibilityNotifierGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {

	if (p_cancel) {
		notifier->set_aabb(p_restore);
		return;
	}

	UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();
	ur->create_action(TTR("Change Notifier Extents"));
	ur->add_do_method(notifier, "set_aabb", notifier->get_aabb());
	ur->add_undo_method(notifier, "set_aabb", p_restore);
	ur->commit_action();
}

void VisibilityNotifierGizmo::redraw() {

	clear();

	Vector<Vector3> lines;
	AABB aabb = notifier->get_aabb();

	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);
		lines.push_back(a);
		lines.push_back(b);
	}

	Vector<Vector3> handles;

	for (int i = 0; i < 3; i++) {

		Vector3 ax;
		ax[i] = aabb.pos[i] + aabb.size[i];
		handles.push_back(ax);
	}

	add_lines(lines, SpatialEditorGizmos::singleton->visibility_notifier_material);
	//add_unscaled_billboard(SpatialEditorGizmos::singleton->visi,0.05);
	add_collision_segments(lines);
	add_handles(handles);
}
VisibilityNotifierGizmo::VisibilityNotifierGizmo(VisibilityNotifier *p_notifier) {

	notifier = p_notifier;
	set_spatial_node(p_notifier);
}

////////

void NavigationMeshSpatialGizmo::redraw() {

	clear();
	Ref<NavigationMesh> navmeshie = navmesh->get_navigation_mesh();
	if (navmeshie.is_null())
		return;

	DVector<Vector3> vertices = navmeshie->get_vertices();
	DVector<Vector3>::Read vr = vertices.read();
	List<Face3> faces;
	for (int i = 0; i < navmeshie->get_polygon_count(); i++) {
		Vector<int> p = navmeshie->get_polygon(i);

		for (int j = 2; j < p.size(); j++) {
			Face3 f;
			f.vertex[0] = vr[p[0]];
			f.vertex[1] = vr[p[j - 1]];
			f.vertex[2] = vr[p[j]];

			faces.push_back(f);
		}
	}

	if (faces.empty())
		return;

	Map<_EdgeKey, bool> edge_map;
	DVector<Vector3> tmeshfaces;
	tmeshfaces.resize(faces.size() * 3);

	{
		DVector<Vector3>::Write tw = tmeshfaces.write();
		int tidx = 0;

		for (List<Face3>::Element *E = faces.front(); E; E = E->next()) {

			const Face3 &f = E->get();

			for (int j = 0; j < 3; j++) {

				tw[tidx++] = f.vertex[j];
				_EdgeKey ek;
				ek.from = f.vertex[j].snapped(CMP_EPSILON);
				ek.to = f.vertex[(j + 1) % 3].snapped(CMP_EPSILON);
				if (ek.from < ek.to)
					SWAP(ek.from, ek.to);

				Map<_EdgeKey, bool>::Element *E = edge_map.find(ek);

				if (E) {

					E->get() = false;

				} else {

					edge_map[ek] = true;
				}
			}
		}
	}
	Vector<Vector3> lines;

	for (Map<_EdgeKey, bool>::Element *E = edge_map.front(); E; E = E->next()) {

		if (E->get()) {
			lines.push_back(E->key().from);
			lines.push_back(E->key().to);
		}
	}

	Ref<TriangleMesh> tmesh = memnew(TriangleMesh);
	tmesh->create(tmeshfaces);

	if (lines.size())
		add_lines(lines, navmesh->is_enabled() ? SpatialEditorGizmos::singleton->navmesh_edge_material : SpatialEditorGizmos::singleton->navmesh_edge_material_disabled);
	add_collision_triangles(tmesh);
	Ref<Mesh> m = memnew(Mesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[0] = tmeshfaces;
	m->add_surface(Mesh::PRIMITIVE_TRIANGLES, a);
	m->surface_set_material(0, navmesh->is_enabled() ? SpatialEditorGizmos::singleton->navmesh_solid_material : SpatialEditorGizmos::singleton->navmesh_solid_material_disabled);
	add_mesh(m);
	add_collision_segments(lines);
}

NavigationMeshSpatialGizmo::NavigationMeshSpatialGizmo(NavigationMeshInstance *p_navmesh) {

	set_spatial_node(p_navmesh);
	navmesh = p_navmesh;
}

//////
///
///

void PinJointSpatialGizmo::redraw() {

	clear();
	Vector<Vector3> cursor_points;
	float cs = 0.25;
	cursor_points.push_back(Vector3(+cs, 0, 0));
	cursor_points.push_back(Vector3(-cs, 0, 0));
	cursor_points.push_back(Vector3(0, +cs, 0));
	cursor_points.push_back(Vector3(0, -cs, 0));
	cursor_points.push_back(Vector3(0, 0, +cs));
	cursor_points.push_back(Vector3(0, 0, -cs));
	add_collision_segments(cursor_points);
	add_lines(cursor_points, SpatialEditorGizmos::singleton->joint_material);
}

PinJointSpatialGizmo::PinJointSpatialGizmo(PinJoint *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

////

void HingeJointSpatialGizmo::redraw() {

	clear();
	Vector<Vector3> cursor_points;
	float cs = 0.25;
	/*cursor_points.push_back(Vector3(+cs,0,0));
	cursor_points.push_back(Vector3(-cs,0,0));
	cursor_points.push_back(Vector3(0,+cs,0));
	cursor_points.push_back(Vector3(0,-cs,0));*/
	cursor_points.push_back(Vector3(0, 0, +cs * 2));
	cursor_points.push_back(Vector3(0, 0, -cs * 2));

	float ll = p3d->get_param(HingeJoint::PARAM_LIMIT_LOWER);
	float ul = p3d->get_param(HingeJoint::PARAM_LIMIT_UPPER);

	if (p3d->get_flag(HingeJoint::FLAG_USE_LIMIT) && ll < ul) {

		const int points = 32;

		for (int i = 0; i < points; i++) {

			float s = ll + i * (ul - ll) / points;
			float n = ll + (i + 1) * (ul - ll) / points;

			Vector3 from = Vector3(-Math::sin(s), Math::cos(s), 0) * cs;
			Vector3 to = Vector3(-Math::sin(n), Math::cos(n), 0) * cs;

			if (i == points - 1) {
				cursor_points.push_back(to);
				cursor_points.push_back(Vector3());
			}
			if (i == 0) {
				cursor_points.push_back(from);
				cursor_points.push_back(Vector3());
			}

			cursor_points.push_back(from);
			cursor_points.push_back(to);
		}

		cursor_points.push_back(Vector3(0, cs * 1.5, 0));
		cursor_points.push_back(Vector3());

	} else {

		const int points = 32;

		for (int i = 0; i < points; i++) {

			float s = ll + i * (Math_PI * 2.0) / points;
			float n = ll + (i + 1) * (Math_PI * 2.0) / points;

			Vector3 from = Vector3(-Math::sin(s), Math::cos(s), 0) * cs;
			Vector3 to = Vector3(-Math::sin(n), Math::cos(n), 0) * cs;

			cursor_points.push_back(from);
			cursor_points.push_back(to);
		}
	}
	add_collision_segments(cursor_points);
	add_lines(cursor_points, SpatialEditorGizmos::singleton->joint_material);
}

HingeJointSpatialGizmo::HingeJointSpatialGizmo(HingeJoint *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

///////
///
////

void SliderJointSpatialGizmo::redraw() {

	clear();
	Vector<Vector3> cursor_points;
	float cs = 0.25;
	/*cursor_points.push_back(Vector3(+cs,0,0));
	cursor_points.push_back(Vector3(-cs,0,0));
	cursor_points.push_back(Vector3(0,+cs,0));
	cursor_points.push_back(Vector3(0,-cs,0));*/
	cursor_points.push_back(Vector3(0, 0, +cs * 2));
	cursor_points.push_back(Vector3(0, 0, -cs * 2));

	float ll = p3d->get_param(SliderJoint::PARAM_ANGULAR_LIMIT_LOWER);
	float ul = p3d->get_param(SliderJoint::PARAM_ANGULAR_LIMIT_UPPER);
	float lll = -p3d->get_param(SliderJoint::PARAM_LINEAR_LIMIT_LOWER);
	float lul = -p3d->get_param(SliderJoint::PARAM_LINEAR_LIMIT_UPPER);

	if (lll > lul) {

		cursor_points.push_back(Vector3(lul, 0, 0));
		cursor_points.push_back(Vector3(lll, 0, 0));

		cursor_points.push_back(Vector3(lul, -cs, -cs));
		cursor_points.push_back(Vector3(lul, -cs, cs));
		cursor_points.push_back(Vector3(lul, -cs, cs));
		cursor_points.push_back(Vector3(lul, cs, cs));
		cursor_points.push_back(Vector3(lul, cs, cs));
		cursor_points.push_back(Vector3(lul, cs, -cs));
		cursor_points.push_back(Vector3(lul, cs, -cs));
		cursor_points.push_back(Vector3(lul, -cs, -cs));

		cursor_points.push_back(Vector3(lll, -cs, -cs));
		cursor_points.push_back(Vector3(lll, -cs, cs));
		cursor_points.push_back(Vector3(lll, -cs, cs));
		cursor_points.push_back(Vector3(lll, cs, cs));
		cursor_points.push_back(Vector3(lll, cs, cs));
		cursor_points.push_back(Vector3(lll, cs, -cs));
		cursor_points.push_back(Vector3(lll, cs, -cs));
		cursor_points.push_back(Vector3(lll, -cs, -cs));

	} else {

		cursor_points.push_back(Vector3(+cs * 2, 0, 0));
		cursor_points.push_back(Vector3(-cs * 2, 0, 0));
	}

	if (ll < ul) {

		const int points = 32;

		for (int i = 0; i < points; i++) {

			float s = ll + i * (ul - ll) / points;
			float n = ll + (i + 1) * (ul - ll) / points;

			Vector3 from = Vector3(0, Math::cos(s), -Math::sin(s)) * cs;
			Vector3 to = Vector3(0, Math::cos(n), -Math::sin(n)) * cs;

			if (i == points - 1) {
				cursor_points.push_back(to);
				cursor_points.push_back(Vector3());
			}
			if (i == 0) {
				cursor_points.push_back(from);
				cursor_points.push_back(Vector3());
			}

			cursor_points.push_back(from);
			cursor_points.push_back(to);
		}

		cursor_points.push_back(Vector3(0, cs * 1.5, 0));
		cursor_points.push_back(Vector3());

	} else {

		const int points = 32;

		for (int i = 0; i < points; i++) {

			float s = ll + i * (Math_PI * 2.0) / points;
			float n = ll + (i + 1) * (Math_PI * 2.0) / points;

			Vector3 from = Vector3(0, Math::cos(s), -Math::sin(s)) * cs;
			Vector3 to = Vector3(0, Math::cos(n), -Math::sin(n)) * cs;

			cursor_points.push_back(from);
			cursor_points.push_back(to);
		}
	}
	add_collision_segments(cursor_points);
	add_lines(cursor_points, SpatialEditorGizmos::singleton->joint_material);
}

SliderJointSpatialGizmo::SliderJointSpatialGizmo(SliderJoint *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

///////
///
////

void ConeTwistJointSpatialGizmo::redraw() {

	clear();
	Vector<Vector3> points;

	float r = 1.0;
	float w = r * Math::sin(p3d->get_param(ConeTwistJoint::PARAM_SWING_SPAN));
	float d = r * Math::cos(p3d->get_param(ConeTwistJoint::PARAM_SWING_SPAN));

	//swing
	for (int i = 0; i < 360; i += 10) {

		float ra = Math::deg2rad(i);
		float rb = Math::deg2rad(i + 10);
		Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * w;
		Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * w;

		/*points.push_back(Vector3(a.x,0,a.y));
		points.push_back(Vector3(b.x,0,b.y));
		points.push_back(Vector3(0,a.x,a.y));
		points.push_back(Vector3(0,b.x,b.y));*/
		points.push_back(Vector3(d, a.x, a.y));
		points.push_back(Vector3(d, b.x, b.y));

		if (i % 90 == 0) {

			points.push_back(Vector3(d, a.x, a.y));
			points.push_back(Vector3());
		}
	}

	points.push_back(Vector3());
	points.push_back(Vector3(1, 0, 0));

	//twist
	/*
	 */
	float ts = Math::rad2deg(p3d->get_param(ConeTwistJoint::PARAM_TWIST_SPAN));
	ts = MIN(ts, 720);

	for (int i = 0; i < int(ts); i += 5) {

		float ra = Math::deg2rad(i);
		float rb = Math::deg2rad(i + 5);
		float c = i / 720.0;
		float cn = (i + 5) / 720.0;
		Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * w * c;
		Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * w * cn;

		/*points.push_back(Vector3(a.x,0,a.y));
		points.push_back(Vector3(b.x,0,b.y));
		points.push_back(Vector3(0,a.x,a.y));
		points.push_back(Vector3(0,b.x,b.y));*/

		points.push_back(Vector3(c, a.x, a.y));
		points.push_back(Vector3(cn, b.x, b.y));
	}

	add_collision_segments(points);
	add_lines(points, SpatialEditorGizmos::singleton->joint_material);
}

ConeTwistJointSpatialGizmo::ConeTwistJointSpatialGizmo(ConeTwistJoint *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

////////
/// \brief SpatialEditorGizmos::singleton
///
///////
///
////

void Generic6DOFJointSpatialGizmo::redraw() {

	clear();
	Vector<Vector3> cursor_points;
	float cs = 0.25;

	for (int ax = 0; ax < 3; ax++) {
		/*cursor_points.push_back(Vector3(+cs,0,0));
		cursor_points.push_back(Vector3(-cs,0,0));
		cursor_points.push_back(Vector3(0,+cs,0));
		cursor_points.push_back(Vector3(0,-cs,0));
		cursor_points.push_back(Vector3(0,0,+cs*2));
		cursor_points.push_back(Vector3(0,0,-cs*2)); */

		float ll;
		float ul;
		float lll;
		float lul;

		int a1, a2, a3;
		bool enable_ang;
		bool enable_lin;

		switch (ax) {
			case 0:
				ll = p3d->get_param_x(Generic6DOFJoint::PARAM_ANGULAR_LOWER_LIMIT);
				ul = p3d->get_param_x(Generic6DOFJoint::PARAM_ANGULAR_UPPER_LIMIT);
				lll = -p3d->get_param_x(Generic6DOFJoint::PARAM_LINEAR_LOWER_LIMIT);
				lul = -p3d->get_param_x(Generic6DOFJoint::PARAM_LINEAR_UPPER_LIMIT);
				enable_ang = p3d->get_flag_x(Generic6DOFJoint::FLAG_ENABLE_ANGULAR_LIMIT);
				enable_lin = p3d->get_flag_x(Generic6DOFJoint::FLAG_ENABLE_LINEAR_LIMIT);
				a1 = 0;
				a2 = 1;
				a3 = 2;
				break;
			case 1:
				ll = p3d->get_param_y(Generic6DOFJoint::PARAM_ANGULAR_LOWER_LIMIT);
				ul = p3d->get_param_y(Generic6DOFJoint::PARAM_ANGULAR_UPPER_LIMIT);
				lll = -p3d->get_param_y(Generic6DOFJoint::PARAM_LINEAR_LOWER_LIMIT);
				lul = -p3d->get_param_y(Generic6DOFJoint::PARAM_LINEAR_UPPER_LIMIT);
				enable_ang = p3d->get_flag_y(Generic6DOFJoint::FLAG_ENABLE_ANGULAR_LIMIT);
				enable_lin = p3d->get_flag_y(Generic6DOFJoint::FLAG_ENABLE_LINEAR_LIMIT);
				a1 = 2;
				a2 = 0;
				a3 = 1;
				break;
			case 2:
				ll = p3d->get_param_z(Generic6DOFJoint::PARAM_ANGULAR_LOWER_LIMIT);
				ul = p3d->get_param_z(Generic6DOFJoint::PARAM_ANGULAR_UPPER_LIMIT);
				lll = -p3d->get_param_z(Generic6DOFJoint::PARAM_LINEAR_LOWER_LIMIT);
				lul = -p3d->get_param_z(Generic6DOFJoint::PARAM_LINEAR_UPPER_LIMIT);
				enable_ang = p3d->get_flag_z(Generic6DOFJoint::FLAG_ENABLE_ANGULAR_LIMIT);
				enable_lin = p3d->get_flag_z(Generic6DOFJoint::FLAG_ENABLE_LINEAR_LIMIT);

				a1 = 1;
				a2 = 2;
				a3 = 0;
				break;
		}

#define ADD_VTX(x, y, z)            \
	{                               \
		Vector3 v;                  \
		v[a1] = (x);                \
		v[a2] = (y);                \
		v[a3] = (z);                \
		cursor_points.push_back(v); \
	}

#define SET_VTX(what, x, y, z) \
	{                          \
		Vector3 v;             \
		v[a1] = (x);           \
		v[a2] = (y);           \
		v[a3] = (z);           \
		what = v;              \
	}

		if (enable_lin && lll >= lul) {

			ADD_VTX(lul, 0, 0);
			ADD_VTX(lll, 0, 0);

			ADD_VTX(lul, -cs, -cs);
			ADD_VTX(lul, -cs, cs);
			ADD_VTX(lul, -cs, cs);
			ADD_VTX(lul, cs, cs);
			ADD_VTX(lul, cs, cs);
			ADD_VTX(lul, cs, -cs);
			ADD_VTX(lul, cs, -cs);
			ADD_VTX(lul, -cs, -cs);

			ADD_VTX(lll, -cs, -cs);
			ADD_VTX(lll, -cs, cs);
			ADD_VTX(lll, -cs, cs);
			ADD_VTX(lll, cs, cs);
			ADD_VTX(lll, cs, cs);
			ADD_VTX(lll, cs, -cs);
			ADD_VTX(lll, cs, -cs);
			ADD_VTX(lll, -cs, -cs);

		} else {

			ADD_VTX(+cs * 2, 0, 0);
			ADD_VTX(-cs * 2, 0, 0);
		}

		if (enable_ang && ll <= ul) {

			const int points = 32;

			for (int i = 0; i < points; i++) {

				float s = ll + i * (ul - ll) / points;
				float n = ll + (i + 1) * (ul - ll) / points;

				Vector3 from;
				SET_VTX(from, 0, Math::cos(s), -Math::sin(s));
				from *= cs;
				Vector3 to;
				SET_VTX(to, 0, Math::cos(n), -Math::sin(n));
				to *= cs;

				if (i == points - 1) {
					cursor_points.push_back(to);
					cursor_points.push_back(Vector3());
				}
				if (i == 0) {
					cursor_points.push_back(from);
					cursor_points.push_back(Vector3());
				}

				cursor_points.push_back(from);
				cursor_points.push_back(to);
			}

			ADD_VTX(0, cs * 1.5, 0);
			cursor_points.push_back(Vector3());

		} else {

			const int points = 32;

			for (int i = 0; i < points; i++) {

				float s = ll + i * (Math_PI * 2.0) / points;
				float n = ll + (i + 1) * (Math_PI * 2.0) / points;

				//				Vector3 from=Vector3(0,Math::cos(s),-Math::sin(s) )*cs;
				//				Vector3 to=Vector3( 0,Math::cos(n),-Math::sin(n) )*cs;

				Vector3 from;
				SET_VTX(from, 0, Math::cos(s), -Math::sin(s));
				from *= cs;
				Vector3 to;
				SET_VTX(to, 0, Math::cos(n), -Math::sin(n));
				to *= cs;

				cursor_points.push_back(from);
				cursor_points.push_back(to);
			}
		}
	}

#undef ADD_VTX
#undef SET_VTX

	add_collision_segments(cursor_points);
	add_lines(cursor_points, SpatialEditorGizmos::singleton->joint_material);
}

Generic6DOFJointSpatialGizmo::Generic6DOFJointSpatialGizmo(Generic6DOFJoint *p_p3d) {

	p3d = p_p3d;
	set_spatial_node(p3d);
}

///////
///
////

SpatialEditorGizmos *SpatialEditorGizmos::singleton = NULL;

Ref<SpatialEditorGizmo> SpatialEditorGizmos::get_gizmo(Spatial *p_spatial) {

	if (p_spatial->cast_to<Light>()) {

		Ref<LightSpatialGizmo> lsg = memnew(LightSpatialGizmo(p_spatial->cast_to<Light>()));
		return lsg;
	}

	if (p_spatial->cast_to<Listener>()) {

		Ref<ListenerSpatialGizmo> misg = memnew(ListenerSpatialGizmo(p_spatial->cast_to<Listener>()));
		return misg;
	}

	if (p_spatial->cast_to<Camera>()) {

		Ref<CameraSpatialGizmo> lsg = memnew(CameraSpatialGizmo(p_spatial->cast_to<Camera>()));
		return lsg;
	}

	if (p_spatial->cast_to<Skeleton>()) {

		Ref<SkeletonSpatialGizmo> lsg = memnew(SkeletonSpatialGizmo(p_spatial->cast_to<Skeleton>()));
		return lsg;
	}

	if (p_spatial->cast_to<Position3D>()) {

		Ref<Position3DSpatialGizmo> lsg = memnew(Position3DSpatialGizmo(p_spatial->cast_to<Position3D>()));
		return lsg;
	}

	if (p_spatial->cast_to<MeshInstance>()) {

		Ref<MeshInstanceSpatialGizmo> misg = memnew(MeshInstanceSpatialGizmo(p_spatial->cast_to<MeshInstance>()));
		return misg;
	}

	if (p_spatial->cast_to<Room>()) {

		Ref<RoomSpatialGizmo> misg = memnew(RoomSpatialGizmo(p_spatial->cast_to<Room>()));
		return misg;
	}

	if (p_spatial->cast_to<NavigationMeshInstance>()) {

		Ref<NavigationMeshSpatialGizmo> misg = memnew(NavigationMeshSpatialGizmo(p_spatial->cast_to<NavigationMeshInstance>()));
		return misg;
	}

	if (p_spatial->cast_to<RayCast>()) {

		Ref<RayCastSpatialGizmo> misg = memnew(RayCastSpatialGizmo(p_spatial->cast_to<RayCast>()));
		return misg;
	}

	if (p_spatial->cast_to<Portal>()) {

		Ref<PortalSpatialGizmo> misg = memnew(PortalSpatialGizmo(p_spatial->cast_to<Portal>()));
		return misg;
	}

	if (p_spatial->cast_to<TestCube>()) {

		Ref<TestCubeSpatialGizmo> misg = memnew(TestCubeSpatialGizmo(p_spatial->cast_to<TestCube>()));
		return misg;
	}

	if (p_spatial->cast_to<SpatialPlayer>()) {

		Ref<SpatialPlayerSpatialGizmo> misg = memnew(SpatialPlayerSpatialGizmo(p_spatial->cast_to<SpatialPlayer>()));
		return misg;
	}

	if (p_spatial->cast_to<CollisionShape>()) {

		Ref<CollisionShapeSpatialGizmo> misg = memnew(CollisionShapeSpatialGizmo(p_spatial->cast_to<CollisionShape>()));
		return misg;
	}

	if (p_spatial->cast_to<VisibilityNotifier>()) {

		Ref<VisibilityNotifierGizmo> misg = memnew(VisibilityNotifierGizmo(p_spatial->cast_to<VisibilityNotifier>()));
		return misg;
	}

	if (p_spatial->cast_to<VehicleWheel>()) {

		Ref<VehicleWheelSpatialGizmo> misg = memnew(VehicleWheelSpatialGizmo(p_spatial->cast_to<VehicleWheel>()));
		return misg;
	}
	if (p_spatial->cast_to<PinJoint>()) {

		Ref<PinJointSpatialGizmo> misg = memnew(PinJointSpatialGizmo(p_spatial->cast_to<PinJoint>()));
		return misg;
	}

	if (p_spatial->cast_to<HingeJoint>()) {

		Ref<HingeJointSpatialGizmo> misg = memnew(HingeJointSpatialGizmo(p_spatial->cast_to<HingeJoint>()));
		return misg;
	}

	if (p_spatial->cast_to<SliderJoint>()) {

		Ref<SliderJointSpatialGizmo> misg = memnew(SliderJointSpatialGizmo(p_spatial->cast_to<SliderJoint>()));
		return misg;
	}

	if (p_spatial->cast_to<ConeTwistJoint>()) {

		Ref<ConeTwistJointSpatialGizmo> misg = memnew(ConeTwistJointSpatialGizmo(p_spatial->cast_to<ConeTwistJoint>()));
		return misg;
	}

	if (p_spatial->cast_to<Generic6DOFJoint>()) {

		Ref<Generic6DOFJointSpatialGizmo> misg = memnew(Generic6DOFJointSpatialGizmo(p_spatial->cast_to<Generic6DOFJoint>()));
		return misg;
	}

	if (p_spatial->cast_to<CollisionPolygon>()) {

		Ref<CollisionPolygonSpatialGizmo> misg = memnew(CollisionPolygonSpatialGizmo(p_spatial->cast_to<CollisionPolygon>()));
		return misg;
	}

	return Ref<SpatialEditorGizmo>();
}

Ref<FixedMaterial> SpatialEditorGizmos::create_line_material(const Color &p_base_color) {

	Ref<FixedMaterial> line_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	line_material->set_flag(Material::FLAG_UNSHADED, true);
	line_material->set_line_width(3.0);
	line_material->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	line_material->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
	line_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, p_base_color);

	return line_material;
}

Ref<FixedMaterial> SpatialEditorGizmos::create_solid_material(const Color &p_base_color) {

	Ref<FixedMaterial> line_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	line_material->set_flag(Material::FLAG_UNSHADED, true);
	line_material->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	line_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, p_base_color);

	return line_material;
}

SpatialEditorGizmos::SpatialEditorGizmos() {

	singleton = this;

	handle_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	handle_material->set_flag(Material::FLAG_UNSHADED, true);
	handle_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(0.8, 0.8, 0.8));

	handle2_material = Ref<FixedMaterial>(memnew(FixedMaterial));
	handle2_material->set_flag(Material::FLAG_UNSHADED, true);
	handle2_material->set_fixed_flag(FixedMaterial::FLAG_USE_POINT_SIZE, true);
	handle_t = SpatialEditor::get_singleton()->get_icon("Editor3DHandle", "EditorIcons");
	handle2_material->set_point_size(handle_t->get_width());
	handle2_material->set_texture(FixedMaterial::PARAM_DIFFUSE, handle_t);
	handle2_material->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1));
	handle2_material->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	handle2_material->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);

	light_material = create_line_material(Color(1, 1, 0.2));

	light_material_omni_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	light_material_omni_icon->set_flag(Material::FLAG_UNSHADED, true);
	light_material_omni_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	light_material_omni_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	light_material_omni_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	light_material_omni_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	light_material_omni_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("GizmoLight", "EditorIcons"));

	light_material_directional_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	light_material_directional_icon->set_flag(Material::FLAG_UNSHADED, true);
	light_material_directional_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	light_material_directional_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	light_material_directional_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	light_material_directional_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	light_material_directional_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("GizmoDirectionalLight", "EditorIcons"));

	camera_material = create_line_material(Color(1.0, 0.5, 1.0));

	navmesh_edge_material = create_line_material(Color(0.1, 0.8, 1.0));
	navmesh_solid_material = create_solid_material(Color(0.1, 0.8, 1.0, 0.4));
	navmesh_edge_material->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, false);
	navmesh_solid_material->set_flag(Material::FLAG_DOUBLE_SIDED, true);

	navmesh_edge_material_disabled = create_line_material(Color(1.0, 0.8, 0.1));
	navmesh_solid_material_disabled = create_solid_material(Color(1.0, 0.8, 0.1, 0.4));
	navmesh_edge_material_disabled->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, false);
	navmesh_solid_material_disabled->set_flag(Material::FLAG_DOUBLE_SIDED, true);

	skeleton_material = create_line_material(Color(0.6, 1.0, 0.3));
	skeleton_material->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	skeleton_material->set_flag(Material::FLAG_UNSHADED, true);
	skeleton_material->set_flag(Material::FLAG_ONTOP, true);
	skeleton_material->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);

	//position 3D Shared mesh

	pos3d_mesh = Ref<Mesh>(memnew(Mesh));
	{

		DVector<Vector3> cursor_points;
		DVector<Color> cursor_colors;
		float cs = 0.25;
		cursor_points.push_back(Vector3(+cs, 0, 0));
		cursor_points.push_back(Vector3(-cs, 0, 0));
		cursor_points.push_back(Vector3(0, +cs, 0));
		cursor_points.push_back(Vector3(0, -cs, 0));
		cursor_points.push_back(Vector3(0, 0, +cs));
		cursor_points.push_back(Vector3(0, 0, -cs));
		cursor_colors.push_back(Color(1, 0.5, 0.5, 0.7));
		cursor_colors.push_back(Color(1, 0.5, 0.5, 0.7));
		cursor_colors.push_back(Color(0.5, 1, 0.5, 0.7));
		cursor_colors.push_back(Color(0.5, 1, 0.5, 0.7));
		cursor_colors.push_back(Color(0.5, 0.5, 1, 0.7));
		cursor_colors.push_back(Color(0.5, 0.5, 1, 0.7));

		Ref<FixedMaterial> mat = memnew(FixedMaterial);
		mat->set_flag(Material::FLAG_UNSHADED, true);
		mat->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
		mat->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
		mat->set_line_width(3);
		Array d;
		d.resize(VS::ARRAY_MAX);
		d[Mesh::ARRAY_VERTEX] = cursor_points;
		d[Mesh::ARRAY_COLOR] = cursor_colors;
		pos3d_mesh->add_surface(Mesh::PRIMITIVE_LINES, d);
		pos3d_mesh->surface_set_material(0, mat);
	}

	listener_line_mesh = Ref<Mesh>(memnew(Mesh));
	{

		DVector<Vector3> cursor_points;
		DVector<Color> cursor_colors;
		cursor_points.push_back(Vector3(0, 0, 0));
		cursor_points.push_back(Vector3(0, 0, -1.0));
		cursor_colors.push_back(Color(0.5, 0.5, 0.5, 0.7));
		cursor_colors.push_back(Color(0.5, 0.5, 0.5, 0.7));

		Ref<FixedMaterial> mat = memnew(FixedMaterial);
		mat->set_flag(Material::FLAG_UNSHADED, true);
		mat->set_fixed_flag(FixedMaterial::FLAG_USE_COLOR_ARRAY, true);
		mat->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
		mat->set_line_width(3);
		Array d;
		d.resize(VS::ARRAY_MAX);
		d[Mesh::ARRAY_VERTEX] = cursor_points;
		d[Mesh::ARRAY_COLOR] = cursor_colors;
		listener_line_mesh->add_surface(Mesh::PRIMITIVE_LINES, d);
		listener_line_mesh->surface_set_material(0, mat);
	}

	sample_player_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	sample_player_icon->set_flag(Material::FLAG_UNSHADED, true);
	sample_player_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	sample_player_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	sample_player_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	sample_player_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	sample_player_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("GizmoSpatialSamplePlayer", "EditorIcons"));

	room_material = create_line_material(Color(1.0, 0.6, 0.9));
	portal_material = create_line_material(Color(1.0, 0.8, 0.6));
	raycast_material = create_line_material(Color(1.0, 0.8, 0.6));
	car_wheel_material = create_line_material(Color(0.6, 0.8, 1.0));
	visibility_notifier_material = create_line_material(Color(1.0, 0.5, 1.0));
	joint_material = create_line_material(Color(0.6, 0.8, 1.0));

	stream_player_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	stream_player_icon->set_flag(Material::FLAG_UNSHADED, true);
	stream_player_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	stream_player_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	stream_player_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	stream_player_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	stream_player_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("GizmoSpatialStreamPlayer", "EditorIcons"));

	visibility_notifier_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	visibility_notifier_icon->set_flag(Material::FLAG_UNSHADED, true);
	visibility_notifier_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	visibility_notifier_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	visibility_notifier_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	visibility_notifier_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	visibility_notifier_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("Visible", "EditorIcons"));

	listener_icon = Ref<FixedMaterial>(memnew(FixedMaterial));
	listener_icon->set_flag(Material::FLAG_UNSHADED, true);
	listener_icon->set_flag(Material::FLAG_DOUBLE_SIDED, true);
	listener_icon->set_depth_draw_mode(Material::DEPTH_DRAW_NEVER);
	listener_icon->set_fixed_flag(FixedMaterial::FLAG_USE_ALPHA, true);
	listener_icon->set_parameter(FixedMaterial::PARAM_DIFFUSE, Color(1, 1, 1, 0.9));
	listener_icon->set_texture(FixedMaterial::PARAM_DIFFUSE, SpatialEditor::get_singleton()->get_icon("GizmoListener", "EditorIcons"));

	{

		DVector<Vector3> vertices;

#undef ADD_VTX
#define ADD_VTX(m_idx) \
	vertices.push_back(face_points[m_idx]);

		for (int i = 0; i < 6; i++) {

			Vector3 face_points[4];

			for (int j = 0; j < 4; j++) {

				float v[3];
				v[0] = 1.0;
				v[1] = 1 - 2 * ((j >> 1) & 1);
				v[2] = v[1] * (1 - 2 * (j & 1));

				for (int k = 0; k < 3; k++) {

					if (i < 3)
						face_points[j][(i + k) % 3] = v[k] * (i >= 3 ? -1 : 1);
					else
						face_points[3 - j][(i + k) % 3] = v[k] * (i >= 3 ? -1 : 1);
				}
			}
			//tri 1
			ADD_VTX(0);
			ADD_VTX(1);
			ADD_VTX(2);
			//tri 2
			ADD_VTX(2);
			ADD_VTX(3);
			ADD_VTX(0);
		}

		test_cube_tm = Ref<TriangleMesh>(memnew(TriangleMesh));
		test_cube_tm->create(vertices);
	}

	shape_material = create_line_material(Color(0.2, 1, 1.0));
}
