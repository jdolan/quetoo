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

r_model_t *r_world_model;

typedef struct {
	const char *extension;
	r_model_type_t type;
	void (*Load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static const r_model_format_t r_model_formats[] = { // supported model formats
	{ ".obj", MOD_MESH, R_LoadObjModel},
	{ ".md3", MOD_MESH, R_LoadMd3Model},
	{ ".bsp", MOD_BSP, R_LoadBspModel}
};

/**
 * @brief Register event listener for models.
 */
static void R_RegisterModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	if (mod->type == MOD_BSP) {

		r_bsp_texinfo_t *texinfo = mod->bsp->texinfo;
		for (int32_t i = 0; i < mod->bsp->num_texinfo; i++, texinfo++) {
			R_RegisterDependency(self, (r_media_t *) texinfo->material);
		}

		if (mod->bsp->lightmap) {
			R_RegisterDependency(self, (r_media_t *) mod->bsp->lightmap->atlas);
		}

		if (mod->bsp->lightgrid) {
			for (int32_t i = 0; i < BSP_LIGHTGRID_TEXTURES; i++) {
				R_RegisterDependency(self, (r_media_t *) mod->bsp->lightgrid->textures[i]);
			}
		}

		// keep a reference to the world model
		r_world_model = mod;

	} else if (mod->type == MOD_MESH) {

		const r_mesh_face_t *face = mod->mesh->faces;
		for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
			if (face->material) {
				R_RegisterDependency(self, (r_media_t *) face->material);
			}
		}
	}
}

/**
 * @brief Free event listener for models.
 */
static void R_FreeModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	if (IS_BSP_MODEL(mod)) {

		glDeleteBuffers(1, &mod->bsp->vertex_buffer);
		glDeleteBuffers(1, &mod->bsp->elements_buffer);
		glDeleteVertexArrays(1, &mod->bsp->vertex_array);

	} else if (IS_BSP_INLINE_MODEL(mod)) {

		g_ptr_array_free(mod->bsp_inline->flare_faces, 1);
		g_ptr_array_free(mod->bsp_inline->opaque_draw_elements, 1);
		g_ptr_array_free(mod->bsp_inline->alpha_blend_draw_elements, 1);

	} else if (IS_MESH_MODEL(mod)) {

		glDeleteBuffers(1, &mod->mesh->vertex_buffer);
		glDeleteBuffers(1, &mod->mesh->elements_buffer);
		
	}

	R_GetError(mod->media.name);
}

/**
 * @brief Loads the model by the specified name.
 */
r_model_t *R_LoadModel(const char *name) {
	char key[MAX_QPATH];

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "R_LoadModel: NULL name\n");
	}

	if (*name == '*') {
		g_snprintf(key, sizeof(key), "%s#%s", r_world_model->media.name, name + 1);
	} else {
		StripExtension(name, key);
	}

	r_model_t *mod = (r_model_t *) R_FindMedia(key, R_MEDIA_MODEL);
	if (mod == NULL) {

		const r_model_format_t *format = r_model_formats;
		char filename[MAX_QPATH];

		size_t i;
		for (i = 0; i < lengthof(r_model_formats); i++, format++) {

			strncpy(filename, key, MAX_QPATH);
			strcat(filename, format->extension);

			if (Fs_Exists(filename)) {
				break;
			}
		}

		if (i == lengthof(r_model_formats)) { // not found
			if (strstr(name, "players/")) {
				Com_Debug(DEBUG_RENDERER, "Failed to load player %s\n", name);
			} else {
				Com_Warn("Failed to load %s\n", name);
			}
			return NULL;
		}

		mod = (r_model_t *) R_AllocMedia(key, sizeof(r_model_t), R_MEDIA_MODEL);

		mod->media.Register = R_RegisterModel;
		mod->media.Free = R_FreeModel;

		mod->type = format->type;

		mod->mins = Vec3_Mins();
		mod->maxs = Vec3_Maxs();

		void *buf = NULL;

		Fs_Load(filename, &buf);

		// load it
		format->Load(mod, buf);

		// free the file
		Fs_Free(buf);

		// calculate an approximate radius from the bounding box
		mod->radius = Vec3_Distance(mod->maxs, mod->mins) / 2.0;

		R_RegisterMedia((r_media_t *) mod);
	}

	return mod;
}

/**
 * @brief Returns the currently loaded world model (BSP).
 */
r_model_t *R_WorldModel(void) {
	return r_world_model;
}

/**
 * @brief Initializes the model facilities.
 */
void R_InitModels(void) {

	r_world_model = NULL;

	Cmd_Add("r_export_bsp", R_ExportBsp_f, CMD_RENDERER, "Export the current map to a .obj model.");

	R_InitBspProgram();

	R_InitMeshProgram();

	R_InitMeshShadowProgram();

	glFrontFace(GL_CW);

	R_GetError(NULL);
}

/**
 * @brief Shuts down the model facilities.
 */
void R_ShutdownModels(void) {

	r_world_model = NULL;

	R_ShutdownBspProgram();

	R_ShutdownMeshProgram();

	R_ShutdownMeshShadowProgram();
}
