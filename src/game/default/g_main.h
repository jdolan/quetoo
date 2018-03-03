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

#pragma once

#include "g_types.h"

#ifdef __GAME_LOCAL_H__

#define G_MatchIsPlaying()   (g_level.match_status & MSTAT_PLAYING)
#define G_MatchIsTimeout()   (g_level.match_status & MSTAT_TIMEOUT)
#define G_MatchIsCountdown() (g_level.match_status & MSTAT_COUNTDOWN)
#define G_MatchIsWarmup()    (g_level.match_status & MSTAT_WARMUP)

extern g_level_t g_level;
extern g_media_t g_media;

extern g_import_t gi;
extern g_export_t ge;

extern ai_export_t *aix;

extern uint32_t g_means_of_death;

extern cvar_t *g_admin_password;
extern cvar_t *g_ammo_respawn_time;
extern cvar_t *g_auto_join;
extern cvar_t *g_balance_bfg_prefire;
extern cvar_t *g_balance_bfg_refire;
extern cvar_t *g_balance_blaster_damage;
extern cvar_t *g_balance_blaster_knockback;
extern cvar_t *g_balance_blaster_refire;
extern cvar_t *g_balance_blaster_speed;
extern cvar_t *g_balance_handgrenade_refire;
extern cvar_t *g_balance_hyperblaster_refire;
extern cvar_t *g_balance_lightning_refire;
extern cvar_t *g_balance_machinegun_damage;
extern cvar_t *g_balance_machinegun_knockback;
extern cvar_t *g_balance_machinegun_refire;
extern cvar_t *g_balance_machinegun_spread_x;
extern cvar_t *g_balance_machinegun_spread_y;
extern cvar_t *g_balance_grenadelauncher_refire;
extern cvar_t *g_balance_railgun_refire;
extern cvar_t *g_balance_rocketlauncher_refire;
extern cvar_t *g_balance_shotgun_damage;
extern cvar_t *g_balance_shotgun_knockback;
extern cvar_t *g_balance_shotgun_pellets;
extern cvar_t *g_balance_shotgun_refire;
extern cvar_t *g_balance_shotgun_spread_x;
extern cvar_t *g_balance_shotgun_spread_y;
extern cvar_t *g_balance_supershotgun_damage;
extern cvar_t *g_balance_supershotgun_knockback;
extern cvar_t *g_balance_supershotgun_pellets;
extern cvar_t *g_balance_supershotgun_refire;
extern cvar_t *g_balance_supershotgun_spread_x;
extern cvar_t *g_balance_supershotgun_spread_y;
extern cvar_t *g_capture_limit;
extern cvar_t *g_cheats;
extern cvar_t *g_ctf;
extern cvar_t *g_techs;
extern cvar_t *g_hook;
extern cvar_t *g_hook_auto_refire;
extern cvar_t *g_hook_distance;
extern cvar_t *g_hook_refire;
extern cvar_t *g_hook_style;
extern cvar_t *g_hook_speed;
extern cvar_t *g_hook_pull_speed;
extern cvar_t *g_frag_limit;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_force_demo;
extern cvar_t *g_force_screenshot;
extern cvar_t *g_gameplay;
extern cvar_t *g_gravity;
extern cvar_t *g_handicap;
extern cvar_t *g_inhibit;
extern cvar_t *g_num_teams;
extern cvar_t *g_match;
extern cvar_t *g_max_entities;
extern cvar_t *g_motd;
extern cvar_t *g_password;
extern cvar_t *g_player_projectile;
extern cvar_t *g_random_map;
extern cvar_t *g_respawn_protection;
extern cvar_t *g_round_limit;
extern cvar_t *g_rounds;
extern cvar_t *g_self_damage;
extern cvar_t *g_show_attacker_stats;
extern cvar_t *g_spawn_farthest;
extern cvar_t *g_spectator_chat;
extern cvar_t *g_teams;
extern cvar_t *g_time_limit;
extern cvar_t *g_timeout_time;
extern cvar_t *g_voting;
extern cvar_t *g_warmup_time;
extern cvar_t *g_weapon_respawn_time;
extern cvar_t *g_weapon_stay;

extern cvar_t *sv_max_clients;
extern cvar_t *sv_hostname;
extern cvar_t *dedicated;

extern g_team_t g_teamlist[MAX_TEAMS];

#define g_team_red (&g_teamlist[TEAM_RED])
#define g_team_blue (&g_teamlist[TEAM_BLUE])
#define g_team_green (&g_teamlist[TEAM_GREEN])
#define g_team_orange (&g_teamlist[TEAM_ORANGE])

void G_Init(void);
void G_Shutdown(void);
void G_CheckHook(void);
void G_CheckTechs(void);
void G_ResetItems(void);
void G_ResetTeams(void);
void G_InitNumTeams(void);
void G_SetTeamNames(void);
const g_team_t *G_TeamDefaults(const g_team_t *team);
void G_ResetSpawnPoints(void);
void G_ResetVote(void);
void G_CallTimeOut(g_entity_t *ent);
void G_CallTimeIn(void);
void G_RunTimers(void);
void G_MuteClient(char *name, _Bool mute);

g_export_t *G_LoadGame(g_import_t *import);

#endif /* __GAME_LOCAL_H__ */
