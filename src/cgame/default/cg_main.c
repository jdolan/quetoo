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

#include "cg_local.h"

cvar_t *cg_add_emits;
cvar_t *cg_add_entities;
cvar_t *cg_add_particles;
cvar_t *cg_add_weather;
cvar_t *cg_bob;
cvar_t *cg_draw_blend;
cvar_t *cg_draw_captures;
cvar_t *cg_draw_crosshair_color;
cvar_t *cg_draw_crosshair_pulse;
cvar_t *cg_draw_crosshair_scale;
cvar_t *cg_draw_crosshair;
cvar_t *cg_draw_frags;
cvar_t *cg_draw_hud;
cvar_t *cg_draw_pickup;
cvar_t *cg_draw_time;
cvar_t *cg_draw_teambar;
cvar_t *cg_draw_weapon;
cvar_t *cg_draw_weapon_alpha;
cvar_t *cg_draw_weapon_x;
cvar_t *cg_draw_weapon_y;
cvar_t *cg_draw_weapon_z;
cvar_t *cg_draw_vitals;
cvar_t *cg_draw_vitals_pulse;
cvar_t *cg_draw_vote;
cvar_t *cg_fov;
cvar_t *cg_fov_zoom;
cvar_t *cg_fov_interpolate;
cvar_t *cg_third_person;
cvar_t *cg_third_person_yaw;

cg_import_t cgi;

/**
 * @brief Called when the client first comes up or switches game directories. Client
 * game modules should bootstrap any globals they require here.
 */
static void Cg_Init(void) {

	cgi.Print("  Client game initialization...\n");

	cg_add_emits = cgi.Cvar("cg_add_emits", "1", 0,
			"Toggles adding client-side entities to the scene.");
	cg_add_entities = cgi.Cvar("cg_add_entities", "1", 0, "Toggles adding entities to the scene.");
	cg_add_particles = cgi.Cvar("cg_add_particles", "1", 0,
			"Toggles adding particles to the scene.");
	cg_add_weather = cgi.Cvar("cg_add_weather", "1", CVAR_ARCHIVE,
			"Control the intensity of atmospheric effects.");

	cg_bob = cgi.Cvar("cg_bob", "1.0", CVAR_ARCHIVE, "Controls weapon bobbing effect.");

	cg_draw_blend = cgi.Cvar("cg_draw_blend", "1.0", CVAR_ARCHIVE,
			"Controls the intensity of screen alpha-blending");
	cg_draw_captures = cgi.Cvar("cg_draw_captures", "1", CVAR_ARCHIVE,
			"Draw the number of captures");
	cg_draw_crosshair = cgi.Cvar("cg_draw_crosshair", "1", CVAR_ARCHIVE, NULL);
	cg_draw_crosshair_color = cgi.Cvar("cg_draw_crosshair_color", "", CVAR_ARCHIVE,
			"Specifies the crosshair color (red|green|yellow|default).");
	cg_draw_crosshair_pulse = cgi.Cvar("cg_draw_crosshair_pulse", "1.0", CVAR_ARCHIVE,
			"Pulse the crosshair when picking up items");
	cg_draw_crosshair_scale = cgi.Cvar("cg_draw_crosshair_scale", "1.0", CVAR_ARCHIVE,
			"Controls the crosshair scale (size).");

	cg_draw_frags = cgi.Cvar("cg_draw_frags", "1", CVAR_ARCHIVE, "Draw the number of frags");
	cg_draw_hud = cgi.Cvar("cg_draw_hud", "1", CVAR_ARCHIVE, "Render the Heads-Up-Display");
	cg_draw_pickup = cgi.Cvar("cg_draw_pickup", "1", CVAR_ARCHIVE, "Draw the current pickup");
	cg_draw_time = cgi.Cvar("cg_draw_time", "1", CVAR_ARCHIVE, "Draw the time remaning");
	cg_draw_teambar = cgi.Cvar("cg_draw_teambar", "1", CVAR_ARCHIVE, "Draw the teambanner");

	cg_draw_weapon = cgi.Cvar("cg_draw_weapon", "1", CVAR_ARCHIVE,
			"Toggle drawing of the weapon model.");
	cg_draw_weapon_alpha = cgi.Cvar("cg_draw_weapon_alpha", "1.0", CVAR_ARCHIVE,
			"The alpha transparency for drawing the weapon model");
	cg_draw_weapon_x = cgi.Cvar("cg_draw_weapon_x", "0.0", CVAR_ARCHIVE,
			"The x offset for drawing the weapon model.");
	cg_draw_weapon_y = cgi.Cvar("cg_draw_weapon_y", "0.0", CVAR_ARCHIVE,
			"The y offset for drawing the weapon model.");
	cg_draw_weapon_z = cgi.Cvar("cg_draw_weapon_z", "0.0", CVAR_ARCHIVE,
			"The z offset for drawing the weapon model.");
	cg_draw_vitals = cgi.Cvar("cg_draw_vitals", "1", CVAR_ARCHIVE,
			"Draw the vitals (health, armor, ammo)");
	cg_draw_vitals_pulse = cgi.Cvar("cg_draw_vitals_pulse", "1", CVAR_ARCHIVE,
			"Pulse the vitals when low");
	cg_draw_vote = cgi.Cvar("cg_draw_vote", "1", CVAR_ARCHIVE, "Draw the current vote on the hud");

	cg_fov = cgi.Cvar("cg_fov", "110.0", CVAR_ARCHIVE, "Horizontal field of view, in degrees");
	cg_fov_zoom = cgi.Cvar("cg_fov_zoom", "55.0", CVAR_ARCHIVE, "Zoomed in field of view");
	cg_fov_interpolate = cgi.Cvar("cg_fov_interpolate", "1.0", CVAR_ARCHIVE,
			"Interpolate between field of view changes (default 1.0).");

	cg_third_person = cgi.Cvar("cg_third_person", "0.0", CVAR_ARCHIVE | CVAR_LO_ONLY,
			"Activate third person perspective.");
	cg_third_person_yaw = cgi.Cvar("cg_third_person_yaw", "0.0", CVAR_ARCHIVE,
			"Yaw offset for third person perspective.");

	// add forward to server commands for tab completion

	cgi.Cmd("wave", NULL, CMD_CGAME, NULL);
	cgi.Cmd("kill", NULL, CMD_CGAME, NULL);
	cgi.Cmd("use", NULL, CMD_CGAME, NULL);
	cgi.Cmd("drop", NULL, CMD_CGAME, NULL);
	cgi.Cmd("say", NULL, CMD_CGAME, NULL);
	cgi.Cmd("say_team", NULL, CMD_CGAME, NULL);
	cgi.Cmd("info", NULL, CMD_CGAME, NULL);
	cgi.Cmd("give", NULL, CMD_CGAME, NULL);
	cgi.Cmd("god", NULL, CMD_CGAME, NULL);
	cgi.Cmd("no_clip", NULL, CMD_CGAME, NULL);
	cgi.Cmd("weapon_next", NULL, CMD_CGAME, NULL);
	cgi.Cmd("weapon_previous", NULL, CMD_CGAME, NULL);
	cgi.Cmd("weapon_last", NULL, CMD_CGAME, NULL);
	cgi.Cmd("vote", NULL, CMD_CGAME, NULL);
	cgi.Cmd("team", NULL, CMD_CGAME, NULL);
	cgi.Cmd("team_name", NULL, CMD_CGAME, NULL);
	cgi.Cmd("team_skin", NULL, CMD_CGAME, NULL);
	cgi.Cmd("spectate", NULL, CMD_CGAME, NULL);
	cgi.Cmd("join", NULL, CMD_CGAME, NULL);
	cgi.Cmd("score", NULL, CMD_CGAME, NULL);
	cgi.Cmd("ready", NULL, CMD_CGAME, NULL);
	cgi.Cmd("unready", NULL, CMD_CGAME, NULL);
	cgi.Cmd("player_list", NULL, CMD_CGAME, NULL);
	cgi.Cmd("config_strings", NULL, CMD_CGAME, NULL);
	cgi.Cmd("baselines", NULL, CMD_CGAME, NULL);

	cgi.Print("  Client game initialized\n");
}

/**
 * @brief Called when switching game directories or quitting.
 */
static void Cg_Shutdown(void) {

	cgi.Print("  Client game shutdown...\n");

	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);
	cgi.FreeTag(MEM_TAG_CGAME);
}

/**
 * @brief Parse a single server command, returning true on success.
 */
static _Bool Cg_ParseMessage(int32_t cmd) {

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

/**
 * @brief
 */
static void Cg_DrawFrame(const cl_frame_t *frame) {

	Cg_DrawHud(&frame->ps);

	Cg_DrawScores(&frame->ps);
}

/**
 * @brief Clear any state that should not persist over multiple server connections.
 */
static void Cg_ClearState(void) {

}

/**
 * @brief An updated configuration string has just been received from the server.
 * Refresh related variables and media that aren't managed by the engine.
 */
static void Cg_UpdateConfigString(uint16_t i) {

	const char *s = cgi.ConfigString(i);

	if (i == CS_WEATHER) {
		Cg_ResolveWeather(s);
	}

	if (i >= CS_CLIENTS && i < CS_CLIENTS + MAX_CLIENTS) {
		cl_client_info_t *ci = &cgi.client->client_info[i - CS_CLIENTS];
		Cg_LoadClient(ci, s);
	}
}

/**
 * @brief
 */
cg_export_t *Cg_LoadCgame(cg_import_t *import) {
	static cg_export_t cge;

	cgi = *import;

	cge.api_version = CGAME_API_VERSION;
	cge.protocol = PROTOCOL_MINOR;

	cge.Init = Cg_Init;
	cge.Shutdown = Cg_Shutdown;
	cge.ClearState = Cg_ClearState;
	cge.UpdateMedia = Cg_UpdateMedia;
	cge.UpdateConfigString = Cg_UpdateConfigString;
	cge.LoadClient = Cg_LoadClient;
	cge.ParseMessage = Cg_ParseMessage;
	cge.PredictMovement = Cg_PredictMovement;
	cge.UpdateView = Cg_UpdateView;
	cge.PopulateView = Cg_PopulateView;
	cge.DrawFrame = Cg_DrawFrame;

	return &cge;
}
