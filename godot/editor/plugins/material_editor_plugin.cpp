#include "material_editor_plugin.h"
#include "scene/main/viewport.h"

void MaterialEditor::_input_event(InputEvent p_event) {
}

void MaterialEditor::_notification(int p_what) {

	if (p_what == NOTIFICATION_FIXED_PROCESS) {
	}

	if (p_what == NOTIFICATION_READY) {

		//get_scene()->connect("node_removed",this,"_node_removed");

		if (first_enter) {
			//it's in propertyeditor so.. could be moved around

			light_1_switch->set_normal_texture(get_icon("MaterialPreviewLight1", "EditorIcons"));
			light_1_switch->set_pressed_texture(get_icon("MaterialPreviewLight1Off", "EditorIcons"));
			light_2_switch->set_normal_texture(get_icon("MaterialPreviewLight2", "EditorIcons"));
			light_2_switch->set_pressed_texture(get_icon("MaterialPreviewLight2Off", "EditorIcons"));

			sphere_switch->set_normal_texture(get_icon("MaterialPreviewSphereOff", "EditorIcons"));
			sphere_switch->set_pressed_texture(get_icon("MaterialPreviewSphere", "EditorIcons"));
			box_switch->set_normal_texture(get_icon("MaterialPreviewCubeOff", "EditorIcons"));
			box_switch->set_pressed_texture(get_icon("MaterialPreviewCube", "EditorIcons"));

			first_enter = false;
		}
	}

	if (p_what == NOTIFICATION_DRAW) {

		Ref<Texture> checkerboard = get_icon("Checkerboard", "EditorIcons");
		Size2 size = get_size();

		draw_texture_rect(checkerboard, Rect2(Point2(), size), true);
	}
}

void MaterialEditor::edit(Ref<Material> p_material) {

	material = p_material;

	if (!material.is_null()) {
		sphere_mesh->surface_set_material(0, material);
		box_mesh->surface_set_material(0, material);
	} else {

		hide();
	}
}

void MaterialEditor::_button_pressed(Node *p_button) {

	if (p_button == light_1_switch) {
		light1->set_enabled(!light_1_switch->is_pressed());
	}

	if (p_button == light_2_switch) {
		light2->set_enabled(!light_2_switch->is_pressed());
	}

	if (p_button == box_switch) {
		box_instance->show();
		sphere_instance->hide();
		box_switch->set_pressed(true);
		sphere_switch->set_pressed(false);
	}

	if (p_button == sphere_switch) {
		box_instance->hide();
		sphere_instance->show();
		box_switch->set_pressed(false);
		sphere_switch->set_pressed(true);
	}
}

void MaterialEditor::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("_input_event"), &MaterialEditor::_input_event);
	ObjectTypeDB::bind_method(_MD("_button_pressed"), &MaterialEditor::_button_pressed);
}

MaterialEditor::MaterialEditor() {

	viewport = memnew(Viewport);
	Ref<World> world;
	world.instance();
	viewport->set_world(world); //use own world
	add_child(viewport);
	viewport->set_disable_input(true);

	camera = memnew(Camera);
	camera->set_transform(Transform(Matrix3(), Vector3(0, 0, 3)));
	camera->set_perspective(45, 0.1, 10);
	viewport->add_child(camera);

	light1 = memnew(DirectionalLight);
	light1->set_transform(Transform().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));
	viewport->add_child(light1);

	light2 = memnew(DirectionalLight);
	light2->set_transform(Transform().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));
	light2->set_color(Light::COLOR_DIFFUSE, Color(0.7, 0.7, 0.7));
	light2->set_color(Light::COLOR_SPECULAR, Color(0.7, 0.7, 0.7));
	viewport->add_child(light2);

	sphere_instance = memnew(MeshInstance);
	viewport->add_child(sphere_instance);

	box_instance = memnew(MeshInstance);
	viewport->add_child(box_instance);

	Transform box_xform;
	box_xform.basis.rotate(Vector3(1, 0, 0), Math::deg2rad(-25));
	box_xform.basis = box_xform.basis * Matrix3().rotated(Vector3(0, 1, 0), Math::deg2rad(-25));
	box_xform.basis.scale(Vector3(0.8, 0.8, 0.8));
	box_instance->set_transform(box_xform);

	{

		sphere_mesh.instance();

		int lats = 32;
		int lons = 32;
		float radius = 1.0;

		DVector<Vector3> vertices;
		DVector<Vector3> normals;
		DVector<Vector2> uvs;
		DVector<float> tangents;
		Matrix3 tt = Matrix3(Vector3(0, 1, 0), Math_PI * 0.5);

		for (int i = 1; i <= lats; i++) {
			double lat0 = Math_PI * (-0.5 + (double)(i - 1) / lats);
			double z0 = Math::sin(lat0);
			double zr0 = Math::cos(lat0);

			double lat1 = Math_PI * (-0.5 + (double)i / lats);
			double z1 = Math::sin(lat1);
			double zr1 = Math::cos(lat1);

			for (int j = lons; j >= 1; j--) {

				double lng0 = 2 * Math_PI * (double)(j - 1) / lons;
				double x0 = Math::cos(lng0);
				double y0 = Math::sin(lng0);

				double lng1 = 2 * Math_PI * (double)(j) / lons;
				double x1 = Math::cos(lng1);
				double y1 = Math::sin(lng1);

				Vector3 v[4] = {
					Vector3(x1 * zr0, z0, y1 * zr0),
					Vector3(x1 * zr1, z1, y1 * zr1),
					Vector3(x0 * zr1, z1, y0 * zr1),
					Vector3(x0 * zr0, z0, y0 * zr0)
				};

#define ADD_POINT(m_idx)                                                                       \
	normals.push_back(v[m_idx]);                                                               \
	vertices.push_back(v[m_idx] * radius);                                                     \
	{                                                                                          \
		Vector2 uv(Math::atan2(v[m_idx].x, v[m_idx].z), Math::atan2(-v[m_idx].y, v[m_idx].z)); \
		uv /= Math_PI;                                                                         \
		uv *= 4.0;                                                                             \
		uv = uv * 0.5 + Vector2(0.5, 0.5);                                                     \
		uvs.push_back(uv);                                                                     \
	}                                                                                          \
	{                                                                                          \
		Vector3 t = tt.xform(v[m_idx]);                                                        \
		tangents.push_back(t.x);                                                               \
		tangents.push_back(t.y);                                                               \
		tangents.push_back(t.z);                                                               \
		tangents.push_back(1.0);                                                               \
	}

				ADD_POINT(0);
				ADD_POINT(1);
				ADD_POINT(2);

				ADD_POINT(2);
				ADD_POINT(3);
				ADD_POINT(0);
			}
		}

		Array arr;
		arr.resize(VS::ARRAY_MAX);
		arr[VS::ARRAY_VERTEX] = vertices;
		arr[VS::ARRAY_NORMAL] = normals;
		arr[VS::ARRAY_TANGENT] = tangents;
		arr[VS::ARRAY_TEX_UV] = uvs;

		sphere_mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, arr);

		sphere_instance->set_mesh(sphere_mesh);
	}
	{

		box_mesh.instance();

		DVector<Vector3> vertices;
		DVector<Vector3> normals;
		DVector<float> tangents;
		DVector<Vector3> uvs;

		int vtx_idx = 0;
#define ADD_VTX(m_idx)                                                             \
	;                                                                              \
	vertices.push_back(face_points[m_idx]);                                        \
	normals.push_back(normal_points[m_idx]);                                       \
	tangents.push_back(normal_points[m_idx][1]);                                   \
	tangents.push_back(normal_points[m_idx][2]);                                   \
	tangents.push_back(normal_points[m_idx][0]);                                   \
	tangents.push_back(1.0);                                                       \
	uvs.push_back(Vector3(uv_points[m_idx * 2 + 0], uv_points[m_idx * 2 + 1], 0)); \
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
		d[VisualServer::ARRAY_TANGENT] = tangents;
		d[VisualServer::ARRAY_TEX_UV] = uvs;
		d[VisualServer::ARRAY_VERTEX] = vertices;

		DVector<int> indices;
		indices.resize(vertices.size());
		for (int i = 0; i < vertices.size(); i++)
			indices.set(i, i);
		d[VisualServer::ARRAY_INDEX] = indices;

		box_mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, d);
		box_instance->set_mesh(box_mesh);
		box_instance->hide();
	}

	set_custom_minimum_size(Size2(1, 150) * EDSCALE);

	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);
	hb->set_area_as_parent_rect(2);

	VBoxContainer *vb_shape = memnew(VBoxContainer);
	hb->add_child(vb_shape);

	sphere_switch = memnew(TextureButton);
	sphere_switch->set_toggle_mode(true);
	sphere_switch->set_pressed(true);
	vb_shape->add_child(sphere_switch);
	sphere_switch->connect("pressed", this, "_button_pressed", varray(sphere_switch));

	box_switch = memnew(TextureButton);
	box_switch->set_toggle_mode(true);
	box_switch->set_pressed(false);
	vb_shape->add_child(box_switch);
	box_switch->connect("pressed", this, "_button_pressed", varray(box_switch));

	hb->add_spacer();

	VBoxContainer *vb_light = memnew(VBoxContainer);
	hb->add_child(vb_light);

	light_1_switch = memnew(TextureButton);
	light_1_switch->set_toggle_mode(true);
	vb_light->add_child(light_1_switch);
	light_1_switch->connect("pressed", this, "_button_pressed", varray(light_1_switch));

	light_2_switch = memnew(TextureButton);
	light_2_switch->set_toggle_mode(true);
	vb_light->add_child(light_2_switch);
	light_2_switch->connect("pressed", this, "_button_pressed", varray(light_2_switch));

	first_enter = true;
}

void MaterialEditorPlugin::edit(Object *p_object) {

	Material *s = p_object->cast_to<Material>();
	if (!s)
		return;

	material_editor->edit(Ref<Material>(s));
}

bool MaterialEditorPlugin::handles(Object *p_object) const {

	return p_object->is_type("Material");
}

void MaterialEditorPlugin::make_visible(bool p_visible) {

	if (p_visible) {
		material_editor->show();
		//		material_editor->set_process(true);
	} else {

		material_editor->hide();
		//		material_editor->set_process(false);
	}
}

MaterialEditorPlugin::MaterialEditorPlugin(EditorNode *p_node) {

	editor = p_node;
	material_editor = memnew(MaterialEditor);
	add_control_to_container(CONTAINER_PROPERTY_EDITOR_BOTTOM, material_editor);
	material_editor->hide();
}

MaterialEditorPlugin::~MaterialEditorPlugin() {
}
