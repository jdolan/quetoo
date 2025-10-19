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
 * @brief
 */
void G_ClientToIntermission(g_client_t *cl) {

  cl->entity->s.origin = g_level.intermission_origin;
  cl->ps.pm_state.origin = g_level.intermission_origin;

  cl->ps.pm_state.view_angles = Vec3_Zero();
  cl->ps.pm_state.delta_angles = g_level.intermission_angle;

  cl->ps.pm_state.view_offset = Vec3_Zero();
  cl->ps.pm_state.step_offset = 0.f;

  cl->ps.pm_state.type = PM_FREEZE;

  cl->entity->s.model1 = 0;
  cl->entity->s.model2 = 0;
  cl->entity->s.model3 = 0;
  cl->entity->s.model4 = 0;
  cl->entity->s.effects = 0;
  cl->entity->s.sound = 0;
  cl->entity->solid = SOLID_NOT;
  cl->entity->dead = true;

  // show scores
  cl->show_scores = true;

  // hide the HUD
  memset(cl->inventory, 0, sizeof(cl->inventory));
  cl->weapon = NULL;

  cl->ammo_index = 0;
  cl->pickup_msg_time = 0;
}

/**
 * @brief Write the scores information for the specified client.
 */
static void G_UpdateScore(const g_client_t *cl, g_score_t *s) {

  memset(s, 0, sizeof(*s));

  s->client = cl->ps.client;
  s->ping = cl->ping < 999 ? cl->ping : 999;

  if (cl->persistent.spectator) {
    s->color = -1;
    s->flags |= SCORE_SPECTATOR;
  } else {
    if (g_level.ctf) {
      if (G_GetFlag(cl)) {
        s->flags |= SCORE_CTF_FLAG;
      }
    }
    if (cl->persistent.team) {
      s->color = cl->persistent.team->color;
      s->team = cl->persistent.team->id + 1;
    } else {
      s->color = cl->persistent.color;
    }
  }

  s->score = cl->persistent.score;
  s->deaths = cl->persistent.deaths;
  s->captures = cl->persistent.captures;
}

/**
 * @brief Returns the number of scores written to the buffer.
 */
static size_t G_UpdateScores(g_score_t *scores) {
  g_score_t *s = scores;
  int32_t i;

  // assemble the client scores
  G_ForEachClient(cl, {
    G_UpdateScore(cl, s++);
  });

  // and optionally concatenate the team scores
  if (g_level.teams || g_level.ctf) {
    memset(s, 0, sizeof(*s) * MAX_TEAMS);

    for (i = 0; i < MAX_TEAMS; i++) {
      g_team_t *team = &g_team_list[i];

      s->client = MAX_CLIENTS;
      s->score = team->score;
      s->captures = team->captures;
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
void G_ClientScores(g_client_t *cl) {
  static g_score_t scores[MAX_CLIENTS + MAX_TEAMS];
  static size_t count;

  if (!cl->show_scores || (cl->scores_time > g_level.time)) {
    return;
  }

  cl->scores_time = g_level.time + 500;

  // update the scoreboard if it's stale; this is shared to all clients
  if (g_level.scores_time <= g_level.time) {
    count = G_UpdateScores(scores);
    g_level.scores_time = g_level.time + 500;
  }

  // send the scores over in chunks
  size_t i = 0, j = 0;
  while (++i < count) {
    const size_t len = (i - j) * sizeof(g_score_t);
    if (len > 512) {
      gi.WriteByte(SV_CMD_SCORES);
      gi.WriteShort((int32_t) j);
      gi.WriteShort((int32_t) i);
      gi.WriteData((const void *) (scores + j), len);
      gi.WriteByte(0); // sequence is incomplete
      gi.Unicast(cl, true);

      j = i;
    }
  }

  // send any remaining scores, and indicate that the sequence is complete
  const size_t len = (i - j) * sizeof(g_score_t);

  gi.WriteByte(SV_CMD_SCORES);
  gi.WriteShort((int32_t) j);
  gi.WriteShort((int32_t) i);
  gi.WriteData((const void *) (scores + j), len);
  gi.WriteByte(1); // sequence is complete
  gi.Unicast(cl, true);
}

/**
 * @brief Writes the stats array of the player state structure. The client's HUD is
 * largely derived from this information.
 */
void G_ClientStats(g_client_t *cl) {

  // ammo
  if (cl->weapon && cl->ammo_index) {
    const g_item_t *ammo = G_ItemByIndex(cl->ammo_index);
    const g_item_t *weap = cl->weapon;
    cl->ps.stats[STAT_AMMO_ICON] = weap->icon_index;
    cl->ps.stats[STAT_AMMO] = cl->inventory[cl->ammo_index];
    cl->ps.stats[STAT_AMMO_LOW] = ammo->quantity;
  } else {
    cl->ps.stats[STAT_AMMO_ICON] = 0;
    cl->ps.stats[STAT_AMMO] = 0;
  }

  // armor
  const g_item_t *armor = G_ClientArmor(cl);
  if (armor) {
    cl->ps.stats[STAT_ARMOR_ICON] = armor->icon_index;
    cl->ps.stats[STAT_ARMOR] = cl->inventory[armor->index];
  } else {
    cl->ps.stats[STAT_ARMOR_ICON] = 0;
    cl->ps.stats[STAT_ARMOR] = 0;
  }

  // tech
  const g_item_t *tech = G_GetTech(cl);
  if (tech) {
    cl->ps.stats[STAT_TECH_ICON] = tech->icon_index;
  } else {
    cl->ps.stats[STAT_TECH_ICON] = -1;
  }

  // captures
  cl->ps.stats[STAT_CAPTURES] = cl->persistent.captures;

  // damage received and inflicted
  cl->ps.stats[STAT_DAMAGE_ARMOR] = cl->damage_armor;
  cl->ps.stats[STAT_DAMAGE_HEALTH] = cl->damage_health;
  cl->ps.stats[STAT_DAMAGE_INFLICT] = cl->damage_inflicted;

  // held flag
  cl->ps.stats[STAT_CARRYING_FLAG] = 0;

  for (int32_t i = 0; i < g_level.num_teams; i++) {

    if (cl->inventory[g_media.items.flags[i]->index]) {
      cl->ps.stats[STAT_CARRYING_FLAG] = i + 1;
      break;
    }
  }

  // frags
  cl->ps.stats[STAT_FRAGS] = cl->persistent.score;
  cl->ps.stats[STAT_DEATHS] = cl->persistent.deaths;

  // health
  if (cl->persistent.spectator || cl->entity->dead) {
    cl->ps.stats[STAT_HEALTH_ICON] = 0;
    cl->ps.stats[STAT_HEALTH] = 0;
  } else {
    if (cl->entity->health > 100) {
      cl->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_MEGA]->icon_index;
    } else if (cl->entity->health > 75) {
      cl->ps.stats[STAT_HEALTH_ICON] = g_media.images.health;
    } else if (cl->entity->health > 25) {
      cl->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_MEDIUM]->icon_index;
    } else {
      cl->ps.stats[STAT_HEALTH_ICON] = g_media.items.health[HEALTH_LARGE]->icon_index;
    }
    cl->ps.stats[STAT_HEALTH] = cl->entity->health;
  }

  // pickup message
  if (g_level.time > cl->pickup_msg_time) {
    cl->ps.stats[STAT_PICKUP_ICON] = -1;
    cl->ps.stats[STAT_PICKUP_STRING] = 0;
  }

  // scores
  cl->ps.stats[STAT_SCORES] = 0;
  if (g_level.intermission_time || cl->show_scores) {
    cl->ps.stats[STAT_SCORES] |= 1;
  }

  if (cl->persistent.team) { // send team ID, -1 is no team
    cl->ps.stats[STAT_TEAM] = cl->persistent.team->id;
  } else {
    cl->ps.stats[STAT_TEAM] = TEAM_NONE;
  }

  // time
  if (g_level.intermission_time) {
    cl->ps.stats[STAT_TIME] = 0;
  } else {
    cl->ps.stats[STAT_TIME] = CS_TIME;
  }

  // weapon
  const g_item_t *weapon = cl->weapon;

  if (weapon) {
    cl->ps.stats[STAT_WEAPON] = cl->weapon->model_index;
    cl->ps.stats[STAT_WEAPON_ICON] = weapon->icon_index;
    cl->ps.stats[STAT_WEAPON_TAG] = weapon->tag;
  } else {
    cl->ps.stats[STAT_WEAPON] = 0;
    cl->ps.stats[STAT_WEAPON_ICON] = 0;
    cl->ps.stats[STAT_WEAPON_TAG] = 0;
  }

  if (cl->next_weapon) {
    cl->ps.stats[STAT_WEAPON_TAG] |= (cl->next_weapon->tag << 8);
  }

  if (g_level.time <= cl->quad_damage_time) {
    cl->ps.stats[STAT_QUAD_TIME] = ceil((cl->quad_damage_time - g_level.time) / 1000.0);
  } else {
    cl->ps.stats[STAT_QUAD_TIME] = 0;
  }

  // change-able weapons
  cl->ps.stats[STAT_WEAPONS] = 0;

  if (!cl->persistent.spectator && !cl->entity->dead) {
    for (int32_t i = WEAPON_NONE + 1; i < WEAPON_TOTAL; i++) {

      const g_item_t *weapon = g_media.items.weapons[i];
      const g_item_t *ammo = weapon->ammo_item;

      if (cl->inventory[weapon->index] && (!ammo || cl->inventory[ammo->index] >= weapon->quantity)) {
        cl->ps.stats[STAT_WEAPONS] |= 1 << (i - 1);
      }
    }
  }
}

/**
 * @brief
 */
void G_ClientSpectatorStats(g_client_t *cl) {

  cl->ps.stats[STAT_SPECTATOR] = 1;

  // chase camera inherits stats from their chase target
  if (cl->chase_target && G_IsMeat(cl->chase_target->entity)) {
    cl->ps.stats[STAT_CHASE] = cl->chase_target->entity->s.number;

    // scores are independent of chase camera target
    if (g_level.intermission_time || cl->show_scores) {
      cl->ps.stats[STAT_SCORES] = 1;
    } else {
      cl->ps.stats[STAT_SCORES] = 0;
    }
  } else {
    G_ClientStats(cl);
    cl->ps.stats[STAT_CHASE] = 0;
  }
}
