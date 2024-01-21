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

		const r_model_format_t formats[] = {
			r_obj_model_format,
			r_md3_model_format,
			r_bsp_model_format
		};

		const r_model_format_t *format = formats;
		char path[MAX_QPATH];

		size_t i;
		for (i = 0; i < lengthof(formats); i++, format++) {

			g_snprintf(path, sizeof(path), "%s.%s", key, format->extension);

			if (Fs_Exists(path)) {
				break;
			}
		}

		if (i == lengthof(formats)) {
			if (strstr(name, "players/")) {
				Com_Debug(DEBUG_RENDERER, "Failed to load player %s\n", name);
			} else {
				Com_Warn("Failed to load %s\n", name);
			}
			return NULL;
		}

		mod = (r_model_t *) R_AllocMedia(key, sizeof(r_model_t), R_MEDIA_MODEL);

		mod->media.Register = format->Register;
		mod->media.Free = format->Free;

		mod->type = format->type;

		mod->bounds = Box3_Null();

		void *buf = NULL;

		Fs_Load(path, &buf);

		format->Load(mod, buf);

		Fs_Free(buf);

		mod->radius = Box3_Radius(mod->bounds);

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

	R_InitModelProgram();

	glFrontFace(GL_CW);

	R_GetError(NULL);
}

/**
 * @brief Shuts down the model facilities.
 */
void R_ShutdownModels(void) {

	r_world_model = NULL;

	R_ShutdownModelProgram();
}
