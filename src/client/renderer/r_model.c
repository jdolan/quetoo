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
} r_model_format_t;

static const r_model_format_t r_model_formats[] = { // supported model formats
	{ ".obj", MOD_OBJ, R_LoadObjModel },
	{ ".md3", MOD_MD3, R_LoadMd3Model },
	{ ".bsp", MOD_BSP, R_LoadBspModel }
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

	if (R_ValidBuffer(&mod->vertex_buffer)) {
		R_DestroyBuffer(&mod->vertex_buffer);
	}

	if (R_ValidBuffer(&mod->normal_buffer)) {
		R_DestroyBuffer(&mod->normal_buffer);
	}

	if (R_ValidBuffer(&mod->tangent_buffer)) {
		R_DestroyBuffer(&mod->tangent_buffer);
	}

	if (R_ValidBuffer(&mod->texcoord_buffer)) {
		R_DestroyBuffer(&mod->texcoord_buffer);
	}

	if (R_ValidBuffer(&mod->lightmap_texcoord_buffer)) {
		R_DestroyBuffer(&mod->lightmap_texcoord_buffer);
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

		mod = (r_model_t *) R_AllocMedia(key, sizeof(r_model_t));

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

	const GLuint null_elements[] = {
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

	R_CreateDataBuffer(&r_model_state.null_vertices, GL_FLOAT, 3, GL_STATIC_DRAW, sizeof(null_vertices), null_vertices);

	R_CreateElementBuffer(&r_model_state.null_elements, GL_UNSIGNED_INT, GL_STATIC_DRAW, sizeof(null_elements), null_elements);
}

/**
 * @brief Shuts down the model facilities.
 */
void R_ShutdownModels(void) {

	R_DestroyBuffer(&r_model_state.null_vertices);

	R_DestroyBuffer(&r_model_state.null_elements);
}
