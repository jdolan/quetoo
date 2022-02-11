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

#include "cl_local.h"

/**
 * @brief Entry point for file downloads, or "precache" from server. Attempt to
 * download .pk3 and .bsp from server. Archive is preferred. Once all precache
 * checks are completed, we load media and ask the server to begin sending
 * us frames.
 */
void Cl_RequestNextDownload(void) {

	if (cls.state < CL_CONNECTED) {
		return;
	}

	// check zip
	if (cl.precache_check == CS_ZIP) {
		cl.precache_check = CS_MODELS;

		if (*cl.config_strings[CS_ZIP] != '\0') {
			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_ZIP])) {
				return;    // started a download
			}
		}
	}

	// check .bsp via models
	if (cl.precache_check == CS_MODELS) { // the map is the only model we care about
		cl.precache_check++;

		if (*cl.config_strings[CS_MODELS] != '\0') {
			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_MODELS])) {
				return; // started a download
			}
		}
	}

	// we're good to go, lock and load (literally)

	Cvar_ResetDeveloper();

	Cl_LoadMedia();

	Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
	Net_WriteString(&cls.net_chan.message, va("begin %i\n", cls.spawn_count));
}


/**
 * @brief Fs_Enumerate function for Cl_Mapshots.
 */
static void Cl_Mapshots_enumerate(const char *path, void *data) {
	GList **list = (GList **) data;

	if (g_str_has_suffix(path, ".jpg") || g_str_has_suffix(path, ".png")) {
		*list = g_list_append(*list, g_strdup(path));
	}
}

/**
 * @return A GList of known mapshots for the given map.
 */
GList *Cl_Mapshots(const char *mapname) {

	char map[MAX_QPATH];
	StripExtension(mapname, map);

	GList *list = NULL;
	Fs_Enumerate(va("mapshots/%s/*", Basename(map)), Cl_Mapshots_enumerate, (void *) &list);

	return list;
}

/**
 * @brief Update the loading progress, handle events and update the screen.
 * This should be called periodically while loading media.
 * @param percent The percent. Positive values for absolute, negative for relative increment.
 */
void Cl_LoadingProgress(int32_t percent, const char *status) {

	if (percent < 0) {
		cls.loading.percent -= percent;
		cls.loading.percent = Mini(Maxi(0, cls.loading.percent), 99);
	} else {
		cls.loading.percent = percent;
	}

	cls.loading.status = status;

	Cl_HandleEvents();

	Cl_SendCommands();

	cls.cgame->UpdateLoading(cls.loading);

	R_BeginFrame();

	Cl_UpdateScreen();

	R_EndFrame();

	quetoo.ticks = SDL_GetTicks();
}

/**
 * @brief
 */
static void Cl_LoadModels(void) {

	for (int32_t i = 0; i < MAX_MODELS; i++) {

		const char *str = cl.config_strings[CS_MODELS + i];
		if (*str == 0) {
			break;
		}

		if (i ^ 1) {
			Cl_LoadingProgress(-1, str);
		}

		cl.models[i] = R_LoadModel(str);
	}
}

/**
 * @brief Fs_Enumerator to load all emoji into the images atlas.
 */
static void Cl_LoadImages_Emoji(const char *path, void *data) {
	R_LoadAtlasImage((r_atlas_t *) data, path, IT_PIC);
}

/**
 * @brief
 */
static void Cl_LoadImages(void) {

	Cl_LoadingProgress(-1, "sky");
	R_LoadSky(cl.config_strings[CS_SKY]);

	r_atlas_t *atlas = R_LoadAtlas("images");
	Fs_Enumerate("pics/emoji/*", Cl_LoadImages_Emoji, atlas);

	for (int32_t i = 0; i < MAX_IMAGES; i++) {

		const char *str = cl.config_strings[CS_IMAGES + i];
		if (*str == 0) {
			break;
		}

		cl.images[i] = (r_image_t *) R_LoadAtlasImage(atlas, str, IT_PIC);
	}

	Cl_LoadingProgress(-1, "compiling images");
	R_CompileAtlas(atlas);
}

/**
 * @brief
 */
static void Cl_LoadSounds(void) {

	if (*cl_chat_sound->string) {
		S_LoadSample(cl_chat_sound->string);
	}

	if (*cl_team_chat_sound->string) {
		S_LoadSample(cl_team_chat_sound->string);
	}

	for (int32_t i = 0; i < MAX_SOUNDS; i++) {

		const char *str = cl.config_strings[CS_SOUNDS + i];
		if (*str == 0) {
			break;
		}

		if (i ^ 1) {
			Cl_LoadingProgress(-1, str);
		}

		cl.sounds[i] = S_LoadSample(str);
	}

	for (int32_t i = 0; i < Cm_Bsp()->num_materials; i++) {
		const cm_footsteps_t *footsteps = &Cm_Bsp()->materials[i]->footsteps;

		const cm_asset_t *sample = footsteps->samples;
		for (int32_t j = 0; j < footsteps->num_samples; j++, sample++) {
			S_LoadSample(sample->name);
		}
	}
}

/**
 * @brief
 */
static void Cl_LoadMusics(void) {

	S_ClearPlaylist();

	for (int32_t i = 0; i < MAX_MUSICS; i++) {

		const char *str = cl.config_strings[CS_MUSICS + i];
		if (*str == 0) {
			break;
		}

		if (i ^ 1) {
			Cl_LoadingProgress(-1, str);
		}

		cl.musics[i] = S_LoadMusic(str);
	}

	S_NextTrack_f();
}

/**
 * @brief Load all game media through the relevant subsystems.
 */
void Cl_LoadMedia(void) {

	cls.cgame->FreeMedia();

	cls.state = CL_LOADING;

	GList *mapshots = Cl_Mapshots(cl.config_strings[CS_MODELS]);
	const size_t len = g_list_length(mapshots);

	if (len > 0) {
		strcpy(cls.loading.mapshot, g_list_nth_data(mapshots, rand() % len));
	} else {
		cls.loading.mapshot[0] = '\0';
	}

	g_list_free_full(mapshots, g_free);

	Cl_UpdatePrediction();

	R_BeginLoading();

	Cl_LoadingProgress(0, cl.config_strings[CS_MODELS]);

	Cl_LoadModels();

	Cl_LoadImages();

	S_Stop();

	S_BeginLoading();

	Cl_LoadSounds();

	Cl_LoadMusics();

	cls.cgame->LoadMedia();

	Cl_LoadingProgress(100, "ready");

	R_EndLoading();

	S_EndLoading();

	Ui_ViewWillAppear();
}
