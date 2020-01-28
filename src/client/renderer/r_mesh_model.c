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
 * @remarks First, it will attempt to use the material explicitly designated on the mesh, if
 * one exists. If that material is not found, it will attempt to load a material based on the mesh's name.
 * Finally, if that fails, it will fall back to using model path + "skin".
 */
r_material_t *R_ResolveModelMaterial(const r_model_t *mod, const r_mesh_t *mesh, const char *mesh_material) {
	char path[MAX_QPATH];
	r_material_t *material;

	// try explicit material
	if (mesh_material != NULL && mesh_material[0]) {
		material = R_FindMaterial(mesh_material, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve explicit material \"%s\"\n", mesh_material);
	}

	// try implicit mesh name
	if (mesh->name[0]) {
		Dirname(mod->media.name, path);
		g_strlcat(path, mesh->name, sizeof(path));

		material = R_FindMaterial(path, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve implicit mesh material \"%s\"\n", mesh->name);
	}

	// fall back to "skin", which will have been force-loaded by
	// R_LoadModelMaterials
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
 * @brief
 */
void R_LoadMeshVertexArray(r_model_t *mod) {

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

