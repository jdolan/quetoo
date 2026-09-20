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
} previous;

static bool installed;

Cvar *g_captureLimit;

static struct {
  uint16_t capture;
  uint16_t return_;
  uint16_t steal;
} module;

/**
 * @brief Returns the team that owns the given flag entity, or `NULL` if the entity is not a flag.
 */
GameTeam *G_TeamForFlag(const GameEntity *ent) {

  if (!ent->item || ent->item->def.type != ITEM_TYPE_FLAG) {
    return NULL;
  }

  for (int32_t i = 0; i < gLevel.numTeams; i++) {

    if (!q_strcmp(ent->classname, gTeamList[i].flag)) {
      return &gTeamList[i];
    }
  }

  return NULL;
}

/**
 * @brief Returns the flag entity currently placed for the given team, or `NULL`
 * if the map placed none.
 */
GameEntity *G_FlagForTeam(const GameTeam *t) {

  return t->flagEntity;
}

/**
 * @brief Returns the entity state effect flag for the given team, or 0 if none.
 */
static int32_t G_EffectForTeam(const GameTeam *t) {

  return t->effect;
}

/**
 * @brief Get the flag a player is holding, or `NULL` if we're not a flag-bearer.
 */
const GameItem *G_GetFlag(const GameClient *cl) {

  for (int32_t i = 0; i < gLevel.numTeams; i++) {

    if (&gTeamList[i] == cl->persistent.team) {
      continue;
    }

    GameEntity *f = G_FlagForTeam(&gTeamList[i]);

    if (f && cl->inventory[f->item->def.tag]) {
      return f->item;
    }
  }

  return NULL;
}

/**
 * @brief A dropped flag has been idle for 30 seconds, return it.
 */
static void G_ResetDroppedFlag(GameEntity *ent) {
  GameTeam *t;
  GameEntity *f;

  if (!(t = G_TeamForFlag(ent)) || !(f = G_FlagForTeam(t))) {
    if (ent->spawnFlags & SF_ITEM_DROPPED) {
      G_FreeEntity(ent); // nothing to return it to; do not strand it
    }
    return;
  }

  f->svFlags &= ~SVF_NO_CLIENT;
  f->s.event = EV_ITEM_RESPAWN;
  f->s.eventData = f->item->def.tag;
  f->solid = SOLID_TRIGGER;

  gi.LinkEntity(f);

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.return_
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n", t->name, t->id + 1);

  if (ent != f) {
    G_FreeEntity(ent); // the base flag was restored in place, so keep it
  }
}

/**
 * @brief Returns a dropped flag to its base, deferring anything else.
 */
static void G_ResetDroppedItem_Ctf(GameEntity *ent) {

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    G_ResetDroppedFlag(ent);
    return;
  }

  previous.ResetDroppedItem(ent);
}

/**
 * @brief Resolves "flag" to whichever flag the client is carrying.
 */
static const GameItem *G_ResolveInventoryItem_Ctf(GameClient *cl, const char *name) {

  if (!q_strcasecmp(name, "flag")) {
    const GameItem *flag = G_GetFlag(cl);
    if (flag) {
      return flag;
    }
  }

  return previous.ResolveInventoryItem(cl, name);
}

/**
 * @brief Applies capture the flag's own cvars.
 */
static bool G_CheckCvars_Ctf(void) {

  if (g_captureLimit->modified) {
    g_captureLimit->modified = false;
    gLevel.captureLimit = g_captureLimit->integer;

    gi.BroadcastPrint(PRINT_HIGH, "Capture limit has been changed to %d\n", gLevel.captureLimit);
  }

  return previous.CheckCvars();
}

/**
 * @brief Plays for captures rather than frags, and so does not defer to previous.
 */
static bool G_CheckWinner_Ctf(void) {

  if (gLevel.captureLimit) {

    for (int32_t i = 0; i < gLevel.numTeams; i++) {
      if (gTeamList[i].captures >= gLevel.captureLimit) {
        gi.BroadcastPrint(PRINT_HIGH, "Capture limit hit\n");
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Names the gameplay for captures, which replaces rather than qualifies
 * what it was handed, and so does not defer to previous.
 */
static void G_FormatGameName_Ctf(char *name, size_t size) {

  q_strlcat(name, " CTF", size);
}

/**
 * @brief Captures are always team deathmatch: instagib and arena do not apply,
 * and teams are not optional. Replaces rather than qualifies, and so does not
 * defer to previous.
 */
static GameplayId G_ClampGameplay_Ctf(GameplayId gameplay) {
  return GAMEPLAY_TEAM_DEATHMATCH;
}

/**
 * @brief Steal the enemy's flag. If our own flag is dropped, return it. Else, if we are
 * carrying the enemy's flag and touch our own flag, that is a capture.
 */
static bool G_PickupFlag(GameClient *cl, GameEntity *ent) {
  int32_t index;

  if (!cl->persistent.team) {
    return false;
  }

  GameTeam *team = G_TeamForFlag(ent);
  if (!team) {
    return false; // a flag for a team this level does not have
  }

  GameEntity *teamFlag = G_FlagForTeam(team);
  if (!teamFlag) {
    return false; // the map placed no base flag for that team
  }

  const GameItem *carriedFlag = G_GetFlag(cl);

  if (team == cl->persistent.team) { // our flag

    if (ent->spawnFlags & SF_ITEM_DROPPED) { // return it if necessary

      teamFlag->solid = SOLID_TRIGGER;
      teamFlag->svFlags &= ~SVF_NO_CLIENT;

      gi.LinkEntity(teamFlag);

      teamFlag->s.event = EV_ITEM_RESPAWN;
      teamFlag->s.eventData = teamFlag->item->def.tag;

      G_MulticastSound(&(const GamePlaySound) {
        .index = module.return_
      }, MULTICAST_PHS);

      gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag :flag%d_return:\n", cl->persistent.netName, team->name, team->id + 1);

      return true;
    }

    if (carriedFlag) {
      const GameTeam *otherTeam = &gTeamList[carriedFlag->def.tag - FLAG_FIRST];
      GameEntity *otherTeamFlag = G_FlagForTeam(otherTeam);
      if (!otherTeamFlag) {
        return false;
      }

      index = otherTeamFlag->item->def.tag;
      if (cl->inventory[index]) { // capture

        cl->inventory[index] = 0;
        cl->entity->s.effects &= ~G_EffectForTeam(otherTeam);
        cl->entity->s.model3 = 0;

        otherTeamFlag->solid = SOLID_TRIGGER;
        otherTeamFlag->svFlags &= ~SVF_NO_CLIENT; // reset the other flag

        gi.LinkEntity(otherTeamFlag);

        otherTeamFlag->s.event = EV_ITEM_RESPAWN;
        otherTeamFlag->s.eventData = otherTeamFlag->item->def.tag;

        G_MulticastSound(&(const GamePlaySound) {
          .index = module.capture
        }, MULTICAST_PHS_R);

        gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag :flag%d_capture:\n", cl->persistent.netName, otherTeam->name, otherTeam->id + 1);

        team->captures++;
        cl->persistent.captures++;

        {
          const bool playerAi = cl->ai != NULL;
          GameCapture capture = {
            .playerAi = playerAi,
            .time = (uint32_t) time(NULL),
          };
          q_strlcpy(capture.level,       gLevel.name,              sizeof(capture.level));
          q_strlcpy(capture.player,      cl->persistent.netName,   sizeof(capture.player));
          q_strlcpy(capture.playerGuid, cl->persistent.guid,       sizeof(capture.playerGuid));
          q_strlcpy(capture.team,        otherTeam->name,          sizeof(capture.team));

          if (capture.playerGuid[0]) {
            $(gLevel.captures, add, &capture);
          }
        }

        return false;
      }
    }

    // touching our own flag for no particular reason
    return false;
  }

  // it's enemy's flag, so take it if we can
  if (carriedFlag) {
    return false; // we have one already
  }

  teamFlag->solid = SOLID_NOT;
  teamFlag->svFlags |= SVF_NO_CLIENT;

  gi.LinkEntity(teamFlag);

  index = teamFlag->item->def.tag;
  cl->inventory[index] = 1;

  // link the flag model to the player
  cl->entity->s.model3 = teamFlag->item->modelIndex;

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.steal,
  }, MULTICAST_PHS_R);

  gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag :flag%d_steal:\n", cl->persistent.netName, team->name, team->id + 1);

  cl->entity->s.effects |= G_EffectForTeam(team);
  return true;
}

/**
 * @brief Sheds the carried flag's effects and announces it, then puts the flag
 * into the world. The caller owns the inventory bookkeeping.
 */
static GameEntity *G_ReleaseFlag(GameClient *cl, const GameItem *flag) {

  const GameTeam *team = &gTeamList[flag->def.tag - FLAG_FIRST];

  cl->entity->s.model3 = 0;
  cl->entity->s.effects &= ~EF_CTF_MASK;

  gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag :flag%d_drop:\n", cl->persistent.netName, team->name, team->id + 1);

  return G_DropItem(cl, flag);
}

/**
 * @brief Tosses the flag the client is carrying into the world, clearing it
 * from their inventory first.
 */
static GameEntity *G_TossFlag(GameClient *cl) {

  const GameItem *flag = G_GetFlag(cl);

  if (!flag || !cl->inventory[flag->def.tag]) {
    return NULL;
  }

  cl->inventory[flag->def.tag] = 0;

  return G_ReleaseFlag(cl, flag);
}

/**
 * @brief Drop command callback that tosses the client's carried CTF flag.
 */
static GameEntity *G_DropFlag(GameClient *cl, const GameItem *item) {
  return G_ReleaseFlag(cl, item);
}

/**
 * @brief Indexes capture the flag's sounds for this level.
 */
static void G_InitMedia_Ctf(void) {

  previous.InitMedia();

  module.capture = gi.SoundIndex("ctf/capture");
  module.return_ = gi.SoundIndex("ctf/return");
  module.steal = gi.SoundIndex("ctf/steal");
}

/**
 * @brief Hides a flag whose team is not playing this level.
 */
static void G_ResetItem_Ctf(GameEntity *ent) {

  previous.ResetItem(ent);

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    const GameTeamId flagTeam = ent->item->def.tag - FLAG_FIRST;

    if (flagTeam >= gLevel.numTeams) {
      ent->svFlags |= SVF_NO_CLIENT;
      ent->solid = SOLID_NOT;

      gi.LinkEntity(ent);
    }
  }
}

/**
 * @brief Exempts the flags from the gameplay modes that withhold items, since
 * without them there is nothing to capture.
 */
static bool G_InhibitItem_Ctf(const GameEntity *ent) {

  if (ent->item->def.type == ITEM_TYPE_FLAG) {
    return false;
  }

  return previous.InhibitItem(ent);
}

/**
 * @brief Answers for the flag item type.
 */
static void G_InitItem_Ctf(GameItem *it) {

  if (it->def.type == ITEM_TYPE_FLAG) {
    it->Pickup = G_PickupFlag;
    it->Drop = G_DropFlag;
    return;
  }

  previous.InitItem(it);
}

/**
 * @brief Tosses the flag a client leaving play is holding.
 */
static void G_TossInventory_Ctf(GameClient *cl) {

  G_TossFlag(cl);

  previous.TossInventory(cl);
}

/**
 * @brief Registers capture the flag's cvars and installs its hooks.
 */
void G_Ctf_Init(void) {

  // G_Init runs on every server initialization, and the module is not always
  // unloaded in between, so installing twice would point previous at ourselves.
  if (!installed) {
    installed = true;

    previous.ResetDroppedItem = G_ResetDroppedItem;
    G_ResetDroppedItem = G_ResetDroppedItem_Ctf;

    previous.ResolveInventoryItem = G_ResolveInventoryItem;
    G_ResolveInventoryItem = G_ResolveInventoryItem_Ctf;

    previous.CheckCvars = G_CheckCvars;
    G_CheckCvars = G_CheckCvars_Ctf;

    G_CheckWinner = G_CheckWinner_Ctf;

    G_ClampGameplay = G_ClampGameplay_Ctf;

    G_FormatGameName = G_FormatGameName_Ctf;
    previous.TossInventory = G_TossInventory;
    G_TossInventory = G_TossInventory_Ctf;

    previous.ResetItem = G_ResetItem;
    G_ResetItem = G_ResetItem_Ctf;

    previous.InhibitItem = G_InhibitItem;
    G_InhibitItem = G_InhibitItem_Ctf;

    previous.InitItem = G_InitItem;
    G_InitItem = G_InitItem_Ctf;

    previous.InitMedia = G_InitMedia;
    G_InitMedia = G_InitMedia_Ctf;
  }

  g_captureLimit = gi.AddCvar("g_captureLimit", "8", CVAR_SERVER_INFO, "The capture limit per level.");

  g_captureLimit->modified = false;
}
