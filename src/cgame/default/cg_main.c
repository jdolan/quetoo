/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "cg_local.h"

cvar_t *cg_blend;
cvar_t *cg_crosshair;
cvar_t *cg_crosshair_color;
cvar_t *cg_crosshair_scale;
cvar_t *cg_hud;
cvar_t *cg_third_person;

cg_import_t cgi;

/*
 * Cg_Init
 */
static void Cg_Init(void) {

	cgi.Print("  Client game initialization...\n");

	cg_blend = cgi.Cvar("cg_blend", "1.0", CVAR_ARCHIVE,
			"Controls the intensity of screen alpha-blending");
	cg_crosshair = cgi.Cvar("cg_crosshair", "1", CVAR_ARCHIVE, NULL);
	cg_crosshair_color = cgi.Cvar("cg_crosshair_color", "default",
			CVAR_ARCHIVE, NULL);
	cg_crosshair_scale = cgi.Cvar("cg_crosshair_scale", "1.0", CVAR_ARCHIVE,
			NULL);
	cg_hud = cgi.Cvar("cg_hud", "1", CVAR_ARCHIVE,
			"Render the Heads-Up-Display");
	cg_third_person = cgi.Cvar("cg_third_person", "0", CVAR_ARCHIVE,
			"Toggles the third person camera.");

	cgi.Print("  Client game initialized.\n");
}

/*
 * Cg_Shutdown
 */
static void Cg_Shutdown(void) {
	cgi.Print("  Client game shutdown...\n");
}

/*
 * Cg_UpdateMedia
 */
static void Cg_UpdateMedia(void) {
	cg_crosshair->modified = true;
	cg_crosshair_color->modified = true;
}

/*
 * Cg_ParseMessage
 */
static boolean_t Cg_ParseMessage(int cmd) {

	switch (cmd) {
	case SV_CMD_SCORES:
		Cg_ParseScores();
		return true;

	default:
		break;
	}

	return false;
}

/*
 * Cg_DrawFrame
 */
static void Cg_DrawFrame(player_state_t *ps) {

	Cg_DrawHud(ps);

	Cg_DrawScores(ps);
}

/*
 * Cg_LoadCgame
 */
cg_export_t *Cg_LoadCgame(cg_import_t *import) {
	static cg_export_t cge;

	cgi = *import;

	cge.api_version = CGAME_API_VERSION;

	cge.Init = Cg_Init;
	cge.Shutdown = Cg_Shutdown;
	cge.UpdateMedia = Cg_UpdateMedia;
	cge.ParseMessage = Cg_ParseMessage;
	cge.DrawFrame = Cg_DrawFrame;

	return &cge;
}
