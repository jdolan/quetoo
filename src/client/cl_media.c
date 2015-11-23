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
 * download .pk3 and .bsp from server. Archive is preferred. Once all precache
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
void Cl_LoadingProgress(uint16_t percent, const char *status) {

	if (percent <= 1) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "mapshots/%s", Basename(cl.config_strings[CS_MODELS]));
		StripExtension(path, path);

		cls.loading.background = R_LoadImage(path, IT_PIC);

		if (cls.loading.background->type == IT_NULL) {
			cls.loading.background = NULL;
		} else {
			Com_Debug("Using %s\n", path);
		}
	}

	cls.loading.percent = percent;
	cls.loading.status = status;

	Cl_HandleEvents();

	Cl_UpdateScreen();
}

void Cl_DrawDownload(void) {
	r_pixel_t cw, ch;

	R_BindFont("small", &cw, &ch);

	R_DrawFill(0, 0, r_context.width, r_context.height, 0, 1.0);

	const int32_t kb = (int32_t) Fs_Tell(cls.download.file) / 1024;
	const char *proto = cls.download.http ? "HTTP" : "UDP";

	const char *status = va("Downloading %s [%s] %dKB ", cls.download.name, proto, kb);

	const r_pixel_t x = (r_context.width - R_StringWidth(status)) / 2;
	const r_pixel_t y = r_context.height / 2;

	R_DrawString(x, y, status, CON_COLOR_DEFAULT);

	R_BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the loading screen.
 */
void Cl_DrawLoading(void) {
	r_pixel_t cw, ch;

	// draw the background
	if (cls.loading.background) {
		R_DrawImage(0, 0, (vec_t) r_context.width / cls.loading.background->width, cls.loading.background);
	} else {
		R_DrawFill(0, 0, r_context.width, r_context.height, 0, 1.0);
	}

	// then the progress bar and percentage
	R_BindFont("medium", &cw, &ch);

	const r_color_t bg = R_MakeColor(24, 128, 24, 48);
	const r_color_t fg = R_MakeColor(48, 255, 48, 128);

	const r_pixel_t x = r_context.width * 0.25;
	const r_pixel_t y = r_context.height * 0.66;

	const r_pixel_t w = r_context.width * 0.5;

	R_DrawFill(x, y - 2, w, ch + 4, bg.c, -1.0);
	R_DrawFill(x + 1, y - 1, (w - 2) * (cls.loading.percent / 100.0), ch + 2, fg.c, -1.0);

	const char *percent = va("%2d%%", cls.loading.percent);
	const r_pixel_t px = (r_context.width - R_StringWidth(percent)) / 2;

	R_DrawString(px, y, percent, CON_COLOR_DEFAULT);

	// and finally the status detail
	R_BindFont("small", &cw, &ch);

	const char *status = va("Loading %s..", cls.loading.status);

	const r_pixel_t sx = (r_context.width - R_StringWidth(status)) / 2;
	const r_pixel_t sy = y + 48;

	R_DrawString(sx, sy, status, CON_COLOR_DEFAULT);

	R_BindFont(NULL, NULL, NULL);
}

/*
 * @brief Load all game media through the relevant subsystems. This is called when
 * spawning into a server. For incremental reloads on subsystem restarts,
 * see Cl_UpdateMedia.
 */
void Cl_LoadMedia(void) {

	cls.state = CL_LOADING;

	Cl_UpdatePrediction();

	R_LoadMedia();

	S_LoadMedia();

	Cl_LoadingProgress(88, "entities");

	Cl_UpdateEntities();

	Cl_LoadingProgress(95, "effects");

	cls.cgame->UpdateMedia();

	Cl_LoadingProgress(100, "ready");

	Cl_SetKeyDest(KEY_GAME);
}

/*
 * @brief Reload stale media references on subsystem restarts.
 */
void Cl_UpdateMedia(void) {

	if ((r_view.update || s_env.update) && cls.state == CL_ACTIVE) {

		Com_Debug("%s %s\n", r_view.update ? "view" : "", s_env.update ? "sound" : "");

		Cl_UpdateEntities();

		cls.cgame->UpdateMedia();
	}
}
