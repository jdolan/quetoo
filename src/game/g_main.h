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

#ifndef __GAME_MAIN_H__
#define __GAME_MAIN_H__

#include "g_types.h"

#ifdef __GAME_LOCAL_H__

#define G_PLAYING (g_level.match_status & MSTAT_PLAYING)
#define G_TIMEOUT (g_level.match_status & MSTAT_TIMEOUT)
#define G_COUNTDOWN (g_level.match_status & MSTAT_COUNTDOWN)
#define G_WARMUP (g_level.match_status & MSTAT_WARMUP)

extern g_level_t g_level;
extern g_media_t g_media;

extern g_import_t gi;
extern g_export_t ge;

extern uint32_t g_means_of_death;

extern cvar_t *g_admin_password;
extern cvar_t *g_ammo_respawn_time;
extern cvar_t *g_auto_join;
extern cvar_t *g_capture_limit;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_frag_limit;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_force_demo;
extern cvar_t *g_force_screenshot;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_handicap;
extern cvar_t *g_match;
extern cvar_t *g_max_entities;
extern cvar_t *g_motd;
extern cvar_t *g_password;
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
extern cvar_t *g_timeout_time;
extern cvar_t *g_voting;
extern cvar_t *g_warmup_time;
extern cvar_t *g_weapon_respawn_time;

extern cvar_t *sv_max_clients;
extern cvar_t *sv_hostname;
extern cvar_t *dedicated;

extern g_team_t g_team_good, g_team_evil;

void G_Init(void);
void G_Shutdown(void);
void G_ResetItems(void);
void G_ResetTeams(void);
void G_ResetVote(void);
void G_CallTimeOut(g_entity_t *ent);
void G_CallTimeIn(void);
void G_RunTimers(void);
void G_MuteClient(char *name, _Bool mute);

g_export_t *G_LoadGame(g_import_t *import);

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_MAIN_H__ */
