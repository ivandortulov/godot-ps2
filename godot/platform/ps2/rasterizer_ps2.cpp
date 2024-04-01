#include "rasterizer_ps2.hpp"

#include <stdint.h>

#include <packet.h>
#include <dma.h>
#include <graph.h>
#include <gs_psm.h>


RasterizerPS2::RasterizerPS2()
{

}

RasterizerPS2::~RasterizerPS2()
{

}

/* TEXTURE API */

RID RasterizerPS2::texture_create()
{
	Texture* texture = memnew(Texture);
	ERR_FAIL_COND_V(!texture, RID());
	return texture_owner.make_rid(texture);
}

void RasterizerPS2::texture_allocate(
	RID p_texture,
	int p_width,
	int p_height,
	Image::Format p_format,
	uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT)
{
	Texture* texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	texture->width = p_width;
	texture->height = p_height;
	texture->format = p_format;
	texture->flags = p_flags;
}

void RasterizerPS2::texture_set_data(
	RID p_texture,
	const Image& p_image,
	VS::CubeMapSide p_cube_side = VS::CUBEMAP_LEFT)
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(texture->format != p_image.get_format());

	texture->image[p_cube_side] = p_image;
}

Image RasterizerPS2::texture_get_data(
	RID p_texture,
	VS::CubeMapSide
	p_cube_side) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, Image());

	return texture->image[p_cube_side];
}

void RasterizerPS2::texture_set_flags(RID p_texture, uint32_t p_flags)
{
	Texture* texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	uint32_t cube = texture->flags & VS::TEXTURE_FLAG_CUBEMAP;
	texture->flags = p_flags | cube;
}

uint32_t RasterizerPS2::texture_get_flags(RID p_texture) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->flags;
}

Image::Format RasterizerPS2::texture_get_format(RID p_texture) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, Image::FORMAT_GRAYSCALE);

	return texture->format;
}

uint32_t RasterizerPS2::texture_get_width(RID p_texture) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->width;
}

uint32_t RasterizerPS2::texture_get_height(RID p_texture) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return texture->height;
}

bool RasterizerPS2::texture_has_alpha(RID p_texture) const
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture, 0);

	return false;
}

void RasterizerPS2::texture_set_size_override(RID p_texture, int p_width, int p_height)
{
	Texture* texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);

	ERR_FAIL_COND(p_width <= 0 || p_width > 4096);
	ERR_FAIL_COND(p_height <= 0 || p_height > 4096);
	//real texture size is in alloc width and height
	//	texture->width=p_width;
	//	texture->height=p_height;
}

void RasterizerPS2::texture_set_reload_hook(
	RID p_texture, ObjectID p_owner, const StringName& p_function) const
{

}


void RasterizerPS2::texture_set_path(RID p_texture, const String& p_path)
{

}

String RasterizerPS2::texture_get_path(RID p_texture) const
{
	return String();
}

void RasterizerPS2::texture_debug_usage(List<VS::TextureInfo>* r_info)
{

}

void RasterizerPS2::texture_set_shrink_all_x2_on_set_data(bool p_enable)
{
	shrink_textures_x2 = p_enable;
}

/* SHADER API */

RID RasterizerPS2::shader_create(VS::ShaderMode p_mode = VS::SHADER_MATERIAL)
{
	Shader* shader = memnew(Shader);
	shader->mode = p_mode;
	shader->fragment_line = 0;
	shader->vertex_line = 0;
	shader->light_line = 0;
	RID rid = shader_owner.make_rid(shader);

	return rid;
}


void RasterizerPS2::shader_set_mode(RID p_shader, VS::ShaderMode p_mode)
{
	ERR_FAIL_INDEX(p_mode, 3);
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
	shader->mode = p_mode;
}

VS::ShaderMode RasterizerPS2::shader_get_mode(RID p_shader) const
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, VS::SHADER_MATERIAL);
	return shader->mode;
}


void RasterizerPS2::shader_set_code(
	RID p_shader,
	const String& p_vertex,
	const String& p_fragment,
	const String& p_light,
	int p_vertex_ofs = 0,
	int p_fragment_ofs = 0,
	int p_light_ofs = 0)
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
	shader->fragment_code = p_fragment;
	shader->vertex_code = p_vertex;
	shader->light_code = p_light;
	shader->fragment_line = p_fragment_ofs;
	shader->vertex_line = p_vertex_ofs;
	shader->light_line = p_vertex_ofs;
}

String RasterizerPS2::shader_get_fragment_code(RID p_shader) const
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, String());
	return shader->fragment_code;
}

String RasterizerPS2::shader_get_vertex_code(RID p_shader) const
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, String());
	return shader->vertex_code;
}

String RasterizerPS2::shader_get_light_code(RID p_shader) const
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader, String());
	return shader->light_code;
}


void RasterizerPS2::shader_get_param_list(RID p_shader, List<PropertyInfo>* p_param_list) const
{
	Shader* shader = shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
}


void RasterizerPS2::shader_set_default_texture_param(RID p_shader, const StringName& p_name, RID p_texture)
{

}

RID RasterizerPS2::shader_get_default_texture_param(RID p_shader, const StringName& p_name) const
{
	return RID();
}


Variant RasterizerPS2::shader_get_default_param(RID p_shader, const StringName& p_name)
{
	return Variant();
}


/* COMMON MATERIAL API */

RID RasterizerPS2::material_create()
{
	return material_owner.make_rid(memnew(Material));
}


void RasterizerPS2::material_set_shader(RID p_shader_material, RID p_shader)
{
	Material* material = material_owner.get(p_shader_material);
	ERR_FAIL_COND(!material);
	material->shader = p_shader;
}

RID RasterizerPS2::material_get_shader(RID p_shader_material) const
{
	Material* material = material_owner.get(p_shader_material);
	ERR_FAIL_COND_V(!material, RID());
	return material->shader;
}


void RasterizerPS2::material_set_param(
	RID p_material, const StringName& p_param, const Variant& p_value)
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	if (p_value.get_type() == Variant::NIL)
		material->shader_params.erase(p_param);
	else
		material->shader_params[p_param] = p_value;
}

Variant RasterizerPS2::material_get_param(
	RID p_material, const StringName& p_param) const
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, Variant());

	if (material->shader_params.has(p_param))
		return material->shader_params[p_param];
	else
		return Variant();
}


void RasterizerPS2::material_set_flag(RID p_material, VS::MaterialFlag p_flag, bool p_enabled)
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	ERR_FAIL_INDEX(p_flag, VS::MATERIAL_FLAG_MAX);
	material->flags[p_flag] = p_enabled;
}

bool RasterizerPS2::material_get_flag(RID p_material, VS::MaterialFlag p_flag) const
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, false);
	ERR_FAIL_INDEX_V(p_flag, VS::MATERIAL_FLAG_MAX, false);
	return material->flags[p_flag];
}


void RasterizerPS2::material_set_depth_draw_mode(RID p_material, VS::MaterialDepthDrawMode p_mode)
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->depth_draw_mode = p_mode;
}

VS::MaterialDepthDrawMode RasterizerPS2::material_get_depth_draw_mode(
	RID p_material) const
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, VS::MATERIAL_DEPTH_DRAW_ALWAYS);
	return material->depth_draw_mode;
}


void RasterizerPS2::material_set_blend_mode(
	RID p_material, VS::MaterialBlendMode p_mode)
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->blend_mode = p_mode;
}

VS::MaterialBlendMode RasterizerPS2::material_get_blend_mode(
	RID p_material) const
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, VS::MATERIAL_BLEND_MODE_ADD);
	return material->blend_mode;
}


void RasterizerPS2::material_set_line_width(RID p_material, float p_line_width)
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->line_width = p_line_width;
}

float RasterizerPS2::material_get_line_width(RID p_material) const
{
	Material* material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material, 0);

	return material->line_width;
}


/* MESH API */

RID RasterizerPS2::mesh_create()
{
	return mesh_owner.make_rid(memnew(Mesh));
}


void RasterizerPS2::mesh_add_surface(RID p_mesh, VS::PrimitiveType p_primitive, const Array& p_arrays, const Array& p_blend_shapes = Array(), bool p_alpha_sort = false)
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);

	ERR_FAIL_INDEX(p_primitive, VS::PRIMITIVE_MAX);
	ERR_FAIL_COND(p_arrays.size() != VS::ARRAY_MAX);

	Surface s;

	s.format = 0;

	for (int i = 0; i < p_arrays.size(); i++) {

		if (p_arrays[i].get_type() == Variant::NIL)
			continue;

		s.format |= (1 << i);

		if (i == VS::ARRAY_VERTEX) {

			Vector3Array v = p_arrays[i];
			int len = v.size();
			ERR_FAIL_COND(len == 0);
			Vector3Array::Read r = v.read();

			for (int i = 0; i < len; i++) {

				if (i == 0)
					s.aabb.pos = r[0];
				else
					s.aabb.expand_to(r[i]);
			}
		}
	}

	ERR_FAIL_COND((s.format & VS::ARRAY_FORMAT_VERTEX) == 0); // mandatory

	s.data = p_arrays;
	s.morph_data = p_blend_shapes;
	s.primitive = p_primitive;
	s.alpha_sort = p_alpha_sort;
	s.morph_target_count = mesh->morph_target_count;
	s.morph_format = s.format;

	Surface* surface = memnew(Surface);
	*surface = s;

	mesh->surfaces.push_back(surface);
}

Array RasterizerPS2::mesh_get_surface_arrays(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, Array());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Array());
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, Array());

	return surface->data;
}

Array RasterizerPS2::mesh_get_surface_morph_arrays(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, Array());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Array());
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, Array());

	return surface->morph_data;
}

void RasterizerPS2::mesh_add_custom_surface(RID p_mesh, const Variant& p_dat)
{
	ERR_EXPLAIN("Dummy Rasterizer does not support custom surfaces. Running on wrong platform?");
	ERR_FAIL_V();
}


void RasterizerPS2::mesh_set_morph_target_count(RID p_mesh, int p_amount)
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_COND(mesh->surfaces.size() != 0);

	mesh->morph_target_count = p_amount;
}

int RasterizerPS2::mesh_get_morph_target_count(RID p_mesh) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, -1);

	return mesh->morph_target_count;
}


void RasterizerPS2::mesh_set_morph_target_mode(RID p_mesh, VS::MorphTargetMode p_mode)
{
	ERR_FAIL_INDEX(p_mode, 2);
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->morph_target_mode = p_mode;
}

VS::MorphTargetMode RasterizerPS2::mesh_get_morph_target_mode(RID p_mesh) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::MORPH_MODE_NORMALIZED);

	return mesh->morph_target_mode;
}


void RasterizerPS2::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material, bool p_owned = false)
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND(!surface);

	if (surface->material_owned && surface->material.is_valid())
		free(surface->material);

	surface->material_owned = p_owned;
	surface->material = p_material;
}

RID RasterizerPS2::mesh_surface_get_material(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, RID());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), RID());
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, RID());

	return surface->material;
}


int RasterizerPS2::mesh_surface_get_array_len(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, -1);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), -1);
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, -1);

	Vector3Array arr = surface->data[VS::ARRAY_VERTEX];
	return arr.size();
}

int RasterizerPS2::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, -1);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), -1);
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, -1);

	IntArray arr = surface->data[VS::ARRAY_INDEX];
	return arr.size();
}

uint32_t RasterizerPS2::mesh_surface_get_format(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, 0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, 0);

	return surface->format;
}

VS::PrimitiveType RasterizerPS2::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, VS::PRIMITIVE_POINTS);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), VS::PRIMITIVE_POINTS);
	Surface* surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V(!surface, VS::PRIMITIVE_POINTS);

	return surface->primitive;
}


void RasterizerPS2::mesh_remove_surface(RID p_mesh, int p_index)
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_index, mesh->surfaces.size());
	Surface* surface = mesh->surfaces[p_index];
	ERR_FAIL_COND(!surface);

	memdelete(mesh->surfaces[p_index]);
	mesh->surfaces.remove(p_index);
}

int RasterizerPS2::mesh_get_surface_count(RID p_mesh) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, -1);

	return mesh->surfaces.size();
}


AABB RasterizerPS2::mesh_get_aabb(RID p_mesh, RID p_skeleton = RID()) const
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());

	AABB aabb;

	for (int i = 0; i < mesh->surfaces.size(); i++) {

		if (i == 0)
			aabb = mesh->surfaces[i]->aabb;
		else
			aabb.merge_with(mesh->surfaces[i]->aabb);
	}

	return aabb;
}


void RasterizerPS2::mesh_set_custom_aabb(RID p_mesh, const AABB& p_aabb)
{
	Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);

	mesh->custom_aabb = p_aabb;
}

AABB RasterizerPS2::mesh_get_custom_aabb(RID p_mesh) const
{
	const Mesh* mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND_V(!mesh, AABB());

	return mesh->custom_aabb;
}


/* MULTIMESH API */

RID RasterizerPS2::multimesh_create()
{
	return multimesh_owner.make_rid(memnew(MultiMesh));
}


void RasterizerPS2::multimesh_set_instance_count(RID p_multimesh, int p_count)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->elements.clear(); // make sure to delete everything, so it "fails" in all implementations
	multimesh->elements.resize(p_count);
}

int RasterizerPS2::multimesh_get_instance_count(RID p_multimesh) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, -1);

	return multimesh->elements.size();
}


void RasterizerPS2::multimesh_set_mesh(RID p_multimesh, RID p_mesh)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->mesh = p_mesh;
}

void RasterizerPS2::multimesh_set_aabb(RID p_multimesh, const AABB& p_aabb)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	multimesh->aabb = p_aabb;
}

void RasterizerPS2::multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform& p_transform)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->elements.size());
	multimesh->elements[p_index].xform = p_transform;
}

void RasterizerPS2::multimesh_instance_set_color(RID p_multimesh, int p_index, const Color& p_color)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->elements.size());
	multimesh->elements[p_index].color = p_color;
}


RID RasterizerPS2::multimesh_get_mesh(RID p_multimesh) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, RID());

	return multimesh->mesh;
}

AABB RasterizerPS2::multimesh_get_aabb(RID p_multimesh) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, AABB());

	return multimesh->aabb;
}


Transform RasterizerPS2::multimesh_instance_get_transform(
	RID p_multimesh, int p_index) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Transform());

	ERR_FAIL_INDEX_V(p_index, multimesh->elements.size(), Transform());

	return multimesh->elements[p_index].xform;
}

Color RasterizerPS2::multimesh_instance_get_color(
	RID p_multimesh, int p_index) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->elements.size(), Color());

	return multimesh->elements[p_index].color;
}


void RasterizerPS2::multimesh_set_visible_instances(RID p_multimesh, int p_visible)
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	multimesh->visible = p_visible;
}

int RasterizerPS2::multimesh_get_visible_instances(RID p_multimesh) const
{
	MultiMesh* multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh, -1);
	return multimesh->visible;
}


/* IMMEDIATE API */

RID RasterizerPS2::immediate_create()
{
	Immediate* im = memnew(Immediate);
	return immediate_owner.make_rid(im);
}

void RasterizerPS2::immediate_begin(
	RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID())
{

}

void RasterizerPS2::immediate_vertex(RID p_immediate, const Vector3& p_vertex)
{

}

void RasterizerPS2::immediate_normal(RID p_immediate, const Vector3& p_normal)
{

}

void RasterizerPS2::immediate_tangent(RID p_immediate, const Plane& p_tangent)
{

}

void RasterizerPS2::immediate_color(RID p_immediate, const Color& p_color)
{

}

void RasterizerPS2::immediate_uv(RID p_immediate, const Vector2& tex_uv)
{

}

void RasterizerPS2::immediate_uv2(RID p_immediate, const Vector2& tex_uv)
{

}

void RasterizerPS2::immediate_end(RID p_immediate)
{

}

void RasterizerPS2::immediate_clear(RID p_immediate)
{

}

void RasterizerPS2::immediate_set_material(RID p_immediate, RID p_material)
{
	Immediate* im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	im->material = p_material;
}

RID RasterizerPS2::immediate_get_material(RID p_immediate) const
{
	const Immediate* im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im, RID());
	return im->material;
}


AABB RasterizerPS2::immediate_get_aabb(RID p_mesh) const
{
	return AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
}


/* PARTICLES API */

RID RasterizerPS2::particles_create()
{
	Particles* particles = memnew(Particles);
	ERR_FAIL_COND_V(!particles, RID());
	return particles_owner.make_rid(particles);
}


void RasterizerPS2::particles_set_amount(RID p_particles, int p_amount)
{
	ERR_FAIL_COND(p_amount < 1);
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.amount = p_amount;
}

int RasterizerPS2::particles_get_amount(RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.amount;
}

void RasterizerPS2::particles_set_emitting(RID p_particles, bool p_emitting)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.emitting = p_emitting;
}

bool RasterizerPS2::particles_is_emitting(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, false);
	return particles->data.emitting;
}


void RasterizerPS2::particles_set_visibility_aabb(
	RID p_particles, const AABB& p_visibility)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.visibility_aabb = p_visibility;
}

AABB RasterizerPS2::particles_get_visibility_aabb(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, AABB());
	return particles->data.visibility_aabb;
}


void RasterizerPS2::particles_set_emission_half_extents(
	RID p_particles, const Vector3& p_half_extents)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);

	particles->data.emission_half_extents = p_half_extents;
}

Vector3 RasterizerPS2::particles_get_emission_half_extents(RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, Vector3());

	return particles->data.emission_half_extents;
}


void RasterizerPS2::particles_set_emission_base_velocity(
	RID p_particles, const Vector3& p_base_velocity)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);

	particles->data.emission_base_velocity = p_base_velocity;
}

Vector3 RasterizerPS2::particles_get_emission_base_velocity(
	RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, Vector3());

	return particles->data.emission_base_velocity;
}


void RasterizerPS2::particles_set_emission_points(
	RID p_particles, const DVector<Vector3>& p_points)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);

	particles->data.emission_points = p_points;
}

DVector<Vector3> RasterizerPS2::particles_get_emission_points(
	RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, DVector<Vector3>());

	return particles->data.emission_points;
}


void RasterizerPS2::particles_set_gravity_normal(
	RID p_particles, const Vector3& p_normal)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);

	particles->data.gravity_normal = p_normal;
}

Vector3 RasterizerPS2::particles_get_gravity_normal(RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, Vector3());

	return particles->data.gravity_normal;
}


void RasterizerPS2::particles_set_variable(
	RID p_particles, VS::ParticleVariable p_variable, float p_value)
{
	ERR_FAIL_INDEX(p_variable, VS::PARTICLE_VAR_MAX);

	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.particle_vars[p_variable] = p_value;
}

float RasterizerPS2::particles_get_variable(
	RID p_particles, VS::ParticleVariable p_variable) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.particle_vars[p_variable];
}


void RasterizerPS2::particles_set_randomness(
	RID p_particles, VS::ParticleVariable p_variable, float p_randomness)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.particle_randomness[p_variable] = p_randomness;
}

float RasterizerPS2::particles_get_randomness(
	RID p_particles, VS::ParticleVariable p_variable) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.particle_randomness[p_variable];
}


void RasterizerPS2::particles_set_color_phase_pos(
	RID p_particles, int p_phase, float p_pos)
{
	ERR_FAIL_INDEX(p_phase, VS::MAX_PARTICLE_COLOR_PHASES);
	if (p_pos < 0.0)
		p_pos = 0.0;
	if (p_pos > 1.0)
		p_pos = 1.0;

	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.color_phases[p_phase].pos = p_pos;
}

float RasterizerPS2::particles_get_color_phase_pos(
	RID p_particles, int p_phase) const
{
	ERR_FAIL_INDEX_V(p_phase, VS::MAX_PARTICLE_COLOR_PHASES, -1.0);

	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.color_phases[p_phase].pos;
}


void RasterizerPS2::particles_set_color_phases(RID p_particles, int p_phases)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	ERR_FAIL_COND(p_phases < 0 || p_phases > VS::MAX_PARTICLE_COLOR_PHASES);
	particles->data.color_phase_count = p_phases;
}

int RasterizerPS2::particles_get_color_phases(RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.color_phase_count;
}


void RasterizerPS2::particles_set_color_phase_color(
	RID p_particles, int p_phase, const Color& p_color)
{
	ERR_FAIL_INDEX(p_phase, VS::MAX_PARTICLE_COLOR_PHASES);
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.color_phases[p_phase].color = p_color;

	//update alpha
	particles->has_alpha = false;
	for (int i = 0; i < VS::MAX_PARTICLE_COLOR_PHASES; i++) {
		if (particles->data.color_phases[i].color.a < 0.99)
			particles->has_alpha = true;
	}
}

Color RasterizerPS2::particles_get_color_phase_color(
	RID p_particles, int p_phase) const
{
	ERR_FAIL_INDEX_V(p_phase, VS::MAX_PARTICLE_COLOR_PHASES, Color());

	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, Color());
	return particles->data.color_phases[p_phase].color;
}


void RasterizerPS2::particles_set_attractors(RID p_particles, int p_attractors)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	ERR_FAIL_COND(p_attractors < 0 || p_attractors > VisualServer::MAX_PARTICLE_ATTRACTORS);
	particles->data.attractor_count = p_attractors;
}

int RasterizerPS2::particles_get_attractors(RID p_particles) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, -1);
	return particles->data.attractor_count;
}


void RasterizerPS2::particles_set_attractor_pos(
	RID p_particles, int p_attractor, const Vector3& p_pos)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	ERR_FAIL_INDEX(p_attractor, particles->data.attractor_count);
	particles->data.attractors[p_attractor].pos = p_pos;
}

Vector3 RasterizerPS2::particles_get_attractor_pos(
	RID p_particles, int p_attractor) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, Vector3());
	ERR_FAIL_INDEX_V(p_attractor, particles->data.attractor_count, Vector3());
	return particles->data.attractors[p_attractor].pos;
}


void RasterizerPS2::particles_set_attractor_strength(RID p_particles, int p_attractor, float p_force)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	ERR_FAIL_INDEX(p_attractor, particles->data.attractor_count);
	particles->data.attractors[p_attractor].force = p_force;
}

float RasterizerPS2::particles_get_attractor_strength(
	RID p_particles, int p_attractor) const
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, 0);
	ERR_FAIL_INDEX_V(p_attractor, particles->data.attractor_count, 0);
	return particles->data.attractors[p_attractor].force;
}


void RasterizerPS2::particles_set_material(
	RID p_particles, RID p_material, bool p_owned = false)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	if (particles->material_owned && particles->material.is_valid())
		free(particles->material);

	particles->material_owned = p_owned;

	particles->material = p_material;
}

RID RasterizerPS2::particles_get_material(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, RID());
	return particles->material;
}


AABB RasterizerPS2::particles_get_aabb(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, AABB());
	return particles->data.visibility_aabb;
}


void RasterizerPS2::particles_set_height_from_velocity(
	RID p_particles, bool p_enable)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.height_from_velocity = p_enable;
}

bool RasterizerPS2::particles_has_height_from_velocity(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, false);
	return particles->data.height_from_velocity;
}


void RasterizerPS2::particles_set_use_local_coordinates(
	RID p_particles, bool p_enable)
{
	Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND(!particles);
	particles->data.local_coordinates = p_enable;
}

bool RasterizerPS2::particles_is_using_local_coordinates(RID p_particles) const
{
	const Particles* particles = particles_owner.get(p_particles);
	ERR_FAIL_COND_V(!particles, false);
	return particles->data.local_coordinates;
}


/* SKELETON API */

RID RasterizerPS2::skeleton_create()
{
	Skeleton* skeleton = memnew(Skeleton);
	ERR_FAIL_COND_V(!skeleton, RID());
	return skeleton_owner.make_rid(skeleton);
}

void RasterizerPS2::skeleton_resize(RID p_skeleton, int p_bones)
{
	Skeleton* skeleton = skeleton_owner.get(p_skeleton);
	ERR_FAIL_COND(!skeleton);
	if (p_bones == skeleton->bones.size())
	{
		return;
	};

	skeleton->bones.resize(p_bones);
}

int RasterizerPS2::skeleton_get_bone_count(RID p_skeleton) const
{
	Skeleton* skeleton = skeleton_owner.get(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, -1);
	return skeleton->bones.size();
}

void RasterizerPS2::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform& p_transform)
{
	Skeleton* skeleton = skeleton_owner.get(p_skeleton);
	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->bones.size());

	skeleton->bones[p_bone] = p_transform;
}

Transform RasterizerPS2::skeleton_bone_get_transform(RID p_skeleton, int p_bone)
{
	Skeleton* skeleton = skeleton_owner.get(p_skeleton);
	ERR_FAIL_COND_V(!skeleton, Transform());
	ERR_FAIL_INDEX_V(p_bone, skeleton->bones.size(), Transform());

	// something
	return skeleton->bones[p_bone];
}


/* LIGHT API */

RID RasterizerPS2::light_create(VS::LightType p_type)
{
	Light* light = memnew(Light);
	light->type = p_type;
	return light_owner.make_rid(light);
}

VS::LightType RasterizerPS2::light_get_type(RID p_light) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, VS::LIGHT_OMNI);
	return light->type;
}


void RasterizerPS2::light_set_color(
	RID p_light, VS::LightColor p_type, const Color& p_color)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	ERR_FAIL_INDEX(p_type, 3);
	light->colors[p_type] = p_color;
}

Color RasterizerPS2::light_get_color(RID p_light, VS::LightColor p_type) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, Color());
	ERR_FAIL_INDEX_V(p_type, 3, Color());
	return light->colors[p_type];
}


void RasterizerPS2::light_set_shadow(RID p_light, bool p_enabled)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->shadow_enabled = p_enabled;
}

bool RasterizerPS2::light_has_shadow(RID p_light) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, false);
	return light->shadow_enabled;
}


void RasterizerPS2::light_set_volumetric(RID p_light, bool p_enabled)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->volumetric_enabled = p_enabled;
}

bool RasterizerPS2::light_is_volumetric(RID p_light) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, false);
	return light->volumetric_enabled;
}


void RasterizerPS2::light_set_projector(RID p_light, RID p_texture)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->projector = p_texture;
}

RID RasterizerPS2::light_get_projector(RID p_light) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, RID());
	return light->projector;
}


void RasterizerPS2::light_set_var(RID p_light, VS::LightParam p_var, float p_value)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	ERR_FAIL_INDEX(p_var, VS::LIGHT_PARAM_MAX);

	light->vars[p_var] = p_value;
}

float RasterizerPS2::light_get_var(RID p_light, VS::LightParam p_var) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, 0);

	ERR_FAIL_INDEX_V(p_var, VS::LIGHT_PARAM_MAX, 0);

	return light->vars[p_var];
}


void RasterizerPS2::light_set_operator(RID p_light, VS::LightOp p_op)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
}

VS::LightOp RasterizerPS2::light_get_operator(RID p_light) const
{
	return VS::LightOp(0);
}


void RasterizerPS2::light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode)
{

}

VS::LightOmniShadowMode RasterizerPS2::light_omni_get_shadow_mode(RID p_light) const
{
	return VS::LightOmniShadowMode(0);
}


void RasterizerPS2::light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode)
{

}

VS::LightDirectionalShadowMode RasterizerPS2::light_directional_get_shadow_mode(
	RID p_light) const
{
	return VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
}

void RasterizerPS2::light_directional_set_shadow_param(
	RID p_light, VS::LightDirectionalShadowParam p_param, float p_value)
{

}

float RasterizerPS2::light_directional_get_shadow_param(
	RID p_light, VS::LightDirectionalShadowParam p_param) const
{
	return .0f;
}


AABB RasterizerPS2::light_get_aabb(RID p_light) const
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, AABB());

	switch (light->type) {

	case VS::LIGHT_SPOT: {

		float len = light->vars[VS::LIGHT_PARAM_RADIUS];
		float size = Math::tan(Math::deg2rad(light->vars[VS::LIGHT_PARAM_SPOT_ANGLE])) * len;
		return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
	} break;
	case VS::LIGHT_OMNI: {

		float r = light->vars[VS::LIGHT_PARAM_RADIUS];
		return AABB(-Vector3(r, r, r), Vector3(r, r, r) * 2);
	} break;
	case VS::LIGHT_DIRECTIONAL: {

		return AABB();
	} break;
	default: {}
	}

	ERR_FAIL_V(AABB());
}


RID RasterizerPS2::light_instance_create(RID p_light)
{
	Light* light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, RID());

	LightInstance* light_instance = memnew(LightInstance);

	light_instance->light = p_light;
	light_instance->base = light;

	return light_instance_owner.make_rid(light_instance);
}

void RasterizerPS2::light_instance_set_transform(RID p_light_instance, const Transform& p_transform)
{
	LightInstance* lighti = light_instance_owner.get(p_light_instance);
	ERR_FAIL_COND(!lighti);
	lighti->transform = p_transform;

}


bool RasterizerPS2::light_instance_has_shadow(RID p_light_instance) const
{
	return false;
}

bool RasterizerPS2::light_instance_assign_shadow(RID p_light_instance)
{
	return false;
}

Rasterizer::ShadowType RasterizerPS2::light_instance_get_shadow_type(
	RID p_light_instance) const
{
	LightInstance* lighti = light_instance_owner.get(p_light_instance);
	ERR_FAIL_COND_V(!lighti, Rasterizer::SHADOW_NONE);

	switch (lighti->base->type)
	{
	case VS::LIGHT_DIRECTIONAL: return SHADOW_PSM; break;
	case VS::LIGHT_OMNI: return SHADOW_DUAL_PARABOLOID; break;
	case VS::LIGHT_SPOT: return SHADOW_SIMPLE; break;
	}

	return Rasterizer::SHADOW_NONE;
}

int RasterizerPS2::light_instance_get_shadow_passes(RID p_light_instance) const
{
	return 0;
}

bool RasterizerPS2::light_instance_get_pssm_shadow_overlap(
	RID p_light_instance) const
{
	return false;
}

void RasterizerPS2::light_instance_set_custom_transform(
	RID p_light_instance,
	int p_index,
	const CameraMatrix& p_camera,
	const Transform& p_transform,
	float p_split_near,
	float p_split_far)
{
	LightInstance* lighti = light_instance_owner.get(p_light_instance);
	ERR_FAIL_COND(!lighti);

	ERR_FAIL_COND(lighti->base->type != VS::LIGHT_DIRECTIONAL);
	ERR_FAIL_INDEX(p_index, 1);

	lighti->custom_projection = p_camera;
	lighti->custom_transform = p_transform;
}

int RasterizerPS2::light_instance_get_shadow_size(
	RID p_light_instance, int p_index = 0) const
{
	return 1;
}

Rasterizer::ShadowType RasterizerPS2::light_instance_get_shadow_type(
	RID p_light_instance, bool p_far = false) const
{
	return SHADOW_NONE;
}

void RasterizerPS2::light_instance_set_shadow_transform(
	RID p_light_instance,
	int p_index,
	const CameraMatrix& p_camera,
	const Transform& p_transform,
	float p_split_near,
	float p_split_far)
{

}


void RasterizerPS2::shadow_clear_near()
{

}

bool RasterizerPS2::shadow_allocate_near(RID p_light)
{
	return false;
}

bool RasterizerPS2::shadow_allocate_far(RID p_light)
{
	return false;
}


/* PARTICLES INSTANCE */

RID RasterizerPS2::particles_instance_create(RID p_particles)
{
	ERR_FAIL_COND_V(!particles_owner.owns(p_particles), RID());
	ParticlesInstance* particles_instance = memnew(ParticlesInstance);
	ERR_FAIL_COND_V(!particles_instance, RID());
	particles_instance->particles = p_particles;
	return particles_instance_owner.make_rid(particles_instance);
}

void RasterizerPS2::particles_instance_set_transform(
	RID p_particles_instance, const Transform& p_transform)
{
	ParticlesInstance* particles_instance = particles_instance_owner.get(
		p_particles_instance);
	ERR_FAIL_COND(!particles_instance);
	particles_instance->transform = p_transform;
}


/* VIEWPORT */

RID RasterizerPS2::viewport_data_create()
{
	return RID();
}


RID RasterizerPS2::render_target_create()
{
	return RID();
}

void RasterizerPS2::render_target_set_size(RID p_render_target, int p_width, int p_height)
{
}

RID RasterizerPS2::render_target_get_texture(RID p_render_target) const
{
	return RID();
}

bool RasterizerPS2::render_target_renedered_in_frame(RID p_render_target)
{
	return false;
}


/* RENDER API */
/* all calls (inside begin/end shadow) are always warranted to be in the following order: */

void RasterizerPS2::begin_frame()
{

}


void RasterizerPS2::set_viewport(const VS::ViewportRect& p_viewport)
{

}

void RasterizerPS2::set_render_target(RID p_render_target, bool p_transparent_bg = false, bool p_vflip = false)
{

}

void RasterizerPS2::clear_viewport(const Color& p_color)
{

}

void RasterizerPS2::capture_viewport(Image* r_capture)
{

}


void RasterizerPS2::begin_scene(RID p_viewport_data, RID p_env, VS::ScenarioDebugMode p_debug)
{

}

void RasterizerPS2::begin_shadow_map(RID p_light_instance, int p_shadow_pass)
{

}


void RasterizerPS2::set_camera(const Transform& p_world, const CameraMatrix& p_projection, bool p_ortho_hint)
{

}


void RasterizerPS2::add_light(RID p_light_instance)
{

}
///< all "add_light" calls happen before add_geometry calls

void RasterizerPS2::add_mesh(const RID& p_mesh, const InstanceData* p_data)
{

}

void RasterizerPS2::add_multimesh(
	const RID& p_multimesh, const InstanceData* p_data)
{

}

void RasterizerPS2::add_immediate(
	const RID& p_immediate, const InstanceData* p_data)
{

}

void RasterizerPS2::add_particles(
	const RID& p_particle_instance, const InstanceData* p_data)
{

}


void RasterizerPS2::end_scene()
{

}

void RasterizerPS2::end_shadow_map()
{

}


void RasterizerPS2::end_frame()
{

}


/* CANVAS API */

void RasterizerPS2::begin_canvas_bg()
{

}

void RasterizerPS2::canvas_begin()
{

}

void RasterizerPS2::canvas_disable_blending()
{

}

void RasterizerPS2::canvas_set_opacity(float p_opacity)
{

}

void RasterizerPS2::canvas_set_blend_mode(VS::MaterialBlendMode p_mode)
{

}

void RasterizerPS2::canvas_begin_rect(const Matrix32& p_transform)
{

}

void RasterizerPS2::canvas_set_clip(bool p_clip, const Rect2& p_rect)
{

}

void RasterizerPS2::canvas_end_rect()
{

}

void RasterizerPS2::canvas_draw_line(
	const Point2& p_from,
	const Point2& p_to,
	const Color& p_color,
	float p_width)
{

}

void RasterizerPS2::canvas_draw_rect(
	const Rect2& p_rect,
	int p_flags,
	const Rect2& p_source,
	RID p_texture,
	const Color& p_modulate)
{

}

void RasterizerPS2::canvas_draw_style_box(
	const Rect2& p_rect,
	const Rect2& p_src_region,
	RID p_texture,
	const float* p_margins,
	bool p_draw_center = true,
	const Color& p_modulate = Color(1, 1, 1))
{

}

void RasterizerPS2::canvas_draw_primitive(
	const Vector<Point2>& p_points,
	const Vector<Color>& p_colors,
	const Vector<Point2>& p_uvs,
	RID p_texture,
	float p_width)
{

}

void RasterizerPS2::canvas_draw_polygon(
	int p_vertex_count,
	const int* p_indices,
	const Vector2* p_vertices,
	const Vector2* p_uvs,
	const Color* p_colors,
	const RID& p_texture,
	bool p_singlecolor)
{

}

void RasterizerPS2::canvas_set_transform(
	const Matrix32& p_transform)
{

}


void RasterizerPS2::canvas_render_items(
	CanvasItem* p_item_list,
	int p_z,
	const Color& p_modulate,
	CanvasLight* p_light)
{

}


RID RasterizerPS2::canvas_light_occluder_create()
{

}

void RasterizerPS2::canvas_light_occluder_set_polylines(
	RID p_occluder, const DVector<Vector2>& p_lines)
{

}


RID RasterizerPS2::canvas_light_shadow_buffer_create(int p_width)
{

}

void RasterizerPS2::canvas_light_shadow_buffer_update(
	RID p_buffer,
	const Matrix32& p_light_xform,
	int p_light_mask,
	float p_near,
	float p_far,
	CanvasLightOccluderInstance* p_occluders,
	CameraMatrix* p_xform_cache)
{

}


void RasterizerPS2::canvas_debug_viewport_shadows(
	CanvasLight* p_lights_with_shadow)
{

}


/* ENVIRONMENT */

RID RasterizerPS2::environment_create()
{
	Environment* env = memnew(Environment);
	return environment_owner.make_rid(env);
}


void RasterizerPS2::environment_set_background(RID p_env, VS::EnvironmentBG p_bg)
{
	ERR_FAIL_INDEX(p_bg, VS::ENV_BG_MAX);
	Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->bg_mode = p_bg;
}

VS::EnvironmentBG RasterizerPS2::environment_get_background(RID p_env) const
{
	const Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env, VS::ENV_BG_MAX);
	return env->bg_mode;
}


void RasterizerPS2::environment_set_background_param(
	RID p_env, VS::EnvironmentBGParam p_param, const Variant& p_value)
{
	ERR_FAIL_INDEX(p_param, VS::ENV_BG_PARAM_MAX);
	Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->bg_param[p_param] = p_value;
}

Variant RasterizerPS2::environment_get_background_param(
	RID p_env, VS::EnvironmentBGParam p_param) const
{
	ERR_FAIL_INDEX_V(p_param, VS::ENV_BG_PARAM_MAX, Variant());
	const Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env, Variant());
	return env->bg_param[p_param];
}


void RasterizerPS2::environment_set_enable_fx(
	RID p_env, VS::EnvironmentFx p_effect, bool p_enabled)
{
	ERR_FAIL_INDEX(p_effect, VS::ENV_FX_MAX);
	Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->fx_enabled[p_effect] = p_enabled;
}

bool RasterizerPS2::environment_is_fx_enabled(
	RID p_env, VS::EnvironmentFx p_effect) const
{
	ERR_FAIL_INDEX_V(p_effect, VS::ENV_FX_MAX, false);
	const Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env, false);
	return env->fx_enabled[p_effect];
}


void RasterizerPS2::environment_fx_set_param(
	RID p_env, VS::EnvironmentFxParam p_param, const Variant& p_value)
{
	ERR_FAIL_INDEX(p_param, VS::ENV_FX_PARAM_MAX);
	Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->fx_param[p_param] = p_value;
}

Variant RasterizerPS2::environment_fx_get_param(
	RID p_env, VS::EnvironmentFxParam p_param) const
{
	ERR_FAIL_INDEX_V(p_param, VS::ENV_FX_PARAM_MAX, Variant());
	const Environment* env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env, Variant());
	return env->fx_param[p_param];
}


/* SAMPLED LIGHT */

RID RasterizerPS2::sampled_light_dp_create(int p_width, int p_height)
{
	return sampled_light_owner.make_rid(memnew(SampledLight));
}

void RasterizerPS2::sampled_light_dp_update(RID p_sampled_light, const Color* p_data, float p_multiplier)
{

}


/*MISC*/

bool RasterizerPS2::is_texture(const RID& p_rid) const
{
	return texture_owner.owns(p_rid);
}

bool RasterizerPS2::is_material(const RID& p_rid) const
{
	return material_owner.owns(p_rid);
}

bool RasterizerPS2::is_mesh(const RID& p_rid) const
{
	return mesh_owner.owns(p_rid);
}

bool RasterizerPS2::is_immediate(const RID& p_rid) const
{
	return immediate_owner.owns(p_rid);
}

bool RasterizerPS2::is_multimesh(const RID& p_rid) const
{
	return multimesh_owner.owns(p_rid);
}

bool RasterizerPS2::is_particles(const RID& p_beam) const
{
	return particles_owner.owns(p_beam);
}


bool RasterizerPS2::is_light(const RID& p_rid) const
{
	return light_owner.owns(p_rid);
}

bool RasterizerPS2::is_light_instance(const RID& p_rid) const
{
	return light_instance_owner.owns(p_rid);
}

bool RasterizerPS2::is_particles_instance(const RID& p_rid) const
{
	return particles_instance_owner.owns(p_rid);
}

bool RasterizerPS2::is_skeleton(const RID& p_rid) const
{
	return skeleton_owner.owns(p_rid);
}

bool RasterizerPS2::is_environment(const RID& p_rid) const
{
	return environment_owner.owns(p_rid);
}

bool RasterizerPS2::is_canvas_light_occluder(const RID& p_rid) const
{
	return false;
}


bool RasterizerPS2::is_shader(const RID& p_rid) const
{
	return false;
}


void RasterizerPS2::free(const RID& p_rid)
{
	if (texture_owner.owns(p_rid))
	{
		// delete the texture
		Texture* texture = texture_owner.get(p_rid);
		texture_owner.free(p_rid);
		memdelete(texture);

	}
	else if (shader_owner.owns(p_rid))
	{
		// delete the shader
		Shader* shader = shader_owner.get(p_rid);
		shader_owner.free(p_rid);
		memdelete(shader);

	}
	else if (material_owner.owns(p_rid))
	{
		// delete the material
		Material* material = material_owner.get(p_rid);
		material_owner.free(p_rid);
		memdelete(material);

	}
	else if (mesh_owner.owns(p_rid))
	{
		// delete the mesh
		Mesh* mesh = mesh_owner.get(p_rid);

		for (int i = 0; i < mesh->surfaces.size(); i++) {

			memdelete(mesh->surfaces[i]);
		};

		mesh->surfaces.clear();
		mesh_owner.free(p_rid);
		memdelete(mesh);

	}
	else if (multimesh_owner.owns(p_rid))
	{
		// delete the multimesh
		MultiMesh* multimesh = multimesh_owner.get(p_rid);
		multimesh_owner.free(p_rid);
		memdelete(multimesh);

	}
	else if (immediate_owner.owns(p_rid))
	{
		// delete immediate
		Immediate* immediate = immediate_owner.get(p_rid);
		immediate_owner.free(p_rid);
		memdelete(immediate);

	}
	else if (particles_owner.owns(p_rid))
	{
		// delete particles
		Particles* particles = particles_owner.get(p_rid);
		particles_owner.free(p_rid);
		memdelete(particles);
	}
	else if (particles_instance_owner.owns(p_rid))
	{
		// delete particles instance
		ParticlesInstance* particles_isntance = particles_instance_owner.get(p_rid);
		particles_instance_owner.free(p_rid);
		memdelete(particles_isntance);

	}
	else if (skeleton_owner.owns(p_rid))
	{
		// delete skeleton
		Skeleton* skeleton = skeleton_owner.get(p_rid);
		skeleton_owner.free(p_rid);
		memdelete(skeleton);

	}
	else if (light_owner.owns(p_rid))
	{
		// delete light
		Light* light = light_owner.get(p_rid);
		light_owner.free(p_rid);
		memdelete(light);

	}
	else if (light_instance_owner.owns(p_rid))
	{
		// delete light instance
		LightInstance* light_instance = light_instance_owner.get(p_rid);
		light_instance_owner.free(p_rid);
		memdelete(light_instance);

	}
	else if (environment_owner.owns(p_rid))
	{
		// delete environment
		Environment* env = environment_owner.get(p_rid);
		environment_owner.free(p_rid);
		memdelete(env);
	}
	else if (sampled_light_owner.owns(p_rid))
	{
		// delete sampled light
		SampledLight* sampled_light = sampled_light_owner.get(p_rid);
		ERR_FAIL_COND(!sampled_light);

		sampled_light_owner.free(p_rid);
		memdelete(sampled_light);
	};
}


void RasterizerPS2::custom_shade_model_set_shader(int p_model, RID p_shader)
{

}

RID RasterizerPS2::custom_shade_model_get_shader(int p_model) const
{
	return RID();
}

void RasterizerPS2::custom_shade_model_set_name(int p_model, const String& p_name)
{

}

String RasterizerPS2::custom_shade_model_get_name(int p_model) const
{
	return String();
}

void RasterizerPS2::custom_shade_model_set_param_info(
	int p_model, const List<PropertyInfo>& p_info)
{

}

void RasterizerPS2::custom_shade_model_get_param_info(
	int p_model, List<PropertyInfo>* p_info) const
{

}


void RasterizerPS2::set_time_scale(float p_scale)
{

}

void RasterizerPS2::init()
{

}

void RasterizerPS2::finish()
{

}


int RasterizerPS2::get_render_info(VS::RenderInfo p_info)
{
	return 0;
}


bool RasterizerPS2::needs_to_draw_next_frame() const
{
	return false;
}


bool RasterizerPS2::has_feature(VS::Features p_feature) const
{
	return false;
}


void RasterizerPS2::restore_framebuffer()
{

}
