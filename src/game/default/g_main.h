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

#ifndef __G_MAIN_H__
#define __G_MAIN_H__

#ifdef __G_LOCAL_H__

// maplist structs
typedef struct g_map_list_elt_s {
	char name[32];
	char title[128];
	char sky[32];
	char weather[64];
	int gravity;
	int gameplay;
	int teams;
	int ctf;
	int match;
	int rounds;
	int frag_limit;
	int round_limit;
	int capture_limit;
	float time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];
	float weight;
} g_map_list_elt_t;

typedef struct g_map_list_s {
	g_map_list_elt_t maps[MAX_MAP_LIST_ELTS];
	unsigned int count, index;

	// weighted random selection
	unsigned int weighted_index[MAP_LIST_WEIGHT];
	float total_weight;
} g_map_list_t;

extern g_map_list_t g_map_list;

extern g_level_t g_level;

extern g_import_t gi;
extern g_export_t ge;

extern unsigned short grenade_index, grenade_hit_index;
extern unsigned short rocket_index, rocket_fly_index;
extern unsigned short lightning_fly_index;
extern unsigned short quad_damage_index;

extern unsigned int means_of_death;

extern cvar_t *g_auto_join;
extern cvar_t *g_capture_limit;
extern cvar_t *g_chat_log;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_frag_limit;
extern cvar_t *g_frag_log;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_match;
extern cvar_t *g_max_entities;
extern cvar_t *g_mysql;
extern cvar_t *g_mysql_db;
extern cvar_t *g_mysql_host;
extern cvar_t *g_mysql_password;
extern cvar_t *g_mysql_user;
extern cvar_t *g_player_projectile;
extern cvar_t *g_random_map;
extern cvar_t *g_round_limit;
extern cvar_t *g_rounds;
extern cvar_t *g_spawn_farthest;
extern cvar_t *g_teams;
extern cvar_t *g_time_limit;
extern cvar_t *g_voting;

extern cvar_t *password;

extern cvar_t *sv_max_clients;
extern cvar_t *dedicated;


extern g_team_t g_team_good, g_team_evil;

// text file logging
extern FILE *frag_log, *chat_log;

void G_Init(void);
void G_Shutdown(void);
void G_ResetTeams(void);
void G_ResetVote(void);
g_export_t *G_LoadGame(g_import_t *import);

#endif /* __G_LOCAL_H__ */

#endif /* __G_MAIN_H__ */
