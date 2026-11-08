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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "g_types.h"

#if defined(__G_LOCAL_H__)

extern GameInfo gameInfo;
extern GameLevel gameLevel;
extern GameMedia gameMedia;

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
float G_LevelSetting(const Cvar *cvar, float requested, const char **source);
float G_LevelGravity(void);
PMovement G_ResolveMovement(const char *name);
GamePlayId G_ResolveGamePlay(const char *name);

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
extern Cvar *g_balanceAdrenalineBoost;
extern Cvar *g_balanceAdrenalineRespawnTime;
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
extern Cvar *g_balanceGrenadeLauncherDamage;
extern Cvar *g_balanceGrenadeLauncherKnockback;
extern Cvar *g_balanceGrenadeLauncherRadius;
extern Cvar *g_balanceGrenadeLauncherRefire;
extern Cvar *g_balanceGrenadeLauncherSpeed;
extern Cvar *g_balanceGrenadeLauncherTimer;
extern Cvar *g_balanceQuadDamageRespawnTime;
extern Cvar *g_balanceQuadDamageTime;
extern Cvar *g_balanceQuakeShotgunDamage;
extern Cvar *g_balanceQuakeShotgunKnockback;
extern Cvar *g_balanceQuakeShotgunPellets;
extern Cvar *g_balanceQuakeShotgunRefire;
extern Cvar *g_balanceQuakeShotgunSpreadX;
extern Cvar *g_balanceQuakeShotgunSpreadY;
extern Cvar *g_balanceQuakeSuperShotgunDamage;
extern Cvar *g_balanceQuakeSuperShotgunKnockback;
extern Cvar *g_balanceQuakeSuperShotgunPellets;
extern Cvar *g_balanceQuakeSuperShotgunRefire;
extern Cvar *g_balanceQuakeSuperShotgunSpreadX;
extern Cvar *g_balanceQuakeSuperShotgunSpreadY;
extern Cvar *g_balanceQuakeNailgunDamage;
extern Cvar *g_balanceQuakeNailgunKnockback;
extern Cvar *g_balanceQuakeNailgunRefire;
extern Cvar *g_balanceQuakeNailgunSpeed;
extern Cvar *g_balanceQuakeSuperNailgunDamage;
extern Cvar *g_balanceQuakeSuperNailgunKnockback;
extern Cvar *g_balanceQuakeSuperNailgunRefire;
extern Cvar *g_balanceQuakeSuperNailgunSpeed;
extern Cvar *g_balanceQuakeGrenadeLauncherDamage;
extern Cvar *g_balanceQuakeGrenadeLauncherKnockback;
extern Cvar *g_balanceQuakeGrenadeLauncherRadius;
extern Cvar *g_balanceQuakeGrenadeLauncherRefire;
extern Cvar *g_balanceQuakeGrenadeLauncherSpeed;
extern Cvar *g_balanceQuakeGrenadeLauncherTimer;
extern Cvar *g_balanceQuakeRocketLauncherDamage;
extern Cvar *g_balanceQuakeRocketLauncherKnockback;
extern Cvar *g_balanceQuakeRocketLauncherRadius;
extern Cvar *g_balanceQuakeRocketLauncherRefire;
extern Cvar *g_balanceQuakeRocketLauncherSpeed;
extern Cvar *g_balanceQuakeThunderboltDamage;
extern Cvar *g_balanceQuakeThunderboltKnockback;
extern Cvar *g_balanceQuakeThunderboltLength;
extern Cvar *g_balanceQuakeThunderboltRefire;
extern Cvar *g_balanceQuake2BlasterDamage;
extern Cvar *g_balanceQuake2BlasterKnockback;
extern Cvar *g_balanceQuake2BlasterRefire;
extern Cvar *g_balanceQuake2BlasterSpeed;
extern Cvar *g_balanceQuake2ShotgunDamage;
extern Cvar *g_balanceQuake2ShotgunKnockback;
extern Cvar *g_balanceQuake2ShotgunPellets;
extern Cvar *g_balanceQuake2ShotgunRefire;
extern Cvar *g_balanceQuake2ShotgunSpreadX;
extern Cvar *g_balanceQuake2ShotgunSpreadY;
extern Cvar *g_balanceQuake2SuperShotgunDamage;
extern Cvar *g_balanceQuake2SuperShotgunKnockback;
extern Cvar *g_balanceQuake2SuperShotgunPellets;
extern Cvar *g_balanceQuake2SuperShotgunRefire;
extern Cvar *g_balanceQuake2SuperShotgunSpreadX;
extern Cvar *g_balanceQuake2SuperShotgunSpreadY;
extern Cvar *g_balanceQuake2MachinegunDamage;
extern Cvar *g_balanceQuake2MachinegunKnockback;
extern Cvar *g_balanceQuake2MachinegunRefire;
extern Cvar *g_balanceQuake2MachinegunSpreadX;
extern Cvar *g_balanceQuake2MachinegunSpreadY;
extern Cvar *g_balanceQuake2ChaingunDamage;
extern Cvar *g_balanceQuake2ChaingunKnockback;
extern Cvar *g_balanceQuake2ChaingunRefire;
extern Cvar *g_balanceQuake2ChaingunSpreadX;
extern Cvar *g_balanceQuake2ChaingunSpreadY;
extern Cvar *g_balanceQuake2HandgrenadeDamage;
extern Cvar *g_balanceQuake2HandgrenadeKnockback;
extern Cvar *g_balanceQuake2HandgrenadeRadius;
extern Cvar *g_balanceQuake2HandgrenadeRefire;
extern Cvar *g_balanceQuake2GrenadeLauncherDamage;
extern Cvar *g_balanceQuake2GrenadeLauncherKnockback;
extern Cvar *g_balanceQuake2GrenadeLauncherRadius;
extern Cvar *g_balanceQuake2GrenadeLauncherRefire;
extern Cvar *g_balanceQuake2GrenadeLauncherSpeed;
extern Cvar *g_balanceQuake2GrenadeLauncherTimer;
extern Cvar *g_balanceQuake2RocketLauncherDamage;
extern Cvar *g_balanceQuake2RocketLauncherKnockback;
extern Cvar *g_balanceQuake2RocketLauncherRadius;
extern Cvar *g_balanceQuake2RocketLauncherRefire;
extern Cvar *g_balanceQuake2RocketLauncherSpeed;
extern Cvar *g_balanceQuake2HyperblasterDamage;
extern Cvar *g_balanceQuake2HyperblasterKnockback;
extern Cvar *g_balanceQuake2HyperblasterRefire;
extern Cvar *g_balanceQuake2HyperblasterSpeed;
extern Cvar *g_balanceQuake2RailgunDamage;
extern Cvar *g_balanceQuake2RailgunKnockback;
extern Cvar *g_balanceQuake2RailgunRefire;
extern Cvar *g_balanceInvisibilityRespawnTime;
extern Cvar *g_balanceInvisibilityTime;
extern Cvar *g_balanceInvulnerabilityRespawnTime;
extern Cvar *g_balanceInvulnerabilityTime;
extern Cvar *g_balanceRailgunDamage;
extern Cvar *g_balanceRailgunKnockback;
extern Cvar *g_balanceRailgunRefire;
extern Cvar *g_balanceRocketLauncherDamage;
extern Cvar *g_balanceRocketLauncherKnockback;
extern Cvar *g_balanceRocketLauncherRadius;
extern Cvar *g_balanceRocketLauncherRefire;
extern Cvar *g_balanceRocketLauncherSpeed;
extern Cvar *g_balanceShotgunDamage;
extern Cvar *g_balanceShotgunKnockback;
extern Cvar *g_balanceShotgunPellets;
extern Cvar *g_balanceShotgunRefire;
extern Cvar *g_balanceShotgunSpreadX;
extern Cvar *g_balanceShotgunSpreadY;
extern Cvar *g_balanceSuperShotgunDamage;
extern Cvar *g_balanceSuperShotgunKnockback;
extern Cvar *g_balanceSuperShotgunPellets;
extern Cvar *g_balanceSuperShotgunRefire;
extern Cvar *g_balanceSuperShotgunSpreadX;
extern Cvar *g_balanceSuperShotgunSpreadY;
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
extern Cvar *g_items;
GameItems G_ResolveItems(const char *name);
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
extern Cvar *g_dropBackpacks;
extern Cvar *g_itemTimers;
extern Cvar *g_numTeams;
extern Cvar *g_motd;
extern Cvar *g_password;
extern Cvar *g_playerProjectile;
extern Cvar *g_spawnProtection;
extern Cvar *g_respawnDelay;
extern Cvar *g_forceRespawn;
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
extern Cvar *sv_practice;
extern Cvar *dedicated;
extern Cvar *editor;

extern GameTeam gameTeamList[MAX_TEAMS];

void G_Init(void);
void G_Shutdown(void);
void G_ResetItems(void);
void G_ResetTeams(void);
void G_InitNumTeams(void);
void G_SetTeamNames(void);
void G_SetTeamState(void);
void G_ResetSpawnPoints(void);
void G_RunTimers(void);
void G_MuteClient(char *name, bool mute);
void G_SetClientMuted(GameClient *cl, bool mute);

GameExport *G_LoadGame(GameImport *import);

#endif
