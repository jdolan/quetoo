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

/**
 * @brief Moves a client to the intermission position and freezes their input.
 */
void G_ClientToIntermission(GameClient *cl) {

  if (!cl->entity) {
    return;
  }

  cl->entity->s.origin = g_level.intermissionOrigin;
  cl->ps.pmState.origin = g_level.intermissionOrigin;

  cl->ps.pmState.viewAngles = Vec3_Zero();
  cl->ps.pmState.deltaAngles = g_level.intermissionAngle;

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
    if (G_GetFlag(cl)) {
      s->flags |= SCORE_CTF_FLAG;
    }
#endif
    if (cl->persistent.team) {
      s->color = cl->persistent.team->color;
      s->team = cl->persistent.team->id + 1;
    } else {
      s->color = cl->persistent.color;
    }
  }

  s->score = cl->persistent.score;
  s->deaths = cl->persistent.deaths;
#if defined(G_CTF)
  s->captures = cl->persistent.captures;
#endif

  G_WriteScore(cl, s);
}

/**
 * @brief The tail of the `G_WriteScore` chain: a notification, so it does nothing.
 */
static void G_WriteScore_Common(const GameClient *cl, GameScore *s) {
}

WriteScore G_WriteScore = G_WriteScore_Common;

/**
 * @brief The tail of the `G_WriteStats` chain: a notification, so it does nothing.
 */
static void G_WriteStats_Common(GameClient *cl) {
}

WriteStats G_WriteStats = G_WriteStats_Common;

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
  if (g_level.teams) {
    memset(s, 0, sizeof(*s) * MAX_TEAMS);

    for (i = 0; i < MAX_TEAMS; i++) {
      GameTeam *team = &g_team_list[i];

      s->client = MAX_CLIENTS;
      s->score = team->score;
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

  if (!cl->showScores || (cl->scoresTime > g_level.time)) {
    return;
  }

  cl->scoresTime = g_level.time + 500;

  // update the scoreboard if it's stale; this is shared to all clients
  if (g_level.scoresTime <= g_level.time) {
    count = G_UpdateScores(scores);
    g_level.scoresTime = g_level.time + 500;
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
  // captures
  cl->ps.stats[STAT_CAPTURES] = cl->persistent.captures;
#endif

  // damage received and inflicted
  cl->ps.stats[STAT_DAMAGE_ARMOR] = cl->damageArmor;
  cl->ps.stats[STAT_DAMAGE_HEALTH] = cl->damageHealth;
  cl->ps.stats[STAT_DAMAGE_INFLICT] = cl->damageInflicted;

  // frags
  cl->ps.stats[STAT_FRAGS] = cl->persistent.score;
  cl->ps.stats[STAT_DEATHS] = cl->persistent.deaths;
  cl->score = cl->persistent.score;

  // health
  if (cl->persistent.spectator || cl->entity->dead) {
    cl->ps.stats[STAT_HEALTH] = 0;
  } else {
    cl->ps.stats[STAT_HEALTH] = cl->entity->health;
  }

  // pickup message
  if (g_level.time > cl->pickupMsgTime) {
    cl->ps.stats[STAT_PICKUP] = 0;
  }

  // ping, as the server measures it, so that the HUD and the scoreboard agree
  cl->ps.stats[STAT_PING] = (int16_t) Mini(cl->ping, 999);

  // scores
  cl->ps.stats[STAT_SCORES] = 0;
  if (g_level.intermissionTime || cl->showScores) {
    cl->ps.stats[STAT_SCORES] |= 1;
  }

  if (cl->persistent.team) { // send team ID, -1 is no team
    cl->ps.stats[STAT_TEAM] = cl->persistent.team->id;
  } else {
    cl->ps.stats[STAT_TEAM] = TEAM_NONE;
  }

  // time
  if (g_level.intermissionTime) {
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

  if (g_level.time <= cl->quadDamageTime) {
    cl->ps.stats[STAT_QUAD_TIME] = ceil((cl->quadDamageTime - g_level.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_QUAD_TIME] = 0;
  }

  if (g_level.time <= cl->invisibilityTime) {
    cl->ps.stats[STAT_INVISIBILITY_TIME] = ceil((cl->invisibilityTime - g_level.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_INVISIBILITY_TIME] = 0;
  }

  if (g_level.time <= cl->invulnerabilityTime) {
    cl->ps.stats[STAT_INVULNERABILITY_TIME] = ceil((cl->invulnerabilityTime - g_level.time) / 1000.0);
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

    cl->ps.stats[STAT_SPECTATOR] = 1;
    cl->ps.stats[STAT_CHASE] = cl->chaseTarget->entity->s.number;

    // scores are independent of chase camera target
    if (g_level.intermissionTime || cl->showScores) {
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
