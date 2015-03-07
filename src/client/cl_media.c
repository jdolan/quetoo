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

/*
 * @brief Entry point for file downloads, or "precache" from server. Attempt to
 * download .zip and .bsp from server. Zip is preferred. Once all precache
 * checks are completed, we load media and ask the server to begin sending
 * us frames.
 */
void Cl_RequestNextDownload(void) {

	if (cls.state < CL_CONNECTED)
		return;

	// check zip
	if (cl.precache_check == CS_ZIP) {
		cl.precache_check = CS_MODELS;

		if (*cl.config_strings[CS_ZIP] != '\0') {
			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_ZIP]))
				return; // started a download
		}
	}

	// check .bsp via models
	if (cl.precache_check == CS_MODELS) { // the map is the only model we care about
		cl.precache_check++;

		if (*cl.config_strings[CS_MODELS] != '\0') {
			if (!Cl_CheckOrDownloadFile(cl.config_strings[CS_MODELS]))
				return; // started a download
		}
	}

	// we're good to go, lock and load (literally)

	Com_InitSubsystem(QUETOO_CLIENT);
	Cvar_ResetLocal();

	Cl_LoadMedia();

	Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
	Net_WriteString(&cls.net_chan.message, va("begin %i\n", cls.spawn_count));
}

/*
 * @brief Update the loading progress, handle events and update the screen.
 * This should be called periodically while loading media.
 */
void Cl_LoadProgress(uint16_t percent) {

	cls.loading = percent;

	Cl_HandleEvents();

	Cl_UpdateScreen();
}


/**
 * @brief Draws the loading screen.
 *
 * TODO: Map-specific and generic loading backgrounds.
 */
void Cl_DrawLoading(void) {

	if (!cls.loading)
		return;

	printf("Loading... %2d%%\n", cls.loading);

	if (cls.download.file) {
		char dl[MAX_PRINT_MSG];

		const int32_t kb = (int32_t) Fs_Tell(cls.download.file) / 1024;

		g_snprintf(dl, sizeof(dl), "%s [%s] %dKB ", cls.download.name,
				(cls.download.http ? "HTTP" : "UDP"), kb);
	}
}

/*
 * @brief Load all game media through the relevant subsystems. This is called when
 * spawning into a server. For incremental reloads on subsystem restarts,
 * see Cl_UpdateMedia.
 */
void Cl_LoadMedia(void) {

	cls.loading = 1;

	Cl_UpdatePrediction();

	R_LoadMedia();

	S_LoadMedia();

	Cl_UpdateEntities();

	cls.cgame->UpdateMedia();

	Cl_ClearNotify();

	cls.key_state.dest = KEY_GAME;

	cls.loading = 0;
}

/*
 * @brief Reload stale media references on subsystem restarts.
 */
void Cl_UpdateMedia(void) {

	if ((r_view.update || s_env.update) && (cls.state == CL_ACTIVE && !cls.loading)) {

		Com_Debug("%s %s\n", r_view.update ? "view" : "", s_env.update ? "sound" : "");

		cls.loading = 1;

		Cl_UpdateEntities();

		cls.cgame->UpdateMedia();

		cls.loading = 0;
	}
}
