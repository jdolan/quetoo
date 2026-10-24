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
  GameResetDroppedItemHook ResetDroppedItem;
  GameResolveInventoryItemHook ResolveInventoryItem;
  GameCheckCvarsHook CheckCvars;
  GameTossInventoryHook TossInventory;
  GameInitMediaHook InitMedia;
  GameResetItemHook ResetItem;
  GameInhibitItemHook InhibitItem;
  GameInitItemHook InitItem;
  GameModifyDamageHook ModifyDamage;
  GameClientWillBeginHook ClientWillBegin;
  GameClientWillDieHook ClientWillDie;
  GameClientWillDisconnectHook ClientWillDisconnect;
} previous;

static bool installed;

Cvar *g_captureLimit;

static struct {
  uint16_t capture;
  uint16_t return_;
  uint16_t steal;
} module;

/**
 * @brief The points of each capture the flag event, as Quake II's CTF gives them.
 */
#define CTF_CAPTURE_POINTS 15
#define CTF_TEAM_CAPTURE_POINTS 10
#define CTF_STEAL_POINTS 1
#define CTF_RETURN_POINTS 1
#define CTF_CARRIER_KILL_POINTS 2
#define CTF_CARRIER_DANGER_POINTS 2
#define CTF_FLAG_DEFENSE_POINTS 1
#define CTF_CARRIER_DEFENSE_POINTS 1
#define CTF_RETURN_ASSIST_POINTS 1
#define CTF_CARRIER_KILL_ASSIST_POINTS 2
#define CTF_CARRIER_DEFENSE_ASSIST_POINTS 1

/**
 * @brief The distance from a flag or a carrier within which a kill defends it, as Quake III's
 * CTF measures it.
 */
#define CTF_DEFENSE_RADIUS 1000.f

/**
 * @brief How long a client that hurt a carrier stays a danger to it.
 */
#define CTF_CARRIER_DANGER_MILLIS 8000

/**
 * @brief How long before a capture a return or a carrier kill earns an assist.
 */
#define CTF_ASSIST_MILLIS 10000

/**
 * @brief Returns the team that owns the given flag entity, or `NULL` if the entity is not a flag.
 */
GameTeam *G_TeamForFlag(const GameEntity *ent) {

  if (!ent->item || ent->item->def.type != ITEM_TYPE_FLAG) {
    return NULL;
  }

  for (int32_t i = 0; i < gameLevel.numTeams; i++) {

    if (!Str_Compare(ent->classname, gameTeamList[i].flag)) {
      return &gameTeamList[i];
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

  for (int32_t i = 0; i < gameLevel.numTeams; i++) {

    if (&gameTeamList[i] == cl->persistent.team) {
      continue;
    }

    GameEntity *f = G_FlagForTeam(&gameTeamList[i]);

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

  f->serverFlags &= ~SVF_NO_CLIENT;
  f->s.event = EV_ITEM_RESPAWN;
  f->s.eventData = f->item->def.tag;
  f->solid = SOLID_TRIGGER;

  gi.LinkEntity(f);

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.return_,
    .flags = SOUND_RELATIVE
  }, MULTICAST_ALL_R);

  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n", t->name, t->id + 1);

  G_Ai_Profile_Flag(NULL, "reset", t->id);

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

  if (!Str_CaseCompare(name, "flag")) {
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
    gameLevel.captureLimit = G_LevelSetting(g_captureLimit, gameLevel.requested.captureLimit, NULL);

    gi.BroadcastPrint(PRINT_HIGH, "Capture limit has been changed to %d\n", gameLevel.captureLimit);
  }

  return previous.CheckCvars();
}

/**
 * @brief Plays for captures rather than frags, and so does not defer to previous.
 */
static bool G_CheckWinner_Ctf(void) {

  if (gameLevel.captureLimit) {

    for (int32_t i = 0; i < gameLevel.numTeams; i++) {
      if (gameTeamList[i].captures >= gameLevel.captureLimit) {
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

  Str_Append(name, " CTF", size);
}

/**
 * @brief Captures are team deathmatch or team instagib: arena does not apply,
 * and teams are not optional. Replaces rather than qualifies, and so does not
 * defer to previous.
 */
static GamePlayId G_ClampGamePlay_Ctf(GamePlayId gameplay) {

  if ((gameplay & ~GAMEPLAY_TEAMS) == GAMEPLAY_INSTAGIB) {
    return GAMEPLAY_TEAM_INSTAGIB;
  }

  return GAMEPLAY_TEAM_DEATHMATCH;
}

/**
 * @return True if `time` is set and is no more than `millis` ago.
 */
static bool G_Ctf_Recent(uint32_t time, uint32_t millis) {
  return time && time <= gameLevel.time && gameLevel.time - time <= millis;
}

/**
 * @brief Gives the client points, and tells the client why.
 */
static void G_Ctf_Award(GameClient *cl, int16_t points, const char *event, const char *reason) {

  cl->persistent.score += points;

  G_Ai_Profile_Points(cl, event, points);

  G_ClientCenterPrint(cl, "%s ^2+%d", reason, points);
}

/**
 * @return The team's flag where it lies, at its base or dropped, or NULL if it is carried.
 */
static const GameEntity *G_Ctf_LyingFlag(const GameTeam *team) {

  const GameEntity *base = G_FlagForTeam(team);

  if (!base) {
    return NULL;
  }

  if (base->solid == SOLID_TRIGGER) {
    return base;
  }

  G_ForEachEntity(ent, {
    if (ent->item == base->item && (ent->spawnFlags & SF_ITEM_DROPPED)) {
      return ent;
    }
  });

  return NULL;
}

/**
 * @return True if `a` or `b` is within the defense radius of `point`, and in its view.
 */
static bool G_Ctf_Defends(const Vec3 point, const GameEntity *a, const GameEntity *b) {

  const GameEntity *ents[] = { a, b };

  for (size_t i = 0; i < lengthof(ents); i++) {

    if (Vec3_Distance(point, ents[i]->s.origin) > CTF_DEFENSE_RADIUS) {
      continue;
    }

    const CollisionTrace tr = gi.Trace(point, ents[i]->s.origin, Box3_Zero(), NULL, CONTENTS_SOLID);
    if (tr.fraction == 1.f) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Gives the attacker the one bonus, the largest, that a kill of an enemy earns: the
 * kill of a carrier, of a danger to a carrier of the attacker's team, near the attacker's flag,
 * or near a carrier of the attacker's team.
 */
static void G_ClientWillDie_Ctf(GameClient *cl, GameEntity *attacker, uint32_t mod) {

  previous.ClientWillDie(cl, attacker, mod);

  GameClient *killer = attacker ? attacker->client : NULL;

  if (!killer || killer == cl) {
    return;
  }

  const GameTeam *team = killer->persistent.team;

  if (!team || team == cl->persistent.team) {
    return;
  }

  if (G_GetFlag(cl)) {
    killer->persistent.ctf.carrierKillTime = gameLevel.time;
    G_Ctf_Award(killer, CTF_CARRIER_KILL_POINTS, "carrier_kill", "Flag carrier killed");
    return;
  }

  const GameClient *hurt = cl->persistent.ctf.hurtCarrier;

  if (hurt && hurt != killer && hurt->persistent.team == team &&
      G_Ctf_Recent(cl->persistent.ctf.hurtCarrierTime, CTF_CARRIER_DANGER_MILLIS)) {
    killer->persistent.ctf.defendedCarrier = hurt;
    killer->persistent.ctf.defendTime = gameLevel.time;
    G_Ctf_Award(killer, CTF_CARRIER_DANGER_POINTS, "carrier_danger", "Flag carrier defended");
    return;
  }

  const GameEntity *flag = G_Ctf_LyingFlag(team);

  if (flag && G_Ctf_Defends(flag->s.origin, cl->entity, killer->entity)) {
    G_Ctf_Award(killer, CTF_FLAG_DEFENSE_POINTS, "flag_defense", "Flag defended");
    return;
  }

  G_ForEachClient(carrier, {
    if (carrier != killer && carrier->persistent.team == team && G_GetFlag(carrier) &&
        G_Ctf_Defends(carrier->entity->s.origin, cl->entity, killer->entity)) {
      killer->persistent.ctf.defendedCarrier = carrier;
      killer->persistent.ctf.defendTime = gameLevel.time;
      G_Ctf_Award(killer, CTF_CARRIER_DEFENSE_POINTS, "carrier_defense", "Flag carrier defended");
      return;
    }
  });
}

/**
 * @brief Remembers a client that hurts an enemy carrier, which a kill of that client defends.
 */
static bool G_ModifyDamage_Ctf(GameEntity *target, GameEntity *attacker, int32_t *damage, int32_t *knockback) {

  if (!previous.ModifyDamage(target, attacker, damage, knockback)) {
    return false;
  }

  if (target->client && attacker && attacker->client && attacker != target &&
      attacker->client->persistent.team != target->client->persistent.team && G_GetFlag(target->client)) {
    attacker->client->persistent.ctf.hurtCarrier = target->client;
    attacker->client->persistent.ctf.hurtCarrierTime = gameLevel.time;
  }

  return true;
}

/**
 * @brief Forgets what the client did on the last level.
 */
static void G_ClientWillBegin_Ctf(GameClient *cl) {

  memset(&cl->persistent.ctf, 0, sizeof(cl->persistent.ctf));

  previous.ClientWillBegin(cl);
}

/**
 * @brief Forgets the client in what the other clients did, so that a client that takes its slot
 * does not inherit a defense.
 */
static void G_ClientWillDisconnect_Ctf(GameClient *cl) {

  G_ForEachClient(other, {
    if (other->persistent.ctf.hurtCarrier == cl) {
      other->persistent.ctf.hurtCarrier = NULL;
    }
    if (other->persistent.ctf.defendedCarrier == cl) {
      other->persistent.ctf.defendedCarrier = NULL;
    }
  });

  previous.ClientWillDisconnect(cl);
}

/**
 * @brief Gives the carrier and its team their points for a capture. Each teammate also earns
 * one assist, the largest, for a return or a carrier kill shortly before, or for a defense of
 * this carrier during this carry.
 */
static void G_Ctf_AwardCapture(GameClient *carrier) {

  const GameTeam *team = carrier->persistent.team;

  G_Ctf_Award(carrier, CTF_CAPTURE_POINTS, "capture", "Capture");

  G_ForEachClient(cl, {
    if (cl == carrier || cl->persistent.team != team) {
      continue;
    }

    const GameClientCtf *ctf = &cl->persistent.ctf;

    int16_t assist = 0;
    const char *reason = NULL;

    if (G_Ctf_Recent(ctf->carrierKillTime, CTF_ASSIST_MILLIS)) {
      assist = CTF_CARRIER_KILL_ASSIST_POINTS;
      reason = "killing the enemy flag carrier";
    } else if (G_Ctf_Recent(ctf->returnTime, CTF_ASSIST_MILLIS)) {
      assist = CTF_RETURN_ASSIST_POINTS;
      reason = "returning the flag";
    } else if (ctf->defendedCarrier == carrier && ctf->defendTime >= carrier->persistent.ctf.carryTime) {
      assist = CTF_CARRIER_DEFENSE_ASSIST_POINTS;
      reason = "defending the flag carrier";
    }

    if (assist) {
      cl->persistent.assists++;
      gi.BroadcastPrint(PRINT_HIGH, "%s gets an assist for %s\n", cl->persistent.netName, reason);
      G_Ctf_Award(cl, CTF_TEAM_CAPTURE_POINTS + assist, "assist", "Capture assist");
    } else {
      G_Ctf_Award(cl, CTF_TEAM_CAPTURE_POINTS, "team_capture", "Team capture");
    }
  });
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
      teamFlag->serverFlags &= ~SVF_NO_CLIENT;

      gi.LinkEntity(teamFlag);

      teamFlag->s.event = EV_ITEM_RESPAWN;
      teamFlag->s.eventData = teamFlag->item->def.tag;

      G_MulticastSound(&(const GamePlaySound) {
        .index = module.return_,
        .flags = SOUND_RELATIVE
      }, MULTICAST_ALL_R);

      gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag :flag%d_return:\n", cl->persistent.netName, team->name, team->id + 1);

      G_Ai_Profile_Flag(cl, "return", team->id);

      cl->persistent.ctf.returnTime = gameLevel.time;
      G_Ctf_Award(cl, CTF_RETURN_POINTS, "return", "Flag returned");

      return true;
    }

    if (carriedFlag) {
      const GameTeam *otherTeam = &gameTeamList[carriedFlag->def.tag - FLAG_FIRST];
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
        otherTeamFlag->serverFlags &= ~SVF_NO_CLIENT; // reset the other flag

        gi.LinkEntity(otherTeamFlag);

        otherTeamFlag->s.event = EV_ITEM_RESPAWN;
        otherTeamFlag->s.eventData = otherTeamFlag->item->def.tag;

        G_MulticastSound(&(const GamePlaySound) {
          .index = module.capture,
          .flags = SOUND_RELATIVE
        }, MULTICAST_ALL_R);

        gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag :flag%d_capture:\n", cl->persistent.netName, otherTeam->name, otherTeam->id + 1);

        G_Ai_Profile_Flag(cl, "capture", otherTeam->id);

        team->captures++;
        cl->persistent.captures++;

        G_Ctf_AwardCapture(cl);

        {
          const bool playerAi = cl->ai != NULL;
          GameCapture capture = {
            .playerAi = playerAi,
            .time = (uint32_t) time(NULL),
          };
          Str_Copy(capture.level,       gameLevel.name,              sizeof(capture.level));
          Str_Copy(capture.player,      cl->persistent.netName,   sizeof(capture.player));
          Str_Copy(capture.playerGuid, cl->persistent.guid,       sizeof(capture.playerGuid));
          Str_Copy(capture.team,        otherTeam->name,          sizeof(capture.team));

          if (capture.playerGuid[0]) {
            $(gameLevel.captures, add, &capture);
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
  teamFlag->serverFlags |= SVF_NO_CLIENT;

  gi.LinkEntity(teamFlag);

  index = teamFlag->item->def.tag;
  cl->inventory[index] = 1;

  // link the flag model to the player
  cl->entity->s.model3 = teamFlag->item->modelIndex;

  G_MulticastSound(&(const GamePlaySound) {
    .index = module.steal,
    .flags = SOUND_RELATIVE
  }, MULTICAST_ALL_R);

  gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag :flag%d_steal:\n", cl->persistent.netName, team->name, team->id + 1);

  G_Ai_Profile_Flag(cl, ent->spawnFlags & SF_ITEM_DROPPED ? "pickup" : "steal", team->id);

  cl->persistent.ctf.carryTime = gameLevel.time;

  if (!(ent->spawnFlags & SF_ITEM_DROPPED)) {
    G_Ctf_Award(cl, CTF_STEAL_POINTS, "steal", "Flag stolen");
  }

  cl->entity->s.effects |= G_EffectForTeam(team);
  return true;
}

/**
 * @brief Sheds the carried flag's effects and announces it, then puts the flag
 * into the world. The caller owns the inventory bookkeeping.
 */
static GameEntity *G_ReleaseFlag(GameClient *cl, const GameItem *flag) {

  const GameTeam *team = &gameTeamList[flag->def.tag - FLAG_FIRST];

  cl->entity->s.model3 = 0;
  cl->entity->s.effects &= ~EF_CTF_MASK;

  gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag :flag%d_drop:\n", cl->persistent.netName, team->name, team->id + 1);

  G_Ai_Profile_Flag(cl, "drop", team->id);

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

    if (flagTeam >= gameLevel.numTeams) {
      ent->serverFlags |= SVF_NO_CLIENT;
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

    G_ClampGamePlay = G_ClampGamePlay_Ctf;

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

    previous.ModifyDamage = G_ModifyDamage;
    G_ModifyDamage = G_ModifyDamage_Ctf;

    previous.ClientWillBegin = G_ClientWillBegin;
    G_ClientWillBegin = G_ClientWillBegin_Ctf;

    previous.ClientWillDie = G_ClientWillDie;
    G_ClientWillDie = G_ClientWillDie_Ctf;

    previous.ClientWillDisconnect = G_ClientWillDisconnect;
    G_ClientWillDisconnect = G_ClientWillDisconnect_Ctf;
  }

  g_captureLimit = gi.AddCvar("g_captureLimit", "default", CVAR_SERVER_INFO, "The capture limit per level: \"default\" defers to the level, else 8.");

  g_captureLimit->modified = false;
}
