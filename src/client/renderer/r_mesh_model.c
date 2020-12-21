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

#include "parse.h"

#include "r_local.h"

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

	// fall back to "skin", which will have been force-loaded by R_LoadModelMaterials
	Dirname(mod->media.name, path);
	g_strlcat(path, "skin", sizeof(path));

	return R_FindMaterial(path, ASSET_CONTEXT_MODELS);
}

/**
 * @brief Loads the specified r_mesh_config_t from the file at path.
 */
static void R_LoadMeshConfig(r_mesh_config_t *config, const char *path) {
	void *buf;
	parser_t parser;
	char token[MAX_STRING_CHARS];

	Matrix4x4_CreateIdentity(&config->transform);

	if (Fs_Load(path, &buf) == -1) {
		return;
	}

	Parse_Init(&parser, (const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "translate")) {

			vec3_t v;
			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
				break;
			}

			Matrix4x4_ConcatTranslate(&config->transform, v.x, v.y, v.z);
			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			vec3_t v;
			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, v.xyz, 3) != 3) {
				break;
			}

			Matrix4x4_ConcatRotate(&config->transform, v.x, 1.0, 0.0, 0.0);
			Matrix4x4_ConcatRotate(&config->transform, v.y, 0.0, 1.0, 0.0);
			Matrix4x4_ConcatRotate(&config->transform, v.z, 0.0, 0.0, 1.0);
			continue;
		}

		if (!g_strcmp0(token, "scale")) {

			float v;
			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &v, 1)) {
				break;
			}

			Matrix4x4_ConcatScale(&config->transform, v);
			continue;
		}

		if (!g_strcmp0(token, "alpha_test")) {
			config->flags |= EF_ALPHATEST;
			continue;
		}

		if (!g_strcmp0(token, "blend")) {
			config->flags |= EF_BLEND;
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
 * @brief Calculates tangent vectors for each MD3 vertex for per-pixel
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

	r_mesh_vertex_t *vertex = mod->mesh->vertexes;
	GLuint *elements = mod->mesh->elements;

	{
		r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {

			memcpy(vertex, face->vertexes, face->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t));
			Mem_Free(face->vertexes);

			face->vertexes = vertex;
			vertex += face->num_vertexes * mod->mesh->num_frames;

			memcpy(elements, face->elements, face->num_elements * sizeof(GLuint));
			Mem_Free(face->elements);

			face->elements = (GLvoid *) ((elements - mod->mesh->elements) * sizeof(int32_t));
			elements += face->num_elements;
		}
	}

	R_LoadMeshTangents(mod);

	glGenVertexArrays(1, &mod->mesh->vertex_array);
	glBindVertexArray(mod->mesh->vertex_array);

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

