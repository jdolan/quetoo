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

#ifndef __GAME_MAIN_H__
#define __GAME_MAIN_H__

#include "g_types.h"

#ifdef __GAME_LOCAL_H__

// maplist structs
typedef struct g_map_list_elt_s {
	char name[32];
	char title[128];
	char sky[32];
	char weather[64];
	int32_t gravity;
	int32_t gameplay;
	int32_t teams;
	int32_t ctf;
	int32_t match;
	int32_t rounds;
	int32_t frag_limit;
	int32_t round_limit;
	int32_t capture_limit;
	float time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];
	float weight;
} g_map_list_elt_t;

typedef struct g_map_list_s {
	g_map_list_elt_t maps[MAX_MAP_LIST_ELTS];
	uint32_t count, index;

	// weighted random selection
	uint32_t weighted_index[MAP_LIST_WEIGHT];
	float total_weight;
} g_map_list_t;

extern g_map_list_t g_map_list;

extern g_level_t g_level;

extern g_import_t gi;
extern g_export_t ge;

extern uint32_t means_of_death;

extern cvar_t *g_ammo_respawn_time;
extern cvar_t *g_auto_join;
extern cvar_t *g_capture_limit;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_frag_limit;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_match;
extern cvar_t *g_max_entities;
extern cvar_t *g_motd;
extern cvar_t *g_mysql;
extern cvar_t *g_mysql_db;
extern cvar_t *g_mysql_host;
extern cvar_t *g_mysql_password;
extern cvar_t *g_mysql_user;
extern cvar_t *g_player_projectile;
extern cvar_t *g_random_map;
extern cvar_t *g_respawn_protection;
extern cvar_t *g_round_limit;
extern cvar_t *g_rounds;
extern cvar_t *g_show_attacker_stats;
extern cvar_t *g_spawn_farthest;
extern cvar_t *g_spectator_chat;
extern cvar_t *g_teams;
extern cvar_t *g_time_limit;
extern cvar_t *g_voting;
extern cvar_t *g_weapon_respawn_time;

extern cvar_t *password;

extern cvar_t *sv_max_clients;
extern cvar_t *sv_hostname;
extern cvar_t *dedicated;

extern g_team_t g_team_good, g_team_evil;

void G_Init(void);
void G_Shutdown(void);
void G_ResetTeams(void);
void G_ResetVote(void);
g_export_t *G_LoadGame(g_import_t *import);
const char *G_SelectNextmap(void);

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_MAIN_H__ */
