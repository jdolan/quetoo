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

#include "g_local.h"

GameImport gi;
GameExport ge;

GameLevel gLevel;
GameMedia gMedia;

Cvar *g_adminPassword;
Cvar *g_ammoRespawnTime;
Cvar *g_autoJoin;
Cvar *g_balanceArmorShardRespawn;
Cvar *g_balanceArmorJacketRespawn;
Cvar *g_balanceArmorCombatRespawn;
Cvar *g_balanceArmorBodyRespawn;
Cvar *g_balanceBfgDamage;
Cvar *g_balanceBfgKnockback;
Cvar *g_balanceBfgPrefire;
Cvar *g_balanceBfgRadius;
Cvar *g_balanceBfgRefire;
Cvar *g_balanceBfgSpeed;
Cvar *g_balanceBlasterDamage;
Cvar *g_balanceBlasterKnockback;
Cvar *g_balanceBlasterRefire;
Cvar *g_balanceBlasterSpeed;
Cvar *g_balanceHandgrenadeRefire;
Cvar *g_balanceHealthSmallRespawn;
Cvar *g_balanceHealthMediumRespawn;
Cvar *g_balanceHealthLargeRespawn;
Cvar *g_balanceHealthMegaRespawn;
Cvar *g_balanceHyperblasterClimbDamage;
Cvar *g_balanceHyperblasterClimbKnockback;
Cvar *g_balanceHyperblasterDamage;
Cvar *g_balanceHyperblasterKnockback;
Cvar *g_balanceHyperblasterRefire;
Cvar *g_balanceHyperblasterSpeed;
Cvar *g_balanceLightningDamage;
Cvar *g_balanceLightningKnockback;
Cvar *g_balanceLightningLength;
Cvar *g_balanceLightningRefire;
Cvar *g_balanceMachinegunDamage;
Cvar *g_balanceMachinegunKnockback;
Cvar *g_balanceMachinegunRefire;
Cvar *g_balanceMachinegunSpreadX;
Cvar *g_balanceMachinegunSpreadY;
Cvar *g_balanceGrenadelauncherDamage;
Cvar *g_balanceGrenadelauncherKnockback;
Cvar *g_balanceGrenadelauncherRadius;
Cvar *g_balanceGrenadelauncherRefire;
Cvar *g_balanceGrenadelauncherSpeed;
Cvar *g_balanceGrenadelauncherTimer;
Cvar *g_balanceQuadDamageRespawnTime;
Cvar *g_balanceQuadDamageTime;
Cvar *g_balanceQuakeShotgunDamage;
Cvar *g_balanceQuakeShotgunKnockback;
Cvar *g_balanceQuakeShotgunPellets;
Cvar *g_balanceQuakeShotgunRefire;
Cvar *g_balanceQuakeShotgunSpreadX;
Cvar *g_balanceQuakeShotgunSpreadY;
Cvar *g_balanceQuakeSupershotgunDamage;
Cvar *g_balanceQuakeSupershotgunKnockback;
Cvar *g_balanceQuakeSupershotgunPellets;
Cvar *g_balanceQuakeSupershotgunRefire;
Cvar *g_balanceQuakeSupershotgunSpreadX;
Cvar *g_balanceQuakeSupershotgunSpreadY;
Cvar *g_balanceQuakeNailgunDamage;
Cvar *g_balanceQuakeNailgunKnockback;
Cvar *g_balanceQuakeNailgunRefire;
Cvar *g_balanceQuakeNailgunSpeed;
Cvar *g_balanceQuakeSupernailgunDamage;
Cvar *g_balanceQuakeSupernailgunKnockback;
Cvar *g_balanceQuakeSupernailgunRefire;
Cvar *g_balanceQuakeSupernailgunSpeed;
Cvar *g_balanceQuakeGrenadelauncherDamage;
Cvar *g_balanceQuakeGrenadelauncherKnockback;
Cvar *g_balanceQuakeGrenadelauncherRadius;
Cvar *g_balanceQuakeGrenadelauncherRefire;
Cvar *g_balanceQuakeGrenadelauncherSpeed;
Cvar *g_balanceQuakeGrenadelauncherTimer;
Cvar *g_balanceQuakeRocketlauncherDamage;
Cvar *g_balanceQuakeRocketlauncherKnockback;
Cvar *g_balanceQuakeRocketlauncherRadius;
Cvar *g_balanceQuakeRocketlauncherRefire;
Cvar *g_balanceQuakeRocketlauncherSpeed;
Cvar *g_balanceQuakeThunderboltDamage;
Cvar *g_balanceQuakeThunderboltKnockback;
Cvar *g_balanceQuakeThunderboltLength;
Cvar *g_balanceQuakeThunderboltRefire;
Cvar *g_balanceInvisibilityRespawnTime;
Cvar *g_balanceInvisibilityTime;
Cvar *g_balanceInvulnerabilityRespawnTime;
Cvar *g_balanceInvulnerabilityTime;
Cvar *g_balanceRailgunDamage;
Cvar *g_balanceRailgunKnockback;
Cvar *g_balanceRailgunRefire;
Cvar *g_balanceRocketlauncherDamage;
Cvar *g_balanceRocketlauncherKnockback;
Cvar *g_balanceRocketlauncherRadius;
Cvar *g_balanceRocketlauncherRefire;
Cvar *g_balanceRocketlauncherSpeed;
Cvar *g_balanceShotgunDamage;
Cvar *g_balanceShotgunKnockback;
Cvar *g_balanceShotgunPellets;
Cvar *g_balanceShotgunRefire;
Cvar *g_balanceShotgunSpreadX;
Cvar *g_balanceShotgunSpreadY;
Cvar *g_balanceSupershotgunDamage;
Cvar *g_balanceSupershotgunKnockback;
Cvar *g_balanceSupershotgunPellets;
Cvar *g_balanceSupershotgunRefire;
Cvar *g_balanceSupershotgunSpreadX;
Cvar *g_balanceSupershotgunSpreadY;
Cvar *g_cheats;
Cvar *g_fragLimit;
Cvar *g_friendlyFire;
Cvar *g_gameplay;
Cvar *g_movement;

/**
 * @brief What this level asked for, remembered so that setting `g_movement`
 * back to "default" returns to it rather than to Quetoo's.
 */
static PMovement gMovementLevel;
static GameplayId gGameplayLevel;

// player movement parameters (hydrated into PMoveParams by G_MovementParams)
Cvar *g_airAcceleration;
Cvar *g_airFriction;
Cvar *g_airSpeed;
Cvar *g_duckSpeed;
Cvar *g_duckStandSpeed;
Cvar *g_gravity;
Cvar *g_groundAcceleration;
Cvar *g_groundAccelerationSlick;
Cvar *g_groundFriction;
Cvar *g_groundFrictionSlick;
Cvar *g_groundSpeed;
Cvar *g_jumpSpeed;
Cvar *g_ladderAcceleration;
Cvar *g_ladderFriction;
Cvar *g_ladderSpeed;
Cvar *g_spectatorAcceleration;
Cvar *g_spectatorFriction;
Cvar *g_spectatorSpeed;
Cvar *g_stopSpeed;
Cvar *g_waterAcceleration;
Cvar *g_waterFriction;
Cvar *g_waterJumpSpeed;
Cvar *g_waterSpeed;
Cvar *g_deathCam;
Cvar *g_deathCamDistance;
Cvar *g_deathCamHeight;
Cvar *g_deathCamRise;
Cvar *g_deathCamTime;
Cvar *g_deathCamVelocity;
Cvar *g_motd;
Cvar *g_numTeams;
Cvar *g_password;
Cvar *g_playerProjectile;
Cvar *g_respawnProtection;
Cvar *g_fallDamage;
Cvar *g_selfDamage;
Cvar *g_selfKnockback;
Cvar *g_showAttackerStats;
Cvar *g_spawnFarthest;
Cvar *g_spectatorChat;
Cvar *g_timeLimit;
Cvar *g_weaponRespawnTime;
Cvar *g_weaponStay;

Cvar *sv_minClients;
Cvar *sv_maxClients;
Cvar *sv_maxEntities;
Cvar *sv_hostname;
Cvar *dedicated;
Cvar *editor;

GameTeam gTeamList[MAX_TEAMS] = {
  [TEAM_RED] = {
    .id = TEAM_RED,
    .name = "Red",
#if defined(G_CTF)
    .flag = "item_flag_team1",
#endif
    .spawn = "info_player_team1",
    .shirt = { .r = 1.f, .g = 0.f, .b = 0.f, .a = 1.f },
    .pants = { .r = 1.f, .g = 0.f, .b = 0.f, .a = 1.f },
    .helmet = { .r = 1.f, .g = 0.f, .b = 0.f, .a = 1.f },
    .color = TEAM_COLOR_RED,
#if defined(G_CTF)
    .effect = EF_CTF_RED,
#endif
  },
  [TEAM_BLUE] = {
    .id = TEAM_BLUE,
    .name = "Blue",
#if defined(G_CTF)
    .flag = "item_flag_team2",
#endif
    .spawn = "info_player_team2",
    .shirt = { .r = 0.f, .g = 0.f, .b = 1.f, .a = 1.f },
    .pants = { .r = 0.f, .g = 0.f, .b = 1.f, .a = 1.f },
    .helmet = { .r = 0.f, .g = 0.f, .b = 1.f, .a = 1.f },
    .color = TEAM_COLOR_BLUE,
#if defined(G_CTF)
    .effect = EF_CTF_BLUE,
#endif
  },
  [TEAM_YELLOW] = {
    .id = TEAM_YELLOW,
    .name = "Yellow",
#if defined(G_CTF)
    .flag = "item_flag_team3",
#endif
    .spawn = "info_player_team3",
    .shirt = { .r = 1.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .pants = { .r = 1.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .helmet = { .r = 1.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .color = TEAM_COLOR_YELLOW,
#if defined(G_CTF)
    .effect = EF_CTF_YELLOW,
#endif
  },
  [TEAM_GREEN] = {
    .id = TEAM_GREEN,
    .name = "Green",
#if defined(G_CTF)
    .flag = "item_flag_team4",
#endif
    .spawn = "info_player_team4",
    .shirt = { .r = 0.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .pants = { .r = 0.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .helmet = { .r = 0.f, .g = 1.f, .b = 0.f, .a = 1.f },
    .color = TEAM_COLOR_GREEN,
#if defined(G_CTF)
    .effect = EF_CTF_GREEN,
#endif
  },
};

/**
 * @brief Resets team runtime state and updates the configstring.
 */
void G_ResetTeams(void) {

  for (int32_t i = 0; i < MAX_TEAMS; i++) {
    GameTeam *team = &gTeamList[i];
    team->score = 0;
    team->spawnPoints = (GameSpawnPoints) { 0 };
#if defined(G_CTF)
    team->captures = 0;
    team->flagEntity = NULL;
#endif
  }

  G_SetTeamNames();
}

/**
 * @brief Send the names of the teams to the clients.
 */
void G_SetTeamNames(void) {
  char teamInfo[MAX_STRING_CHARS] = { '\0' };

  for (int32_t i = 0; i < MAX_TEAMS; i++) {

    if (i != TEAM_RED) {
      q_strlcat(teamInfo, "\\", sizeof(teamInfo));
    }

    q_strlcat(teamInfo, va("%d", gTeamList[i].id), sizeof(teamInfo));
    q_strlcat(teamInfo, "\\", sizeof(teamInfo));
    q_strlcat(teamInfo, gTeamList[i].name, sizeof(teamInfo));
    q_strlcat(teamInfo, "\\", sizeof(teamInfo));
    q_strlcat(teamInfo, va("%d", gTeamList[i].color), sizeof(teamInfo));
    q_strlcat(teamInfo, "\\", sizeof(teamInfo));
    q_strlcat(teamInfo, Color_Unparse(gTeamList[i].shirt), sizeof(teamInfo));
  }

  gi.SetConfigString(CS_TEAM_INFO, teamInfo);
}

/**
 * @brief Reset all items in the level based on gameplay, CTF, etc.
 */
void G_ResetItems(void) {

  G_ForEachEntity(ent, {

    if (!ent->item) {
      continue;
    }

    if (ent->spawnFlags & SF_ITEM_DROPPED) {
      G_FreeEntity(ent);
      continue;
    }

#if defined(G_TECH)
    if (ent->item->def.type == ITEM_TYPE_TECH) {
      G_FreeEntity(ent);
      continue;
    }
#endif

    G_ResetItem(ent);
  });

#if defined(G_TECH)
  G_Tech_SpawnAll();
#endif
}

/**
 * @brief Setup the effects for spawn points.
 */
static void G_ResetTeamSpawnPoints(GameSpawnPoints *points, const GameEntityTrail trail, const GameTeamId teamId) {

  for (size_t i = 0; i < points->count; i++) {
    GameEntity *ent = points->spots[i];

    if (trail && gLevel.teams) {

      if (ent->s.trail) {
        // Shared spawn point (already claimed by another team): use yellow
        ent->s.color = Color_Color32(ColorHSV(color_hue_yellow, 1.f, 1.f));
      } else {
        ent->s.color = Color_Color32(ColorHSV(gTeamList[teamId].color, 1.f, 1.f));
      }

      ent->s.trail = trail;
      ent->svFlags = 0;

      gi.LinkEntity(ent);
    } else {

      ent->s.trail = 0;
      ent->s.color = (Color32) { .rgba = 0 };
      ent->svFlags = SVF_NO_CLIENT;

      gi.UnlinkEntity(ent);
    }
  }
}

/**
 * @brief Setup the effects for spawn points.
 */
void G_ResetSpawnPoints(void) {

  // reset trails to 0 first
  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    G_ResetTeamSpawnPoints(&gTeamList[t].spawnPoints, 0, 0);
  }

  // then apply team-based trails, this is done twice so neutrality gets applied properly
  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    G_ResetTeamSpawnPoints(&gTeamList[t].spawnPoints, TRAIL_PLAYER_SPAWN, t);
  }
}

/**
 * @brief Reset scores and respawn. Teams are only reset when teamz is true.
 */
static void G_RestartGame(bool teamz) {

  G_ForEachClient(cl, {

    cl->persistent.score = 0;
#if defined(G_CTF)
    cl->persistent.captures = 0;
#endif
    cl->persistent.deaths = 0;

    if (teamz) { // reset teams
      cl->persistent.team = NULL;
    }

    // determine spectator or team affiliations

    if (gLevel.teams) {

      if (!cl->persistent.team) {
        if (g_autoJoin->value) {
          G_AddClientToTeam(cl, G_SmallestTeam()->name);
        } else {
          cl->persistent.spectator = true;
        }
      }
    }

    G_ClientUserInfoChanged(cl, cl->persistent.userInfo);
    G_ClientRespawn(cl, false);
  });

  G_ConfigureLevel();

  G_ResetItems();

  G_ResetSpawnPoints();

  G_InitNumTeams();

  for (int32_t i = 0; i < MAX_TEAMS; i++) {
    gTeamList[i].score = 0;
#if defined(G_CTF)
    gTeamList[i].captures = 0;
#endif
  }

  gi.BroadcastPrint(PRINT_HIGH, "Game restarted\n");

  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.teleport
  }, MULTICAST_PHS_R);
}

/**
 * @brief Sets or clears the muted flag on a client, in chat and in voice.
 */
void G_SetClientMuted(GameClient *cl, bool mute) {

  cl->persistent.muted = mute;

  G_ForEachClient(other, {
    gi.MuteVoice(other, cl, mute);
  });
}

/**
 * @brief Sets or clears the muted flag on the named client.
 */
void G_MuteClient(char *name, bool mute) {
  GameClient *cl;

  if (!(cl = G_ClientByName(name))) {
    return;
  }

  G_SetClientMuted(cl, mute);
}

/**
 * @brief Submits accumulated frags, and captures where the module has them, to
 * the server for stats processing.
 * @details The capture arguments are part of the game import ABI, which the
 * engine and every module share, whether or not the module scores captures.
 */
static void G_PostStats(void) {

  GameCapture *captures = NULL;
  int32_t numCaptures = 0;

#if defined(G_CTF)
  captures = (GameCapture *) gLevel.captures->elements;
  numCaptures = (int32_t) gLevel.captures->count;
#endif

  gi.PostStats((GameFrag *) gLevel.frags->elements, (int32_t) gLevel.frags->count,
               captures, numCaptures);

  gLevel.frags = release(gLevel.frags);

#if defined(G_CTF)
  gLevel.captures = release(gLevel.captures);
#endif
}

/**
 * @brief Starts an intermission sequence, moving all clients to the intermission viewpoint
 * and selecting the next map.
 */
static void G_BeginIntermission(void) {

  if (gLevel.intermissionTime) {
    return; // already activated
  }

  gLevel.intermissionTime = gLevel.time;

  G_PostStats();

  // respawn any dead clients
  G_ForEachClient(cl, {
    if (cl->entity && cl->entity->dead) {
      G_ClientRespawn(cl, false);
    }
  });

  // find an intermission spot
  GameEntity *ent = G_Find(NULL, EOFS(classname), "info_player_intermission");
  if (!ent) { // map does not have an intermission point
    ent = G_Find(NULL, EOFS(classname), "info_player_start");
    if (!ent) {
      ent = G_Find(NULL, EOFS(classname), "info_player_deathmatch");
    }
  }

  gLevel.intermissionOrigin = ent->s.origin;
  gLevel.intermissionAngle = ent->s.angles;

  if (ent->target) {
    const GameEntity *target = G_PickTarget(ent->target);
    if (target) {
      const Vec3 dir = Vec3_Subtract(target->s.origin, ent->s.origin);
      gLevel.intermissionAngle = Vec3_Euler(dir);
    } else {
      G_Debug("%s has invalid target %s\n", etos(ent), ent->target);
    }
  }

  // move all clients to the intermission point
  G_ForEachClient(cl, {
    G_ClientToIntermission(cl);
  });

  // play a dramatic sound effect
  G_MulticastSound(&(const GamePlaySound) {
    .index = gMedia.sounds.roar
  }, MULTICAST_PHS_R);

}

/**
 * @brief The time limit, frag limit, etc.. has been exceeded.
 */
static void G_EndLevel(void) {
  G_BeginIntermission();
}

/**
 * @brief Formats a millisecond time value as a "MM:SS" string, highlighting countdowns.
 * @return A static formatted time string.
 */
char *G_FormatTime(uint32_t time) {
  static char formattedTime[MAX_QPATH];
  static uint32_t lastTime = 0xffffffff;
  const uint32_t m = (time / 1000) / 60;
  const uint32_t s = (time / 1000) % 60;
  char *c;

  // highlight for countdowns
  if (time < (30 * 1000) && time < lastTime && (s & 1)) {
    c = "^2";
  } else {
    c = "^7";
  }

  q_snprintf(formattedTime, sizeof(formattedTime), "%s%2u:%02u", c, m, s);

  lastTime = time;

  return formattedTime;
}

/**
 * @brief Factory for `PMoveParams`, hydrated fresh from the g_* movement cvars at
 * each `Pm_Move` call site. Values are passed through verbatim; `Pm_Move`
 * performs all sanitization (clamping, divide-by-zero guards).
 *
 * Every movement but Quetoo's is defined by its own parameters rather than by
 * this server's cvars: one that drifted with a cvar would not be a movement
 * anyone could set a comparable record under. Gravity is the exception, as the
 * level's when the level sets one; see `G_LevelGravity`.
 */
PMoveParams G_MovementParams(void) {

  const PMovementInfo *movement = Pm_Movement(gLevel.movement);
  PMoveParams params;

  if (movement->params) {
    params = *movement->params;
  } else {
    params = (PMoveParams) {
      .gravity = DEFAULT_GRAVITY,

      .accelGround = g_groundAcceleration->value,
      .accelGroundSlick = g_groundAccelerationSlick->value,
      .accelAir = g_airAcceleration->value,
      .accelWater = g_waterAcceleration->value,
      .accelSpectator = g_spectatorAcceleration->value,
      .accelLadder = g_ladderAcceleration->value,

      .frictionGround = g_groundFriction->value,
      .frictionGroundSlick = g_groundFrictionSlick->value,
      .frictionAir = g_airFriction->value,
      .frictionWater = g_waterFriction->value,
      .frictionSpectator = g_spectatorFriction->value,
      .frictionLadder = g_ladderFriction->value,

      .speedGround = g_groundSpeed->value,
      .speedAir = g_airSpeed->value,
      .speedWater = g_waterSpeed->value,
      .speedLadder = g_ladderSpeed->value,
      .speedSpectator = g_spectatorSpeed->value,
      .speedStop = g_stopSpeed->value,
      .speedJump = g_jumpSpeed->value,
      .speedDucked = g_duckSpeed->value,
      .speedDuckStand = g_duckStandSpeed->value,
      .speedWaterJump = g_waterJumpSpeed->value,

      .bounds = PM_BOUNDS,
      .boundsDucked = PM_CROUCHED_BOUNDS,
      .boundsDead = PM_DEAD_BOUNDS,
    };
  }

  params.movement = gLevel.movement;

  if (gLevel.gravity) {
    params.gravity = gLevel.gravity;
  }

  return params;
}

/**
 * @brief The gravity in effect: the level's, if `g_gravity`, the map's metadata
 * or its worldspawn set one, and otherwise the movement's own.
 */
float G_LevelGravity(void) {

  if (gLevel.gravity) {
    return gLevel.gravity;
  }

  const PMovementInfo *movement = Pm_Movement(gLevel.movement);

  return movement->params ? movement->params->gravity : DEFAULT_GRAVITY;
}

/**
 * @brief Parses `g_movement` over what the level asked for, and coerces the cvar
 * itself to whichever canonical name results.
 * @details A name nothing answers to is warned about and falls back rather than
 * silently running something else, which is the whole reason this coerces: an
 * unknown movement that quietly behaved like Quetoo's would be indistinguishable
 * from a working one.
 */
static PMovement G_CoerceMovement(void) {

  PMovement movement = gMovementLevel;

  if (q_strcmp(g_movement->string, "default")) { // "default" defers to the level
    if (!Pm_MovementByName(g_movement->string, &movement)) {
      G_Warn("Unknown movement \"%s\", using %s\n",
              g_movement->string, Pm_Movement(movement)->name);
    }

    gi.SetCvarString(g_movement->name, Pm_Movement(movement)->name);
  }

  gi.ForceSetCvarString("g_movementMode", Pm_Movement(movement)->name);

  return movement;
}

/**
 * @brief Resolves the movement for a level that asks for `name`, which may be
 * empty. `g_movement` still wins if the admin named one.
 */
PMovement G_ResolveMovement(const char *name) {

  gMovementLevel = G_MOVEMENT_DEFAULT;

  if (name && *name) {
    if (!Pm_MovementByName(name, &gMovementLevel)) {
      G_Warn("Unknown movement \"%s\" in this level, using %s\n",
              name, Pm_Movement(gMovementLevel)->name);
    }
  }

  return G_CoerceMovement();
}

/**
 * @brief Parses `g_gameplay` over what the level asked for, lets the module
 * clamp it to a mode it supports, and coerces the cvar itself to whichever
 * canonical name results. "default" defers to the level, and is left alone.
 */
static GameplayId G_CoerceGameplay(void) {

  GameplayId gameplay = gGameplayLevel;

  if (q_strcmp(g_gameplay->string, "default")) { // "default" defers to the level
    gameplay = G_ClampGameplay(G_GameplayByName(g_gameplay->string)->id);

    gi.SetCvarString(g_gameplay->name, G_GameplayById(gameplay)->name); // reject garbage values
  } else {
    gameplay = G_ClampGameplay(gameplay);
  }

  // g_gameplay holds what the admin asked for, which may be an alias, or "default";
  // publish what it resolved to as well, since that is what a server browser shows
  gi.ForceSetCvarString("g_gameplayMode", G_GameplayById(gameplay)->name);

  return gameplay;
}

/**
 * @brief Resolves the gameplay for a level that asks for `name`, which may be
 * empty. `g_gameplay` still wins if the admin named one.
 */
GameplayId G_ResolveGameplay(const char *name) {

  gGameplayLevel = name && *name ? G_GameplayByName(name)->id : GAMEPLAY_DEATHMATCH;

  return G_CoerceGameplay();
}

/**
 * @brief Inspects and enforces gameplay rules each server frame, including the win
 * condition, time limits and live cvar-driven gameplay changes.
 */
static void G_CheckRules(void) {
  bool restart = false;

  if (gLevel.intermissionTime) {
    return;
  }

  if (editor->value) {
    return;
  }

  if (G_Ai_InDeveloperMode()) {
    return;
  }

  G_RunTimers();

  if (G_CheckWinner()) {
    G_EndLevel();
    return;
  }

  if (g_gameplay->modified) { // change gameplay and teams, fix items, respawn clients

    const GameplayId gameplay = G_CoerceGameplay();

    // SetCvarString above re-marks modified whenever the string actually changed
    // (i.e. whenever we just coerced garbage, or the module clamped it to something
    // else); clear it last so that settles here instead of re-running this whole
    // block again next frame
    g_gameplay->modified = false;

    gLevel.gameplay = gameplay;
    gLevel.teams = (gLevel.gameplay & GAMEPLAY_TEAMS) != 0;

    gi.SetConfigString(CS_GAMEPLAY, va("%d", gLevel.gameplay));

    G_InitNumTeams();

    restart = true;

    gi.BroadcastPrint(PRINT_HIGH, "Gameplay has changed to %s\n", G_GameplayById(gLevel.gameplay)->label);
  }

  if (g_movement->modified) { // change how players move, with no restart

    const PMovement movement = G_CoerceMovement();

    // as above, the coercion re-marks modified whenever it changed the string
    g_movement->modified = false;

    if (movement != gLevel.movement) {
      gLevel.movement = movement;

      // the parameters are hydrated per client per frame and travel inside the
      // player state, so the change reaches everyone without a restart; the one
      // cost is a frame of misprediction, and a taller box has to fit where the
      // player is standing
      gi.BroadcastPrint(PRINT_HIGH, "Movement has changed to %s\n", Pm_Movement(movement)->label);
    }
  }

  if (g_friendlyFire->modified) {
    g_friendlyFire->modified = false;

    gi.SetCvarValue(g_friendlyFire->name, Clampf(g_friendlyFire->value, 0.0, 4.0));

    gi.BroadcastPrint(PRINT_HIGH, "Friendly fire has been changed to %g\n", g_friendlyFire->value);
  }

  if (g_selfDamage->modified) {
    g_selfDamage->modified = false;

    gi.SetCvarValue(g_selfDamage->name, Clampf(g_selfDamage->value, 0.0, 4.0));

    gi.BroadcastPrint(PRINT_HIGH, "Self damage has been changed to %g\n", g_selfDamage->value);
  }

  if (g_selfKnockback->modified) {
    g_selfKnockback->modified = false;

    gi.SetCvarValue(g_selfKnockback->name, Clampf(g_selfKnockback->value, 0.0, 4.0));

    gi.BroadcastPrint(PRINT_HIGH, "Self knockback has been changed to %g\n", g_selfKnockback->value);
  }

  if (g_gravity->modified) { // G_MovementParams() reads gLevel.gravity each move
    g_gravity->modified = false;

    gLevel.gravity = g_gravity->integer;
  }

  if (g_numTeams->modified) { // reset teams, scores, etc
    g_numTeams->modified = false;

    int32_t numTeams;

    if (!q_strcmp(g_numTeams->string, "default")) {
      numTeams = -1; // G_InitNumTeams will pick this up
    } else {
      numTeams = Clampf(g_numTeams->integer, 2, MAX_TEAMS);
    }

    if (gLevel.numTeams != numTeams) {
      gLevel.numTeams = numTeams;

      if (gLevel.teams) {
        G_InitNumTeams();

        gi.BroadcastPrint(PRINT_HIGH, "Number of teams set to %i\n",
                  gLevel.numTeams);

        restart = true;
      }
    }
  }

  if (g_cheats->modified) { // notify when cheats changes
    g_cheats->modified = false;

    gi.BroadcastPrint(PRINT_HIGH, "Cheats have been %s\n", g_cheats->integer ? "enabled" : "disabled");
  }

  if (g_fragLimit->modified) {
    g_fragLimit->modified = false;
    gLevel.fragLimit = g_fragLimit->integer;

    gi.BroadcastPrint(PRINT_HIGH, "Frag limit has been changed to %d\n", gLevel.fragLimit);
  }

  if (g_timeLimit->modified) {
    g_timeLimit->modified = false;
    gLevel.timeLimit = g_timeLimit->value * 60 * 1000;

    gi.BroadcastPrint(PRINT_HIGH, "Time limit has been changed to %3.1f\n", g_timeLimit->value);
  }

  if (g_weaponStay->modified) {
    g_weaponStay->modified = false;

    gi.BroadcastPrint(PRINT_HIGH, "Weapon's Stay has been %s\n", g_weaponStay->integer ? "enabled" : "disabled");

    // respawn all the weapons sitting around
    if (g_weaponStay->integer) {
      G_ForEachEntity(ent, {

        if (!ent->item) {
          continue;
        }

        if (ent->spawnFlags & SF_ITEM_DROPPED) {
          continue;
        }

        if (ent->item->def.type != ITEM_TYPE_WEAPON) {
          continue;
        }

        if (!(ent->svFlags & SVF_NO_CLIENT)) {
          continue;
        }

        if (!ent->Think) {
          continue;
        }

        ent->nextThink = 0;
        ent->Think(ent); // force a respawn
      });
    }
  }

  restart |= G_CheckCvars();

  if (restart) {
    G_RestartGame(true); // reset all clients
  }
}

/**
 * @brief The tail of the `G_FrameWillBegin` chain: a notification, so it does nothing.
 */
static void G_FrameWillBegin_Common(void) {
}

FrameWillBegin G_FrameWillBegin = G_FrameWillBegin_Common;

/**
 * @brief Runs one server frame: the timers, every entity, the rules, and the
 * client frames that close it.
 */
static void G_Frame(void) {

  gLevel.frameNum++;
  gLevel.time = gLevel.frameNum * QUETOO_TICK_MILLIS;

  G_FrameWillBegin();

  // check for level change after running intermission
  if (gLevel.intermissionTime) {
    if (gLevel.time > gLevel.intermissionTime + INTERMISSION && G_AllowNextMap()) {
      gLevel.intermissionTime = 0;

      gi.Cbuf("nextMap\n");

      G_EndClientFrames();
      return;
    }
  }

  // treat each object in turn, even the world gets a chance to think
  G_ForEachEntity(ent, {
    gLevel.currentEntity = ent;

    if (ent->client) {
      G_ClientBeginFrame(ent->client);
    } else {
      G_RunEntity(ent);
    }

    gLevel.currentEntity = NULL;
  });

  // let the AI think
  G_Ai_Frame();

  // inspect and enforce gameplay rules
  G_CheckRules();

  // build the PlayerState structures for all players
  G_EndClientFrames();
}

/**
 * @brief Returns the game name advertised by the server in info strings.
 */
static const char *G_GameName(void) {
  static char name[64];
  const size_t size = sizeof(name);

  q_strlcpy(name, G_GameplayById(gLevel.gameplay)->label, size);

  G_FormatGameName(name, size);

  return name;
}

/**
 * @brief Restart the game.
 */
static void G_Restart_f(void) {
  G_RestartGame(false);
}

/**
 * @brief Set up the `CS_NUM_TEAMS` configstring to the number of valid teams we have
 */
void G_InitNumTeams(void) {

  if (gLevel.numTeams == -1) { // set to default, so let's set number of teams
    gLevel.numTeams = 0;

    for (int32_t t = 0; t < MAX_TEAMS; t++) {

      if (!gTeamList[t].spawnPoints.count) {
        break;
      }

      gLevel.numTeams++;
    }

    gLevel.numTeams = Clampf(gLevel.numTeams, 2, MAX_TEAMS);
  }

  gi.SetConfigString(CS_NUM_TEAMS, va("%d", gLevel.teams ? gLevel.numTeams : 0));
}

/**
 * @brief This will be called when the game module is first loaded.
 */
void G_Init(void) {

#if defined(G_HOOK)
  G_Hook_Init();
#endif

#if defined(G_TECH)
  G_Tech_Init();
#endif

#if defined(G_CTF)
  G_Ctf_Init();
#endif

  G_Vote_Init();
  G_Intermission_Init();

  G_Module_Init();

  ge.ClipEntity = G_ClipEntity;

  for (int32_t i = 0; i < sv_maxClients->integer; i++) {
    ge.clients[i] = gi.Malloc(sizeof(GameClient), MEM_TAG_GAME);
    ge.clients[i]->ps.client = i;
  }

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {
    ge.entities[i] = gi.Malloc(sizeof(GameEntity), MEM_TAG_GAME);
    ge.entities[i]->s.number = i;
  }

  gi.Print("Game module initialization...\n");

  const char *s = va("%s %s", BUILD, VERSION);
  Cvar *gameVersion = gi.AddCvar("gameVersion", s, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

  gi.Print("  Version:    ^2%s^7\n", gameVersion->string);

  gi.AddCvar("gameName", GAME_NAME, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);
  gi.AddCvar("gameDate", __DATE__, CVAR_SERVER_INFO | CVAR_NO_SET, NULL);

  g_adminPassword = gi.AddCvar("g_adminPassword", "", CVAR_LATCH, "Password to authenticate as an admin.");
  g_ammoRespawnTime = gi.AddCvar("g_ammoRespawnTime", "20.0", CVAR_SERVER_INFO, "Ammo respawn interval in seconds.");
  g_autoJoin = gi.AddCvar("g_autoJoin", "1", CVAR_SERVER_INFO, "Automatically assigns players to teams.");
  g_balanceArmorShardRespawn = gi.AddCvar("g_balanceArmorShardRespawn", "15", 0, NULL);
  g_balanceArmorJacketRespawn = gi.AddCvar("g_balanceArmorJacketRespawn", "25", 0, NULL);
  g_balanceArmorCombatRespawn = gi.AddCvar("g_balanceArmorCombatRespawn", "25", 0, NULL);
  g_balanceArmorBodyRespawn = gi.AddCvar("g_balanceArmorBodyRespawn", "30", 0, NULL);
  g_balanceBfgDamage = gi.AddCvar("g_balanceBfgDamage", "180", 0, NULL);
  g_balanceBfgKnockback = gi.AddCvar("g_balanceBfgKnockback", "140", 0, NULL);
  g_balanceBfgPrefire = gi.AddCvar("g_balanceBfgPrefire", "1", 0, "The prefire warmup delay for the BFG10K in seconds.");
  g_balanceBfgRadius = gi.AddCvar("g_balanceBfgRadius", "512", 0, NULL);
  g_balanceBfgRefire = gi.AddCvar("g_balanceBfgRefire", "2", 0, NULL);
  g_balanceBfgSpeed = gi.AddCvar("g_balanceBfgSpeed", "720", 0, NULL);
  g_balanceBlasterDamage = gi.AddCvar("g_balanceBlasterDamage", "15", 0, NULL);
  g_balanceBlasterKnockback = gi.AddCvar("g_balanceBlasterKnockback", "2", 0, NULL);
  g_balanceBlasterRefire = gi.AddCvar("g_balanceBlasterRefire", "0.45", 0, NULL);
  g_balanceBlasterSpeed = gi.AddCvar("g_balanceBlasterSpeed", "2000", 0, NULL);
  g_balanceHandgrenadeRefire = gi.AddCvar("g_balanceHandgrenadeRefire", "2", 0, NULL);
  g_balanceHealthSmallRespawn = gi.AddCvar("g_balanceHealthSmallRespawn", "15", 0, NULL);
  g_balanceHealthMediumRespawn = gi.AddCvar("g_balanceHealthMediumRespawn", "20", 0, NULL);
  g_balanceHealthLargeRespawn = gi.AddCvar("g_balanceHealthLargeRespawn", "30", 0, NULL);
  g_balanceHealthMegaRespawn = gi.AddCvar("g_balanceHealthMegaRespawn", "60", 0, NULL);
  g_balanceHyperblasterClimbDamage = gi.AddCvar("g_balanceHyperblasterClimbDamage", "3", 0, NULL);
  g_balanceHyperblasterClimbKnockback = gi.AddCvar("g_balanceHyperblasterClimbKnockback", "68", 0, NULL);
  g_balanceHyperblasterDamage = gi.AddCvar("g_balanceHyperblasterDamage", "16", 0, NULL);
  g_balanceHyperblasterKnockback = gi.AddCvar("g_balanceHyperblasterKnockback", "4", 0, NULL);
  g_balanceHyperblasterRefire = gi.AddCvar("g_balanceHyperblasterRefire", "0.1", 0, NULL);
  g_balanceHyperblasterSpeed = gi.AddCvar("g_balanceHyperblasterSpeed", "1800", 0, NULL);
  g_balanceLightningDamage = gi.AddCvar("g_balanceLightningDamage", "12", 0, NULL);
  g_balanceLightningKnockback = gi.AddCvar("g_balanceLightningKnockback", "6", 0, NULL);
  g_balanceLightningLength = gi.AddCvar("g_balanceLightningLength", "600", 0, NULL);
  g_balanceLightningRefire = gi.AddCvar("g_balanceLightningRefire", "0.1", 0, NULL);
  g_balanceMachinegunDamage = gi.AddCvar("g_balanceMachinegunDamage", "8", 0, NULL);
  g_balanceMachinegunKnockback = gi.AddCvar("g_balanceMachinegunKnockback", "2", 0, NULL);
  g_balanceMachinegunRefire = gi.AddCvar("g_balanceMachinegunRefire", "0.1", 0, NULL);
  g_balanceMachinegunSpreadX = gi.AddCvar("g_balanceMachinegunSpreadX", "20", 0, NULL);
  g_balanceMachinegunSpreadY = gi.AddCvar("g_balanceMachinegunSpreadY", "200", 0, NULL);
  g_balanceGrenadelauncherDamage = gi.AddCvar("g_balanceGrenadelauncherDamage", "120", 0, NULL);
  g_balanceGrenadelauncherKnockback = gi.AddCvar("g_balanceGrenadelauncherKnockback", "120", 0, NULL);
  g_balanceGrenadelauncherRadius = gi.AddCvar("g_balanceGrenadelauncherRadius", "185", 0, NULL);
  g_balanceGrenadelauncherRefire = gi.AddCvar("g_balanceGrenadelauncherRefire", "1", 0, NULL);
  g_balanceGrenadelauncherSpeed = gi.AddCvar("g_balanceGrenadelauncherSpeed", "800", 0, NULL);
  g_balanceGrenadelauncherTimer = gi.AddCvar("g_balanceGrenadelauncherTimer", "2.5", 0, NULL);
  g_balanceQuadDamageRespawnTime = gi.AddCvar("g_balanceQuadDamageRespawnTime", "60", 0, NULL);
  g_balanceQuadDamageTime = gi.AddCvar("g_balanceQuadDamageTime", "30", 0, NULL);
  g_balanceQuakeShotgunDamage = gi.AddCvar("g_balanceQuakeShotgunDamage", "4", 0, NULL);
  g_balanceQuakeShotgunKnockback = gi.AddCvar("g_balanceQuakeShotgunKnockback", "2", 0, NULL);
  g_balanceQuakeShotgunPellets = gi.AddCvar("g_balanceQuakeShotgunPellets", "6", 0, NULL);
  g_balanceQuakeShotgunRefire = gi.AddCvar("g_balanceQuakeShotgunRefire", "0.5", 0, NULL);
  g_balanceQuakeShotgunSpreadX = gi.AddCvar("g_balanceQuakeShotgunSpreadX", "500", 0, NULL);
  g_balanceQuakeShotgunSpreadY = gi.AddCvar("g_balanceQuakeShotgunSpreadY", "400", 0, NULL);
  g_balanceQuakeSupershotgunDamage = gi.AddCvar("g_balanceQuakeSupershotgunDamage", "4", 0, NULL);
  g_balanceQuakeSupershotgunKnockback = gi.AddCvar("g_balanceQuakeSupershotgunKnockback", "2", 0, NULL);
  g_balanceQuakeSupershotgunPellets = gi.AddCvar("g_balanceQuakeSupershotgunPellets", "14", 0, NULL);
  g_balanceQuakeSupershotgunRefire = gi.AddCvar("g_balanceQuakeSupershotgunRefire", "0.7", 0, NULL);
  g_balanceQuakeSupershotgunSpreadX = gi.AddCvar("g_balanceQuakeSupershotgunSpreadX", "1000", 0, NULL);
  g_balanceQuakeSupershotgunSpreadY = gi.AddCvar("g_balanceQuakeSupershotgunSpreadY", "800", 0, NULL);
  g_balanceQuakeNailgunDamage = gi.AddCvar("g_balanceQuakeNailgunDamage", "9", 0, NULL);
  g_balanceQuakeNailgunKnockback = gi.AddCvar("g_balanceQuakeNailgunKnockback", "4", 0, NULL);
  g_balanceQuakeNailgunRefire = gi.AddCvar("g_balanceQuakeNailgunRefire", "0.1", 0, NULL);
  g_balanceQuakeNailgunSpeed = gi.AddCvar("g_balanceQuakeNailgunSpeed", "1400", 0, NULL);
  g_balanceQuakeSupernailgunDamage = gi.AddCvar("g_balanceQuakeSupernailgunDamage", "18", 0, NULL);
  g_balanceQuakeSupernailgunKnockback = gi.AddCvar("g_balanceQuakeSupernailgunKnockback", "8", 0, NULL);
  g_balanceQuakeSupernailgunRefire = gi.AddCvar("g_balanceQuakeSupernailgunRefire", "0.1", 0, NULL);
  g_balanceQuakeSupernailgunSpeed = gi.AddCvar("g_balanceQuakeSupernailgunSpeed", "2000", 0, NULL);
  g_balanceQuakeGrenadelauncherDamage = gi.AddCvar("g_balanceQuakeGrenadelauncherDamage", "120", 0, NULL);
  g_balanceQuakeGrenadelauncherKnockback = gi.AddCvar("g_balanceQuakeGrenadelauncherKnockback", "120", 0, NULL);
  g_balanceQuakeGrenadelauncherRadius = gi.AddCvar("g_balanceQuakeGrenadelauncherRadius", "185", 0, NULL);
  g_balanceQuakeGrenadelauncherRefire = gi.AddCvar("g_balanceQuakeGrenadelauncherRefire", "0.8", 0, NULL);
  g_balanceQuakeGrenadelauncherSpeed = gi.AddCvar("g_balanceQuakeGrenadelauncherSpeed", "700", 0, NULL);
  g_balanceQuakeGrenadelauncherTimer = gi.AddCvar("g_balanceQuakeGrenadelauncherTimer", "2.5", 0, NULL);
  g_balanceQuakeRocketlauncherDamage = gi.AddCvar("g_balanceQuakeRocketlauncherDamage", "100", 0, NULL);
  g_balanceQuakeRocketlauncherKnockback = gi.AddCvar("g_balanceQuakeRocketlauncherKnockback", "75", 0, NULL);
  g_balanceQuakeRocketlauncherRadius = gi.AddCvar("g_balanceQuakeRocketlauncherRadius", "150", 0, NULL);
  g_balanceQuakeRocketlauncherRefire = gi.AddCvar("g_balanceQuakeRocketlauncherRefire", "0.8", 0, NULL);
  g_balanceQuakeRocketlauncherSpeed = gi.AddCvar("g_balanceQuakeRocketlauncherSpeed", "1000", 0, NULL);
  g_balanceQuakeThunderboltDamage = gi.AddCvar("g_balanceQuakeThunderboltDamage", "14", 0, NULL);
  g_balanceQuakeThunderboltKnockback = gi.AddCvar("g_balanceQuakeThunderboltKnockback", "8", 0, NULL);
  g_balanceQuakeThunderboltLength = gi.AddCvar("g_balanceQuakeThunderboltLength", "768", 0, NULL);
  g_balanceQuakeThunderboltRefire = gi.AddCvar("g_balanceQuakeThunderboltRefire", "0.05", 0, NULL);
  g_balanceInvisibilityRespawnTime = gi.AddCvar("g_balanceInvisibilityRespawnTime", "60", 0, NULL);
  g_balanceInvisibilityTime = gi.AddCvar("g_balanceInvisibilityTime", "30", 0, NULL);
  g_balanceInvulnerabilityRespawnTime = gi.AddCvar("g_balanceInvulnerabilityRespawnTime", "60", 0, NULL);
  g_balanceInvulnerabilityTime = gi.AddCvar("g_balanceInvulnerabilityTime", "30", 0, NULL);
  g_balanceRailgunDamage = gi.AddCvar("g_balanceRailgunDamage", "100", 0, NULL);
  g_balanceRailgunKnockback = gi.AddCvar("g_balanceRailgunKnockback", "80", 0, NULL);
  g_balanceRailgunRefire = gi.AddCvar("g_balanceRailgunRefire", "1.4", 0, NULL);
  g_balanceRocketlauncherDamage = gi.AddCvar("g_balanceRocketlauncherDamage", "100", 0, NULL);
  g_balanceRocketlauncherKnockback = gi.AddCvar("g_balanceRocketlauncherKnockback", "75", 0, NULL);
  g_balanceRocketlauncherRadius = gi.AddCvar("g_balanceRocketlauncherRadius", "150", 0, NULL);
  g_balanceRocketlauncherRefire = gi.AddCvar("g_balanceRocketlauncherRefire", "1", 0, NULL);
  g_balanceRocketlauncherSpeed = gi.AddCvar("g_balanceRocketlauncherSpeed", "1000", 0, NULL);
  g_balanceShotgunDamage = gi.AddCvar("g_balanceShotgunDamage", "4", 0, NULL);
  g_balanceShotgunKnockback = gi.AddCvar("g_balanceShotgunKnockback", "2", 0, NULL);
  g_balanceShotgunPellets = gi.AddCvar("g_balanceShotgunPellets", "12", 0, NULL);
  g_balanceShotgunRefire = gi.AddCvar("g_balanceShotgunRefire", "0.6", 0, NULL);
  g_balanceShotgunSpreadX = gi.AddCvar("g_balanceShotgunSpreadX", "700", 0, NULL);
  g_balanceShotgunSpreadY = gi.AddCvar("g_balanceShotgunSpreadY", "300", 0, NULL);
  g_balanceSupershotgunDamage = gi.AddCvar("g_balanceSupershotgunDamage", "4", 0, NULL);
  g_balanceSupershotgunKnockback = gi.AddCvar("g_balanceSupershotgunKnockback", "2", 0, NULL);
  g_balanceSupershotgunPellets = gi.AddCvar("g_balanceSupershotgunPellets", "24", 0, NULL);
  g_balanceSupershotgunRefire = gi.AddCvar("g_balanceSupershotgunRefire", "0.8", 0, NULL);
  g_balanceSupershotgunSpreadX = gi.AddCvar("g_balanceSupershotgunSpreadX", "1600", 0, NULL);
  g_balanceSupershotgunSpreadY = gi.AddCvar("g_balanceSupershotgunSpreadY", "500", 0, NULL);
  g_cheats = gi.AddCvar("g_cheats",
#if _DEBUG
    "1"
#else
    "0"
#endif
    , CVAR_SERVER_INFO, NULL);
  g_fragLimit = gi.AddCvar("g_fragLimit", "30", CVAR_SERVER_INFO, "The frag limit per level.");
  g_friendlyFire = gi.AddCvar("g_friendlyFire", "1", CVAR_SERVER_INFO, "Factor of how much damage can be dealt to teammates.");
  g_gameplay = gi.AddCvar("g_gameplay", "default", CVAR_SERVER_INFO,
    "Selects deathmatch, instagib or arena combat. Prefix with team_ for team play, "
    "e.g. team_deathmatch, team_instagib or team_arena.");
  gi.AddCvar("g_gameplayMode", "", CVAR_SERVER_INFO | CVAR_NO_SET,
    "The gameplay mode this level actually resolved to, published for the server browser. "
    "Read g_gameplay for what was requested.");
  gi.AddCvar("g_movementMode", "", CVAR_SERVER_INFO | CVAR_NO_SET,
    "The player movement this level actually resolved to, published for the server browser. "
    "Read g_movement for what was requested.");

  // player movement parameters (hydrated into PMoveParams by G_MovementParams)
  g_airAcceleration = gi.AddCvar("g_airAcceleration", "2.0", 0, "Acceleration applied while airborne. Default 2.0; set 0 for classic-Quake2 movement.");
  g_airFriction = gi.AddCvar("g_airFriction", "0.125", 0, "Friction applied while airborne. Default 0.125; set 0 to remove air drag.");
  g_airSpeed = gi.AddCvar("g_airSpeed", "350", 0, "Wish-speed cap while airborne. Default 350.");
  g_duckSpeed = gi.AddCvar("g_duckSpeed", "140.0", 0, "Maximum ground speed while ducked. Default 140.0.");
  g_duckStandSpeed = gi.AddCvar("g_duckStandSpeed", "200.0", 0, "Rate the view rises/falls when standing/ducking. Default 200.0.");
  g_gravity = gi.AddCvar("g_gravity", "800", CVAR_SERVER_INFO, NULL);
  g_movement = gi.AddCvar("g_movement", "default", CVAR_SERVER_INFO, "The player movement to run: \"default\" defers to the level, otherwise a movement name such as \"quetoo\" or \"quake\".");
  g_groundAcceleration = gi.AddCvar("g_groundAcceleration", "10.0", 0, "Ground acceleration. Default 10.0.");
  g_groundAccelerationSlick = gi.AddCvar("g_groundAccelerationSlick", "4.375", 0, "Ground acceleration on slick surfaces. Default 4.375.");
  g_groundFriction = gi.AddCvar("g_groundFriction", "6.0", 0, "Ground friction. Default 6.0.");
  g_groundFrictionSlick = gi.AddCvar("g_groundFrictionSlick", "2.0", 0, "Ground friction on slick surfaces. Default 2.0.");
  g_groundSpeed = gi.AddCvar("g_groundSpeed", "300.0", 0, "Maximum ground running speed. Default 300.0.");
  g_jumpSpeed = gi.AddCvar("g_jumpSpeed", "270.0", 0, "Upward velocity imparted by a jump. Default 270.0.");
  g_ladderAcceleration = gi.AddCvar("g_ladderAcceleration", "16.0", 0, "Acceleration while on ladders. Default 16.0.");
  g_ladderFriction = gi.AddCvar("g_ladderFriction", "5.0", 0, "Friction while on ladders. Default 5.0.");
  g_ladderSpeed = gi.AddCvar("g_ladderSpeed", "125.0", 0, "Maximum ladder-climbing speed. Default 125.0.");
  g_spectatorAcceleration = gi.AddCvar("g_spectatorAcceleration", "2.5", 0, "Spectator free-fly acceleration. Default 2.5.");
  g_spectatorFriction = gi.AddCvar("g_spectatorFriction", "2.5", 0, "Spectator free-fly friction. Default 2.5.");
  g_spectatorSpeed = gi.AddCvar("g_spectatorSpeed", "500.0", 0, "Maximum spectator free-fly speed. Default 500.0.");
  g_stopSpeed = gi.AddCvar("g_stopSpeed", "100.0", 0, "Speed below which friction is amplified to stop the player. Default 100.0.");
  g_waterAcceleration = gi.AddCvar("g_waterAcceleration", "3.0", 0, "Acceleration applied underwater. Default 3.0.");
  g_waterFriction = gi.AddCvar("g_waterFriction", "2.0", 0, "Friction applied underwater. Default 2.0.");
  g_waterJumpSpeed = gi.AddCvar("g_waterJumpSpeed", "420.0", 0, "Upward velocity when jumping out of water. Default 420.0.");
  g_waterSpeed = gi.AddCvar("g_waterSpeed", "140.0", 0, "Maximum swimming speed. Default 140.0.");

  g_deathCam = gi.AddCvar("g_deathCam", "1", CVAR_SERVER_INFO, "Detach the view and watch your corpse after certain deaths (0 off, 1 selected MODs, 2 always).");
  g_deathCamDistance = gi.AddCvar("g_deathCamDistance", "120", 0, "Distance the death camera settles behind the point of death.");
  g_deathCamHeight = gi.AddCvar("g_deathCamHeight", "80", 0, "Height the death camera settles above the point of death.");
  g_deathCamRise = gi.AddCvar("g_deathCamRise", "90", 0, "Initial upward speed of the death camera, in units per second.");
  g_deathCamTime = gi.AddCvar("g_deathCamTime", "800", 0, "Time in milliseconds for the death camera to settle.");
  g_deathCamVelocity = gi.AddCvar("g_deathCamVelocity", "0.25", 0, "Fraction of the player's velocity inherited by the death camera.");
  g_numTeams = gi.AddCvar("g_numTeams", "default", CVAR_SERVER_INFO, "The number of teams allowed. By default, picks the valid amount for the map, or 2.");
  g_motd = gi.AddCvar("g_motd", "", CVAR_SERVER_INFO, "Message of the day, shown to clients on initial connect.");
  g_password = gi.AddCvar("g_password", "", CVAR_USER_INFO, "The server password.");
  g_playerProjectile = gi.AddCvar("g_playerProjectile", "1", CVAR_SERVER_INFO, "Scales player velocity to projectiles.");
  g_respawnProtection = gi.AddCvar("g_respawnProtection", "0", 0, "Respawn protection in seconds.");
  g_fallDamage = gi.AddCvar("g_fallDamage", "1", CVAR_SERVER_INFO, "Scales fall damage. 0 disables fall damage entirely (cf. Quake2 DF_NO_FALLING).");
  g_selfDamage = gi.AddCvar("g_selfDamage", "1", CVAR_SERVER_INFO, "Scales self-inflicted damage (rocket splash, grenade splash, etc)");
  g_selfKnockback = gi.AddCvar("g_selfKnockback", "1", CVAR_SERVER_INFO, "Scales self-inflicted knockback (rocket jump, plasma climb, etc)");
  g_showAttackerStats = gi.AddCvar("g_showAttackerStats", "0", CVAR_SERVER_INFO, "Allows can see their attackers' health and armor when they die.");
  g_spawnFarthest = gi.AddCvar("g_spawnFarthest", "1", CVAR_SERVER_INFO, NULL);
  g_spectatorChat = gi.AddCvar("g_spectatorChat", "1", CVAR_SERVER_INFO, "If enabled, spectators can only talk to other spectators.");
  g_timeLimit = gi.AddCvar("g_timeLimit", "20", CVAR_SERVER_INFO, "The time limit per level in minutes.");
  g_weaponRespawnTime = gi.AddCvar("g_weaponRespawnTime", "5", CVAR_SERVER_INFO, "Weapon respawn interval in seconds.");
  g_weaponStay = gi.AddCvar("g_weaponStay", "0", CVAR_SERVER_INFO, "If enabled, weapons will remain when picked up rather than respawn with delay.");

  G_Ai_Init();

  // fix the cvar string itself before anything runs, so e.g. a ctf server
  // started with "+set g_gameplay arena" advertises "team_deathmatch" from the
  // first frame rather than whatever garbage/unsupported value it was started
  // with; the level's own gameplay resolution (G_worldspawn) is unaffected
  G_CoerceGameplay();

  // set these to false to avoid spurious game restarts and alerts on init
      g_cheats->modified =
      g_fragLimit->modified =
      g_friendlyFire->modified =
      g_gameplay->modified =
      g_numTeams->modified =
      g_selfDamage->modified =
      g_selfKnockback->modified =
      g_timeLimit->modified =
      g_weaponStay->modified = false;

  // add game-specific server console commands
  gi.AddCmd("mute", G_Mute_f, CMD_GAME, "Prevent a client from talking");
  gi.AddCmd("unmute", G_Mute_f, CMD_GAME, "Allow a muted client to talk again");
  gi.AddCmd("restart", G_Restart_f, CMD_GAME, "Force the game to restart");

  gi.Print("Game module initialized\n");
}

/**
 * @brief Shuts down the game module. This is called when the game is unloaded
 * (complements `G_Init`).
 */
void G_Shutdown(void) {

  gi.Print("Game module shutdown...\n");

  G_Module_Shutdown();

  G_Ai_Shutdown();

  gLevel.frags = release(gLevel.frags);

#if defined(G_CTF)
  gLevel.captures = release(gLevel.captures);
#endif

  gi.FreeTag(MEM_TAG_GAME_LEVEL);
  gi.FreeTag(MEM_TAG_GAME);
}

/**
 * @brief Handles clock, countdown, and timeout timers for the current level.
 */
void G_RunTimers(void) {
  uint32_t time = gLevel.time;

  if (gLevel.timeLimit) { // check timeLimit
    if (time >= (uint32_t) gLevel.timeLimit) {
      gi.BroadcastPrint(PRINT_HIGH, "Time limit hit\n");
      G_EndLevel();
      return;
    }
    time = gLevel.timeLimit - gLevel.time; // count down
  }

  if (gLevel.frameNum % QUETOO_TICK_RATE == 0) { // send time updates once per second
    gi.SetConfigString(CS_TIME, G_FormatTime(time));
  }
}

/**
 * @brief This is the entry point responsible for aligning the server and game module.
 * The server resolves this symbol upon successfully loading the game library,
 * and invokes it. We're responsible for copying the import structure so that
 * we can call back into the server, and returning a populated game export
 * structure.
 */
GameExport *G_LoadGame(GameImport *import) {

  gi = *import;

  sv_minClients = gi.GetCvar("sv_minClients");
  sv_maxClients = gi.GetCvar("sv_maxClients");
  sv_maxEntities = gi.GetCvar("sv_maxEntities");
  sv_hostname = gi.GetCvar("sv_hostname");

  dedicated = gi.GetCvar("dedicated");
  editor = gi.GetCvar("editor");

  memset(&ge, 0, sizeof(ge));

  ge.apiVersion = GAME_API_VERSION;
  ge.protocol = PROTOCOL_MINOR;
  ge.cgame = GAME_NAME;

  ge.Init = G_Init;
  ge.Shutdown = G_Shutdown;
  ge.SpawnEntities = G_SpawnEntities;
  ge.SpawnEditorEntity = G_SpawnEditorEntity;
  ge.FreeEditorEntity = G_FreeEditorEntity;
  ge.ClientThink = G_ClientThink;
  ge.ClientConnect = G_ClientConnect;
  ge.ClientUserInfoChanged = G_ClientUserInfoChanged;
  ge.ClientDisconnect = G_ClientDisconnect;
  ge.ClientBegin = G_ClientBegin;
  ge.ClientCanHearVoice = G_ClientCanHearVoice;
  ge.ClientCommand = G_ClientCommand;

  ge.Frame = G_Frame;

  ge.GameName = G_GameName;

  return &ge;
}
