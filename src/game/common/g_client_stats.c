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

_Static_assert(MAX_CLIENTS <= 64, "GameWeaponStats::hitClients must have a bit for each client");

/**
 * @brief The weapon that fires a means of death, or 0 if no player weapon fires it.
 */
static GameItemTag G_WeaponTagForMod(GameMeansOfDeath mod) {

  switch (mod & ~MOD_FRIENDLY_FIRE) {
    case MOD_BLASTER:
      return WEAPON_BLASTER;
    case MOD_SHOTGUN:
      return WEAPON_SHOTGUN;
    case MOD_SUPER_SHOTGUN:
      return WEAPON_SUPER_SHOTGUN;
    case MOD_MACHINEGUN:
      return WEAPON_MACHINEGUN;
    case MOD_HANDGRENADE:
    case MOD_HANDGRENADE_SPLASH:
    case MOD_HANDGRENADE_SUICIDE:
    case MOD_HANDGRENADE_KAMIKAZE:
      return WEAPON_HAND_GRENADE;
    case MOD_GRENADE:
    case MOD_GRENADE_SPLASH:
      return WEAPON_GRENADE_LAUNCHER;
    case MOD_ROCKET:
    case MOD_ROCKET_SPLASH:
      return WEAPON_ROCKET_LAUNCHER;
    case MOD_HYPERBLASTER:
    case MOD_HYPERBLASTER_CLIMB:
      return WEAPON_HYPERBLASTER;
    case MOD_LIGHTNING:
    case MOD_LIGHTNING_DISCHARGE:
      return WEAPON_LIGHTNING;
    case MOD_RAILGUN:
      return WEAPON_RAILGUN;
    case MOD_BFG_LASER:
    case MOD_BFG_BLAST:
      return WEAPON_BFG10K;
    case MOD_QUAKE_SHOTGUN:
      return WEAPON_QUAKE_SHOTGUN;
    case MOD_QUAKE_SUPER_SHOTGUN:
      return WEAPON_QUAKE_SUPER_SHOTGUN;
    case MOD_QUAKE_NAILGUN:
      return WEAPON_QUAKE_NAILGUN;
    case MOD_QUAKE_SUPER_NAILGUN:
      return WEAPON_QUAKE_SUPER_NAILGUN;
    case MOD_QUAKE_GRENADE:
    case MOD_QUAKE_GRENADE_SPLASH:
      return WEAPON_QUAKE_GRENADE_LAUNCHER;
    case MOD_QUAKE_ROCKET:
    case MOD_QUAKE_ROCKET_SPLASH:
      return WEAPON_QUAKE_ROCKET_LAUNCHER;
    case MOD_QUAKE_THUNDERBOLT:
    case MOD_QUAKE_THUNDERBOLT_DISCHARGE:
      return WEAPON_QUAKE_THUNDERBOLT;
    default:
      return 0;
  }
}

/**
 * @brief The client's stats for the weapon with the given tag, or `NULL` if the tag is not a weapon.
 */
static GameWeaponStats *G_WeaponStats(GameClient *cl, GameItemTag tag) {

  if (tag < WEAPON_FIRST || tag >= WEAPON_LAST) {
    return NULL;
  }

  return &cl->persistent.weaponStats[tag - WEAPON_FIRST];
}

/**
 * @brief True if the attacker is a client, and the target is a client on another team.
 */
static bool G_IsEnemy(const GameEntity *attacker, const GameEntity *target) {

  if (!attacker->client || !target->client || attacker == target) {
    return false;
  }

  return !G_OnSameTeam(attacker->client, target->client);
}

/**
 * @brief Clears the client's match stats.
 */
void G_ClientResetStats(GameClient *cl) {

  memset(cl->persistent.weaponStats, 0, sizeof(cl->persistent.weaponStats));

  cl->persistent.damageDealt = 0;
  cl->persistent.damageTaken = 0;
}

/**
 * @brief Counts a shot of the client's current weapon.
 */
void G_ClientShotStats(GameClient *cl) {

  GameWeaponStats *stats = G_WeaponStats(cl, cl->weapon ? cl->weapon->def.tag : 0);
  if (stats) {
    stats->shots++;
  }
}

/**
 * @brief Counts damage to a living client: taken by the target, and dealt and hit by an enemy attacker.
 * @remarks A weapon counts one hit for each enemy that it damages in a frame, so that the pellets
 * of one blast or the splash of one rocket count once for each enemy they reach. The BFG's laser
 * damages every enemy near the ball on every frame of its flight, so it counts no hits.
 */
void G_ClientDamageStats(const GameEntity *attacker, const GameEntity *target, int32_t damage, GameMeansOfDeath mod) {

  target->client->persistent.damageTaken += damage;

  if (!G_IsEnemy(attacker, target)) {
    return;
  }

  attacker->client->persistent.damageDealt += damage;

  GameWeaponStats *stats = G_WeaponStats(attacker->client, G_WeaponTagForMod(mod));
  if (stats == NULL) {
    return;
  }

  stats->damage += damage;

  if (damage <= 0 || (mod & ~MOD_FRIENDLY_FIRE) == MOD_BFG_LASER) {
    return;
  }

  if (stats->hitFrame != gameLevel.frameNum) {
    stats->hitFrame = gameLevel.frameNum;
    stats->hitClients = 0;
  }

  const uint64_t bit = 1ull << target->client->ps.client;
  if (!(stats->hitClients & bit)) {
    stats->hitClients |= bit;
    stats->hits++;
  }
}

/**
 * @brief Counts a kill by an enemy attacker for the weapon that made it.
 */
void G_ClientKillStats(const GameEntity *attacker, const GameEntity *target, GameMeansOfDeath mod) {

  if (!G_IsEnemy(attacker, target)) {
    return;
  }

  GameWeaponStats *stats = G_WeaponStats(attacker->client, G_WeaponTagForMod(mod));
  if (stats) {
    stats->kills++;
  }
}

/**
 * @brief Sends the client their damage totals, and the stats of each weapon that they used.
 * @remarks This goes with the scores through the intermission, so that a client game reloaded
 * by `r_restart` gets them again.
 */
static void G_ClientMatchStats(GameClient *cl) {

  if (cl->ai) {
    return;
  }

  uint8_t count = 0;
  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    const GameWeaponStats *stats = &cl->persistent.weaponStats[i];
    if (stats->shots || stats->damage || stats->kills) {
      count++;
    }
  }

  gi.WriteByte(SV_CMD_MATCH_STATS);
  gi.WriteLong((int32_t) cl->persistent.damageDealt);
  gi.WriteLong((int32_t) cl->persistent.damageTaken);
  gi.WriteByte(count);

  for (int32_t i = 0; i < WEAPON_TOTAL; i++) {
    const GameWeaponStats *stats = &cl->persistent.weaponStats[i];
    if (stats->shots || stats->damage || stats->kills) {
      gi.WriteByte(WEAPON_FIRST + i);
      gi.WriteLong((int32_t) stats->shots);
      gi.WriteLong((int32_t) stats->hits);
      gi.WriteLong((int32_t) stats->damage);
      gi.WriteShort((int32_t) Mini(stats->kills, INT16_MAX));
    }
  }

  gi.Unicast(cl, true);
}

/**
 * @brief Moves a client to the intermission position and freezes their input.
 */
void G_ClientToIntermission(GameClient *cl) {

  if (!cl->entity) {
    return;
  }

  cl->entity->s.origin = gameLevel.intermission.origin;
  cl->ps.pmState.origin = gameLevel.intermission.origin;

  cl->ps.pmState.viewAngles = Vec3_Zero();
  cl->ps.pmState.deltaAngles = gameLevel.intermission.angles;

  cl->ps.pmState.viewOffset = Vec3_Zero();
  cl->ps.pmState.stepOffset = 0.f;

  cl->ps.pmState.flags &= ~PMF_DEATH_CAM;
  cl->ps.pmState.type = PM_FREEZE;

  cl->entity->s.model1 = 0;
  cl->entity->s.model2 = 0;
  cl->entity->s.model3 = 0;
  cl->entity->s.model4 = 0;
  cl->entity->s.effects = 0;
  cl->entity->s.sound = 0;
  cl->entity->solid = SOLID_NOT;
  cl->entity->dead = true;

  // show scores
  cl->showScores = true;

  // hide the HUD
  memset(cl->inventory, 0, sizeof(cl->inventory));
  cl->weapon = NULL;

  cl->ammoIndex = 0;
  cl->pickupMsgTime = 0;
}

/**
 * @brief Write the scores information for the specified client.
 */
static void G_UpdateScore(const GameClient *cl, GameScore *s) {

  memset(s, 0, sizeof(*s));

  s->client = cl->ps.client;
  s->ping = cl->ping < 999 ? cl->ping : 999;

  if (cl->persistent.spectator) {
    s->color = -1;
    s->flags |= SCORE_SPECTATOR;
  } else {
#if defined(G_CTF)
    const GameItem *flag = G_GetFlag(cl);
    if (flag) {
      s->flags |= SCORE_CTF_FLAG | SCORE_CTF_FLAG_TEAM(flag->def.tag - FLAG_FIRST);
    }
#endif
    if (cl->persistent.team) {
      s->color = cl->persistent.team->color;
      s->team = cl->persistent.team->id + 1;
    } else {
      s->color = cl->persistent.color;
    }
  }

  s->frags = cl->persistent.frags;
  s->score = cl->persistent.score;
  s->deaths = cl->persistent.deaths;
#if defined(G_CTF)
  s->captures = cl->persistent.captures;
  s->assists = cl->persistent.assists;
#endif

  G_WriteScore(cl, s);
}

/**
 * @brief The tail of the `G_WriteScore` chain: a notification, so it does nothing.
 */
static void G_WriteScore_Common(const GameClient *cl, GameScore *s) {
}

GameWriteScoreHook G_WriteScore = G_WriteScore_Common;

/**
 * @brief The tail of the `G_WriteStats` chain: a notification, so it does nothing.
 */
static void G_WriteStats_Common(GameClient *cl) {
}

GameWriteStatsHook G_WriteStats = G_WriteStats_Common;

/**
 * @brief Returns the number of scores written to the buffer.
 */
static size_t G_UpdateScores(GameScore *scores) {
  GameScore *s = scores;
  int32_t i;

  // assemble the client scores
  G_ForEachClient(cl, {
    G_UpdateScore(cl, s++);
  });

  // and optionally concatenate the team scores
  if (gameLevel.teams) {
    memset(s, 0, sizeof(*s) * MAX_TEAMS);

    for (i = 0; i < MAX_TEAMS; i++) {
      GameTeam *team = &gameTeamList[i];

      s->client = MAX_CLIENTS;
      s->frags = team->frags;
      s->score = team->frags;
#if defined(G_CTF)
      s->captures = team->captures;
#endif
      s->flags = team->id | SCORE_AGGREGATE;
      s++;
    }
  }

  return (size_t) (s - scores);
}

/**
 * @brief Assemble the binary scores data for the client. Scores are sent in
 * chunks to overcome the 1400 byte UDP packet limitation.
 */
void G_ClientScores(GameClient *cl) {
  static GameScore scores[MAX_CLIENTS + MAX_TEAMS];
  static size_t count;

  if (!cl->showScores || (cl->scoresTime > gameLevel.time)) {
    return;
  }

  cl->scoresTime = gameLevel.time + 500;

  // update the scoreboard if it's stale; this is shared to all clients
  if (gameLevel.scoresTime <= gameLevel.time) {
    count = G_UpdateScores(scores);
    gameLevel.scoresTime = gameLevel.time + 500;
  }

  // send the scores over in chunks
  size_t i = 0, j = 0;
  while (++i < count) {
    const size_t len = (i - j) * sizeof(GameScore);
    if (len > 512) {
      gi.WriteByte(SV_CMD_SCORES);
      gi.WriteShort((int32_t) j);
      gi.WriteShort((int32_t) (i - j));
      gi.WriteData((const void *) (scores + j), len);
      gi.WriteByte(0); // sequence is incomplete
      gi.Unicast(cl, true);

      j = i;
    }
  }

  // send any remaining scores, and indicate that the sequence is complete
  const size_t len = (i - j) * sizeof(GameScore);

  gi.WriteByte(SV_CMD_SCORES);
  gi.WriteShort((int32_t) j);
  gi.WriteShort((int32_t) (i - j));
  gi.WriteData((const void *) (scores + j), len);
  gi.WriteByte(1); // sequence is complete
  gi.Unicast(cl, true);

  if (gameLevel.intermission.time) {
    G_ClientMatchStats(cl);
  }
}

/**
 * @brief Writes the stats array of the player state structure. The client's HUD is
 * largely derived from this information.
 */
void G_ClientStats(GameClient *cl) {

  // armor
  const GameItem *armor = G_ClientArmor(cl);
  if (armor) {
    cl->ps.stats[STAT_ARMOR] = cl->inventory[armor->def.tag];
  } else {
    cl->ps.stats[STAT_ARMOR] = 0;
  }

#if defined(G_TECH)
  // tech
  const GameItem *tech = G_GetTech(cl);
  cl->ps.stats[STAT_TECH] = tech ? tech->def.tag : 0;
#endif

#if defined(G_CTF)
  // captures, assists and points
  cl->ps.stats[STAT_CAPTURES] = cl->persistent.captures;
  cl->ps.stats[STAT_ASSISTS] = cl->persistent.assists;
  cl->ps.stats[STAT_SCORE] = cl->persistent.score;
#endif

  // damage received and inflicted
  cl->ps.stats[STAT_DAMAGE_ARMOR] = cl->damageArmor;
  cl->ps.stats[STAT_DAMAGE_HEALTH] = cl->damageHealth;
  cl->ps.stats[STAT_DAMAGE_INFLICT] = cl->damageInflicted;

  // frags
  cl->ps.stats[STAT_FRAGS] = cl->persistent.frags;
  cl->ps.stats[STAT_DEATHS] = cl->persistent.deaths;
  cl->score = cl->persistent.score;

  // health
  if (cl->persistent.spectator || cl->entity->dead) {
    cl->ps.stats[STAT_HEALTH] = 0;
  } else {
    cl->ps.stats[STAT_HEALTH] = cl->entity->health;
  }

  // pickup message
  if (gameLevel.time > cl->pickupMsgTime) {
    cl->ps.stats[STAT_PICKUP] = 0;
  }

  // seconds until a dead player may respawn, for the HUD's countdown of a long delay
  if (cl->entity->dead && g_respawnDelay->value > 2.f && gameLevel.time < cl->respawnTime) {
    cl->ps.stats[STAT_RESPAWN] = (int16_t) Mini(ceil((cl->respawnTime - gameLevel.time) / 1000.0),
                                                INT16_MAX);
  } else {
    cl->ps.stats[STAT_RESPAWN] = 0;
  }

  // ping, as the server measures it, so that the HUD and the scoreboard agree
  cl->ps.stats[STAT_PING] = (int16_t) Mini(cl->ping, 999);

  // scores
  cl->ps.stats[STAT_SCORES] = 0;
  if (gameLevel.intermission.time || cl->showScores) {
    cl->ps.stats[STAT_SCORES] |= 1;
  }

  if (cl->persistent.team) { // send team ID, -1 is no team
    cl->ps.stats[STAT_TEAM] = cl->persistent.team->id;
  } else {
    cl->ps.stats[STAT_TEAM] = TEAM_NONE;
  }

  // time
  if (gameLevel.intermission.time) {
    cl->ps.stats[STAT_TIME] = 0;
  } else {
    cl->ps.stats[STAT_TIME] = CS_TIME;
  }

  // weapon: lower byte = current tag, upper byte = switching-to tag
  const GameItem *weapon = cl->weapon;

  if (weapon) {
    cl->ps.stats[STAT_WEAPON] = weapon->def.tag;
  } else {
    cl->ps.stats[STAT_WEAPON] = 0;
  }

  if (cl->nextWeapon) {
    cl->ps.stats[STAT_WEAPON] |= (cl->nextWeapon->def.tag << 8);
  }

  if (gameLevel.time <= cl->quadDamageTime) {
    cl->ps.stats[STAT_QUAD_TIME] = ceil((cl->quadDamageTime - gameLevel.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_QUAD_TIME] = 0;
  }

  if (gameLevel.time <= cl->invisibilityTime) {
    cl->ps.stats[STAT_INVISIBILITY_TIME] = ceil((cl->invisibilityTime - gameLevel.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_INVISIBILITY_TIME] = 0;
  }

  if (gameLevel.time <= cl->invulnerabilityTime) {
    cl->ps.stats[STAT_INVULNERABILITY_TIME] = ceil((cl->invulnerabilityTime - gameLevel.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_INVULNERABILITY_TIME] = 0;
  }

  // copy full inventory to player state for delta-compression over the wire
  memcpy(cl->ps.inventory, cl->inventory, sizeof(cl->inventory));

  G_WriteStats(cl);
}

/**
 * @brief Updates the player stats HUD for a spectating client.
 */
void G_ClientSpectatorStats(GameClient *cl) {

  cl->ps.stats[STAT_SPECTATOR] = 1;

  // chase camera inherits stats from their chase target
  if (cl->chaseTarget && G_IsMeat(cl->chaseTarget->entity)) {

    memcpy(cl->ps.stats, cl->chaseTarget->ps.stats, sizeof(cl->ps.stats));
    memcpy(cl->ps.inventory, cl->chaseTarget->ps.inventory, sizeof(cl->ps.inventory));

    cl->ps.stats[STAT_SPECTATOR] = 1;
    cl->ps.stats[STAT_CHASE] = cl->chaseTarget->ps.entity;

    // scores are independent of chase camera target
    if (gameLevel.intermission.time || cl->showScores) {
      cl->ps.stats[STAT_SCORES] = 1;
    } else {
      cl->ps.stats[STAT_SCORES] = 0;
    }

    // as is the ping, which belongs to this client's connection, not the target's
    cl->ps.stats[STAT_PING] = (int16_t) Mini(cl->ping, 999);
  } else {
    G_ClientStats(cl);
    cl->ps.stats[STAT_CHASE] = 0;
  }
}
