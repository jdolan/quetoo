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

#if defined(__G_LOCAL_H__)

extern GameLevel g_level;
extern GameMedia g_media;

/**
 * @brief The movement a level falls back to when neither `g_movement`, its
 * metadata nor its worldspawn choose one. A module's `g_types.h` MAY say otherwise.
 */
#if !defined(G_MOVEMENT_DEFAULT)
#define G_MOVEMENT_DEFAULT PM_MOVEMENT_QUETOO
#endif

/**
 * @brief How long the intermission runs before the level advances.
 */
#define INTERMISSION (10.0 * 1000)

char *G_FormatTime(uint32_t time);
PlayerMoveParams G_MovementParams(void);
float G_LevelGravity(void);
PlayerMovement G_ResolveMovement(const char *name);
GameplayId G_ResolveGameplay(const char *name);

extern GameImport gi;

/**
 * @brief Console logging, in the shape `Com_Debug`, `Com_Warn` and `Com_Error`
 * take in the engine: a prefixed macro supplying `__func__`, which C gives a
 * function no way to learn for itself, over the real function behind it.
 * @details These live here rather than in `g_local.h` because that is per-module,
 * and a macro every common source uses should not be copied into each mod.
 */
#define G_Debug(...) gi.Debug(DEBUG_GAME, __func__, __VA_ARGS__)
#define G_Warn(...) gi.Warn(__func__, __VA_ARGS__)
#define G_Error(...) gi.Error(__func__, __VA_ARGS__)
extern GameExport ge;

extern Cvar *g_admin_password;
extern Cvar *g_ammo_respawn_time;
extern Cvar *g_auto_join;
extern Cvar *g_balance_armor_shard_respawn;
extern Cvar *g_balance_armor_jacket_respawn;
extern Cvar *g_balance_armor_combat_respawn;
extern Cvar *g_balance_armor_body_respawn;
extern Cvar *g_balance_bfg_damage;
extern Cvar *g_balance_bfg_knockback;
extern Cvar *g_balance_bfg_prefire;
extern Cvar *g_balance_bfg_radius;
extern Cvar *g_balance_bfg_refire;
extern Cvar *g_balance_bfg_speed;
extern Cvar *g_balance_blaster_damage;
extern Cvar *g_balance_blaster_knockback;
extern Cvar *g_balance_blaster_refire;
extern Cvar *g_balance_blaster_speed;
extern Cvar *g_balance_handgrenade_refire;
extern Cvar *g_balance_health_small_respawn;
extern Cvar *g_balance_health_medium_respawn;
extern Cvar *g_balance_health_large_respawn;
extern Cvar *g_balance_health_mega_respawn;
extern Cvar *g_balance_hyperblaster_climb_damage;
extern Cvar *g_balance_hyperblaster_climb_knockback;
extern Cvar *g_balance_hyperblaster_damage;
extern Cvar *g_balance_hyperblaster_knockback;
extern Cvar *g_balance_hyperblaster_refire;
extern Cvar *g_balance_hyperblaster_speed;
extern Cvar *g_balance_lightning_damage;
extern Cvar *g_balance_lightning_knockback;
extern Cvar *g_balance_lightning_length;
extern Cvar *g_balance_lightning_refire;
extern Cvar *g_balance_machinegun_damage;
extern Cvar *g_balance_machinegun_knockback;
extern Cvar *g_balance_machinegun_refire;
extern Cvar *g_balance_machinegun_spread_x;
extern Cvar *g_balance_machinegun_spread_y;
extern Cvar *g_balance_grenadelauncher_damage;
extern Cvar *g_balance_grenadelauncher_knockback;
extern Cvar *g_balance_grenadelauncher_radius;
extern Cvar *g_balance_grenadelauncher_refire;
extern Cvar *g_balance_grenadelauncher_speed;
extern Cvar *g_balance_grenadelauncher_timer;
extern Cvar *g_balance_quad_damage_respawn_time;
extern Cvar *g_balance_quad_damage_time;
extern Cvar *g_balance_quake_shotgun_damage;
extern Cvar *g_balance_quake_shotgun_knockback;
extern Cvar *g_balance_quake_shotgun_pellets;
extern Cvar *g_balance_quake_shotgun_refire;
extern Cvar *g_balance_quake_shotgun_spread_x;
extern Cvar *g_balance_quake_shotgun_spread_y;
extern Cvar *g_balance_quake_supershotgun_damage;
extern Cvar *g_balance_quake_supershotgun_knockback;
extern Cvar *g_balance_quake_supershotgun_pellets;
extern Cvar *g_balance_quake_supershotgun_refire;
extern Cvar *g_balance_quake_supershotgun_spread_x;
extern Cvar *g_balance_quake_supershotgun_spread_y;
extern Cvar *g_balance_quake_nailgun_damage;
extern Cvar *g_balance_quake_nailgun_knockback;
extern Cvar *g_balance_quake_nailgun_refire;
extern Cvar *g_balance_quake_nailgun_speed;
extern Cvar *g_balance_quake_supernailgun_damage;
extern Cvar *g_balance_quake_supernailgun_knockback;
extern Cvar *g_balance_quake_supernailgun_refire;
extern Cvar *g_balance_quake_supernailgun_speed;
extern Cvar *g_balance_quake_grenadelauncher_damage;
extern Cvar *g_balance_quake_grenadelauncher_knockback;
extern Cvar *g_balance_quake_grenadelauncher_radius;
extern Cvar *g_balance_quake_grenadelauncher_refire;
extern Cvar *g_balance_quake_grenadelauncher_speed;
extern Cvar *g_balance_quake_grenadelauncher_timer;
extern Cvar *g_balance_quake_rocketlauncher_damage;
extern Cvar *g_balance_quake_rocketlauncher_knockback;
extern Cvar *g_balance_quake_rocketlauncher_radius;
extern Cvar *g_balance_quake_rocketlauncher_refire;
extern Cvar *g_balance_quake_rocketlauncher_speed;
extern Cvar *g_balance_quake_thunderbolt_damage;
extern Cvar *g_balance_quake_thunderbolt_knockback;
extern Cvar *g_balance_quake_thunderbolt_length;
extern Cvar *g_balance_quake_thunderbolt_refire;
extern Cvar *g_balance_invisibility_respawn_time;
extern Cvar *g_balance_invisibility_time;
extern Cvar *g_balance_invulnerability_respawn_time;
extern Cvar *g_balance_invulnerability_time;
extern Cvar *g_balance_railgun_damage;
extern Cvar *g_balance_railgun_knockback;
extern Cvar *g_balance_railgun_refire;
extern Cvar *g_balance_rocketlauncher_damage;
extern Cvar *g_balance_rocketlauncher_knockback;
extern Cvar *g_balance_rocketlauncher_radius;
extern Cvar *g_balance_rocketlauncher_refire;
extern Cvar *g_balance_rocketlauncher_speed;
extern Cvar *g_balance_shotgun_damage;
extern Cvar *g_balance_shotgun_knockback;
extern Cvar *g_balance_shotgun_pellets;
extern Cvar *g_balance_shotgun_refire;
extern Cvar *g_balance_shotgun_spread_x;
extern Cvar *g_balance_shotgun_spread_y;
extern Cvar *g_balance_supershotgun_damage;
extern Cvar *g_balance_supershotgun_knockback;
extern Cvar *g_balance_supershotgun_pellets;
extern Cvar *g_balance_supershotgun_refire;
extern Cvar *g_balance_supershotgun_spread_x;
extern Cvar *g_balance_supershotgun_spread_y;
extern Cvar *g_cheats;
extern Cvar *g_frag_limit;
extern Cvar *g_friendly_fire;
extern Cvar *g_gameplay;

// player movement parameters (hydrated into PlayerMoveParams by G_MovementParams)
extern Cvar *g_air_acceleration;
extern Cvar *g_air_friction;
extern Cvar *g_air_speed;
extern Cvar *g_duck_speed;
extern Cvar *g_duck_stand_speed;
extern Cvar *g_gravity;
extern Cvar *g_movement;
extern Cvar *g_ground_acceleration;
extern Cvar *g_ground_acceleration_slick;
extern Cvar *g_ground_friction;
extern Cvar *g_ground_friction_slick;
extern Cvar *g_ground_speed;
extern Cvar *g_jump_speed;
extern Cvar *g_ladder_acceleration;
extern Cvar *g_ladder_friction;
extern Cvar *g_ladder_speed;
extern Cvar *g_spectator_acceleration;
extern Cvar *g_spectator_friction;
extern Cvar *g_spectator_speed;
extern Cvar *g_stop_speed;
extern Cvar *g_water_acceleration;
extern Cvar *g_water_friction;
extern Cvar *g_water_jump_speed;
extern Cvar *g_water_speed;
extern Cvar *g_death_cam;
extern Cvar *g_death_cam_distance;
extern Cvar *g_death_cam_height;
extern Cvar *g_death_cam_rise;
extern Cvar *g_death_cam_time;
extern Cvar *g_death_cam_velocity;
extern Cvar *g_num_teams;
extern Cvar *g_motd;
extern Cvar *g_password;
extern Cvar *g_player_projectile;
extern Cvar *g_respawn_protection;
extern Cvar *g_fall_damage;
extern Cvar *g_self_damage;
extern Cvar *g_self_knockback;
extern Cvar *g_show_attacker_stats;
extern Cvar *g_spawn_farthest;
extern Cvar *g_spectator_chat;
extern Cvar *g_time_limit;
extern Cvar *g_weapon_respawn_time;
extern Cvar *g_weapon_stay;

extern Cvar *sv_max_clients;
extern Cvar *sv_max_entities;
extern Cvar *sv_min_clients;
extern Cvar *sv_hostname;
extern Cvar *dedicated;
extern Cvar *editor;

extern GameTeam g_team_list[MAX_TEAMS];

#define g_team_red (&g_team_list[TEAM_RED])
#define g_team_blue (&g_team_list[TEAM_BLUE])
#define g_team_yellow (&g_team_list[TEAM_YELLOW])
#define g_team_green (&g_team_list[TEAM_GREEN])

void G_Init(void);
void G_Shutdown(void);
void G_ResetItems(void);
void G_ResetTeams(void);
void G_InitNumTeams(void);
void G_SetTeamNames(void);
void G_ResetSpawnPoints(void);
void G_CallTimeOut(GameEntity *ent);
void G_CallTimeIn(void);
void G_RunTimers(void);
void G_MuteClient(char *name, bool mute);
void G_SetClientMuted(GameClient *cl, bool mute);

GameExport *G_LoadGame(GameImport *import);

#endif
