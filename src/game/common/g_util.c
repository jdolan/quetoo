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
#include "bg_pmove.h"

/**
 * @brief Appends a spawn point to `spawns`, allocating the vector on first use.
 */
void G_AddSpawn(Vector **spawns, GameEntity *spot) {

  if (!*spawns) {
    *spawns = $(alloc(Vector), initWithSize, sizeof(GameEntity *));
  }

  $(*spawns, add, &spot);
}

/**
 * @brief Appends every entity of the given class to `spawns`.
 */
void G_CollectSpawns(const char *className, Vector **spawns) {

  GameEntity *spot = NULL;

  while ((spot = G_Find(spot, EOFS(classname), className)) != NULL) {
    G_AddSpawn(spawns, spot);
  }
}

/**
 * @brief Copies collected spawn points into an array that lives for the level.
 */
void G_SetSpawnPoints(GameSpawnPoints *points, const Vector *spawns) {

  points->count = spawns ? (int32_t) spawns->count : 0;

  if (points->count) {
    points->spots = gi.Malloc(sizeof(GameEntity *) * points->count, MEM_TAG_GAME_LEVEL);

    for (uint32_t i = 0; i < (uint32_t) points->count; i++) {
      points->spots[i] = VectorValue(spawns, GameEntity *, i);
    }
  } else {
    points->spots = NULL;
  }
}

/**
 * @brief The standing player box under the level's movement. A client in hand
 * has its own in `ps.pmState.params`; this is for when there is none.
 */
Box3 G_PlayerBounds(void) {

  const PMovementInfo *movement = Pm_Movement(gLevel.movement);

  return movement->params ? movement->params->bounds : PM_BOUNDS;
}

/**
 * @brief Initializes a player spawn point entity.
 * @details This used to nudge the spawn preview up and forward, dating from when
 * Quake II BSPs were supported and a spawn point meant something else. Both
 * nudges were scaled by a global box scale that was always 1, so both were
 * `ceilf(0)` and neither moved anything.
 */
void G_InitPlayerSpawn(GameEntity *ent) {

  if (!q_strcmp(ent->classname, "info_player_intermission")) {
    G_Ai_DropItemLikeNode(ent);
  }
}

/**
 * @brief Determines the initial position and directional vectors of a projectile.
 */
void G_ClientProjectile(const GameClient *cl, Vec3 *forward, Vec3 *right, Vec3 *up, Vec3 *org, float hand) {

  // resolve the projectile destination
  const Vec3 start = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 end = Vec3_Fmaf(start, MAX_WORLD_DIST, cl->forward);
  const CmTrace tr = gi.Trace(start, end, Box3_Zero(), cl->entity, CONTENTS_MASK_CLIP_PROJECTILE);

  // resolve the projectile origin
  Vec3 entForward, entRight, entUp;
  Vec3_Vectors(cl->angles, &entForward, &entRight, &entUp);

  // use the client-supplied muzzle offset if valid
  const Vec3 muzzle = cl->cmd.muzzle;
  const float muzzleLen = Vec3_Length(muzzle);
  if ((cl->cmd.buttons & BUTTON_ATTACK) && muzzleLen > 0.f && muzzleLen <= 64.f) {
    *org = Vec3_Add(cl->entity->s.origin, muzzle);
  } else {
    *org = Vec3_Fmaf(start, 24.f, entForward);

    switch (cl->persistent.hand) {
      case HAND_RIGHT:
        *org = Vec3_Fmaf(*org, +6.f * hand, entRight);
        break;
      case HAND_LEFT:
        *org = Vec3_Fmaf(*org, -6.f * hand, entRight);
        break;
      default:
        break;
    }

    *org = Vec3_Fmaf(*org, -12.f, entUp);
  }

  const CmTrace check = gi.Trace(*org, tr.end, Box3f(8.f, 8.f, 8.f), cl->entity, CONTENTS_MASK_CLIP_PROJECTILE);
  if (Vec3_Distance(tr.end, check.end) > 16.f) {
    *org = start;
  }

  if (forward) {
    // return the projectile's directional vectors
    *forward = Vec3_Subtract(tr.end, *org);
    *forward = Vec3_Normalize(*forward);

    const Vec3 euler = Vec3_Euler(*forward);
    Vec3_Vectors(euler, NULL, right, up);
  }
}

/**
 * @brief Searches all active entities for the next one that holds the matching string
 * at field offset (use the `ELOFS` macro) in the structure.
 *
 * Searches beginning at the entity after from, or the beginning if `NULL`
 * `NULL` will be returned if the end of the list is reached.
 *
 * Example:
 *   `G_Find(NULL, EOFS(classname), "info_player_deathmatch")`
 *
 */
GameEntity *G_Find(GameEntity *from, ptrdiff_t field, const char *match) {
  
  for (int32_t i = from ? from->s.number + 1 : 0; i < sv_maxEntities->integer; i++) {

    GameEntity *ent = ge.entities[i];
    if (!ent->inUse) {
      continue;
    }
    char *s = *(char **) ((byte *) ent + field);
    if (!s) {
      continue;
    }
    if (!q_strcasecmp(s, match)) {
      return ent;
    }
  }

  return NULL;
}

#define MAX_TARGETS 8

/**
 * @brief Searches all active entities for the next targeted one.
 */
GameEntity *G_PickTarget(const char *targetName) {
  GameEntity *choice[MAX_TARGETS];
  int32_t numChoices = 0;

  if (!targetName) {
    G_Debug("NULL target_name\n");
    return NULL;
  }

  GameEntity *ent = NULL;
  while (true) {

    ent = G_Find(ent, EOFS(targetName), targetName);

    if (!ent) {
      break;
    }

    choice[numChoices++] = ent;

    if (numChoices == MAX_TARGETS) {
      break;
    }
  }

  if (!numChoices) {
    G_Debug("Target %s not found\n", targetName);
    return NULL;
  }

  return choice[RandomRangeu(0, numChoices)];
}

/**
 * @brief Fires targets on behalf of an entity after a delay.
 */
static void G_UseTargets_Delay(GameEntity *ent) {
  G_UseTargets(ent, ent->activator);
  G_FreeEntity(ent);
}

/**
 * @brief Search for all entities that the specified entity targets, and call their
 * use functions. Set their activator to our activator. Print our message,
 * if set, to the activator.
 */
void G_UseTargets(GameEntity *ent, GameEntity *activator) {

  // check for a delay
  if (ent->delay) {
    // create a temp entity to fire at a later time
    GameEntity *temp = G_AllocEntity(__func__);
    temp->nextThink = gLevel.time + ent->delay * 1000;
    temp->Think = G_UseTargets_Delay;
    temp->activator = activator;
    if (!activator) {
      G_Debug("No activator for %s\n", etos(ent));
    }
    temp->def = ent->def;
    temp->message = ent->message;
    temp->target = ent->target;
    return;
  }

  // print the message
  if ((ent->message) && activator->client) {

    gi.WriteByte(SV_CMD_CENTER_PRINT);
    gi.WriteString(ent->message);
    gi.Unicast(activator->client, true);

    G_UnicastSound(&(const GamePlaySound) {
      .index = ent->sound ?: gMedia.sounds.chat,
    }, activator->client, true);
  }

  // kill kill_targets
  const char *killTarget = gi.EntityValue(ent->def, "killtarget")->nullableString;
  if (killTarget) {
    GameEntity *target = NULL;
    while ((target = G_Find(target, EOFS(targetName), killTarget))) {
      G_FreeEntity(target);
      if (!ent->inUse) {
        G_Debug("%s was removed while using kill_targets\n", etos(ent));
        return;
      }
    }
  }

  // fire targets
  if (ent->target) {
    GameEntity *target = NULL;
    while ((target = G_Find(target, EOFS(targetName), ent->target))) {

      if (target == ent) {
        G_Debug("%s tried to use itself\n", etos(ent));
        continue;
      }

      if (target->Use) {
        target->Use(target, ent, activator);
        if (!ent->inUse) { // see if our target freed us
          G_Debug("%s was removed while using targets\n", etos(ent));
          break;
        }
      }
    }
  }
}

/**
 * @brief Derives and sets a movement direction from the entity's angles, then clears the angles.
 */
void G_SetMoveDir(GameEntity *ent) {

  const Vec3 anglesUp = MakeVec3(0.0, -1.0, 0.0);
  const Vec3 dirUp = MakeVec3(0.0, 0.0, 1.0 );
  const Vec3 anglesDown = MakeVec3(0.0, -2.0, 0.0);
  const Vec3 dirDown = MakeVec3(0.0, 0.0, -1.0);

  if (Vec3_Equal(ent->s.angles, anglesUp)) {
    ent->moveDir = dirUp;
  } else if (Vec3_Equal(ent->s.angles, anglesDown)) {
    ent->moveDir = dirDown;
  } else {
    Vec3_Vectors(ent->s.angles, &ent->moveDir, NULL, NULL);
  }

  ent->s.angles = Vec3_Zero();
}

/**
 * @brief Allocates the entity at the specified index, which must be free.
 */
GameEntity *G_AllocEntityAt(int32_t number, const char *classname) {
  static uint8_t nextSpawnId;

  if (number < 0 || number >= sv_maxEntities->integer) {
    G_Error("Entity %d out of range (sv_maxEntities=%d)\n", number, sv_maxEntities->integer);
  }

  GameEntity *e = ge.entities[number];

  if (e->inUse) {
    G_Error("Entity %d is already in use: %s\n", number, etos(e));
  }

  e->classname = classname;
  e->inUse = true;
  e->waterLevel = WATER_UNKNOWN;
  e->timestamp = gLevel.time;
  e->s.number = number;
  e->s.spawnId = nextSpawnId++;

  return e;
}

/**
 * @brief Allocates an entity for use.
 */
GameEntity *G_AllocEntity(const char *classname) {

  for (int32_t i = 0; i < sv_maxEntities->integer; i++) {

    if (!ge.entities[i]->inUse) {
      return G_AllocEntityAt(i, classname);
    }
  }

  G_Error("No free entities\n");
}

/**
 * @brief Clears any cached reference to the given entity, on any other entity.
 * Must be called before an entity is freed: its slot stays allocated and gets
 * recycled, so any lingering pointer to it (a bot's target, a projectile's
 * enemy, an entity it's standing on, etc.) would otherwise dangle or silently
 * come to mean something else entirely.
 */
void G_InvalidateEntityReferences(const GameEntity *ent) {

  G_ForEachEntity(other, {

    if (other == ent) {
      continue;
    }

    if (other->owner == ent) {
      other->owner = NULL;
    }
    if (other->enemy == ent) {
      other->enemy = NULL;
    }
    if (other->activator == ent) {
      other->activator = NULL;
    }
    if (other->targetEnt == ent) {
      other->targetEnt = NULL;
    }
    if (other->ground.ent == ent) {
      other->ground.ent = NULL;
    }

    if (other->client) {
      if (other->client->heldGrenade == ent) {
        other->client->heldGrenade = NULL;
      }
      if (other->client->ai) {
        G_Ai_InvalidateReferences(other->client->ai, ent);
      }
    }
  });
}

/**
 * @brief Frees the specified entity.
 */
void G_FreeEntity(GameEntity *ent) {

  if (ent->classname) {
    G_Debug("%s\n", etos(ent));
  }

  G_InvalidateEntityReferences(ent);

  gi.UnlinkEntity(ent);

  memset(ent, 0, sizeof(*ent));
}

/**
 * @brief Kills all entities that would touch the proposed new positioning of the entity.
 * FIXME gibs randomly, need to fix this
 * @remarks This doesn't work correctly for rotating BSP entities.
 */
void G_KillBox(GameEntity *ent) {
  GameEntity *ents[MAX_ENTITIES];

  const Box3 bounds = Box3_Translate(ent->bounds, ent->s.origin);

  size_t i, len = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_COLLIDE);
  for (i = 0; i < len; i++) {

    if (ents[i] == ge.entities[0]) {
      continue;
    }

    if (ents[i] == ent) {
      continue;
    }

    if (G_IsMeat(ents[i])) {

      G_Damage(&(GameDamage) {
        .target = ents[i],
        .inflictor = NULL,
        .attacker = ent,
        .dir = Vec3_Zero(),
        .point = ents[i]->s.origin,
        .normal = Vec3_Zero(),
        .damage = 999,
        .knockback = 0,
        .flags = DMG_NO_GOD,
        .mod = MOD_TELEFRAG
      });

      if (ents[i]->inUse && !ents[i]->dead) {
        break;
      }
    } else {
      break;
    }
  }

  if (i < len && ents[i] != ent) {
    if (G_IsMeat(ent)) {
      G_Damage(&(GameDamage) {
        .target = ent,
        .inflictor = NULL,
        .attacker = ents[i],
        .dir = Vec3_Zero(),
        .point = ent->s.origin,
        .normal = Vec3_Zero(),
        .damage = 999,
        .knockback = 0,
        .flags = DMG_NO_GOD,
        .mod = MOD_ACT_OF_GOD
      });
    }
  }
}

/**
 * @brief Kills the specified entity via explosion, potentially taking nearby
 * entities with it. Certain pickup items are reset after exploding.
 */
void G_Explode(GameEntity *ent, int16_t damage, int16_t knockback, float radius, uint32_t mod) {

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_EXPLOSION);
  gi.WritePosition(ent->s.origin);
  gi.WriteDir(Vec3_Up());
  gi.Multicast(ent->s.origin, MULTICAST_PHS);

  G_RadiusDamage(ent, ent, NULL, damage, knockback, radius, mod ?: MOD_EXPLOSIVE);

  const GameItem *item = ent->item;
  if (item) {
    G_ResetDroppedItem(ent);
  } else {
    G_FreeEntity(ent);
  }
}

/**
 * @brief Kills the specified entity via gib effect.
 */
void G_Gib(GameEntity *ent) {

  gi.WriteByte(SV_CMD_TEMP_ENTITY);
  gi.WriteByte(TE_GIB);
  gi.WritePosition(ent->s.origin);
  gi.Multicast(ent->s.origin, MULTICAST_PVS);

  G_FreeEntity(ent);
}

/**
 * @brief Returns the `gGameplayModes` entry whose `->name` matches the given
 * cvar string, case-insensitively. Anything that doesn't match - including
 * empty strings, garbage, and "default" itself - falls back to the table's
 * first entry (plain deathmatch, no teams). "default" is deliberately not a
 * row in the table: it is the cvar's own sentinel for "defer to map metadata",
 * handled by callers that check for it explicitly before ever calling this.
 * @details Falls back to a legacy abbreviation match - an optional "team_"
 * prefix, then the first 5 characters of what remains against "insta" or
 * "arena" - before giving up, so values like "insta" or "team_insta" that
 * predate this table (and are shorter than the canonical "instagib") still
 * resolve, exactly as the original hand-rolled parser accepted them.
 */
const Gameplay *G_GameplayByName(const char *c) {

  if (c && *c) {
    char lower[64];
    q_strlcpy(lower, c, sizeof(lower));
    for (char *p = lower; *p; p++) {
      *p = (char) tolower((unsigned char) *p);
    }

    for (size_t i = 0; i < lengthof(gGameplayModes); i++) {
      if (!q_strcmp(lower, gGameplayModes[i].name)) {
        return &gGameplayModes[i];
      }
    }

    const char *mode = lower;
    int32_t id = GAMEPLAY_DEATHMATCH;

    if (!q_strncmp(lower, "team_", 5)) {
      id |= GAMEPLAY_TEAMS;
      mode = lower + 5;
    }

    if (!q_strncmp(mode, "insta", 5)) {
      id |= GAMEPLAY_INSTAGIB;
    } else if (!q_strncmp(mode, "arena", 5)) {
      id |= GAMEPLAY_ARENA;
    }

    return G_GameplayById((GameplayId) id);
  }

  return &gGameplayModes[0];
}

/**
 * @brief Returns the `gGameplayModes` entry for the given mode id. Used
 * after `G_ClampGameplay`, which operates on the scalar id, to recover the
 * `->name` and `->label` for the id it decided on.
 * @details A module's `ClampGameplay` MUST only ever return an id that is
 * actually one of the six rows in `gGameplayModes`, so this should never
 * miss; it falls back to the first entry rather than asserting, matching
 * `G_GameplayByName`'s own fallback.
 */
const Gameplay *G_GameplayById(GameplayId id) {

  for (size_t i = 0; i < lengthof(gGameplayModes); i++) {
    if (gGameplayModes[i].id == id) {
      return &gGameplayModes[i];
    }
  }

  return &gGameplayModes[0];
}

/**
 * @brief Finds a team by name, performing a case-insensitive strip-compare.
 * @return The matching team, or `NULL` if not found.
 */
GameTeam *G_TeamByName(const char *c) {

  if (!c || !*c) {
    return NULL;
  }

  for (int32_t i = 0; i < gLevel.numTeams; i++) {

    if (!q_strcolorcmp(gTeamList[i].name, c)) {
      return &gTeamList[i];
    }
  }

  return NULL;
}

/**
 * @brief Returns the number of players currently assigned to the given team.
 */
size_t G_TeamSize(const GameTeam *team) {
  size_t count = 0;

  G_ForEachClient(cl, {
    if (cl->persistent.team == team) {
      count++;
    }
  });

  return count;
}

/**
 * @brief Returns the team with the fewest players, used for auto-assignment.
 * @return The smallest team, or `NULL` if no teams are active.
 */
GameTeam *G_SmallestTeam(void) {

  GameTeam *smallest = NULL;
  size_t size = SIZE_MAX;

  GameTeam *team = gTeamList;
  for (int32_t i = 0; i < gLevel.numTeams; i++, team++) {
    const size_t s = G_TeamSize(team);
    if (s < size) {
      smallest = team;
      size = s;
    }
  }

  return smallest;
}

/**
 * @brief Finds a client by name using a case-insensitive strip-compare.
 * @return The best-matching client, or `NULL` if not found.
 */
GameClient *G_ClientByName(char *name) {

  GameClient *client = NULL;
  int32_t match = INT32_MAX;

  G_ForEachClient(cl, {
    const int32_t m = q_strcmp(name, cl->persistent.netName);
    if (m < match) {
      client = cl;
      match = m;
    }
  });

  return client;
}

/**
 * @return True if the specified entity should bleed when damaged.
 */
bool G_IsMeat(const GameEntity *ent) {

  if (!ent || !ent->inUse) {
    return false;
  }

  if (ent->solid == SOLID_BOX && ent->client) {
    return true;
  }

  if (ent->solid == SOLID_DEAD) {
    return true;
  }

  return false;
}

/**
 * @return True if the specified entity is likely stationary.
 */
bool G_IsStationary(const GameEntity *ent) {

  if (!ent || !ent->inUse) {
    return false;
  }

  if (ent->moveType) {
    return false;
  }

  if (!Vec3_Equal(Vec3_Zero(), ent->velocity)) {
    return false;
  }

  if (!Vec3_Equal(Vec3_Zero(), ent->avelocity)) {
    return false;
  }

  return true;
}

/**
 * @return True if the specified entity and surface are structural.
 */
bool G_IsStructural(const CmTrace *trace) {

  if ((trace->contents & CONTENTS_MASK_SOLID) && !G_IsSky(trace)) {
    return true;
  }

  return false;
}

/**
 * @return True if the specified entity and surface are sky.
 */
bool G_IsSky(const CmTrace *trace) {
  return trace->surface & SURF_SKY;
}

/**
 * @brief Writes the specified animation byte, toggling the high bit to restart the
 * sequence if desired and necessary.
 */
static void G_SetAnimation_(byte *dest, EntityAnimation anim, bool restart) {

  if (restart) {
    if (*dest == anim) {
      anim |= ANIM_TOGGLE_BIT;
    }
  }

  *dest = anim;
}

/**
 * @brief Assigns the specified animation to the correct member(s) on the specified
 * entity. If requested, the current animation will be restarted.
 */
void G_SetAnimation(GameClient *cl, EntityAnimation anim, bool restart) {

  // certain sequences go to both torso and leg animations

  if (anim < ANIM_TORSO_GESTURE) {
    G_SetAnimation_(&cl->entity->s.animation1, anim, restart);
    G_SetAnimation_(&cl->entity->s.animation2, anim, restart);
    return;
  }

  // while most go to one or the other, and are throttled

  if (anim < ANIM_LEGS_WALKCR) {
    if (restart || cl->animation1Time <= gLevel.time) {
      G_SetAnimation_(&cl->entity->s.animation1, anim, restart);
      cl->animation1Time = gLevel.time + 50;
    }
  } else {
    if (restart || cl->animation2Time <= gLevel.time) {
      G_SetAnimation_(&cl->entity->s.animation2, anim, restart);
      cl->animation2Time = gLevel.time + 50;
    }
  }
}

/**
 * @brief Returns true if the entity is currently using the specified animation.
 */
bool G_IsAnimation(GameClient *cl, EntityAnimation anim) {
  byte a;

  if (anim < ANIM_LEGS_WALK) {
    a = cl->entity->s.animation1;
  } else {
    a = cl->entity->s.animation2;
  }

  return (a & ANIM_MASK_VALUE) == anim;
}

/**
 * @brief Send a centerprint to everyone on the supplied team
 */
void G_TeamCenterPrint(const GameTeam *team, const char *fmt, ...) {
  char string[MAX_STRING_CHARS];
  va_list args;

  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  G_ForEachClient(cl, {
    if (cl->persistent.team == team) {
      gi.WriteByte(SV_CMD_CENTER_PRINT);
      gi.WriteString(string);
      gi.Unicast(cl, true);
    }
  });
}
