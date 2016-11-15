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

r_model_state_t r_model_state;

typedef struct {
	const char *extension;
	r_model_type_t type;
	void (*Load)(r_model_t *mod, void *buffer);
	r_media_type_t media_type;
} r_model_format_t;

static const r_model_format_t r_model_formats[] = { // supported model formats
	{ ".obj", MOD_OBJ, R_LoadObjModel, MEDIA_OBJ },
	{ ".md3", MOD_MD3, R_LoadMd3Model, MEDIA_MD3 },
	{ ".bsp", MOD_BSP, R_LoadBspModel, MEDIA_BSP }
};

/**
 * @brief Register event listener for models.
 */
static void R_RegisterModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	if (mod->type == MOD_BSP) {
		uint16_t i;

		const r_bsp_texinfo_t *t = mod->bsp->texinfo;

		for (i = 0; i < mod->bsp->num_texinfo; i++, t++) {
			R_RegisterDependency(self, (r_media_t *) t->material);
		}

		const r_bsp_surface_t *s = mod->bsp->surfaces;

		for (i = 0; i < mod->bsp->num_surfaces; i++, s++) {
			R_RegisterDependency(self, (r_media_t *) s->lightmap);
			R_RegisterDependency(self, (r_media_t *) s->deluxemap);
		}

		// keep a reference to the world model
		r_model_state.world = mod;

	} else if (IS_MESH_MODEL(mod)) {
		R_RegisterDependency(self, (r_media_t *) mod->mesh->material);
	}
}

/**
 * @brief Free event listener for models.
 */
static void R_FreeModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	if (IS_BSP_MODEL(mod)) {
		
		R_DestroyBuffer(&mod->bsp->vertex_buffer);
		R_DestroyBuffer(&mod->bsp->element_buffer);

	} else if (IS_MESH_MODEL(mod)) {
		
		R_DestroyBuffer(&mod->mesh->vertex_buffer);

		if (R_ValidBuffer(&mod->mesh->texcoord_buffer)) {
			R_DestroyBuffer(&mod->mesh->texcoord_buffer);
		}

		R_DestroyBuffer(&mod->mesh->element_buffer);

		if (R_ValidBuffer(&mod->mesh->shell_vertex_buffer)) {

			R_DestroyBuffer(&mod->mesh->shell_vertex_buffer);
			R_DestroyBuffer(&mod->mesh->shell_element_buffer);
		}
	}

	R_GetError(mod->media.name);
}

/**
 * @brief Loads the model by the specified name.
 */
r_model_t *R_LoadModel(const char *name) {
	r_model_t *mod;
	char key[MAX_QPATH];
	size_t i;

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "R_LoadModel: NULL name\n");
	}

	if (*name == '*') {
		g_snprintf(key, sizeof(key), "%s#%s", r_model_state.world->media.name, name + 1);
	} else {
		StripExtension(name, key);
	}

	if (!(mod = (r_model_t *) R_FindMedia(key))) {

		void *buf;
		const r_model_format_t *format = r_model_formats;
		for (i = 0; i < lengthof(r_model_formats); i++, format++) {

			StripExtension(name, key);
			strcat(key, format->extension);

			if (Fs_Load(key, &buf) != -1) {
				break;
			}
		}

		if (i == lengthof(r_model_formats)) { // not found
			if (strstr(name, "players/")) {
				Com_Debug("Failed to load player %s\n", name);
			} else {
				Com_Warn("Failed to load %s\n", name);
			}
			return NULL;
		}

		StripExtension(name, key);

		mod = (r_model_t *) R_AllocMedia(key, sizeof(r_model_t), format->media_type);

		mod->media.Register = R_RegisterModel;
		mod->media.Free = R_FreeModel;

		mod->type = format->type;

		// load the materials first, so that we can resolve surfaces lists
		R_LoadMaterials(mod);

		// load it
		format->Load(mod, buf);

		// free the file
		Fs_Free(buf);

		// calculate an approximate radius from the bounding box
		vec3_t tmp;

		VectorSubtract(mod->maxs, mod->mins, tmp);
		mod->radius = VectorLength(tmp) / 2.0;

		R_RegisterMedia((r_media_t *) mod);
	}

	return mod;
}

/**
 * @brief Returns the currently loaded world model (BSP).
 */
r_model_t *R_WorldModel(void) {
	return r_model_state.world;
}

static r_buffer_layout_t r_bound_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_COLOR, .type = R_ATTRIB_UNSIGNED_BYTE, .count = 4, .size = sizeof(u8vec4_t), .offset = 12, .normalized = true },
	{ .attribute = -1 }
};

/**
 * @brief Initializes the model facilities.
 */
void R_InitModels(void) {
	memset(&r_model_state, 0, sizeof(r_model_state));

	const vec3_t null_vertices[] = {
		{ 0.0, 0.0, -16.0 },
		{ 16.0 * cos(0 * M_PI_2), 16.0 * sin(0 * M_PI_2), 0.0 },
		{ 16.0 * cos(1 * M_PI_2), 16.0 * sin(1 * M_PI_2), 0.0 },
		{ 16.0 * cos(2 * M_PI_2), 16.0 * sin(2 * M_PI_2), 0.0 },
		{ 16.0 * cos(3 * M_PI_2), 16.0 * sin(3 * M_PI_2), 0.0 },
		{ 0.0, 0.0, 16.0 }
	};

	const GLubyte null_elements[] = {
		0, 1, 2,
		0, 2, 3,
		0, 3, 4,
		0, 4, 1,

		1, 2, 5,
		2, 3, 5,
		3, 4, 5,
		4, 1, 5
	};

	r_model_state.null_elements_count = lengthof(null_elements);

	R_CreateDataBuffer(&r_model_state.null_vertices, R_ATTRIB_FLOAT, 3, false, GL_STATIC_DRAW, sizeof(null_vertices),
	                   null_vertices);

	R_CreateElementBuffer(&r_model_state.null_elements, R_ATTRIB_UNSIGNED_INT, GL_STATIC_DRAW, sizeof(null_elements),
	                      null_elements);

	const GLubyte bound_elements[] = {
		// bottom
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// top
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// connections
		0, 4,
		1, 5,
		2, 6,
		3, 7,

		// origin
		8, 9,
		10, 11,
		12, 13
	};

	r_model_state.bound_element_count = lengthof(bound_elements);

	R_CreateElementBuffer(&r_model_state.bound_element_buffer, R_ATTRIB_UNSIGNED_BYTE, GL_STATIC_DRAW,
	                      sizeof(bound_elements),
	                      bound_elements);

	Vector4Set(r_model_state.bound_vertices[8].color, 255, 0, 0, 255);
	Vector4Set(r_model_state.bound_vertices[9].color, 255, 0, 0, 255);
	Vector4Set(r_model_state.bound_vertices[10].color, 0, 255, 0, 255);
	Vector4Set(r_model_state.bound_vertices[11].color, 0, 255, 0, 255);
	Vector4Set(r_model_state.bound_vertices[12].color, 0, 0, 255, 255);
	Vector4Set(r_model_state.bound_vertices[13].color, 0, 0, 255, 255);

	VectorSet(r_model_state.bound_vertices[8].position, 0, 0, 0);
	VectorSet(r_model_state.bound_vertices[9].position, 8, 0, 0);
	VectorSet(r_model_state.bound_vertices[10].position, 0, 0, 0);
	VectorSet(r_model_state.bound_vertices[11].position, 0, 8, 0);
	VectorSet(r_model_state.bound_vertices[12].position, 0, 0, 0);
	VectorSet(r_model_state.bound_vertices[13].position, 0, 0, 8);

	R_CreateInterleaveBuffer(&r_model_state.bound_vertice_buffer, sizeof(r_bound_interleave_vertex_t),
	                         r_bound_buffer_layout, GL_STATIC_DRAW, sizeof(r_model_state.bound_vertices), r_model_state.bound_vertices);
}

/**
 * @brief Shuts down the model facilities.
 */
void R_ShutdownModels(void) {

	R_DestroyBuffer(&r_model_state.null_vertices);
	R_DestroyBuffer(&r_model_state.null_elements);

	R_DestroyBuffer(&r_model_state.bound_vertice_buffer);
	R_DestroyBuffer(&r_model_state.bound_element_buffer);
}
