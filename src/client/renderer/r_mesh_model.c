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

			Cm_Tangents(vertexes, 0, face->num_vertexes, (int32_t *) face->elements, face->num_elements);
		}

		Mem_Free(vertexes);
	}
}

/**
 * @brief Calculates smoothened normal vectors for translucent shell effects.
 */
static void R_SetupMeshShellNormals(r_model_t *mod, r_mesh_face_t *face) {

	assert(mod->mesh);
	assert(mod->mesh->num_vertexes);
	assert(mod->mesh->num_frames);

	int32_t remap[face->num_vertexes];
	int32_t num_normals[face->num_vertexes];

	for (int32_t f = 0; f < mod->mesh->num_frames; f++) {
		const ptrdiff_t frame_vert_offset = (f * face->num_vertexes);

		// reset remap lists
		memset(remap, -1, sizeof(remap));
		memset(num_normals, 0, sizeof(num_normals));

		// first, remap vertices on every frame if they have the same position.
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
		for (int32_t i = 0; i < face->num_vertexes; i++) {
			const uint32_t v_i = remap[i];
			vec3_t *smooth_normal = &face->vertexes[v_i].smooth_normal;

			const GLuint *e = (GLuint *) face->elements;

			for (int32_t k = 0; k < face->num_elements; k += 3, e += 3) {
				const uint32_t a = remap[*e], b = remap[*(e + 1)], c = remap[*(e + 2)];

				if (v_i == a || v_i == b || v_i == c) {
					const vec3_t va = face->vertexes[frame_vert_offset + a].position;
					const vec3_t vb = face->vertexes[frame_vert_offset + b].position;
					const vec3_t vc = face->vertexes[frame_vert_offset + c].position;

					const vec3_t v1 = Vec3_Subtract(va, vc);
					const vec3_t v2 = Vec3_Subtract(vc, vb);

					*smooth_normal = Vec3_Add(*smooth_normal, Vec3_Cross(v1, v2));
					num_normals[v_i]++;
				}
			}
		}

		// divide and normalize
		r_mesh_vertex_t *v = face->vertexes;
		for (int32_t i = 0; i < face->num_vertexes; i++, v++) {
			if (remap[i] == i) {
				v->smooth_normal = Vec3_Normalize(Vec3_Scale(v->smooth_normal, num_normals[i]));
			} else {
				v->smooth_normal = face->vertexes[remap[i]].smooth_normal;
			}
		}
	}
}

/**
 * @brief Appends the given model's vertex and element arrays to the shared mesh VAO.
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

	r_mesh_vertex_t *vertex = mod->mesh->vertexes;
	GLuint *elements = mod->mesh->elements;

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

			face->elements = elements;
			elements += face->num_elements;
		}
	}

	R_LoadMeshTangents(mod);

	{
		GLuint vertex_buffer;
		glGenBuffers(1, &vertex_buffer);

		glBindBuffer(GL_COPY_READ_BUFFER, r_models.mesh.vertex_buffer);
		glBindBuffer(GL_COPY_WRITE_BUFFER, vertex_buffer);

		GLint old_size;
		glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &old_size);

		mod->mesh->base_vertex = old_size / sizeof(r_mesh_vertex_t);

		r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
			face->base_vertex = mod->mesh->base_vertex + (GLint) (face->vertexes - mod->mesh->vertexes);
		}

		GLint new_size = old_size + mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t);

		glBufferData(GL_COPY_WRITE_BUFFER, new_size, NULL, GL_STATIC_DRAW);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, old_size);

		glBufferSubData(GL_COPY_WRITE_BUFFER,
						old_size,
						mod->mesh->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t),
						mod->mesh->vertexes);

		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

		glDeleteBuffers(1, &r_models.mesh.vertex_buffer);
		r_models.mesh.vertex_buffer = vertex_buffer;
	}

	{
		GLuint elements_buffer;
		glGenBuffers(1, &elements_buffer);

		glBindBuffer(GL_COPY_READ_BUFFER, r_models.mesh.elements_buffer);
		glBindBuffer(GL_COPY_WRITE_BUFFER, elements_buffer);

		GLint old_size;
		glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &old_size);

		mod->mesh->indices = (GLvoid *) (ptrdiff_t) old_size;

		r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
			face->indices = mod->mesh->indices + ((face->elements - mod->mesh->elements) * sizeof(GLuint));
		}

		GLint new_size = old_size + mod->mesh->num_elements * sizeof(GLuint);

		glBufferData(GL_COPY_WRITE_BUFFER, new_size, NULL, GL_STATIC_DRAW);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, old_size);

		glBufferSubData(GL_COPY_WRITE_BUFFER,
						old_size,
						mod->mesh->num_elements * sizeof(GLuint),
						mod->mesh->elements);

		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

		glDeleteBuffers(1, &r_models.mesh.elements_buffer);
		r_models.mesh.elements_buffer = elements_buffer;
	}

	glBindVertexArray(r_models.mesh.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_models.mesh.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_models.mesh.elements_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, smooth_normal));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, tangent));
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, bitangent));
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, diffusemap));
	glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));
	glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, normal));
	glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, smooth_normal));
	glVertexAttribPointer(9, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, tangent));
	glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, bitangent));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glEnableVertexAttribArray(5);
	glEnableVertexAttribArray(6);
	glEnableVertexAttribArray(7);
	glEnableVertexAttribArray(8);
	glEnableVertexAttribArray(9);
	glEnableVertexAttribArray(10);

	glBindVertexArray(r_models.mesh.depth_pass.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_models.mesh.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_models.mesh.elements_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
	
	R_GetError(mod->media.name);
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
