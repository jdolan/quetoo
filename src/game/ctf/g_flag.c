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
 * @brief `g_module_t` function pointers.
 */
static struct {
  ResetDroppedItem ResetDroppedItem;
  DropInventoryItem DropInventoryItem;
  CheckCvars CheckCvars;
} super;

static bool installed;

cvar_t *g_capture_limit;

/**
 * @brief Returns the team that owns the given flag entity, or `NULL` if the entity is not a flag.
 */
g_team_t *G_TeamForFlag(const g_entity_t *ent) {

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

/**
 * @brief A dropped flag has been idle for 30 seconds, return it.
 */
void G_ResetDroppedFlag(g_entity_t *ent) {
  g_team_t *t;
  g_entity_t *f;

  if (!(t = G_TeamForFlag(ent)) || !(f = G_FlagForTeam(t))) {
    if (ent->spawn_flags & SF_ITEM_DROPPED) {
      G_FreeEntity(ent); // nothing to return it to; do not strand it
    }
    return;
  }

  f->sv_flags &= ~SVF_NO_CLIENT;
  f->s.event = EV_ITEM_RESPAWN;
  f->s.event_data = f->item->def.tag;
  f->solid = SOLID_TRIGGER;

  gi.LinkEntity(f);

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_media.sounds.ctf_return
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n", t->name, t->id + 1);

  if (ent != f) {
    G_FreeEntity(ent); // the base flag was restored in place, so keep it
  }
}

/**
 * @brief Returns a dropped flag to its base, deferring anything else.
 */
static void G_ResetDroppedItem_Flag(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    G_ResetDroppedFlag(ent);
    return;
  }

  super.ResetDroppedItem(ent);
}

/**
 * @brief Resolves "flag" to whichever flag the client is carrying.
 */
static void G_DropInventoryItem_Flag(g_client_t *cl, const char *name) {

  if (!q_strcasecmp(name, "flag")) {
    const g_item_t *flag = G_GetFlag(cl);
    if (flag) {
      name = flag->def.name;
    }
  }

  super.DropInventoryItem(cl, name);
}

/**
 * @brief Applies the captures' own cvars.
 */
static bool G_CheckCvars_Flag(void) {

  if (g_capture_limit->modified) {
    g_capture_limit->modified = false;
    g_level.capture_limit = g_capture_limit->integer;

    gi.BroadcastPrint(PRINT_HIGH, "Capture limit has been changed to %d\n", g_level.capture_limit);
  }

  return super.CheckCvars();
}

/**
 * @brief Plays for captures rather than frags, and so does not defer to super.
 */
static bool G_CheckWinCondition_Flag(void) {

  if (g_level.capture_limit) {

    for (int32_t i = 0; i < g_level.num_teams; i++) {
      if (g_team_list[i].captures >= g_level.capture_limit) {
        gi.BroadcastPrint(PRINT_HIGH, "Capture limit hit\n");
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Registers the captures' cvars and installs the flags' hooks.
 */
void G_Flag_Init(void) {

  // G_Init runs on every server initialization, and the module is not always
  // unloaded in between, so installing twice would point super at ourselves.
  if (!installed) {
    installed = true;

    super.ResetDroppedItem = G_ResetDroppedItem;
    G_ResetDroppedItem = G_ResetDroppedItem_Flag;

    super.DropInventoryItem = G_DropInventoryItem;
    G_DropInventoryItem = G_DropInventoryItem_Flag;

    super.CheckCvars = G_CheckCvars;
    G_CheckCvars = G_CheckCvars_Flag;

    G_CheckWinCondition = G_CheckWinCondition_Flag;
  }

  g_capture_limit = gi.AddCvar("g_capture_limit", "8", CVAR_SERVER_INFO, "The capture limit per level.");

  g_capture_limit->modified = false;
}
