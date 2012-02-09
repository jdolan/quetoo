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

cvar_t *cg_add_emits;
cvar_t *cg_add_entities;
cvar_t *cg_add_particles;
cvar_t *cg_add_weather;
cvar_t *cg_bob;
cvar_t *cg_crosshair_color;
cvar_t *cg_crosshair_scale;
cvar_t *cg_crosshair;
cvar_t *cg_draw_blend;
cvar_t *cg_draw_hud;
cvar_t *cg_draw_weapon;
cvar_t *cg_fov;
cvar_t *cg_fov_zoom;
cvar_t *cg_third_person;

cg_import_t cgi;

/*
 * Cg_Init
 */
static void Cg_Init(void) {

	cgi.Print("  Client game initialization...\n");

	cg_add_emits = cgi.Cvar("cg_add_emits", "1", 0,
			"Toggles adding client-side entities to the scene.");
	cg_add_particles = cgi.Cvar("cg_add_particles", "1", 0,
			"Toggles adding particles to the scene.");
	cg_add_entities = cgi.Cvar("cg_add_entities", "1", 0, "Toggles adding entities to the scene.");
	cg_add_weather = cgi.Cvar("cg_add_weather", "1", CVAR_ARCHIVE,
			"Control the intensity of atmospheric effects.");

	cg_bob = cgi.Cvar("cg_bob", "1.0", CVAR_ARCHIVE, "Controls weapon bobbing effect.");

	cg_crosshair = cgi.Cvar("cg_crosshair", "1", CVAR_ARCHIVE, NULL);
	cg_crosshair_color = cgi.Cvar("cg_crosshair_color", "default", CVAR_ARCHIVE,
			"Specifies the crosshair color.");
	cg_crosshair_scale = cgi.Cvar("cg_crosshair_scale", "1.0", CVAR_ARCHIVE,
			"Controls the crosshair scale (size).");

	cg_draw_blend = cgi.Cvar("cg_draw_blend", "1.0", CVAR_ARCHIVE,
			"Controls the intensity of screen alpha-blending");
	cg_draw_hud = cgi.Cvar("cg_draw_hud", "1", CVAR_ARCHIVE, "Render the Heads-Up-Display");
	cg_draw_weapon = cgi.Cvar("cg_draw_weapon", "1", CVAR_ARCHIVE,
			"Toggle drawing of the weapon model.");

	cg_fov = Cvar_Get("cg_fov", "100.0", CVAR_ARCHIVE, NULL);
	cg_fov_zoom = Cvar_Get("cg_fov_zoom", "40.0", CVAR_ARCHIVE, NULL);

	cg_third_person = cgi.Cvar("cg_third_person", "0.0", 0, "Activate third person perspective.");

	cgi.Print("  Client game initialized.\n");
}

/*
 * Cg_Shutdown
 */
static void Cg_Shutdown(void) {
	cgi.Print("  Client game shutdown...\n");
}

/*
 * Cg_ParseMessage
 *
 * Parse a single server command, returning true on success.
 */
static boolean_t Cg_ParseMessage(int cmd) {

	switch (cmd) {
	case SV_CMD_TEMP_ENTITY:
		Cg_ParseTempEntity();
		return true;

	case SV_CMD_MUZZLE_FLASH:
		Cg_ParseMuzzleFlash();
		return true;

	case SV_CMD_SCORES:
		Cg_ParseScores();
		return true;

	case SV_CMD_CENTER_PRINT:
		Cg_ParseCenterPrint();
		return true;

	default:
		break;
	}

	return false;
}

/*
 * Cg_DrawFrame
 */
static void Cg_DrawFrame(const cl_frame_t *frame) {

	Cg_DrawHud(&frame->ps);

	Cg_DrawScores(&frame->ps);
}

/*
 * Cg_ClearState
 */
static void Cg_ClearState(void) {

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
	cge.ClearState = Cg_ClearState;
	cge.UpdateMedia = Cg_UpdateMedia;
	cge.ParseMessage = Cg_ParseMessage;
	cge.UpdateView = Cg_UpdateView;
	cge.PopulateView = Cg_PopulateView;
	cge.DrawFrame = Cg_DrawFrame;

	return &cge;
}
