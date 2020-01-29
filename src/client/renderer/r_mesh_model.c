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
r_material_t *R_ResolveModelMaterial(const r_model_t *mod, const r_mesh_face_t *face, const char *name) {
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

	if (Fs_Load(path, &buf) == -1) {
		return;
	}

	Parse_Init(&parser, (const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "translate")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, config->translate, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, config->rotate, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &config->scale, 1)) {
				break;
			}

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

	mod->mesh->config.world.scale = 1.0;

	Dirname(mod->media.name, path);

	R_LoadMeshConfig(&mod->mesh->config.world, va("%sworld.cfg", path));

	// by default, additional configs inherit from world
	mod->mesh->config.view = mod->mesh->config.link = mod->mesh->config.world;

	R_LoadMeshConfig(&mod->mesh->config.view, va("%sview.cfg", path));
	R_LoadMeshConfig(&mod->mesh->config.world, va("%slink.cfg", path));
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

		for (int32_t j = 0; j < mod->mesh->num_frames; j++) {
			r_mesh_vertex_t *v = face->vertexes + face->num_vertexes * j;

			vec3_t *sdir = (vec3_t *) Mem_Malloc(face->num_vertexes * sizeof(vec3_t));
			vec3_t *tdir = (vec3_t *) Mem_Malloc(face->num_vertexes * sizeof(vec3_t));

			GLuint *e = (GLuint *) ((byte *) mod->mesh->elements + (ptrdiff_t) face->elements);
			for (int32_t k = 0; k < face->num_elements; k += 3) {

				const r_mesh_vertex_t *a = &v[e[k + 0]];
				const r_mesh_vertex_t *b = &v[e[k + 1]];
				const r_mesh_vertex_t *c = &v[e[k + 2]];

				vec3_t s, t;

				const vec_t bx_ax = b->position[0] - a->position[0];
				const vec_t cx_ax = c->position[0] - a->position[0];
				const vec_t by_ay = b->position[1] - a->position[1];
				const vec_t cy_ay = c->position[1] - a->position[1];
				const vec_t bz_az = b->position[2] - a->position[2];
				const vec_t cz_az = c->position[2] - a->position[2];

				const vec_t bs_as = b->diffuse[0] - a->diffuse[0];
				const vec_t cs_as = c->diffuse[0] - a->diffuse[0];
				const vec_t bt_at = b->diffuse[1] - a->diffuse[1];
				const vec_t ct_at = c->diffuse[1] - a->diffuse[1];

				const vec_t r = 1.0 / (bs_as * ct_at - cs_as * bt_at);

				VectorSet(s,
						  (ct_at * bx_ax - bt_at * cx_ax),
						  (ct_at * by_ay - bt_at * cy_ay),
						  (ct_at * bz_az - bt_at * cz_az));

				VectorScale(s, r, s);

				VectorSet(t,
						  (bs_as * cx_ax - cs_as * bx_ax),
						  (bs_as * cy_ay - cs_as * by_ay),
						  (bs_as * cz_az - cs_as * bz_az));

				VectorScale(t, r, t);

				VectorAdd(sdir[e[k + 0]], s, sdir[e[k + 0]]);
				VectorAdd(sdir[e[k + 1]], s, sdir[e[k + 1]]);
				VectorAdd(sdir[e[k + 2]], s, sdir[e[k + 2]]);

				VectorAdd(tdir[e[k + 0]], t, tdir[e[k + 0]]);
				VectorAdd(tdir[e[k + 1]], t, tdir[e[k + 1]]);
				VectorAdd(tdir[e[k + 2]], t, tdir[e[k + 2]]);
			}

			for (int32_t k = 0; k < face->num_vertexes; k++) {
				TangentVectors(v->normal, sdir[k], tdir[k], v->tangent, v->bitangent);
			}

			Mem_Free(sdir);
			Mem_Free(tdir);
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

			face->elements = (GLvoid *) ((elements - mod->mesh->elements) * sizeof(GLuint));
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
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) offsetof(r_mesh_vertex_t, diffuse));

	R_GetError(mod->media.name);

	glBindVertexArray(0);
}

