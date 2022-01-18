/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief Loads the materials file for the specified mesh model.
 * @remarks Player models may optionally define materials, but are not required to.
 * @remarks Other mesh models must resolve at least one material. If no materials file is found,
 * we attempt to load ${model_dir}/skin.tga as the default material.
 */
void R_LoadMeshMaterials(r_model_t *mod) {
	char path[MAX_QPATH];

	GList *materials = NULL;

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	if (g_str_has_prefix(mod->media.name, "players/")) {
		R_LoadMaterials(path, ASSET_CONTEXT_PLAYERS, &materials);
	} else {
		if (R_LoadMaterials(path, ASSET_CONTEXT_MODELS, &materials) < 1) {

			Dirname(mod->media.name, path);
			g_strlcat(path, "skin", sizeof(path));

			materials = g_list_prepend(materials, R_LoadMaterial(path, ASSET_CONTEXT_MODELS));
		}

		assert(materials);
	}

	for (GList *list = materials; materials; materials = materials->next) {
		R_RegisterDependency((r_media_t *) mod, (r_media_t *) list->data);
	}

	g_list_free(materials);
}

/**
 * @brief Resolves a material for the specified mesh model.
 * @remarks First, it will attempt to use the material explicitly designated on the face, if
 * one exists. If that material is not found, it will attempt to load a material based on the face name.
 * Finally, if that fails, it will fall back to using model path + "skin".
 */
r_material_t *R_ResolveMeshMaterial(const r_model_t *mod, const r_mesh_face_t *face, const char *name) {
	char path[MAX_QPATH];
	r_material_t *material;

	// try explicit material
	if (name && name[0]) {
		material = R_FindMaterial(name, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve explicit material \"%s\"\n", name);
	}

	// try implicit face name
	if (face->name[0]) {
		Dirname(mod->media.name, path);
		g_strlcat(path, face->name, sizeof(path));

		material = R_FindMaterial(path, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve implicit mesh material \"%s\"\n", face->name);
	}

	// fall back to "skin"
	Dirname(mod->media.name, path);
	g_strlcat(path, "skin", sizeof(path));

	return R_FindMaterial(path, ASSET_CONTEXT_MODELS);
}

/**
 * @brief Loads the specified r_mesh_config_t from the file at path.
 */
static void R_LoadMeshConfig(r_mesh_config_t *config, const char *path) {
	void *buf;
	char token[MAX_STRING_CHARS];

	config->transform = Mat4_Identity();

	if (Fs_Load(path, &buf) == -1) {
		return;
	}

	parser_t parser = Parse_Init((const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "translate")) {

			vec3_t v;
			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
				break;
			}

			config->transform = Mat4_ConcatTranslation(config->transform, v);
			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			vec3_t v;
			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
				break;
			}

			config->transform = Mat4_ConcatRotation3(config->transform, v);
			continue;
		}

		if (!g_strcmp0(token, "scale")) {

			float v;
			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &v, 1)) {
				break;
			}

			config->transform = Mat4_ConcatScale(config->transform, v);
			continue;
		}
	}

	Fs_Free(buf);
}

/**
 * @brief Loads all r_mesh_config_t for the specified r_model_t. These allow
 * models to be positioned and scaled relative to their own origins, which is
 * useful because artists contribute models in almost arbitrary dimensions at
 * times.
 */
void R_LoadMeshConfigs(r_model_t *mod) {
	char path[MAX_QPATH];

	Dirname(mod->media.name, path);

	R_LoadMeshConfig(&mod->mesh->config.world, va("%s/world.cfg", path));
	R_LoadMeshConfig(&mod->mesh->config.view, va("%s/view.cfg", path));
	R_LoadMeshConfig(&mod->mesh->config.link, va("%s/link.cfg", path));
}

/**
 * @brief Calculates tangent vectors for each vertex for per-pixel
 * lighting. See http://www.terathon.com/code/tangent.html.
 */
static void R_LoadMeshTangents(r_model_t *mod) {

	assert(mod->mesh);
	assert(mod->mesh->num_faces);

	const r_mesh_face_t *face = mod->mesh->faces;
	for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {

		cm_vertex_t *vertexes = Mem_Malloc(sizeof(cm_vertex_t) * face->num_vertexes);

		for (int32_t j = 0; j < mod->mesh->num_frames; j++) {

			r_mesh_vertex_t *v = face->vertexes + face->num_vertexes * j;
			for (int32_t k = 0; k < face->num_vertexes; k++, v++) {
				vertexes[k] = (cm_vertex_t) {
					.position = &v->position,
					.normal = &v->normal,
					.tangent = &v->tangent,
					.bitangent = &v->bitangent,
					.st = &v->diffusemap
				};
			}

			const int32_t *elements = (int32_t *) ((byte *) mod->mesh->elements + (ptrdiff_t) face->elements);

			Cm_Tangents(vertexes, face->num_vertexes, elements, face->num_elements);
		}

		Mem_Free(vertexes);
	}
}

/**
 * @brief Calculates normal vectors for shells, which need to have
 * proper smooth vertex normals to look "right".
 */
static void R_SetupMeshShellNormals(r_model_t *mod, r_mesh_face_t *face) {

	assert(mod->mesh);
	assert(mod->mesh->num_vertexes);
	assert(mod->mesh->num_frames);

	const int32_t total_vertices = face->num_vertexes * mod->mesh->num_frames;

	face->shell_normals = Mem_LinkMalloc(sizeof(vec3_t) * total_vertices, mod->mesh);
	
	int32_t remap[face->num_vertexes];
	int32_t num_normals[face->num_vertexes];

	for (int32_t f = 0; f < mod->mesh->num_frames; f++) {
		const ptrdiff_t frame_vert_offset = (f * face->num_vertexes);

		// reset remap lists
		memset(remap, -1, sizeof(remap));
		memset(num_normals, 0, sizeof(num_normals));

		// first, remap vertices on every frame if they have
		// the same position.
		for (int32_t i = face->num_vertexes - 1; i >= 0; i--) {
			const r_mesh_vertex_t *v = &face->vertexes[frame_vert_offset + i];
			int32_t j;

			for (j = 0; j < i; j++) {
				const r_mesh_vertex_t *v2 = &face->vertexes[frame_vert_offset + j];

				if (Vec3_Equal(v2->position, v->position)) {
					break;
				}
			}

			remap[i] = j;
		}

		// re-calculate normals for every vertex
		vec3_t *normals = face->shell_normals + frame_vert_offset;

		for (int32_t i = 0; i < face->num_vertexes; i++) {
			const uint32_t v_i = remap[i];
			vec3_t *normal = normals + v_i;

			const GLuint *e = (GLuint *) face->elements;

			for (int32_t k = 0; k < face->num_elements; k += 3, e += 3) {
				const uint32_t a = remap[*e], b = remap[*(e + 1)], c = remap[*(e + 2)];

				if (v_i == a || v_i == b || v_i == c) {
					const vec3_t va = face->vertexes[frame_vert_offset + a].position;
					const vec3_t vb = face->vertexes[frame_vert_offset + b].position;
					const vec3_t vc = face->vertexes[frame_vert_offset + c].position;

					const vec3_t v1 = Vec3_Subtract(va, vc);
					const vec3_t v2 = Vec3_Subtract(vc, vb);

					*normal = Vec3_Add(*normal, Vec3_Cross(v1, v2));
					num_normals[v_i]++;
				}
			}
		}

		// divide and normalize
		for (int32_t i = 0; i < face->num_vertexes; i++) {
			if (remap[i] == i) {
				normals[i] = Vec3_Normalize(Vec3_Scale(normals[i], num_normals[i]));
			} else {
				normals[i] = normals[remap[i]];
			}
		}
	}
}

/**
 * @brief
 */
void R_LoadMeshVertexArray(r_model_t *mod) {

	assert(mod->mesh);
	assert(mod->mesh->num_faces);

	{
		const r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
			mod->mesh->num_vertexes += face->num_vertexes;
			mod->mesh->num_elements += face->num_elements;
		}
	}

	assert(mod->mesh->num_vertexes);
	assert(mod->mesh->num_elements);

	mod->mesh->vertexes = Mem_LinkMalloc(mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t), mod->mesh);
	mod->mesh->elements = Mem_LinkMalloc(mod->mesh->num_elements * sizeof(GLuint), mod->mesh);
	mod->mesh->shell_normals = Mem_LinkMalloc(mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(vec3_t), mod->mesh);

	r_mesh_vertex_t *vertex = mod->mesh->vertexes;
	GLuint *elements = mod->mesh->elements;
	vec3_t *shell_normal = mod->mesh->shell_normals;

	{
		r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {

			R_SetupMeshShellNormals(mod, face);

			memcpy(vertex, face->vertexes, face->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t));
			Mem_Free(face->vertexes);

			face->vertexes = vertex;
			vertex += face->num_vertexes * mod->mesh->num_frames;

			memcpy(elements, face->elements, face->num_elements * sizeof(GLuint));
			Mem_Free(face->elements);

			face->elements = (GLvoid *) ((elements - mod->mesh->elements) * sizeof(GLuint));
			elements += face->num_elements;

			memcpy(shell_normal, face->shell_normals, face->num_vertexes * mod->mesh->num_frames * sizeof(vec3_t));
			Mem_Free(face->shell_normals);

			face->shell_normals = shell_normal;
			shell_normal += face->num_vertexes * mod->mesh->num_frames;
		}
	}

	R_LoadMeshTangents(mod);

	glGenVertexArrays(1, &mod->mesh->vertex_array);
	glBindVertexArray(mod->mesh->vertex_array);

	glGenBuffers(1, &mod->mesh->shell_normals_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, mod->mesh->shell_normals_buffer);
	glBufferData(GL_ARRAY_BUFFER, mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(vec3_t), mod->mesh->shell_normals, GL_STATIC_DRAW);

	glGenBuffers(1, &mod->mesh->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, mod->mesh->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t), mod->mesh->vertexes, GL_STATIC_DRAW);

	glGenBuffers(1, &mod->mesh->elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->mesh->elements_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->mesh->num_elements * sizeof(GLuint), mod->mesh->elements, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, tangent));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, bitangent));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, diffusemap));

	R_GetError(mod->media.name);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);
}

/**
 * @brief
 */
void R_RegisterMeshModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	const r_mesh_face_t *face = mod->mesh->faces;
	for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
		if (face->material) {
			R_RegisterDependency(self, (r_media_t *) face->material);
		}
	}
}

/**
 * @brief
 */
void R_FreeMeshModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	glDeleteBuffers(1, &mod->mesh->vertex_buffer);
	glDeleteBuffers(1, &mod->mesh->elements_buffer);
}
