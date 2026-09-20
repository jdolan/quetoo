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

extern GameLevel gLevel;
extern GameMedia gMedia;

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
PMoveParams G_MovementParams(void);
float G_LevelGravity(void);
PMovement G_ResolveMovement(const char *name);
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

extern Cvar *g_adminPassword;
extern Cvar *g_ammoRespawnTime;
extern Cvar *g_autoJoin;
extern Cvar *g_balanceArmorShardRespawn;
extern Cvar *g_balanceArmorJacketRespawn;
extern Cvar *g_balanceArmorCombatRespawn;
extern Cvar *g_balanceArmorBodyRespawn;
extern Cvar *g_balanceBfgDamage;
extern Cvar *g_balanceBfgKnockback;
extern Cvar *g_balanceBfgPrefire;
extern Cvar *g_balanceBfgRadius;
extern Cvar *g_balanceBfgRefire;
extern Cvar *g_balanceBfgSpeed;
extern Cvar *g_balanceBlasterDamage;
extern Cvar *g_balanceBlasterKnockback;
extern Cvar *g_balanceBlasterRefire;
extern Cvar *g_balanceBlasterSpeed;
extern Cvar *g_balanceHandgrenadeRefire;
extern Cvar *g_balanceHealthSmallRespawn;
extern Cvar *g_balanceHealthMediumRespawn;
extern Cvar *g_balanceHealthLargeRespawn;
extern Cvar *g_balanceHealthMegaRespawn;
extern Cvar *g_balanceHyperblasterClimbDamage;
extern Cvar *g_balanceHyperblasterClimbKnockback;
extern Cvar *g_balanceHyperblasterDamage;
extern Cvar *g_balanceHyperblasterKnockback;
extern Cvar *g_balanceHyperblasterRefire;
extern Cvar *g_balanceHyperblasterSpeed;
extern Cvar *g_balanceLightningDamage;
extern Cvar *g_balanceLightningKnockback;
extern Cvar *g_balanceLightningLength;
extern Cvar *g_balanceLightningRefire;
extern Cvar *g_balanceMachinegunDamage;
extern Cvar *g_balanceMachinegunKnockback;
extern Cvar *g_balanceMachinegunRefire;
extern Cvar *g_balanceMachinegunSpreadX;
extern Cvar *g_balanceMachinegunSpreadY;
extern Cvar *g_balanceGrenadelauncherDamage;
extern Cvar *g_balanceGrenadelauncherKnockback;
extern Cvar *g_balanceGrenadelauncherRadius;
extern Cvar *g_balanceGrenadelauncherRefire;
extern Cvar *g_balanceGrenadelauncherSpeed;
extern Cvar *g_balanceGrenadelauncherTimer;
extern Cvar *g_balanceQuadDamageRespawnTime;
extern Cvar *g_balanceQuadDamageTime;
extern Cvar *g_balanceQuakeShotgunDamage;
extern Cvar *g_balanceQuakeShotgunKnockback;
extern Cvar *g_balanceQuakeShotgunPellets;
extern Cvar *g_balanceQuakeShotgunRefire;
extern Cvar *g_balanceQuakeShotgunSpreadX;
extern Cvar *g_balanceQuakeShotgunSpreadY;
extern Cvar *g_balanceQuakeSupershotgunDamage;
extern Cvar *g_balanceQuakeSupershotgunKnockback;
extern Cvar *g_balanceQuakeSupershotgunPellets;
extern Cvar *g_balanceQuakeSupershotgunRefire;
extern Cvar *g_balanceQuakeSupershotgunSpreadX;
extern Cvar *g_balanceQuakeSupershotgunSpreadY;
extern Cvar *g_balanceQuakeNailgunDamage;
extern Cvar *g_balanceQuakeNailgunKnockback;
extern Cvar *g_balanceQuakeNailgunRefire;
extern Cvar *g_balanceQuakeNailgunSpeed;
extern Cvar *g_balanceQuakeSupernailgunDamage;
extern Cvar *g_balanceQuakeSupernailgunKnockback;
extern Cvar *g_balanceQuakeSupernailgunRefire;
extern Cvar *g_balanceQuakeSupernailgunSpeed;
extern Cvar *g_balanceQuakeGrenadelauncherDamage;
extern Cvar *g_balanceQuakeGrenadelauncherKnockback;
extern Cvar *g_balanceQuakeGrenadelauncherRadius;
extern Cvar *g_balanceQuakeGrenadelauncherRefire;
extern Cvar *g_balanceQuakeGrenadelauncherSpeed;
extern Cvar *g_balanceQuakeGrenadelauncherTimer;
extern Cvar *g_balanceQuakeRocketlauncherDamage;
extern Cvar *g_balanceQuakeRocketlauncherKnockback;
extern Cvar *g_balanceQuakeRocketlauncherRadius;
extern Cvar *g_balanceQuakeRocketlauncherRefire;
extern Cvar *g_balanceQuakeRocketlauncherSpeed;
extern Cvar *g_balanceQuakeThunderboltDamage;
extern Cvar *g_balanceQuakeThunderboltKnockback;
extern Cvar *g_balanceQuakeThunderboltLength;
extern Cvar *g_balanceQuakeThunderboltRefire;
extern Cvar *g_balanceInvisibilityRespawnTime;
extern Cvar *g_balanceInvisibilityTime;
extern Cvar *g_balanceInvulnerabilityRespawnTime;
extern Cvar *g_balanceInvulnerabilityTime;
extern Cvar *g_balanceRailgunDamage;
extern Cvar *g_balanceRailgunKnockback;
extern Cvar *g_balanceRailgunRefire;
extern Cvar *g_balanceRocketlauncherDamage;
extern Cvar *g_balanceRocketlauncherKnockback;
extern Cvar *g_balanceRocketlauncherRadius;
extern Cvar *g_balanceRocketlauncherRefire;
extern Cvar *g_balanceRocketlauncherSpeed;
extern Cvar *g_balanceShotgunDamage;
extern Cvar *g_balanceShotgunKnockback;
extern Cvar *g_balanceShotgunPellets;
extern Cvar *g_balanceShotgunRefire;
extern Cvar *g_balanceShotgunSpreadX;
extern Cvar *g_balanceShotgunSpreadY;
extern Cvar *g_balanceSupershotgunDamage;
extern Cvar *g_balanceSupershotgunKnockback;
extern Cvar *g_balanceSupershotgunPellets;
extern Cvar *g_balanceSupershotgunRefire;
extern Cvar *g_balanceSupershotgunSpreadX;
extern Cvar *g_balanceSupershotgunSpreadY;
extern Cvar *g_cheats;
extern Cvar *g_fragLimit;
extern Cvar *g_friendlyFire;
extern Cvar *g_gameplay;

// player movement parameters (hydrated into PMoveParams by G_MovementParams)
extern Cvar *g_airAcceleration;
extern Cvar *g_airFriction;
extern Cvar *g_airSpeed;
extern Cvar *g_duckSpeed;
extern Cvar *g_duckStandSpeed;
extern Cvar *g_gravity;
extern Cvar *g_movement;
extern Cvar *g_groundAcceleration;
extern Cvar *g_groundAccelerationSlick;
extern Cvar *g_groundFriction;
extern Cvar *g_groundFrictionSlick;
extern Cvar *g_groundSpeed;
extern Cvar *g_jumpSpeed;
extern Cvar *g_ladderAcceleration;
extern Cvar *g_ladderFriction;
extern Cvar *g_ladderSpeed;
extern Cvar *g_spectatorAcceleration;
extern Cvar *g_spectatorFriction;
extern Cvar *g_spectatorSpeed;
extern Cvar *g_stopSpeed;
extern Cvar *g_waterAcceleration;
extern Cvar *g_waterFriction;
extern Cvar *g_waterJumpSpeed;
extern Cvar *g_waterSpeed;
extern Cvar *g_deathCam;
extern Cvar *g_deathCamDistance;
extern Cvar *g_deathCamHeight;
extern Cvar *g_deathCamRise;
extern Cvar *g_deathCamTime;
extern Cvar *g_deathCamVelocity;
extern Cvar *g_numTeams;
extern Cvar *g_motd;
extern Cvar *g_password;
extern Cvar *g_playerProjectile;
extern Cvar *g_respawnProtection;
extern Cvar *g_fallDamage;
extern Cvar *g_selfDamage;
extern Cvar *g_selfKnockback;
extern Cvar *g_showAttackerStats;
extern Cvar *g_spawnFarthest;
extern Cvar *g_spectatorChat;
extern Cvar *g_timeLimit;
extern Cvar *g_weaponRespawnTime;
extern Cvar *g_weaponStay;

extern Cvar *sv_maxClients;
extern Cvar *sv_maxEntities;
extern Cvar *sv_minClients;
extern Cvar *sv_hostname;
extern Cvar *dedicated;
extern Cvar *editor;

extern GameTeam gTeamList[MAX_TEAMS];

#define g_team_red (&gTeamList[TEAM_RED])
#define g_team_blue (&gTeamList[TEAM_BLUE])
#define g_team_yellow (&gTeamList[TEAM_YELLOW])
#define g_team_green (&gTeamList[TEAM_GREEN])

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
