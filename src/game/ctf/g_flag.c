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
 * @brief Returns the team that owns the given flag entity, or `NULL` if the entity is not a flag.
 */
g_team_t *G_TeamForFlag(const g_entity_t *ent) {

  if (!g_level.ctf) {
    return NULL;
  }

  if (!ent->item || ent->item->def.type != ITEM_TYPE_FLAG) {
    return NULL;
  }

  for (int32_t i = 0; i < g_level.num_teams; i++) {

    if (!q_strcmp(ent->classname, g_team_list[i].flag)) {
      return &g_team_list[i];
    }
  }

  return NULL;
}

/**
 * @brief Returns the flag entity currently placed for the given team, or `NULL` if CTF is off.
 */
g_entity_t *G_FlagForTeam(const g_team_t *t) {

  if (!g_level.ctf) {
    return NULL;
  }

  return t->flag_entity;
}

/**
 * @brief Returns the entity state effect flag for the given team, or 0 if none.
 */
int32_t G_EffectForTeam(const g_team_t *t) {

  if (!t) {
    return 0;
  }

  return t->effect;
}

/**
 * @brief Get the flag a player is holding, or `NULL` if we're not a flag-bearer.
 */
const g_item_t *G_GetFlag(const g_client_t *cl) {

  for (int32_t i = 0; i < g_level.num_teams; i++) {

    if (&g_team_list[i] == cl->persistent.team) {
      continue;
    }

    g_entity_t *f = G_FlagForTeam(&g_team_list[i]);

    if (f && cl->inventory[f->item->def.tag]) {
      return f->item;
    }
  }

  return NULL;
}
