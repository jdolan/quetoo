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

#include "g_local.h"

g_import_t gi;
g_export_t ge;

ai_export_t *aix;

g_game_t g_game;

g_level_t g_level;
g_media_t g_media;

cvar_t *g_admin_password;
cvar_t *g_ammo_respawn_time;
cvar_t *g_auto_join;
cvar_t *g_balance_bfg_damage;
cvar_t *g_balance_bfg_knockback;
cvar_t *g_balance_bfg_prefire;
cvar_t *g_balance_bfg_radius;
cvar_t *g_balance_bfg_refire;
cvar_t *g_balance_bfg_speed;
cvar_t *g_balance_blaster_damage;
cvar_t *g_balance_blaster_knockback;
cvar_t *g_balance_blaster_refire;
cvar_t *g_balance_blaster_speed;
cvar_t *g_balance_handgrenade_refire;
cvar_t *g_balance_hyperblaster_climb_damage;
cvar_t *g_balance_hyperblaster_climb_knockback;
cvar_t *g_balance_hyperblaster_damage;
cvar_t *g_balance_hyperblaster_knockback;
cvar_t *g_balance_hyperblaster_refire;
cvar_t *g_balance_hyperblaster_speed;
cvar_t *g_balance_lightning_damage;
cvar_t *g_balance_lightning_knockback;
cvar_t *g_balance_lightning_length;
cvar_t *g_balance_lightning_refire;
cvar_t *g_balance_machinegun_damage;
cvar_t *g_balance_machinegun_knockback;
cvar_t *g_balance_machinegun_refire;
cvar_t *g_balance_machinegun_spread_x;
cvar_t *g_balance_machinegun_spread_y;
cvar_t *g_balance_grenadelauncher_damage;
cvar_t *g_balance_grenadelauncher_knockback;
cvar_t *g_balance_grenadelauncher_radius;
cvar_t *g_balance_grenadelauncher_refire;
cvar_t *g_balance_grenadelauncher_speed;
cvar_t *g_balance_grenadelauncher_timer;
cvar_t *g_balance_railgun_damage;
cvar_t *g_balance_railgun_knockback;
cvar_t *g_balance_railgun_refire;
cvar_t *g_balance_rocketlauncher_damage;
cvar_t *g_balance_rocketlauncher_knockback;
cvar_t *g_balance_rocketlauncher_radius;
cvar_t *g_balance_rocketlauncher_refire;
cvar_t *g_balance_rocketlauncher_speed;
cvar_t *g_balance_shotgun_damage;
cvar_t *g_balance_shotgun_knockback;
cvar_t *g_balance_shotgun_pellets;
cvar_t *g_balance_shotgun_refire;
cvar_t *g_balance_shotgun_spread_x;
cvar_t *g_balance_shotgun_spread_y;
cvar_t *g_balance_shotgun_pellets;
cvar_t *g_balance_supershotgun_damage;
cvar_t *g_balance_supershotgun_knockback;
cvar_t *g_balance_supershotgun_pellets;
cvar_t *g_balance_supershotgun_refire;
cvar_t *g_balance_supershotgun_spread_x;
cvar_t *g_balance_supershotgun_spread_y;
cvar_t *g_capture_limit;
cvar_t *g_cheats;
cvar_t *g_ctf;
cvar_t *g_techs;
cvar_t *g_hook;
cvar_t *g_hook_auto_refire;
cvar_t *g_hook_distance;
cvar_t *g_hook_refire;
cvar_t *g_hook_style;
cvar_t *g_hook_speed;
cvar_t *g_hook_pull_speed;
cvar_t *g_frag_limit;
cvar_t *g_friendly_fire;
cvar_t *g_force_demo;
cvar_t *g_force_screenshot;
cvar_t *g_gameplay;
cvar_t *g_gravity;
cvar_t *g_handicap;
cvar_t *g_inhibit;
cvar_t *g_match;
cvar_t *g_max_entities;
cvar_t *g_motd;
cvar_t *g_num_teams;
cvar_t *g_password;
static cvar_t *g_paused;
cvar_t *g_player_projectile;
cvar_t *g_quad_damage_respawn_time;
cvar_t *g_quad_damage_time;
cvar_t *g_random_map;
cvar_t *g_respawn_protection;
cvar_t *g_round_limit;
cvar_t *g_rounds;
cvar_t *g_self_damage;
cvar_t *g_show_attacker_stats;
cvar_t *g_spawn_farthest;
cvar_t *g_spectator_chat;
cvar_t *g_teams;
cvar_t *g_time_limit;
cvar_t *g_timeout_time;
cvar_t *g_voting;
cvar_t *g_warmup_time;
cvar_t *g_weapon_respawn_time;
cvar_t *g_weapon_stay;

cvar_t *sv_max_clients;
cvar_t *sv_hostname;
cvar_t *dedicated;

g_team_t g_teamlist[MAX_TEAMS];
static g_team_t g_teamlist_default[MAX_TEAMS];

static struct {
	cvar_t		*g_team_name;
	cvar_t		*g_team_shirt;
	cvar_t		*g_team_color;
} g_team_cvars[MAX_TEAMS];

static const struct {
	const char	*name;
	const char	*shirt;
	uint16_t	color;
} g_default_team_values[MAX_TEAMS] = {
	{ "Red", "ff0000", TEAM_COLOR_RED },
	{ "Blue", "0000ff", TEAM_COLOR_BLUE },
	{ "Yellow", "ffff00", TEAM_COLOR_YELLOW },
	{ "White", "ffffff", TEAM_COLOR_WHITE }
};

/**
 * @brief
 */
static void G_InitTeam(const g_team_id_t id, const char *name,
					   const char *shirt,
					   const int16_t color,
					   const uint32_t effect) {

	g_team_t *team = &g_teamlist[id];

	team->id = id;

	g_strlcpy(team->name, name, sizeof(team->name));

	team->color = color;

	if (!Color_Parse(shirt, &team->shirt)) {
		gi.Warn("Failed to parse default team color %s\n", shirt);
		team->shirt = color_white;
	}
	team->pants = team->helmet = team->shirt;

	// FIXME: enforcing "ctf" skin is kinda dumb if we have tints. it should just enforce
	// that the skin has tints (on client side), and if it doesn't, fall back to "ctf", "team" or "default", whichever
	// first has tints
	g_strlcpy(team->skin, DEFAULT_TEAM_SKIN, sizeof(team->skin));

	team->effect = effect;

	g_strlcpy(team->flag, va("item_flag_team%i", id + 1), sizeof(team->flag));
	g_strlcpy(team->spawn, va("info_player_team%i", id + 1), sizeof(team->spawn));
}

/**
 * @brief
 */
void G_ResetTeams(void) {

	memset(g_teamlist, 0, sizeof(g_teamlist));

	for (int32_t i = 0; i < MAX_TEAMS; i++) {
		G_InitTeam(i,
				   g_team_cvars[i].g_team_name->string,
				   g_team_cvars[i].g_team_shirt->string,
				   g_team_cvars[i].g_team_color->integer,
				   EF_CTF_RED << i);
	}

	memcpy(g_teamlist_default, g_teamlist, sizeof(g_teamlist));

	G_SetTeamNames();
}

/**
 * @brief Send the names of the teams to the clients.
 */
void G_SetTeamNames(void) {
	char team_info[MAX_STRING_CHARS] = { '\0' };

	for (int32_t t = 0; t < MAX_TEAMS; t++) {

		if (t != TEAM_RED) {
			g_strlcat(team_info, "\\", sizeof(team_info));
		}

		g_strlcat(team_info, g_teamlist[t].name, sizeof(team_info));
		g_strlcat(team_info, "\\", sizeof(team_info));
		g_strlcat(team_info, va("%i", g_teamlist[t].color), sizeof(team_info));
		g_strlcat(team_info, "\\", sizeof(team_info));
		g_strlcat(team_info, Color_Unparse(g_teamlist[t].shirt), sizeof(team_info));
	}

	gi.SetConfigString(CS_TEAM_INFO, team_info);
}

/**
 * @brief Fetch the defaults for a team
 */
const g_team_t *G_TeamDefaults(const g_team_t *team) {

	return &g_teamlist_default[team->id];
}

/**
 * @brief
 */
void G_ResetVote(void) {
	int32_t i;

	for (i = 0; i < sv_max_clients->integer; i++) { //reset vote flags

		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		g_game.entities[i + 1].client->locals.persistent.vote = VOTE_NO_OP;
	}

	gi.SetConfigString(CS_VOTE, NULL);

	g_level.votes[0] = g_level.votes[1] = g_level.votes[2] = 0;
	g_level.vote_cmd[0] = 0;

	g_level.vote_time = 0;
}

/**
 * @brief Reset all items in the level based on gameplay, CTF, etc.
 */
void G_ResetItems(void) {

	for (uint16_t i = 1; i < ge.num_entities; i++) { // reset items

		g_entity_t *ent = &g_game.entities[i];

		if (!ent->in_use) {
			continue;
		}

		if (!ent->locals.item) {
			continue;
		}

		if (ent->locals.spawn_flags & SF_ITEM_DROPPED) { // free dropped ones
			G_FreeEntity(ent);
			continue;
		}

		if (ent->locals.item->type == ITEM_TECH) { // free techs, we'll spawn them again
			G_FreeEntity(ent);
			continue;
		}

		G_ResetItem(ent);
	}

	G_SpawnTechs();

	G_Ai_RegisterItems();
}

/**
 * @brief Checks and sets up the hook state
 */
void G_CheckHook(void) {

	if (g_strcmp0(g_hook->string, "default")) { // check cvar first
		g_level.hook_allowed = !!g_hook->integer;
	} else if (g_level.hook_map != -1) { // check maps.lst
		g_level.hook_allowed = (g_level.hook_map == -1) ? g_level.ctf : !!g_level.hook_map;
	} else { // check worldspawn
		const cm_entity_t *hook = gi.EntityValue(g_game.entities->def, "hook");
		if (hook->parsed & ENTITY_INTEGER) {
			g_level.hook_allowed = hook->integer;
		} else {
			g_level.hook_allowed = g_level.ctf;
		}
	}

	if (g_hook_distance->modified) {
		g_hook_distance->value = Clampf(g_hook_distance->value, PM_HOOK_MIN_DIST, PM_HOOK_MAX_DIST);
		g_hook_distance->modified = false;
	}
}

/**
 * @brief Checks and sets up the tech states
 */
void G_CheckTechs(void) {

	if (g_strcmp0(g_techs->string, "default")) { // check cvar first
		g_level.techs = !!g_techs->integer;
	} else if (g_level.techs_map != -1) { // check maps.lst
		g_level.techs = (g_level.techs_map == -1) ? g_level.ctf : !!g_level.techs_map;
	} else { // check worldspawn
		const cm_entity_t *techs = gi.EntityValue(g_game.entities->def, "techs");
		if (techs->parsed & ENTITY_INTEGER) {
			g_level.techs = techs->integer;
		} else {
			g_level.techs = g_level.ctf;
		}
	}
}

/**
 * @brief Setup the effects for spawn points
 */
static void G_ResetTeamSpawnPoints(g_spawn_points_t *points, const g_entity_trail_t trail, const g_team_id_t team_id) {

	for (size_t i = 0; i < points->count; i++) {
		g_entity_t *ent = points->spots[i];

		if (trail && (g_level.teams || g_level.ctf)) {

			if (ent->s.trail) {
				ent->s.client = MAX_TEAMS;
			} else {
				ent->s.client = team_id;
			}
			ent->s.trail = trail;
			ent->sv_flags = 0;

			gi.LinkEntity(ent);
		} else {

			ent->s.trail = 0;
			ent->sv_flags = SVF_NO_CLIENT;

			gi.UnlinkEntity(ent);
		}
	}
}

/**
 * @brief Setup the effects for spawn points
 */
void G_ResetSpawnPoints(void) {

	// reset trails to 0 first
	for (int32_t t = 0; t < MAX_TEAMS; t++) {
		G_ResetTeamSpawnPoints(&g_teamlist[t].spawn_points, 0, 0);
	}

	// then apply team-based trails, this is done twice
	// so neutrality gets applied properly
	for (int32_t t = 0; t < MAX_TEAMS; t++) {
		G_ResetTeamSpawnPoints(&g_teamlist[t].spawn_points, TRAIL_PLAYER_SPAWN, t);
	}
}

/**
 * @brief For normal games, this just means reset scores and respawn.
 * For match games, this means cancel the match and force everyone
 * to ready again. Teams are only reset when teamz is true.
 */
static void G_RestartGame(_Bool teamz) {

	if (aix) { // reset bot level state before they respawn
		aix->GameRestarted();
	}

	if (g_level.match_time) {
		g_level.match_num++;
	}

	if (g_level.round_time) {
		g_level.round_num++;
	}

	for (int32_t i = 0; i < sv_max_clients->integer; i++) { // reset clients

		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		g_entity_t *ent = &g_game.entities[i + 1];
		g_client_t *cl = ent->client;

		cl->locals.persistent.ready = false; // back to warmup
		cl->locals.persistent.score = 0;
		cl->locals.persistent.captures = 0;
		cl->locals.persistent.deaths = 0;

		if (teamz) { // reset teams
			cl->locals.persistent.team = NULL;
		}

		// determine spectator or team affiliations

		if (g_level.match) {
			if (cl->locals.persistent.match_num == g_level.match_num) {
				cl->locals.persistent.spectator = false;
			} else {
				cl->locals.persistent.spectator = true;
			}
		}

		else if (g_level.rounds) {
			if (cl->locals.persistent.round_num == g_level.round_num) {
				cl->locals.persistent.spectator = false;
			} else {
				cl->locals.persistent.spectator = true;
			}
		}

		if (g_level.teams || g_level.ctf) {

			if (!cl->locals.persistent.team) {
				if (g_auto_join->value && g_level.gameplay != GAME_DUEL) {
					G_AddClientToTeam(ent, G_SmallestTeam()->name);
				} else {
					cl->locals.persistent.spectator = true;
				}
			}
		}

		G_ClientUserInfoChanged(ent, ent->client->locals.persistent.user_info);
		G_ClientRespawn(ent, false);
	}

	G_CheckHook();

	G_CheckTechs();

	G_ResetItems();

	G_ResetSpawnPoints();

	G_InitNumTeams();

	g_level.match_time = g_level.round_time = 0;

	for (int32_t i = 0; i < MAX_TEAMS; i++) {
		g_teamlist[i].score = g_teamlist[i].captures = 0;
	}

	gi.BroadcastPrint(PRINT_HIGH, "Game restarted\n");

	G_Sound(&(const g_play_sound_t) {
		.index = g_media.sounds.teleport,
		.atten = SOUND_ATTEN_NONE
	});
}

/**
 * @brief
 */
void G_MuteClient(char *name, _Bool mute) {
	g_client_t *cl;

	if (!(cl = G_ClientByName(name))) {
		return;
	}

	cl->locals.persistent.muted = mute;
}

/**
 * @brief
 */
static void G_BeginIntermission(const char *map) {

	if (g_level.intermission_time) {
		return; // already activated
	}

	g_level.intermission_time = g_level.time;

	// respawn any dead clients
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *client = g_game.entities + 1 + i;

		if (!client->in_use) {
			continue;
		}

		if (client->locals.health <= 0) {
			G_ClientRespawn(client, false);
		}
	}

	// find an intermission spot
	g_entity_t *ent = G_Find(NULL, EOFS(class_name), "info_player_intermission");
	if (!ent) { // map does not have an intermission point
		ent = G_Find(NULL, EOFS(class_name), "info_player_start");
		if (!ent) {
			ent = G_Find(NULL, EOFS(class_name), "info_player_deathmatch");
		}
	}

	g_level.intermission_origin = ent->s.origin;
	g_level.intermission_angle = ent->s.angles;

	// move all clients to the intermission point
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *client = g_game.entities + 1 + i;

		if (!client->in_use) {
			continue;
		}

		G_ClientToIntermission(client);
	}

	// play a dramatic sound effect
	G_Sound(&(const g_play_sound_t) {
		.index = g_media.sounds.roar,
		.atten = SOUND_ATTEN_NONE
	});

	// stay on same level if not provided
	g_level.next_map = map ?: g_level.name;
}

/**
 * @brief The time limit, frag limit, etc.. has been exceeded.
 */
static void G_EndLevel(void) {

	const g_map_list_map_t *map = G_MapList_Next();

	g_level.match_status = 0;

	// always stay on the same map when in match mode
	if (map && !g_level.match) {
		G_BeginIntermission(map->name);
	} else {
		G_BeginIntermission(NULL);
	}
}

/**
 * @brief
 */
static void G_CheckVote(void) {
	int32_t i, count = 0;

	if (!g_voting->value) {
		return;
	}

	if (g_level.vote_time == 0) {
		return;
	}

	if (g_level.time - g_level.vote_time > MAX_VOTE_TIME) {
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" expired\n", g_level.vote_cmd);
		G_ResetVote();
		return;
	}

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}
		count++;
	}

	if (g_level.votes[VOTE_YES] >= count * VOTE_MAJORITY) { // vote passed

		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" passed\n", g_level.vote_cmd);

		if (!strncmp(g_level.vote_cmd, "map ", 4)) { // special case for map
			G_BeginIntermission(g_level.vote_cmd + 4);
		} else if (!g_strcmp0(g_level.vote_cmd, "next_map")) { // and next_map
			G_EndLevel();
		} else if (!g_strcmp0(g_level.vote_cmd, "restart")) { // and restart
			G_RestartGame(false);
		} else if (!strncmp(g_level.vote_cmd, "mute ", 5)) { // and mute
			G_MuteClient(g_level.vote_cmd + 5, true);
		} else if (!strncmp(g_level.vote_cmd, "unmute ", 7)) {
			G_MuteClient(g_level.vote_cmd + 7, false);
		} else { // general case, just execute the command
			gi.Cbuf(g_level.vote_cmd);
		}
		G_ResetVote();
	} else if (g_level.votes[VOTE_NO] >= count * VOTE_MAJORITY) { // vote failed
		gi.BroadcastPrint(PRINT_HIGH, "Vote \"%s\" failed\n", g_level.vote_cmd);
		G_ResetVote();
	}
}

/**
 * @brief
 */
static void G_CheckRoundStart(void) {
	int32_t i, clients;
	g_client_t *cl;

	if (!g_level.rounds) {
		return;
	}

	if (g_level.round_time) {
		return;
	}

	clients = 0;

	uint8_t teams_ready[MAX_TEAMS];
	memset(teams_ready, 0, sizeof(teams_ready));

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		cl = g_game.entities[i + 1].client;

		if (cl->locals.persistent.spectator) {
			continue;
		}

		clients++;

		if (g_level.teams && cl->locals.persistent.team) {
			teams_ready[cl->locals.persistent.team->id]++;
		}
	}

	if (clients < 2) { // need at least 2 clients to trigger countdown
		return;
	}

	if (g_level.teams || g_level.ctf) { // need at least 1 player per team

		for (int32_t i = 0; i < g_level.num_teams; i++) {
			if (!teams_ready[i]) {
				return;
			}
		}
	}

	if ((int32_t) g_level.teams == 2 || (int32_t) g_level.ctf == 2) { // balanced teams required

		for (int32_t i = 0; i < g_level.num_teams; i++) {
			if (teams_ready[i] != teams_ready[0]) {
				if (g_level.frame_num % 100 == 0) {
					gi.BroadcastPrint(PRINT_HIGH, "Teams must be balanced for round to start\n");
				}
				return;
			}
		}
	}

	gi.BroadcastPrint(PRINT_HIGH, "Round starting in 10 seconds...\n");
	g_level.round_time = g_level.time + 10000;

	g_level.start_round = true;
}

/**
 * @brief
 */
static void G_CheckRoundLimit() {
	int32_t i;
	g_entity_t *ent;
	g_client_t *cl;

	if (g_level.round_num >= (uint32_t) g_level.round_limit) { // enforce round_limit
		gi.BroadcastPrint(PRINT_HIGH, "Roundlimit hit\n");
		G_EndLevel();
		return;
	}

	// or attempt to re-join previously active players
	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		ent = &g_game.entities[i + 1];
		cl = ent->client;

		if (cl->locals.persistent.round_num != g_level.round_num) {
			continue; // they were intentionally spectating, skip them
		}

		if (g_level.teams || g_level.ctf) { // rejoin a team
			if (cl->locals.persistent.team) {
				G_AddClientToTeam(ent, cl->locals.persistent.team->name);
			} else {
				G_AddClientToTeam(ent, G_SmallestTeam()->name);
			}
		} else
			// just rejoin the game
		{
			cl->locals.persistent.spectator = false;
		}

		G_ClientRespawn(ent, false);
	}
}

/**
 * @brief
 */
static void G_CheckRoundEnd(void) {

	if (!g_level.rounds) {
		return;
	}

	if (!g_level.round_time || g_level.round_time > g_level.time) {
		return; // no round currently running
	}

	g_entity_t *winner = NULL;
	int32_t clients = 0;

	uint8_t teams_count[MAX_TEAMS];
	memset(teams_count, 0, sizeof(teams_count));

	for (int32_t j = 0; j < sv_max_clients->integer; j++) {
		if (!g_game.entities[j + 1].in_use) {
			continue;
		}

		g_client_t *cl = g_game.entities[j + 1].client;

		if (cl->locals.persistent.spectator) { // true spectator, or dead
			continue;
		}

		winner = &g_game.entities[j + 1];

		if (g_level.teams) {
			teams_count[cl->locals.persistent.team->id]++;
		}

		clients++;
	}

	if (clients == 0) { // corner case where everyone was fragged
		gi.BroadcastPrint(PRINT_HIGH, "Tie!\n");
		g_level.round_time = 0;
		G_CheckRoundLimit();
		return;
	}

	if (g_level.teams || g_level.ctf) { // teams rounds continue if each team has a player
		_Bool do_continue = true;

		for (int32_t i = 0; do_continue && i < g_level.num_teams; i++) {
			do_continue = do_continue && !teams_count[i];
		}

		if (do_continue) {
			return;
		}
	} else if (clients > 1) { // ffa continues if two players are alive
		return;
	}

	// allow enemy projectiles to expire before declaring a winner
	for (int32_t i = 0; i < ge.num_entities; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		if (!g_game.entities[i + 1].owner) {
			continue;
		}

		g_client_t *cl = g_game.entities[i + 1].owner->client;

		if (!cl) {
			continue;
		}

		if (g_level.teams || g_level.ctf) {
			if (cl->locals.persistent.team != winner->client->locals.persistent.team) {
				return;
			}
		} else {
			if (g_game.entities[i + 1].owner != winner) {
				return;
			}
		}
	}

	// we have a winner
	gi.BroadcastPrint(PRINT_HIGH, "%s wins!\n",
	                  (g_level.teams || g_level.ctf ? winner->client->locals.persistent.team->name
	                   : winner->client->locals.persistent.net_name));

	g_level.round_time = 0;

	G_CheckRoundLimit();
}

/**
 * @brief
 */
static void G_CheckMatchEnd(void) {
	int32_t i, clients;
	g_client_t *cl;

	if (!g_level.match) {
		return;
	}

	if (!g_level.match_time || g_level.match_time > g_level.time) {
		return; // no match currently running
	}

	clients = 0;

	uint8_t teams_count[MAX_TEAMS];
	memset(teams_count, 0, sizeof(teams_count));

	for (i = 0; i < sv_max_clients->integer; i++) {
		if (!g_game.entities[i + 1].in_use) {
			continue;
		}

		cl = g_game.entities[i + 1].client;

		if (cl->locals.persistent.spectator) {
			continue;
		}

		if (g_level.teams || g_level.ctf) {
			teams_count[cl->locals.persistent.team->id]++;
		}

		clients++;
	}

	if (clients == 0) { // everyone left
		gi.BroadcastPrint(PRINT_HIGH, "No players left\n");
		g_level.match_time = 0;
		return;
	}

	if (g_level.teams || g_level.ctf) {
		for (int32_t i = 0; i < g_level.num_teams; i++) {

			if (teams_count[i]) {
				continue;
			}

			gi.BroadcastPrint(PRINT_HIGH, "Not enough players left\n");
			g_level.match_time = 0;
			return;
		}
	}
}

/**
 * @brief
 */
static char *G_FormatTime(uint32_t time) {
	static char formatted_time[MAX_QPATH];
	static uint32_t last_time = 0xffffffff;
	const uint32_t m = (time / 1000) / 60;
	const uint32_t s = (time / 1000) % 60;
	char *c;

	// highlight for countdowns
	if (time < (30 * 1000) && time < last_time && (s & 1)) {
		c = "^2";
	} else {
		c = "^7";
	}

	g_snprintf(formatted_time, sizeof(formatted_time), "%s%2u:%02u", c, m, s);

	last_time = time;

	return formatted_time;
}

/**
 * @brief
 */
static void G_CheckRules(void) {
	int32_t i;
	_Bool restart = false;

	if (g_level.intermission_time) {
		return;
	}

	// match mode, no match, or countdown underway
	g_level.warmup = g_level.match && (!g_level.match_time || g_level.match_time > g_level.time);

	// arena mode, no round, or countdown underway
	g_level.warmup |= g_level.rounds && (!g_level.round_time || g_level.round_time > g_level.time);

	if (g_level.start_match && g_level.time >= g_level.match_time) {

		// players have readied, begin match
		g_level.start_match = false;
		g_level.warmup = false;
		g_level.time_limit = (g_time_limit->value * 60 * 1000) + g_level.time;
		g_level.match_status = MSTAT_PLAYING;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (!g_game.entities[i + 1].in_use) {
				continue;
			}
			G_ClientRespawn(&g_game.entities[i + 1], false);
		}

		G_Sound(&(const g_play_sound_t) {
			.index = g_media.sounds.teleport,
			.atten = SOUND_ATTEN_NONE
		});

		gi.BroadcastPrint(PRINT_HIGH, "Match has started\n");
	}

	if (g_level.start_round && g_level.time >= g_level.round_time) {
		// pre-game expired, begin round
		g_level.start_round = false;
		g_level.warmup = false;

		for (i = 0; i < sv_max_clients->integer; i++) {
			if (!g_game.entities[i + 1].in_use) {
				continue;
			}
			G_ClientRespawn(&g_game.entities[i + 1], false);
		}

		G_Sound(&(const g_play_sound_t) {
			.index = g_media.sounds.teleport,
			.atten = SOUND_ATTEN_NONE
		});

		gi.BroadcastPrint(PRINT_HIGH, "Round has started\n");
	}

	G_RunTimers();

	if (!g_level.ctf && g_level.frag_limit) { // check frag_limit

		if (g_level.teams) { // check team scores
			for (int32_t i = 0; i < g_level.num_teams; i++) {
				if (g_teamlist[i].score >= g_level.frag_limit) {
					gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
					G_EndLevel();
					return;
				}
			}
		} else { // or individual scores
			for (i = 0; i < sv_max_clients->integer; i++) {
				g_client_t *cl = g_game.clients + i;
				if (!g_game.entities[i + 1].in_use) {
					continue;
				}

				if (cl->locals.persistent.score >= g_level.frag_limit) {
					gi.BroadcastPrint(PRINT_HIGH, "Frag limit hit\n");
					G_EndLevel();
					return;
				}
			}
		}
	}

	if (g_level.ctf && g_level.capture_limit) { // check capture limit

		for (int32_t i = 0; i < g_level.num_teams; i++) {
			if (g_teamlist[i].captures >= g_level.capture_limit) {
				gi.BroadcastPrint(PRINT_HIGH, "Capture limit hit\n");
				G_EndLevel();
				return;
			}
		}
	}

	if (g_gameplay->modified) { // change gameplay, fix items, respawn clients
		g_gameplay->modified = false;

		g_level.gameplay = G_GameplayByName(g_gameplay->string);
		gi.SetConfigString(CS_GAMEPLAY, va("%d", g_level.gameplay));

		restart = true;

		gi.BroadcastPrint(PRINT_HIGH, "Gameplay has changed to %s\n",
		                  G_GameplayName(g_level.gameplay));

		if (g_level.gameplay == GAME_DUEL) {

			// force all requirements for DUEL mode in a single server restart
			if (g_teams->integer == 0) {
				g_teams->integer = 1;
				g_teams->modified = true;
			}

			if (g_strcmp0(g_num_teams->string, "default") && g_num_teams->integer != 2) {
				gi.SetCvarString(g_num_teams->name, "default");
			}

			if (g_match->integer == 0) {
				g_match->integer = 1;
				g_match->modified = true;
			}
		}
	}

	if (g_hook->modified) {
		g_hook->modified = false;

		G_CheckHook();

		gi.BroadcastPrint(PRINT_HIGH, "Hook has been %s\n",
		                  g_level.hook_allowed ? "enabled" : "disabled");
	}

	if (g_hook_speed->modified) {
		g_hook_speed->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Hook speed has been changed to %g\n",
		                  g_hook_speed->value);
	}

	if (g_friendly_fire->modified) {
		g_friendly_fire->modified = false;

		gi.SetCvarValue(g_friendly_fire->name, Clampf(g_friendly_fire->value, 0.0, 4.0));

		gi.BroadcastPrint(PRINT_HIGH, "Friendly fire has been changed to %g\n",
		                  g_friendly_fire->value);
	}

	if (g_self_damage->modified) {
		g_self_damage->modified = false;

		gi.SetCvarValue(g_self_damage->name, Clampf(g_self_damage->value, 0.0, 4.0));

		gi.BroadcastPrint(PRINT_HIGH, "Self damage has been changed to %g\n",
		                  g_self_damage->value);
	}

	if (g_hook_pull_speed->modified) {
		g_hook_pull_speed->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Hook pull speed has been changed to %g\n",
		                  g_hook_pull_speed->value);

		gi.SetConfigString(CS_HOOK_PULL_SPEED, g_hook_pull_speed->string);
	}

	if (g_hook_style->modified) {
		g_hook_style->modified = false;

		// reset all the hook styles on the players
		for (i = 0; i < sv_max_clients->integer; i++) {
			G_SetClientHookStyle(&g_game.entities[i + 1]);
		}

		gi.BroadcastPrint(PRINT_HIGH, "Hook style has been changed to %s\n",
		                  g_hook_style->string);
	}

	if (g_gravity->modified) { // set gravity, G_ClientMove will read it
		g_gravity->modified = false;

		g_level.gravity = g_gravity->integer;
	}

	if (g_num_teams->modified) { // reset teams, scores, etc
		g_num_teams->modified = false;

		int32_t num_teams;

		if (!g_strcmp0(g_num_teams->string, "default")) {
			num_teams = -1; // G_InitNumTeams will pick this up
		} else {
			num_teams = Clampf(g_num_teams->integer, 2, MAX_TEAMS);
		}

		if (g_level.num_teams != num_teams) {
			g_level.num_teams = num_teams;

			if (g_teams->integer || g_ctf->integer) {
				G_InitNumTeams();

				gi.BroadcastPrint(PRINT_HIGH, "Number of teams set to %i\n",
								  g_level.num_teams);

				restart = true;
			}
		}
	}

	if (g_teams->modified) { // reset teams, scores
		g_teams->modified = false;

		// teams are required for duel
		if (g_level.gameplay == GAME_DUEL && g_teams->integer == 0) {
			gi.Print("Teams can't be disabled in DUEL mode, enabling...\n");
			gi.SetCvarValue(g_teams->name, 1.0);
		} else {
			g_level.teams = g_teams->integer;
			G_InitNumTeams();

			gi.BroadcastPrint(PRINT_HIGH, "Teams have been %s\n",
			                  g_level.teams ? "enabled" : "disabled");

			restart = true;
		}
	}

	if (g_ctf->modified) { // reset teams, scores
		g_ctf->modified = false;

		g_level.ctf = g_ctf->integer;
		gi.SetConfigString(CS_CTF, va("%d", g_level.ctf));

		gi.BroadcastPrint(PRINT_HIGH, "CTF has been %s\n", g_level.ctf ? "enabled" : "disabled");

		restart = true;
	}

	if (g_techs->modified) {
		g_techs->modified = false;

		G_CheckTechs();

		gi.BroadcastPrint(PRINT_HIGH, "Techs have been %s\n", g_level.techs ? "enabled" : "disabled");

		restart = true;
	}

	if (g_match->modified) { // reset scores
		g_match->modified = false;

		if (g_level.gameplay == GAME_DUEL && g_match->integer == 0) {
			gi.Print("Matches can't be disabled in DUEL mode, enabling...\n");
			gi.SetCvarValue(g_match->name, 1.0);
		} else {
			g_level.match = g_match->integer;
			gi.SetConfigString(CS_MATCH, va("%d", g_level.match));

			g_level.warmup = g_level.match; // toggle warmup
			g_level.match_status = MSTAT_WARMUP;

			gi.BroadcastPrint(PRINT_HIGH, "Matches have been %s\n",
			                  g_level.match ? "enabled" : "disabled");

			restart = true;
		}
	}

	if (g_rounds->modified) { // reset scores
		g_rounds->modified = false;

		g_level.rounds = g_rounds->integer;
		gi.SetConfigString(CS_ROUNDS, va("%d", g_level.rounds));

		g_level.warmup = g_level.rounds; // toggle warmup

		gi.BroadcastPrint(PRINT_HIGH, "Rounds have been %s\n",
		                  g_level.rounds ? "enabled" : "disabled");

		restart = true;
	}

	if (g_cheats->modified) { // notify when cheats changes
		g_cheats->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Cheats have been %s\n",
		                  g_cheats->integer ? "enabled" : "disabled");
	}

	if (g_frag_limit->modified) {
		g_frag_limit->modified = false;
		g_level.frag_limit = g_frag_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Frag limit has been changed to %d\n", g_level.frag_limit);
	}

	if (g_round_limit->modified) {
		g_round_limit->modified = false;
		g_level.round_limit = g_round_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Round limit has been changed to %d\n", g_level.round_limit);
	}

	if (g_capture_limit->modified) {
		g_capture_limit->modified = false;
		g_level.capture_limit = g_capture_limit->integer;

		gi.BroadcastPrint(PRINT_HIGH, "Capture limit has been changed to %d\n",
		                  g_level.capture_limit);
	}

	if (g_time_limit->modified) {
		g_time_limit->modified = false;
		g_level.time_limit = g_time_limit->value * 60 * 1000;

		gi.BroadcastPrint(PRINT_HIGH, "Time limit has been changed to %3.1f\n", g_time_limit->value);
	}

	if (g_weapon_stay->modified) {
		g_weapon_stay->modified = false;

		gi.BroadcastPrint(PRINT_HIGH, "Weapon's Stay has been %s\n", g_weapon_stay->integer ? "enabled" : "disabled");

		// respawn all the weapons sitting around
		if (g_weapon_stay->integer) {
			for (uint16_t i = 1; i < ge.num_entities; i++) {

				g_entity_t *ent = &g_game.entities[i];

				if (!ent->in_use) {
					continue;
				}

				if (!ent->locals.item) {
					continue;
				}

				if (ent->locals.spawn_flags & SF_ITEM_DROPPED) {
					continue;
				}

				if (ent->locals.item->type != ITEM_WEAPON) {
					continue;
				}

				if (!(ent->sv_flags & SVF_NO_CLIENT)) {
					continue;
				}

				if (!ent->locals.Think) {
					continue;
				}

				ent->locals.next_think = 0;
				ent->locals.Think(ent); // force a respawn
			}
		}
	}

	for (int32_t i = 0; i < MAX_TEAMS; i++) {
		_Bool changed = false, reset_userinfo = false;

		if (g_team_cvars[i].g_team_name->modified) {
			g_team_cvars[i].g_team_name->modified = false;
		
			if (strlen(g_team_cvars[i].g_team_name->string) > 1 && strlen(g_team_cvars[i].g_team_name->string) < lengthof(g_teamlist[i].name) - 1 &&
				!strstr(g_team_cvars[i].g_team_name->string, "\\") && strcmp(g_team_cvars[i].g_team_name->string, g_teamlist[i].name)) {

				gi.BroadcastPrint(PRINT_HIGH, "Team \"%s\"'s name has been changed to \"%s\"\n", g_teamlist[i].name, g_team_cvars[i].g_team_name->string);
				strcpy(g_teamlist[i].name, g_team_cvars[i].g_team_name->string);
				changed = true;
			}
		}

		if (g_team_cvars[i].g_team_shirt->modified) {
			g_team_cvars[i].g_team_shirt->modified = false;

			color_t shirt;

			if (Color_Parse(g_team_cvars[i].g_team_shirt->string, &shirt) &&
				Color_Color32(shirt).rgba != Color_Color32(g_teamlist[i].shirt).rgba) {

				gi.BroadcastPrint(PRINT_HIGH, "Team \"%s\"'s shirt color has been changed to \"%s\"\n",
								  g_teamlist[i].name, g_team_cvars[i].g_team_shirt->string);
				g_teamlist[i].shirt = g_teamlist[i].helmet = g_teamlist[i].pants = shirt;
				changed = true;
				reset_userinfo = true;
			}
		}

		if (g_team_cvars[i].g_team_color->modified) {
			g_team_cvars[i].g_team_color->modified = false;
			char *end_point;

			int16_t hue = strtol(g_team_cvars[i].g_team_color->string, &end_point, 10);

			if (end_point && hue >= 0 && hue <= 361 && hue != g_teamlist[i].color) {

				gi.BroadcastPrint(PRINT_HIGH, "Team \"%s\"'s effect color has been changed to \"%i\"\n", g_teamlist[i].name, hue);
				g_teamlist[i].color = hue;
				changed = true;
				reset_userinfo = true;
			}
		}

		if (changed) {
			G_SetTeamNames();
		}

		if (reset_userinfo) {

			for (uint16_t i = 1; i < sv_max_clients->integer; i++) {

				g_entity_t *ent = &g_game.entities[i];

				if (!ent->in_use) {
					continue;
				}

				if (!ent->client->locals.persistent.team) {
					continue;
				}

				G_ClientUserInfoChanged(ent, ent->client->locals.persistent.user_info);
			}
		}
	}

	if (restart) {
		G_RestartGame(true);	// reset all clients
	}
}

/**
 * @brief
 */
static void G_ExitLevel(void) {

	gi.Cbuf(va("map %s\n", g_level.next_map));

	g_level.next_map = NULL;
	g_level.intermission_time = 0;

	G_EndClientFrames();
}

#define INTERMISSION (10.0 * 1000) // intermission duration

/**
 * @brief The main game module "think" function, called once per server frame.
 * Nothing would happen in Quake land if this weren't called.
 */
static void G_Frame(void) {

	if (g_paused->integer) {
		G_EndClientFrames();
		return;
	}

	g_level.frame_num++;
	g_level.time = g_level.frame_num * QUETOO_TICK_MILLIS;

	// check for level change after running intermission
	if (g_level.intermission_time) {
		if (g_level.time > g_level.intermission_time + INTERMISSION) {
			G_ExitLevel();
			return;
		}
	}

	if (!G_MatchIsTimeout()) {
		// treat each object in turn
		// even the world gets a chance to think
		g_entity_t *ent = &g_game.entities[0];
		for (uint16_t i = 0; i < ge.num_entities; i++, ent++) {

			if (!ent->in_use) {
				continue;
			}

			g_level.current_entity = ent;

			if (ent->client) {
				G_ClientBeginFrame(ent);
			} else {
				G_RunEntity(ent);
			}
		}

		G_Ai_Frame();
	}

	// see if a vote has passed
	G_CheckVote();

	// inspect and enforce gameplay rules
	G_CheckRules();

	// see if a match should end
	G_CheckMatchEnd();

	// see if an arena round should start
	G_CheckRoundStart();

	// see if an arena round should end
	G_CheckRoundEnd();

	// build the player_state_t structures for all players
	G_EndClientFrames();
}

/**
 * @brief Returns the game name advertised by the server in info strings.
 */
static const char *G_GameName(void) {
	static char name[64];
	const size_t size = sizeof(name);

	g_strlcpy(name, G_GameplayName(g_level.gameplay), size);

	// teams are implied for capture the flag and duel
	if (g_level.ctf) {
		g_strlcat(name, " CTF", size);
	} else if (g_level.teams && g_level.gameplay != GAME_DUEL) {
		g_strlcpy(name, va("Team %s", name), size);
	}

	return name;
}

/**
 * @brief Restart the game.
 */
static void G_Restart_Sv_f(void) {

	G_RestartGame(false);
}

/**
 * @brief Set up the CS_TEAMS configstring to the number of valid teams we have
 */
void G_InitNumTeams(void) {

	if (g_level.num_teams == -1) { // set to default, so let's set number of teams
		g_level.num_teams = 0;

		for (int32_t t = 0; t < MAX_TEAMS; t++) {

			if (!g_teamlist[t].spawn_points.count) {
				break;
			}

			g_level.num_teams++;
		}

		g_level.num_teams = Clampf(g_level.num_teams, 2, MAX_TEAMS);
	}

	gi.SetConfigString(CS_TEAMS, va("%d", (g_level.teams || g_level.ctf) ? g_level.num_teams : 0));
}

/**
 * @brief This will be called when the game module is first loaded.
 */
void G_Init(void) {

	gi.Print("Game module initialization...\n");

	const char *s = va("%s %s %s", VERSION, BUILD, REVISION);
	cvar_t *game_version = gi.AddCvar("game_version", s, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	gi.Print("  Version:    ^2%s^7\n", game_version->string);

	memset(&g_game, 0, sizeof(g_game));

	gi.AddCvar("game_name", GAME_NAME, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);
	gi.AddCvar("game_date", __DATE__, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

	g_admin_password = gi.AddCvar("g_admin_password", "", CVAR_LATCH, "Password to authenticate as an admin.");
	g_ammo_respawn_time = gi.AddCvar("g_ammo_respawn_time", "20.0", CVAR_SERVER_INFO, "Ammo respawn interval in seconds.");
	g_auto_join = gi.AddCvar("g_auto_join", "0", CVAR_SERVER_INFO,
	                      "Automatically assigns players to teams, ignored for duel mode.");
	g_balance_bfg_damage = gi.AddCvar("g_balance_bfg_damage", "180", CVAR_SERVER_INFO, NULL);
	g_balance_bfg_knockback = gi.AddCvar("g_balance_bfg_knockback", "140", CVAR_SERVER_INFO, NULL);
	g_balance_bfg_prefire = gi.AddCvar("g_balance_bfg_prefire", "1", CVAR_SERVER_INFO,
					"The prefire warmup delay for the BFG10K in seconds.");
	g_balance_bfg_radius = gi.AddCvar("g_balance_bfg_radius", "512", CVAR_SERVER_INFO, NULL);
	g_balance_bfg_refire = gi.AddCvar("g_balance_bfg_refire", "2", CVAR_SERVER_INFO, NULL);
	g_balance_bfg_speed = gi.AddCvar("g_balance_bfg_speed", "720", CVAR_SERVER_INFO, NULL);
	g_balance_blaster_damage = gi.AddCvar("g_balance_blaster_damage", "15", CVAR_SERVER_INFO, NULL);
	g_balance_blaster_knockback = gi.AddCvar("g_balance_blaster_knockback", "2", CVAR_SERVER_INFO, NULL);
	g_balance_blaster_refire = gi.AddCvar("g_balance_blaster_refire", "0.45", CVAR_SERVER_INFO, NULL);
	g_balance_blaster_speed = gi.AddCvar("g_balance_blaster_speed", "2000", CVAR_SERVER_INFO, NULL);
	g_balance_handgrenade_refire = gi.AddCvar("g_balance_handgrenade_refire", "2", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_climb_damage = gi.AddCvar("g_balance_hyperblaster_climb_damage", "3", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_climb_knockback = gi.AddCvar("g_balance_hyperblaster_climb_knockback", "68", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_damage = gi.AddCvar("g_balance_hyperblaster_damage", "16", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_knockback = gi.AddCvar("g_balance_hyperblaster_knockback", "4", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_refire = gi.AddCvar("g_balance_hyperblaster_refire", "0.1", CVAR_SERVER_INFO, NULL);
	g_balance_hyperblaster_speed = gi.AddCvar("g_balance_hyperblaster_speed", "1800", CVAR_SERVER_INFO, NULL);
	g_balance_lightning_damage = gi.AddCvar("g_balance_lightning_damage", "12", CVAR_SERVER_INFO, NULL);
	g_balance_lightning_knockback = gi.AddCvar("g_balance_lightning_knockback", "6", CVAR_SERVER_INFO, NULL);
	g_balance_lightning_length = gi.AddCvar("g_balance_lightning_length", "600", CVAR_SERVER_INFO, NULL);
	g_balance_lightning_refire = gi.AddCvar("g_balance_lightning_refire", "0.1", CVAR_SERVER_INFO, NULL);
	g_balance_machinegun_damage = gi.AddCvar("g_balance_machinegun_damage", "8", CVAR_SERVER_INFO, NULL);
	g_balance_machinegun_knockback = gi.AddCvar("g_balance_machinegun_knockback", "2", CVAR_SERVER_INFO, NULL);
	g_balance_machinegun_refire = gi.AddCvar("g_balance_machinegun_refire", "0.1", CVAR_SERVER_INFO, NULL);
	g_balance_machinegun_spread_x = gi.AddCvar("g_balance_machinegun_spread_x", "20", CVAR_SERVER_INFO, NULL);
	g_balance_machinegun_spread_y = gi.AddCvar("g_balance_machinegun_spread_y", "200", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_damage = gi.AddCvar("g_balance_grenadelauncher_damage", "120", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_knockback = gi.AddCvar("g_balance_grenadelauncher_knockback", "120", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_radius = gi.AddCvar("g_balance_grenadelauncher_radius", "185", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_refire = gi.AddCvar("g_balance_grenadelauncher_refire", "1", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_speed = gi.AddCvar("g_balance_grenadelauncher_speed", "800", CVAR_SERVER_INFO, NULL);
	g_balance_grenadelauncher_timer = gi.AddCvar("g_balance_grenadelauncher_timer", "2.5", CVAR_SERVER_INFO, NULL);
	g_balance_railgun_damage = gi.AddCvar("g_balance_railgun_damage", "100", CVAR_SERVER_INFO, NULL);
	g_balance_railgun_knockback = gi.AddCvar("g_balance_railgun_knockback", "80", CVAR_SERVER_INFO, NULL);
	g_balance_railgun_refire = gi.AddCvar("g_balance_railgun_refire", "1.4", CVAR_SERVER_INFO, NULL);
	g_balance_rocketlauncher_damage = gi.AddCvar("g_balance_rocketlauncher_damage", "100", CVAR_SERVER_INFO, NULL);
	g_balance_rocketlauncher_knockback = gi.AddCvar("g_balance_rocketlauncher_knockback", "100", CVAR_SERVER_INFO, NULL);
	g_balance_rocketlauncher_radius = gi.AddCvar("g_balance_rocketlauncher_radius", "150", CVAR_SERVER_INFO, NULL);
	g_balance_rocketlauncher_refire = gi.AddCvar("g_balance_rocketlauncher_refire", "1", CVAR_SERVER_INFO, NULL);
	g_balance_rocketlauncher_speed = gi.AddCvar("g_balance_rocketlauncher_speed", "1000", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_damage = gi.AddCvar("g_balance_shotgun_damage", "4", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_knockback = gi.AddCvar("g_balance_shotgun_knockback", "2", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_pellets = gi.AddCvar("g_balance_shotgun_pellets", "12", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_refire = gi.AddCvar("g_balance_shotgun_refire", "0.6", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_spread_x = gi.AddCvar("g_balance_shotgun_spread_x", "700", CVAR_SERVER_INFO, NULL);
	g_balance_shotgun_spread_y = gi.AddCvar("g_balance_shotgun_spread_y", "300", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_damage = gi.AddCvar("g_balance_supershotgun_damage", "4", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_knockback = gi.AddCvar("g_balance_supershotgun_knockback", "2", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_pellets = gi.AddCvar("g_balance_supershotgun_pellets", "24", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_refire = gi.AddCvar("g_balance_supershotgun_refire", "0.8", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_spread_x = gi.AddCvar("g_balance_supershotgun_spread_x", "1600", CVAR_SERVER_INFO, NULL);
	g_balance_supershotgun_spread_y = gi.AddCvar("g_balance_supershotgun_spread_y", "500", CVAR_SERVER_INFO, NULL);
	g_capture_limit = gi.AddCvar("g_capture_limit", "8", CVAR_SERVER_INFO, "The capture limit per level.");
	g_cheats = gi.AddCvar("g_cheats", "0", CVAR_SERVER_INFO, NULL);
	g_ctf = gi.AddCvar("g_ctf", "0", CVAR_SERVER_INFO, "Enables capture the flag gameplay.");
	g_hook = gi.AddCvar("g_hook", "default", CVAR_SERVER_INFO,
	                 "Whether to allow the hook to be used or not. \"default\" only allows hook in CTF; 1 is always allow, 0 is never allow.");
	g_hook_auto_refire = gi.AddCvar("g_hook_auto_refire", "0", CVAR_SERVER_INFO,
				     "If the hook automatically refires when it hits a non-solid surface, like players or weapon clips. (Currently non-functional)");
	g_hook_distance = gi.AddCvar("g_hook_distance", va("%.1f", PM_HOOK_DEF_DIST), CVAR_SERVER_INFO,
							  "The maximum distance the hook will travel.");
	g_hook_refire = gi.AddCvar("g_hook_refire", "0.25", CVAR_SERVER_INFO,
	                       "The refire delay on the grapple hook in seconds.");
	g_hook_style = gi.AddCvar("g_hook_style", "default", CVAR_SERVER_INFO,
	                       "Whether to allow only \"pull\", \"swing_manual\", \"swing_auto\" or any (\"default\") hook swing style.");
	g_hook_speed = gi.AddCvar("g_hook_speed", "1200", CVAR_SERVER_INFO, "The speed that the hook will fly at.");
	g_hook_pull_speed = gi.AddCvar("g_hook_pull_speed", "800", CVAR_SERVER_INFO,
	                            "The speed that you get pulled towards the hook.");
	g_frag_limit = gi.AddCvar("g_frag_limit", "30", CVAR_SERVER_INFO, "The frag limit per level.");
	g_friendly_fire = gi.AddCvar("g_friendly_fire", "1", CVAR_SERVER_INFO, "Factor of how much damage can be dealt to teammates.");
	g_force_demo = gi.AddCvar("g_force_demo", "0", CVAR_SERVER_INFO, "Force all players to record a demo.");
	g_force_screenshot = gi.AddCvar("g_force_screenshot", "0", CVAR_SERVER_INFO, "Force all players to take a screenshot.");
	g_gameplay = gi.AddCvar("g_gameplay", "default", CVAR_SERVER_INFO, "Selects deathmatch, duel, arena, or instagib combat.");
	g_gravity = gi.AddCvar("g_gravity", "800", CVAR_SERVER_INFO, NULL);
	g_handicap = gi.AddCvar("g_handicap", "1", CVAR_SERVER_INFO,
	                     "Allows usage of player handicap. 0 disallows handicap, 1 allows handicap, 2 allows handicap but disables damage reduction. (default 1)");
	g_inhibit = gi.AddCvar("g_inhibit", "", CVAR_SERVER_INFO,
	                    "Prevents entities from spawning using a class name filter (e.g.: \"weapon_bfg ammo_nukes item_quad\")");
	g_num_teams = gi.AddCvar("g_num_teams", "default", CVAR_SERVER_INFO,
			      "The number of teams allowed. By default, picks the valid amount for the map, or 2.");
	g_match = gi.AddCvar("g_match", "0", CVAR_SERVER_INFO, "Enables match play requiring players to ready.");
	g_max_entities = gi.AddCvar("g_max_entities", "1024", CVAR_LATCH, NULL);
	g_motd = gi.AddCvar("g_motd", "", CVAR_SERVER_INFO, "Message of the day, shown to clients on initial connect.");
	g_password = gi.AddCvar("g_password", "", CVAR_USER_INFO, "The server password.");
	g_paused = gi.AddCvar("g_paused", "0", CVAR_DEVELOPER, "Pause the server. Developer tool.");
	g_player_projectile = gi.AddCvar("g_player_projectile", "1", CVAR_SERVER_INFO, "Scales player velocity to projectiles.");
	g_quad_damage_respawn_time = gi.AddCvar("g_quad_damage_respawn_time", "60", CVAR_SERVER_INFO, "How long it takes for Quad Damage to respawn, in seconds.");
	g_quad_damage_time = gi.AddCvar("g_quad_damage_time", "20", CVAR_SERVER_INFO, "How long Quad Damage lasts, in seconds.");
	g_random_map = gi.AddCvar("g_random_map", "0", 0, "Enables map shuffling.");
	g_respawn_protection = gi.AddCvar("g_respawn_protection", "0.0", 0, "Respawn protection in seconds.");
	g_round_limit = gi.AddCvar("g_round_limit", "30", CVAR_SERVER_INFO, "The number of rounds to run per level.");
	g_rounds = gi.AddCvar("g_rounds", "0", CVAR_SERVER_INFO, "Enables rounds-based play, where last player standing wins.");
	g_self_damage = gi.AddCvar("g_self_damage", "1", CVAR_SERVER_INFO, "Factor of how much damage can be dealt to yourself.");
	g_show_attacker_stats = gi.AddCvar("g_show_attacker_stats", "0", CVAR_SERVER_INFO,
					"Allows can see their attackers' health and armor when they die.");
	g_spawn_farthest = gi.AddCvar("g_spawn_farthest", "0", CVAR_SERVER_INFO, NULL);
	g_spectator_chat = gi.AddCvar("g_spectator_chat", "1", CVAR_SERVER_INFO,
	                           "If enabled, spectators can only talk to other spectators.");
	g_teams = gi.AddCvar("g_teams", "0", CVAR_SERVER_INFO, "Enables teams-based play.");
	g_techs = gi.AddCvar("g_techs", "default", CVAR_SERVER_INFO,
	                 "Whether to allow techs or not. \"default\" only allows techs in CTF; 1 is always allow, 0 is never allow.");
	g_time_limit = gi.AddCvar("g_time_limit", "20.0", CVAR_SERVER_INFO, "The time limit per level in minutes.");
	g_timeout_time = gi.AddCvar("g_timeout_time", "120", CVAR_SERVER_INFO, "Length in seconds of a timeout, 0 = disabled.");
	g_voting = gi.AddCvar("g_voting", "1", CVAR_SERVER_INFO, "Enables voting.");
	g_warmup_time = gi.AddCvar("g_warmup_time", "15", CVAR_SERVER_INFO, "Match warmup countdown in seconds, up to 30.");
	g_weapon_respawn_time = gi.AddCvar("g_weapon_respawn_time", "5.0", CVAR_SERVER_INFO, "Weapon respawn interval in seconds.");
	g_weapon_stay = gi.AddCvar("g_weapon_stay", "0", CVAR_SERVER_INFO, "Controls whether weapons will respawn like normal or always stay.");

	sv_max_clients = gi.GetCvar("sv_max_clients");
	sv_hostname = gi.GetCvar("sv_hostname");

	dedicated = gi.GetCvar("dedicated");

	for (int32_t i = 0; i < MAX_TEAMS; i++) {
		g_team_cvars[i].g_team_name = gi.AddCvar(va("g_team_%i_name", i + 1), g_default_team_values[i].name, 0, NULL);
		g_team_cvars[i].g_team_shirt = gi.AddCvar(va("g_team_%i_shirt", i + 1), g_default_team_values[i].shirt, 0, NULL);
		g_team_cvars[i].g_team_color = gi.AddCvar(va("g_team_%i_color", i + 1), va("%i", g_default_team_values[i].color), 0, NULL);
	}

	G_InitVote();

	// initialize entities and clients for this game
	g_game.entities = gi.Malloc(g_max_entities->integer * sizeof(g_entity_t), MEM_TAG_GAME);
	g_game.clients = gi.Malloc(sv_max_clients->integer * sizeof(g_client_t), MEM_TAG_GAME);

	ge.entities = g_game.entities;
	ge.max_entities = g_max_entities->integer;
	ge.num_entities = sv_max_clients->integer + 1;

	// set up client pointers
	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {
		g_game.entities[i].client = g_game.clients + (i - 1);
	}

	G_Ai_Init(); // initialize the AI

	G_MapList_Init();

	// set these to false to avoid spurious game restarts and alerts on init
	g_gameplay->modified =
			g_ctf->modified =
			g_teams->modified =
			g_num_teams->modified =
			g_techs->modified =
			g_cheats->modified =
			g_frag_limit->modified =
			g_round_limit->modified =
			g_capture_limit->modified =
			g_weapon_stay->modified =
			g_time_limit->modified = false;

	// add game-specific server console commands
	gi.AddCmd("mute", G_Mute_Sv_f, CMD_GAME, "Prevent a client from talking");
	gi.AddCmd("unmute", G_Mute_Sv_f, CMD_GAME, "Allow a muted client to talk again");
	gi.AddCmd("stuff", G_Stuff_Sv_f, CMD_GAME, "Force a client to execute a command");
	gi.AddCmd("stuff_all", G_StuffAll_Sv_f, CMD_GAME, "Force all players to execute a command");
	gi.AddCmd("g_restart", G_Restart_Sv_f, CMD_GAME, "Force the game to restart");

	gi.Print("Game module initialized\n");
}

/**
 * @brief Shuts down the game module. This is called when the game is unloaded
 * (complements G_Init).
 */
void G_Shutdown(void) {

	gi.Print("Game module shutdown...\n");

	G_MapList_Shutdown();
	
	G_Ai_Shutdown();

	G_ShutdownVote();

	gi.FreeTag(MEM_TAG_GAME_LEVEL);
	gi.FreeTag(MEM_TAG_GAME);
}

void G_CallTimeOut(g_entity_t *ent) {

	if (g_timeout_time->integer == 0) {
		gi.ClientPrint(ent, PRINT_HIGH, "Timeouts are disabled\n");
		return;
	}

	g_level.match_status |= MSTAT_TIMEOUT;
	g_level.timeout_caller = ent;
	g_level.timeout_time = g_level.time + (g_timeout_time->integer * 1000);
	g_level.timeout_frame = g_level.frame_num;

	// lock everyone in place
	for (int32_t i = 1; i < sv_max_clients->integer; i++) {
		g_game.entities[i].client->ps.pm_state.type = PM_FREEZE;
	}

	gi.BroadcastPrint(PRINT_HIGH, "%s called a timeout, play with resume in %s\n",
	                  ent->client->locals.persistent.net_name, G_FormatTime(g_timeout_time->integer * 1000));
}

void G_CallTimeIn(void) {

	g_level.frame_num = g_level.timeout_frame; // where we were before timeout

	// unlock everyone
	for (int32_t i = 1; i < sv_max_clients->integer; i++) {
		g_game.entities[i].client->ps.pm_state.type = PM_NORMAL;
	}

	g_level.match_status = MSTAT_PLAYING;
	g_level.timeout_caller = NULL;
	g_level.timeout_time = 0;
	g_level.timeout_frame = 0;
}

/*
 * Timer based stuff for the game (clock, countdowns, timeouts, etc)
 */
void G_RunTimers(void) {
	uint32_t j;
	uint32_t time = g_level.time;

	if (g_level.rounds) {
		if (g_level.round_time > g_level.time) { // round about to start, show pre-game countdown
			time = g_level.round_time - g_level.time;
		} else if (g_level.round_time) {
			time = g_level.time - g_level.round_time; // round started, count up
		} else {
			time = 0;
		}
	} else if (g_level.match) {
		if (g_level.match_time > g_level.time) { // match about to start, show pre-game countdown
			time = g_level.match_time - g_level.time;
		} else if (g_level.match_time) {
			if (g_level.time_limit) { // count down to time_limit
				time = g_level.time_limit - g_level.time;
			} else {
				time = g_level.time - g_level.match_time; // count up
			}
		} else {
			time = 0;
		}
	}

	if (g_level.time_limit) { // check time_limit
		if (time >= g_level.time_limit) {
			gi.BroadcastPrint(PRINT_HIGH, "Time limit hit\n");
			G_EndLevel();
			return;
		}
		time = g_level.time_limit - g_level.time; // count down
	}

	if (g_level.frame_num % QUETOO_TICK_RATE == 0) { // send time updates once per second

		if (G_MatchIsCountdown() && !G_MatchIsPlaying()) { // match mode, everyone ready, show countdown

			j = (g_level.match_time - g_level.time) / 1000 % 60;
			gi.SetConfigString(CS_TIME, va("Warmup %s", G_FormatTime(g_level.match_time - g_level.time)));

			if (j <= 5) {

				if (j > 0) {
					G_Sound(&(const g_play_sound_t) {
						.index = g_media.sounds.countdown[j],
						.atten = SOUND_ATTEN_NONE
					});
				}

				for (int32_t i = 0; i < g_level.num_teams; i++) {
					G_TeamCenterPrint(&g_teamlist[i], "%s\n", (!j) ? "Fight!" : va("%d", j));
				}
			}

		} else if (G_MatchIsWarmup() || g_level.warmup) {	// not everyone ready yet
			gi.SetConfigString(CS_TIME, va("Warmup %s", G_FormatTime(g_time_limit->integer * 60 * 1000)));
		} else if (G_MatchIsTimeout()) { // mid match, player called timeout
			j = (g_level.timeout_time - g_level.time) / 1000;
			gi.SetConfigString(CS_TIME, va("Timeout %s",
			                               G_FormatTime(g_level.timeout_time - g_level.time))
			                  );

			if (j <= 10) {

				if (j > 0) {
					G_Sound(&(const g_play_sound_t) {
						.index = g_media.sounds.countdown[j],
						.atten = SOUND_ATTEN_NONE
					});
				} else {
					G_CallTimeIn();
				}

				for (int32_t i = 0; i < g_level.num_teams; i++) {
					G_TeamCenterPrint(&g_teamlist[i], "%s\n", (!j) ? "Fight!" : va("%d", j));
				}
			}
		} else {
			gi.SetConfigString(CS_TIME, G_FormatTime(time));
		}
	}
}

/**
 * @brief This is the entry point responsible for aligning the server and game module.
 * The server resolves this symbol upon successfully loading the game library,
 * and invokes it. We're responsible for copying the import structure so that
 * we can call back into the server, and returning a populated game export
 * structure.
 */
g_export_t *G_LoadGame(g_import_t *import) {

	gi = *import;

	memset(&ge, 0, sizeof(ge));

	ge.api_version = GAME_API_VERSION;
	ge.protocol = PROTOCOL_MINOR;

	ge.Init = G_Init;
	ge.Shutdown = G_Shutdown;
	ge.SpawnEntities = G_SpawnEntities;

	ge.ClientThink = G_ClientThink;
	ge.ClientConnect = G_ClientConnect;
	ge.ClientUserInfoChanged = G_ClientUserInfoChanged;
	ge.ClientDisconnect = G_ClientDisconnect;
	ge.ClientBegin = G_ClientBegin;
	ge.ClientCommand = G_ClientCommand;

	ge.Frame = G_Frame;

	ge.GameName = G_GameName;

	ge.entity_size = sizeof(g_entity_t);

	return &ge;
}
