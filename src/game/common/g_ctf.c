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
 * @brief `g_module.h` function pointers.
 */
static struct {
  ResetDroppedItem ResetDroppedItem;
  ResolveInventoryItem ResolveInventoryItem;
  CheckCvars CheckCvars;
  TossInventory TossInventory;
  InitMedia InitMedia;
  ResetItem ResetItem;
  InhibitItem InhibitItem;
  InitItem InitItem;
} super;

static bool installed;

cvar_t *g_capture_limit;

static struct {
  uint16_t capture;
  uint16_t return_;
  uint16_t steal;
} g_ctf_media;

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
 * @brief Returns the flag entity currently placed for the given team, or `NULL`
 * if the map placed none.
 */
g_entity_t *G_FlagForTeam(const g_team_t *t) {

  return t->flag_entity;
}

/**
 * @brief Returns the entity state effect flag for the given team, or 0 if none.
 */
static int32_t G_EffectForTeam(const g_team_t *t) {

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
static void G_ResetDroppedFlag(g_entity_t *ent) {
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
    .index = g_ctf_media.return_
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n", t->name, t->id + 1);

  if (ent != f) {
    G_FreeEntity(ent); // the base flag was restored in place, so keep it
  }
}

/**
 * @brief Returns a dropped flag to its base, deferring anything else.
 */
static void G_ResetDroppedItem_Ctf(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    G_ResetDroppedFlag(ent);
    return;
  }

  super.ResetDroppedItem(ent);
}

/**
 * @brief Resolves "flag" to whichever flag the client is carrying.
 */
static const g_item_t *G_ResolveInventoryItem_Ctf(g_client_t *cl, const char *name) {

  if (!q_strcasecmp(name, "flag")) {
    const g_item_t *flag = G_GetFlag(cl);
    if (flag) {
      return flag;
    }
  }

  return super.ResolveInventoryItem(cl, name);
}

/**
 * @brief Applies capture the flag's own cvars.
 */
static bool G_CheckCvars_Ctf(void) {

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
static bool G_CheckWinner_Ctf(void) {

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
 * @brief Names the gameplay for captures, which replaces rather than qualifies
 * what it was handed, and so does not defer to super.
 */
static void G_FormatGameName_Ctf(char *name, size_t size) {

  q_strlcat(name, " CTF", size);
}

/**
 * @brief Steal the enemy's flag. If our own flag is dropped, return it. Else, if we are
 * carrying the enemy's flag and touch our own flag, that is a capture.
 */
static bool G_PickupFlag(g_client_t *cl, g_entity_t *ent) {
  int32_t index;

  if (!cl->persistent.team) {
    return false;
  }

  g_team_t *team = G_TeamForFlag(ent);
  if (!team) {
    return false; // a flag for a team this level does not have
  }

  g_entity_t *team_flag = G_FlagForTeam(team);
  if (!team_flag) {
    return false; // the map placed no base flag for that team
  }

  const g_item_t *carried_flag = G_GetFlag(cl);

  if (team == cl->persistent.team) { // our flag

    if (ent->spawn_flags & SF_ITEM_DROPPED) { // return it if necessary

      team_flag->solid = SOLID_TRIGGER;
      team_flag->sv_flags &= ~SVF_NO_CLIENT;

      gi.LinkEntity(team_flag);

      team_flag->s.event = EV_ITEM_RESPAWN;
      team_flag->s.event_data = team_flag->item->def.tag;

      G_MulticastSound(&(const g_play_sound_t) {
        .index = g_ctf_media.return_
      }, MULTICAST_PHS);

      gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag :flag%d_return:\n", cl->persistent.net_name, team->name, team->id + 1);

      return true;
    }

    if (carried_flag) {
      const g_team_t *other_team = &g_team_list[carried_flag->def.tag - FLAG_FIRST];
      g_entity_t *other_team_flag = G_FlagForTeam(other_team);
      if (!other_team_flag) {
        return false;
      }

      index = other_team_flag->item->def.tag;
      if (cl->inventory[index]) { // capture

        cl->inventory[index] = 0;
        cl->entity->s.effects &= ~G_EffectForTeam(other_team);
        cl->entity->s.model3 = 0;

        other_team_flag->solid = SOLID_TRIGGER;
        other_team_flag->sv_flags &= ~SVF_NO_CLIENT; // reset the other flag

        gi.LinkEntity(other_team_flag);

        other_team_flag->s.event = EV_ITEM_RESPAWN;
        other_team_flag->s.event_data = other_team_flag->item->def.tag;

        G_MulticastSound(&(const g_play_sound_t) {
          .index = g_ctf_media.capture
        }, MULTICAST_PHS_R);

        gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag :flag%d_capture:\n", cl->persistent.net_name, other_team->name, other_team->id + 1);

        team->captures++;
        cl->persistent.captures++;

        {
          const bool player_ai = cl->ai != NULL;
          g_capture_t capture = {
            .player_ai = player_ai,
            .time = (uint32_t) time(NULL),
          };
          q_strlcpy(capture.level,       g_level.name,              sizeof(capture.level));
          q_strlcpy(capture.player,      cl->persistent.net_name,   sizeof(capture.player));
          q_strlcpy(capture.player_guid, cl->persistent.guid,       sizeof(capture.player_guid));
          q_strlcpy(capture.team,        other_team->name,          sizeof(capture.team));

          if (capture.player_guid[0]) {
            $(g_level.captures, add, &capture);
          }
        }

        return false;
      }
    }

    // touching our own flag for no particular reason
    return false;
  }

  // it's enemy's flag, so take it if we can
  if (carried_flag) {
    return false; // we have one already
  }

  team_flag->solid = SOLID_NOT;
  team_flag->sv_flags |= SVF_NO_CLIENT;

  gi.LinkEntity(team_flag);

  index = team_flag->item->def.tag;
  cl->inventory[index] = 1;

  // link the flag model to the player
  cl->entity->s.model3 = team_flag->item->model_index;

  G_MulticastSound(&(const g_play_sound_t) {
    .index = g_ctf_media.steal,
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag :flag%d_steal:\n", cl->persistent.net_name, team->name, team->id + 1);

  cl->entity->s.effects |= G_EffectForTeam(team);
  return true;
}

/**
 * @brief Sheds the carried flag's effects and announces it, then puts the flag
 * into the world. The caller owns the inventory bookkeeping.
 */
static g_entity_t *G_ReleaseFlag(g_client_t *cl, const g_item_t *flag) {

  const g_team_t *team = &g_team_list[flag->def.tag - FLAG_FIRST];

  cl->entity->s.model3 = 0;
  cl->entity->s.effects &= ~EF_CTF_MASK;

  gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag :flag%d_drop:\n", cl->persistent.net_name, team->name, team->id + 1);

  return G_DropItem(cl, flag);
}

/**
 * @brief Tosses the flag the client is carrying into the world, clearing it
 * from their inventory first.
 */
static g_entity_t *G_TossFlag(g_client_t *cl) {

  const g_item_t *flag = G_GetFlag(cl);

  if (!flag || !cl->inventory[flag->def.tag]) {
    return NULL;
  }

  cl->inventory[flag->def.tag] = 0;

  return G_ReleaseFlag(cl, flag);
}

/**
 * @brief Drop command callback that tosses the client's carried CTF flag.
 */
static g_entity_t *G_DropFlag(g_client_t *cl, const g_item_t *item) {
  return G_ReleaseFlag(cl, item);
}

/**
 * @brief Indexes capture the flag's sounds for this level.
 */
static void G_InitMedia_Ctf(void) {

  super.InitMedia();

  g_ctf_media.capture = gi.SoundIndex("ctf/capture");
  g_ctf_media.return_ = gi.SoundIndex("ctf/return");
  g_ctf_media.steal = gi.SoundIndex("ctf/steal");
}

/**
 * @brief Hides a flag whose team is not playing this level.
 */
static void G_ResetItem_Ctf(g_entity_t *ent) {

  super.ResetItem(ent);

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    const g_team_id_t flag_team = ent->item->def.tag - FLAG_FIRST;

    if (flag_team >= g_level.num_teams) {
      ent->sv_flags |= SVF_NO_CLIENT;
      ent->solid = SOLID_NOT;

      gi.LinkEntity(ent);
    }
  }
}

/**
 * @brief Exempts the flags from the gameplay modes that withhold items, since
 * without them there is nothing to capture.
 */
static bool G_InhibitItem_Ctf(const g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    return false;
  }

  return super.InhibitItem(ent);
}

/**
 * @brief Answers for the flag item type.
 */
static void G_InitItem_Ctf(g_item_t *it) {

  if (it->def.type == ITEM_TYPE_FLAG) {
    it->Pickup = G_PickupFlag;
    it->Drop = G_DropFlag;
    return;
  }

  super.InitItem(it);
}

/**
 * @brief Tosses the flag a client leaving play is holding.
 */
static void G_TossInventory_Ctf(g_client_t *cl) {

  G_TossFlag(cl);

  super.TossInventory(cl);
}

/**
 * @brief Registers capture the flag's cvars and installs its hooks.
 */
void G_Ctf_Init(void) {

  // G_Init runs on every server initialization, and the module is not always
  // unloaded in between, so installing twice would point super at ourselves.
  if (!installed) {
    installed = true;

    super.ResetDroppedItem = G_ResetDroppedItem;
    G_ResetDroppedItem = G_ResetDroppedItem_Ctf;

    super.ResolveInventoryItem = G_ResolveInventoryItem;
    G_ResolveInventoryItem = G_ResolveInventoryItem_Ctf;

    super.CheckCvars = G_CheckCvars;
    G_CheckCvars = G_CheckCvars_Ctf;

    G_CheckWinner = G_CheckWinner_Ctf;

    G_FormatGameName = G_FormatGameName_Ctf;
    super.TossInventory = G_TossInventory;
    G_TossInventory = G_TossInventory_Ctf;

    super.ResetItem = G_ResetItem;
    G_ResetItem = G_ResetItem_Ctf;

    super.InhibitItem = G_InhibitItem;
    G_InhibitItem = G_InhibitItem_Ctf;

    super.InitItem = G_InitItem;
    G_InitItem = G_InitItem_Ctf;

    super.InitMedia = G_InitMedia;
    G_InitMedia = G_InitMedia_Ctf;
  }

  g_capture_limit = gi.AddCvar("g_capture_limit", "8", CVAR_SERVER_INFO, "The capture limit per level.");

  g_capture_limit->modified = false;
}
