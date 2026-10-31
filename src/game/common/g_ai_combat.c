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
#include "bg_pmove.h"

/**
 * @brief Returns true if the given entity is a valid (enemy, alive, boxed) target for the AI.
 */
static inline bool G_Ai_IsTargetable(const GameClient *cl, const GameEntity *other) {

  if (other->client && other->client != cl && other->solid == SOLID_BOX &&
      !G_OnSameTeam(cl, other->client)) {
    return true;
  }

  return false;
}

/**
 * @brief Range constants.
 */
typedef enum {
  RANGE_DONT_CARE = 0,
  RANGE_MELEE = 32,
  RANGE_SHORT = 128,
  RANGE_MED = 512,
  RANGE_LONG = 1024
} GameAiRange;

/**
 * @brief Classifies a distance value into a discrete range category.
 */
static GameAiRange G_Ai_GetRange(const float distance) {
  if (distance < (float) RANGE_MELEE) {
    return RANGE_MELEE;
  } else if (distance < (float) RANGE_SHORT) {
    return RANGE_SHORT;
  } else if (distance < (float) RANGE_MED) {
    return RANGE_MED;
  }

  return RANGE_LONG;
}

/**
 * @brief The factor by which another weapon must outscore the one in hand for a bot to change to
 * it, so that a target near the edge of a range, or a small change in health, does not make the
 * bot change weapons back and forth.
 */
#define AI_WEAPON_CHANGE_MARGIN 1.3f

/**
 * @brief The time in milliseconds that a bot holds a weapon that it changed to before it may
 * change again, as a player does not change weapons more often than this.
 */
#define AI_WEAPON_HOLD_MILLIS 2000

/**
 * @brief The time in milliseconds after a bot last saw its target that it keeps the weapon that it
 * chose for that target, so that losing and finding the target does not change its weapon.
 */
#define AI_WEAPON_KEEP_MILLIS 3000

/**
 * @return A factor from .7 to 1.3 for the given weapon, fixed for the bot's `weaponNoiseSeed`.
 */
static float G_Ai_WeaponNoise(const GameClient *cl, const GameItemTag tag) {

  uint32_t h = cl->ai->weaponNoiseSeed ^ ((uint32_t) tag * 0x9e3779b9u);
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;

  return Mixf(.7f, 1.3f, (float) h * 0x1p-32f);
}

/**
 * @brief Picks a weapon for the AI based on its target
 */
static void G_Ai_PickWeapon(GameClient *cl) {

  cl->ai->weaponCheckTime = gameLevel.time + 250; // don't try again for a bit

  if (cl->weaponChangeTime > gameLevel.time || cl->ai->weaponHoldTime > gameLevel.time) {
    return;
  }

  GameAiRange targRange;

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
    const GameEntity *target = cl->ai->combatTarget.entity.ent;
    targRange = G_Ai_GetRange(Vec3_Distance(cl->entity->s.origin, target->s.origin));
  } else {
    targRange = RANGE_DONT_CARE;
  }

  GameAiItemPick weapons[WEAPON_TOTAL];
  size_t numWeapons = 0;

  const int16_t *inventory = cl->inventory;

  for (GameItemTag t = WEAPON_FIRST; t < WEAPON_LAST; t++) {
    const GameItem *it = &gameItems[t];

    if (!inventory[t]) { // not in stock
      continue;
    }

    if (it->def.ammo) {
      const int32_t ammoHave = inventory[it->def.ammo];
      const int32_t ammoNeed = it->def.quantity;
      if (ammoHave < ammoNeed) {
        continue;
      }
    }

    // calculate weight, start with base weapon priority
    float weight = it->def.priority;

    switch (targRange) { // range bonus
      case RANGE_DONT_CARE:
        break;
      case RANGE_MELEE:
      case RANGE_SHORT:
        if (it->def.flags & WF_SHORT_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
      case RANGE_MED:
        if (it->def.flags & WF_MED_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
      case RANGE_LONG:
        if (it->def.flags & WF_LONG_RANGE) {
          weight *= 2.5f;
        } else {
          weight /= 2.5f;
        }
        break;
    }

    if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {
      if ((cl->ai->combatTarget.entity.ent->health < 25) &&
          (it->def.flags & WF_EXPLOSIVE)) { // bonus for explosive at low enemy health
        weight *= 1.5f;
      }
    }

    // additional penalty for long range + projectile unless explicitly long range
    if (targRange == RANGE_LONG &&
        (it->def.flags & WF_PROJECTILE) &&
        !(it->def.flags & WF_LONG_RANGE)) {
      weight /= 2.f;
    }

    // penalty for explosive weapons at short range
    if ((it->def.flags & WF_EXPLOSIVE) &&
        (targRange != RANGE_DONT_CARE && targRange <= RANGE_SHORT)) {
      weight /= 2.f;
    }

    // penalty for explosive weapons at low cl health
    if ((cl->entity->health < 25) &&
        (it->def.flags & WF_EXPLOSIVE)) {
      weight /= 2.f;
    }

    // less aware bots have noisier weapon scoring
    if (cl->ai->personality.awareness < .5f) {
      weight *= G_Ai_WeaponNoise(cl, t);
    }

    weapons[numWeapons++] = (GameAiItemPick) {
      .item = it,
      .weight = weight
    };
  }

  if (numWeapons <= 1) { // if we only have 1 here, we're already using it
    return;
  }

  qsort(weapons, numWeapons, sizeof(GameAiItemPick), G_Ai_CompareItems);

  const GameAiItemPick *bestWeapon = &weapons[0];

  if (cl->weapon == bestWeapon->item) {
    return;
  }

  for (size_t i = 1; i < numWeapons; i++) {
    if (weapons[i].item == cl->weapon) {
      if (bestWeapon->weight < weapons[i].weight * AI_WEAPON_CHANGE_MARGIN) {
        return;
      }
      if (targRange == RANGE_DONT_CARE &&
          gameLevel.time - cl->ai->combatTargetSeenTime < AI_WEAPON_KEEP_MILLIS) {
        return;
      }
      break;
    }
  }

  G_Ai_Profile_WeaponChange(cl, bestWeapon->item, "pick");

  gi.TokenizeString(va("use %s", bestWeapon->item->def.name));
  ge.ClientCommand(cl);
  cl->ai->weaponCheckTime = gameLevel.time + 300; // don't try again for a bit
  cl->ai->weaponHoldTime = gameLevel.time + AI_WEAPON_HOLD_MILLIS;
  G_Ai_Debug("weapon choice: %s (%d choices)\n", bestWeapon->item->def.name, (int32_t) numWeapons);
}

/**
 * @brief Calculate a priority for the specified target.
 */
static float G_Ai_EnemyPriority(const GameClient *cl, const GameEntity *target,
                                const bool visible) {

  float priority = 1.f;

  // closer enemies are higher priority
  const float dist = Vec3_Distance(cl->entity->s.origin, target->s.origin);
  priority += Clampf(1.f - dist / 1024.f, 0.f, 1.f) * 3.f;

  // low health enemies are higher priority
  if (target->health < 50) {
    priority += 2.f;
  }

  if (G_Ai_IsAttacker(cl, target)) {
    priority += 2.f;
  }

#if defined(G_CTF)
  // flag carriers are highest priority
  if (target->client) {
    const int16_t *inventory = target->client->inventory;
    for (GameItemTag t = FLAG_FIRST; t < FLAG_LAST; t++) {
      if (inventory[t]) {
        priority += 5.f;
        break;
      }
    }
  }
#endif

  return priority;
}

/**
 * @brief The time in milliseconds that a bot keeps an enemy as its target after losing sight of it,
 * so that a brief occlusion does not swing its view away and back.
 */
#define AI_TARGET_MEMORY_MILLIS 750

/**
 * @brief The time in milliseconds after a bot last saw its target within which it may fire.
 */
#define AI_TARGET_SEEN_MILLIS 100

/**
 * @brief The time in milliseconds after a player spawns that bots do not take the player as a
 * target, unless the player attacks them, so that the player sees the level before a bot fires.
 */
#define AI_SPAWN_GRACE_MILLIS 3000

/**
 * @return True if the bot sees the enemy, or the enemy hurt it recently and nothing lies between
 * them, so that a bot shot from behind turns and fights back. An enemy that spawned within
 * `AI_SPAWN_GRACE_MILLIS` is perceived only if it hurt the bot.
 */
static bool G_Ai_Perceives(const GameClient *cl, const GameEntity *other) {

  if (!G_Ai_IsTargetable(cl, other)) {
    return false;
  }

  if (gameLevel.time < other->client->respawnTime + AI_SPAWN_GRACE_MILLIS &&
      !(G_Ai_IsAttacker(cl, other) && cl->ai->attackedTime >= other->client->respawnTime)) {
    return false;
  }

  if (G_Ai_CanSee(cl, other)) {
    return true;
  }

  return G_Ai_IsAttacker(cl, other) && G_Ai_HasLineOfSight(cl, other);
}

/**
 * @brief The distance within which an unarmed bot takes an enemy that has not attacked it as its
 * target. The Blaster is a sidearm: a bot fires it to defend itself, not at an enemy across the
 * map.
 */
#define AI_SIDEARM_RANGE 256.f

/**
 * @brief The multiple of `AI_SIDEARM_RANGE` within which an unarmed bot keeps its target, so that
 * an enemy near the range is not dropped and taken again at each think, which restarts the aim.
 */
#define AI_SIDEARM_KEEP_SCALE 1.5f

/**
 * @return True if the bot would take the enemy as its target: any enemy if the bot is armed, and if
 * it is not, only one that attacked it within `AI_ATTACKER_MILLIS` or stands within
 * `AI_SIDEARM_RANGE`, or `AI_SIDEARM_KEEP_SCALE` of it to `keep` its current target. An unarmed bot
 * that targets an enemy far away turns to aim at it, and so cannot strafe jump to the weapon that
 * it needs, and fires a Blaster that will not kill it.
 */
static bool G_Ai_WouldTarget(const GameClient *cl, const GameEntity *enemy, const bool keep) {

  if (G_Ai_IsArmed(cl) || G_Ai_IsAttacker(cl, enemy)) {
    return true;
  }

  const float range = keep ? AI_SIDEARM_RANGE * AI_SIDEARM_KEEP_SCALE : AI_SIDEARM_RANGE;

  return Vec3_Distance(cl->entity->s.origin, enemy->s.origin) <= range;
}

/**
 * @brief Readies the aim at a new combat target: the flick error of the turn toward it, the lead
 * error, the weapon, the combat style and the reaction time before the bot may fire. Aggressive
 * bots prefer close combat, and cautious bots flank or wander. Skilled bots react faster: from 100
 * to 400 milliseconds, against 500 to 1200 for the least skilled.
 */
static void G_Ai_AcquireTarget(GameClient *cl, const GameEntity *enemy, const float priority) {

  G_Ai_SetEntityGoal(cl, &cl->ai->combatTarget, priority, enemy);
  cl->ai->combatTargetSeenTime = gameLevel.time;
  cl->ai->combatTargetSeenPosition = enemy->s.origin;
  cl->ai->combatTargetLost = false;

  const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 toEnemy = Vec3_Normalize(Vec3_Subtract(Box3_Center(enemy->absBounds), eyeOrigin));
  const float flick = Degrees(acosf(Clampf(Vec3_Dot(cl->forward, toEnemy), -1.f, 1.f))) *
                      Mixf(.25f, .08f, cl->ai->personality.skill);
  const float direction = RandomRadian();

  cl->ai->perceivedVelocity = Vec3_Zero();
  cl->ai->aimError = MakeVec2(sinf(direction) * flick * .6f, cosf(direction) * flick);

  const float leadSpread = Mixf(.3f, .05f, cl->ai->personality.skill);
  cl->ai->leadError = RandomRangef(-leadSpread, leadSpread);

  cl->ai->weaponNoiseSeed = Randomu();

  if (cl->ai->weaponCheckTime < gameLevel.time) {
    G_Ai_PickWeapon(cl);
  }

  if (cl->ai->personality.aggression > .6f) {
    cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_CLOSE :
                                             RandomRangei(AI_COMBAT_CLOSE, AI_COMBAT_TOTAL);
  } else if (cl->ai->personality.aggression < .3f) {
    cl->ai->combatTarget.entity.combatType = Randomb() ? AI_COMBAT_FLANK : AI_COMBAT_WANDER;
  } else {
    cl->ai->combatTarget.entity.combatType = RandomRangei(AI_COMBAT_CLOSE, AI_COMBAT_TOTAL);
  }

  const uint32_t lockMin = (uint32_t) Mixf(500.f, 100.f, cl->ai->personality.skill);
  const uint32_t lockMax = (uint32_t) Mixf(1200.f, 400.f, cl->ai->personality.skill);
  cl->ai->combatTarget.entity.lockOnTime = gameLevel.time + RandomRangeu(lockMin, lockMax);

  if (cl->ai->combatTarget.entity.combatType == AI_COMBAT_FLANK) {
    cl->ai->combatTarget.entity.flankAngle = Randomb() ? -90 : 90;
  }
}

/**
 * @brief Funcgoal that chooses the bot's combat target. It does not move the bot: the state
 * machine decides whether to fight, chase or retreat. A bot keeps a target that it loses sight of
 * for `AI_TARGET_MEMORY_MILLIS`, and takes a target even when it retreats, so that it fires back.
 * An unarmed bot takes only the targets of `G_Ai_WouldTarget`.
 */
uint32_t G_Ai_Target(GameClient *cl, PMoveCmd *cmd) {

  if (G_IsMeat(cl->entity) && cl->entity->dead) {
    return 1;
  }

  if (g_aiNoTarget->integer) {
    G_Ai_ClearGoal(&cl->ai->combatTarget);
    return 1;
  }

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY) {

    const GameEntity *enemy = cl->ai->combatTarget.entity.ent;

    if (!G_Ai_WouldTarget(cl, enemy, true)) {
      G_Ai_ClearGoal(&cl->ai->combatTarget);
      cl->ai->combatTargetLost = false;
    } else if (G_Ai_Perceives(cl, enemy)) {
      cl->ai->combatTargetSeenTime = gameLevel.time;
      cl->ai->combatTargetSeenPosition = enemy->s.origin;
    } else if (!G_Ai_IsTargetable(cl, enemy)) {
      G_Ai_ClearGoal(&cl->ai->combatTarget);
      cl->ai->combatTargetLost = false;
    } else if (gameLevel.time - cl->ai->combatTargetSeenTime > AI_TARGET_MEMORY_MILLIS) {
      G_Ai_ClearGoal(&cl->ai->combatTarget);
      cl->ai->combatTargetLost = true;
    }
  }

  GameEntity *bestEnemy = NULL;
  float bestPriority = 0.f;

  G_ForEachEntity(ent, {
    const bool current = ent == cl->ai->combatTarget.entity.ent;
    if (G_Ai_Perceives(cl, ent) && G_Ai_WouldTarget(cl, ent, current)) {

      const float priority = G_Ai_EnemyPriority(cl, ent, true);
      if (priority > bestPriority) {
        bestPriority = priority;
        bestEnemy = ent;
      }
    }
  });

  if (bestEnemy) {
    const float currentPriority = cl->ai->combatTarget.type == AI_GOAL_ENTITY &&
                                  cl->ai->combatTargetSeenTime == gameLevel.time
        ? G_Ai_EnemyPriority(cl, cl->ai->combatTarget.entity.ent, true)
        : 0.f;

    if (bestPriority > currentPriority + 1.f || cl->ai->combatTarget.type != AI_GOAL_ENTITY) {
      G_Ai_AcquireTarget(cl, bestEnemy, bestPriority);
    }
  }

  return 50;
}

/**
 * @brief The projectile speed that a bot assumes for a weapon with no ballistics type, such as
 * hand grenades, whose speed depends on how long they are held.
 */
#define AI_PROJECTILE_SPEED 1050

/**
 * @return The projectile speed of the given weapon.
 */
static float G_Ai_ProjectileSpeed(const GameItem *weapon) {

  const char *classname = weapon->def.classname;
  const char *name = classname + (Str_HasPrefix(classname, "weapon_") ? strlen("weapon_") : 0);

  return G_Ballistics_Speed(name) ?: AI_PROJECTILE_SPEED;
}

/**
 * @brief The upward speeds that launched and thrown grenades receive on top of their launch speed.
 */
#define AI_GRENADE_LOFT 100.f
#define AI_HAND_GRENADE_LOFT 200.f

/**
 * @brief The range of distances at which a bot lobs grenades at a spot, beyond its own splash.
 */
#define AI_SPAM_MIN_DISTANCE 256.f
#define AI_SPAM_MAX_DISTANCE 1000.f

/**
 * @brief The largest size of a brush entity that a bot will spam, so that it does not lob
 * grenades at the center of a map-wide trigger.
 */
#define AI_SPAM_MAX_SIZE 512.f

/**
 * @brief The grenades that a bot keeps in reserve for combat.
 */
#define AI_SPAM_RESERVE 5

/**
 * @brief The longest time in milliseconds that a burst of grenade spam lasts.
 */
#define AI_SPAM_BURST_MILLIS 3000

/**
 * @brief The view error in degrees within which a bot releases a grenade at its spot.
 */
#define AI_SPAM_AIM_DEGREES 3.f

/**
 * @return The grenade weapon that the bot would spam with, or NULL if it has none to spare.
 */
static const GameItem *G_Ai_SpamWeapon(const GameClient *cl) {

  static const GameItemTag tags[] = {
    WEAPON_GRENADE_LAUNCHER,
    WEAPON_QUAKE_GRENADE_LAUNCHER,
    WEAPON_HAND_GRENADE,
  };

  for (size_t i = 0; i < lengthof(tags); i++) {
    const GameItem *weapon = &gameItems[tags[i]];

    if (cl->inventory[tags[i]] && cl->inventory[weapon->def.ammo] > AI_SPAM_RESERVE) {
      return weapon;
    }
  }

  return NULL;
}

/**
 * @brief Resolves the view angles that lob a grenade from the bot's eye to the given point.
 * @param holdMillis For hand grenades, the time to hold the grenade so that it flies far enough.
 * @return True if a grenade can reach the point.
 */
static bool G_Ai_SpamAngles(const GameClient *cl, const GameItem *weapon, const Vec3 point,
                            Vec3 *angles, uint32_t *holdMillis) {

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const float gravity = G_LevelGravity();
  const float distance = Vec2_Distance(Vec3_XY(eye), Vec3_XY(point));
  const float height = point.z - eye.z;

  if (distance < 1.f || gravity <= 0.f) {
    return false;
  }

  float speed, loft;
  if (weapon->def.flags & WF_TIMED) {
    loft = AI_HAND_GRENADE_LOFT;
    speed = Clampf(sqrtf(gravity * distance) * 1.2f, 500.f, 1200.f);
    *holdMillis = (uint32_t) (speed / 500.f * 1000.f);
  } else {
    loft = AI_GRENADE_LOFT;
    speed = G_Ai_ProjectileSpeed(weapon);
    *holdMillis = 0;
  }

  const float launch = sqrtf(speed * speed + loft * loft);
  const float launchSquared = launch * launch;
  const float lift = gravity * distance * distance + 2.f * height * launchSquared;
  const float discriminant = launchSquared * launchSquared - gravity * lift;

  if (discriminant < 0.f) {
    return false;
  }

  const float elevation = atanf((launchSquared - sqrtf(discriminant)) / (gravity * distance)) -
                          atan2f(loft, speed);
  const Vec3 dir = Vec3_Normalize(Vec3_Subtract(point, eye));

  *angles = MakeVec3(-Degrees(elevation), Vec3_Euler(dir).y, 0.f);
  return true;
}

/**
 * @return True if the entity is a spot where players gather or pass, and so worth spamming.
 */
static bool G_Ai_IsSpamSpot(const GameEntity *ent) {

  const char *classname = ent->classname;

  if (classname == NULL) {
    return false;
  }

  if (Str_HasPrefix(classname, "info_player_")) {
    return Str_Compare(classname, "info_player_intermission") != 0;
  }

  return Str_HasPrefix(classname, "func_") ||
         Str_HasPrefix(classname, "trigger_") ||
         !Str_Compare(classname, "misc_teleporter_dest");
}

/**
 * @brief Picks a random visible spot within grenade range for the bot to spam.
 * @return True if a spot was found.
 */
static bool G_Ai_SpamPoint(const GameClient *cl, const GameItem *weapon, Vec3 *point) {

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  uint32_t count = 0;

  G_ForEachEntity(ent, {
    if (!G_Ai_IsSpamSpot(ent)) {
      continue;
    }

    const Vec3 size = Box3_Size(ent->absBounds);
    if (size.x > AI_SPAM_MAX_SIZE || size.y > AI_SPAM_MAX_SIZE || size.z > AI_SPAM_MAX_SIZE) {
      continue;
    }

    const Vec3 spot = Vec3_Equal(size, Vec3_Zero()) ? ent->s.origin : Box3_Center(ent->absBounds);
    const float distance = Vec3_Distance(eye, spot);
    if (distance < AI_SPAM_MIN_DISTANCE || distance > AI_SPAM_MAX_DISTANCE) {
      continue;
    }

    const CollisionTrace tr = gi.Trace(eye, spot, Box3_Zero(), cl->entity, CONTENTS_MASK_SOLID);
    if (tr.fraction < 1.f && tr.ent != ent) {
      continue;
    }

    Vec3 angles;
    uint32_t holdMillis;
    if (!G_Ai_SpamAngles(cl, weapon, spot, &angles, &holdMillis)) {
      continue;
    }

    if (Randomf() * ++count < 1.f) {
      *point = spot;
    }
  });

  return count > 0;
}

/**
 * @return True if the bot is in a burst of grenade spam.
 */
static inline bool G_Ai_IsSpamming(const GameClient *cl) {
  return cl->ai->spamAmmo > 0;
}

/**
 * @brief Ends the bot's burst of grenade spam, and schedules the next by its aggression.
 */
static void G_Ai_EndSpam(GameClient *cl) {

  const float cooldown = Mixf(15000.f, 5000.f, cl->ai->personality.aggression) *
                         RandomRangef(.75f, 1.25f);

  cl->ai->spamAmmo = 0;
  cl->ai->spamTime = gameLevel.time + (uint32_t) cooldown;
  cl->ai->weaponCheckTime = 0;
}

/**
 * @return True if the bot moves in a way that leaves its view free to spam, which is not the case
 * while it trick-jumps, climbs a ladder or swims.
 */
static bool G_Ai_CanSpam(const GameClient *cl) {

  if (cl->ai->moveTarget.type == AI_GOAL_PATH && cl->ai->moveTarget.path.trickJump) {
    return false;
  }

  if (cl->ps.pmState.flags & PMF_ON_LADDER) {
    return false;
  }

  return cl->entity->waterLevel < WATER_WAIST;
}

/**
 * @return True if the weapon lobs its projectile in an arc: the grenade launchers and hand
 * grenades.
 */
static bool G_Ai_IsLobbed(const GameItem *weapon) {
  return weapon == &gameItems[WEAPON_GRENADE_LAUNCHER] ||
         weapon == &gameItems[WEAPON_QUAKE_GRENADE_LAUNCHER] ||
         weapon == &gameItems[WEAPON_HAND_GRENADE];
}

/**
 * @brief The time step in seconds of the arc that `G_Ai_LobTrace` follows.
 */
#define AI_LOB_STEP .05f

/**
 * @brief The longest flight in seconds that `G_Ai_LobTrace` follows.
 */
#define AI_LOB_SECONDS 2.f

/**
 * @return The trace of the bot's grenade from its eye along its view, on the arc that gravity
 * bends, to where it first strikes: the launch speed and loft of `G_Ai_SpamAngles`, with the hand
 * grenade's speed from `aimHoldMillis`, as `G_FireHandGrenade` throws it.
 */
static CollisionTrace G_Ai_LobTrace(const GameClient *cl) {

  const bool timed = (cl->weapon->def.flags & WF_TIMED) != 0;
  const float speed = timed ? Clampf(cl->ai->aimHoldMillis * .5f, 500.f, 1200.f) :
                              G_Ai_ProjectileSpeed(cl->weapon);
  const float loft = timed ? AI_HAND_GRENADE_LOFT : AI_GRENADE_LOFT;
  const float gravity = G_LevelGravity();

  Vec3 origin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  Vec3 velocity = Vec3_Fmaf(MakeVec3(0.f, 0.f, loft), speed, cl->forward);

  CollisionTrace trace = { .fraction = 1.f, .end = origin };

  for (float t = 0.f; t < AI_LOB_SECONDS; t += AI_LOB_STEP) {

    velocity.z -= gravity * AI_LOB_STEP;
    const Vec3 next = Vec3_Fmaf(origin, AI_LOB_STEP, velocity);

    trace = gi.Trace(origin, next, Box3_Zero(), cl->entity, CONTENTS_MASK_CLIP_PROJECTILE);
    if (trace.fraction < 1.f) {
      break;
    }

    origin = next;
  }

  return trace;
}

/**
 * @brief The distance within which a bot holds fire with an explosive weapon, so that its own
 * splash does not hit it.
 */
#define AI_SELF_SPLASH_DISTANCE 160.f

/**
 * @return The distance from a burst of the weapon within which its splash damages a player, or 0
 * for a weapon without splash, or if teammates take no damage. `G_RadiusDamage` reaches as far as
 * the weapon's radius, and subtracts half a point of damage for each unit from the burst to the
 * player's bounds, so the reach is also at most twice the damage.
 */
static float G_Ai_SplashRadius(const GameItem *weapon) {

  if (!g_friendlyFire->value) {
    return 0.f;
  }

  const Cvar *damage = NULL, *radius = NULL;

  if (weapon == &gameItems[WEAPON_ROCKET_LAUNCHER]) {
    damage = g_balanceRocketlauncherDamage;
    radius = g_balanceRocketlauncherRadius;
  } else if (weapon == &gameItems[WEAPON_GRENADE_LAUNCHER]) {
    damage = g_balanceGrenadelauncherDamage;
    radius = g_balanceGrenadelauncherRadius;
  } else if (weapon == &gameItems[WEAPON_BFG10K]) {
    damage = g_balanceBfgDamage;
    radius = g_balanceBfgRadius;
  } else if (weapon == &gameItems[WEAPON_QUAKE_ROCKET_LAUNCHER]) {
    damage = g_balanceQuakeRocketlauncherDamage;
    radius = g_balanceQuakeRocketlauncherRadius;
  } else if (weapon == &gameItems[WEAPON_QUAKE_GRENADE_LAUNCHER]) {
    damage = g_balanceQuakeGrenadelauncherDamage;
    radius = g_balanceQuakeGrenadelauncherRadius;
  } else {
    return 0.f;
  }

  return Maxf(0.f, Minf(damage->integer * 2.f, radius->value));
}

/**
 * @return True if nothing solid lies between the point and the player, as `G_CanDamage` tests it:
 * to its origin, or to one of four corners around it.
 */
static bool G_Ai_IsExposed(const Vec3 point, const GameEntity *ent) {

  const Vec3 offsets[] = {
    { { 0.f, 0.f, 0.f } },
    { { 15.f, 15.f, 0.f } },
    { { 15.f, -15.f, 0.f } },
    { { -15.f, 15.f, 0.f } },
    { { -15.f, -15.f, 0.f } },
  };

  for (size_t i = 0; i < lengthof(offsets); i++) {
    const Vec3 dest = Vec3_Add(ent->s.origin, offsets[i]);

    if (gi.Trace(point, dest, Box3_Zero(), NULL, CONTENTS_MASK_SOLID).fraction == 1.f) {
      return true;
    }
  }

  return false;
}

/**
 * @return True if a living teammate of the bot is within `reach` of the segment from `start` to
 * `end`, measured to its bounds as `G_RadiusDamage` measures it, and exposed to the nearest point
 * of the segment (`G_Ai_IsExposed`).
 */
static bool G_Ai_IsTeammateNear(const GameClient *cl, const Vec3 start, const Vec3 end,
                                const float reach) {

  const Vec3 dir = Vec3_Subtract(end, start);
  const float length = Vec3_Dot(dir, dir);

  G_ForEachClient(other, {
    const GameEntity *ent = other->entity;

    if (other == cl || !ent || ent->dead || ent->solid != SOLID_BOX || !G_OnSameTeam(cl, other)) {
      continue;
    }

    const float along = Vec3_Dot(Vec3_Subtract(ent->s.origin, start), dir);
    const float t = length > 0.f ? Clampf(along / length, 0.f, 1.f) : 0.f;
    const Vec3 point = Vec3_Fmaf(start, t, dir);

    if (Vec3_Distance(ent->s.origin, point) - Box3_Radius(ent->bounds) < reach &&
        G_Ai_IsExposed(point, ent)) {
      return true;
    }
  });

  return false;
}

/**
 * @return True if the bot's line of fire is clear: no teammate is in it, no teammate is within the
 * splash of a burst at its end (`G_Ai_SplashReach`), or for the BFG, within the reach of its laser
 * along it, and an explosive weapon would not burst within `AI_SELF_SPLASH_DISTANCE` of the bot, on
 * a wall or on the enemy. A lobbed weapon's line of fire is its arc, to where the grenade first
 * strikes. A grenade bounces from a wall, so the burst there is an estimate.
 */
static bool G_Ai_ClearToFire(const GameClient *cl, const GameEntity *enemy) {

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 end = Vec3_Fmaf(eye, MAX_WORLD_DIST, cl->forward);

  const CollisionTrace trace = G_Ai_IsLobbed(cl->weapon) ? G_Ai_LobTrace(cl) :
                               gi.Trace(eye, end, Box3_Zero(), cl->entity,
                                        CONTENTS_MASK_CLIP_PROJECTILE);

  const GameEntity *hit = trace.ent;

  if (hit && hit != enemy && hit->client && G_OnSameTeam(cl, hit->client)) {
    return false;
  }

  if ((cl->weapon->def.flags & WF_EXPLOSIVE) &&
      Vec3_Distance(eye, trace.end) < AI_SELF_SPLASH_DISTANCE) {
    return false;
  }

  const float reach = G_Ai_SplashRadius(cl->weapon);

  if (reach > 0.f && G_Ai_IsTeammateNear(cl, trace.end, trace.end, reach)) {
    return false;
  }

  if (reach > 0.f && cl->weapon == &gameItems[WEAPON_BFG10K] &&
      G_Ai_IsTeammateNear(cl, eye, trace.end, g_balanceBfgRadius->value)) {
    return false;
  }

  return true;
}

/**
 * @brief Lobs grenades at spots where players gather while the bot has no combat target, if its
 * line of fire is clear (`G_Ai_ClearToFire`).
 */
static void G_Ai_Weaponry_spam(GameClient *cl, PMoveCmd *cmd) {

  const GameItem *weapon = G_Ai_SpamWeapon(cl);

  if (!G_Ai_CanSpam(cl)) {
    if (G_Ai_IsSpamming(cl)) {
      G_Ai_EndSpam(cl);
    }
    return;
  }

  if (!G_Ai_IsSpamming(cl)) {
    if (cl->ai->spamTime > gameLevel.time) {
      return;
    }

    G_Ai_EndSpam(cl);

    if (weapon == NULL || !G_Ai_SpamPoint(cl, weapon, &cl->ai->spamPoint)) {
      return;
    }

    cl->ai->spamAmmo = cl->inventory[weapon->def.ammo] - RandomRangei(1, 4);
    cl->ai->spamTime = gameLevel.time + AI_SPAM_BURST_MILLIS;
    G_Ai_Debug("spamming %s\n", weapon->def.name);
  }

  const bool expired = gameLevel.time > cl->ai->spamTime && !cl->grenadeHoldTime;

  if (weapon == NULL || expired || cl->inventory[weapon->def.ammo] <= cl->ai->spamAmmo) {
    G_Ai_EndSpam(cl);
    return;
  }

  if (cl->weapon != weapon) {
    if (cl->ai->weaponCheckTime < gameLevel.time && cl->ai->weaponHoldTime <= gameLevel.time &&
        cl->weaponChangeTime <= gameLevel.time) {
      G_Ai_Profile_WeaponChange(cl, weapon, "spam");

      gi.TokenizeString(va("use %s", weapon->def.name));
      ge.ClientCommand(cl);
      cl->ai->weaponCheckTime = gameLevel.time + 300;
    }
    return;
  }

  Vec3 angles;
  uint32_t holdMillis;
  if (!G_Ai_SpamAngles(cl, weapon, cl->ai->spamPoint, &angles, &holdMillis)) {
    G_Ai_EndSpam(cl);
    return;
  }

  if (cl->grenadeHoldTime) {
    if (gameLevel.time - cl->grenadeHoldTime < holdMillis) {
      cmd->buttons |= BUTTON_ATTACK;
    }
    return;
  }

  const Vec3 view = cl->angles;
  if (fabsf(G_Ai_AngleDelta(view.x, angles.x)) < AI_SPAM_AIM_DEGREES &&
      fabsf(G_Ai_AngleDelta(view.y, angles.y)) < AI_SPAM_AIM_DEGREES &&
      G_Ai_ClearToFire(cl, NULL)) {
    cmd->buttons |= BUTTON_ATTACK;
  }
}

/**
 * @brief The view error in degrees within which a bot fires at its shoot target.
 */
#define AI_SHOOT_AIM_DEGREES 4.f

/**
 * @see g_ai_combat.h
 */
bool G_Ai_CanShootButton(const GameClient *cl, const GameEntity *button) {

  if (!cl->weapon || G_Ai_IsLobbed(cl->weapon)) {
    return false;
  }

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 center = Box3_Center(button->absBounds);

  const CollisionTrace trace = gi.Trace(eye, center, Box3_Zero(), cl->entity,
                                        CONTENTS_MASK_CLIP_PROJECTILE);
  const float impact = Vec3_Distance(eye, trace.end);

  if ((cl->weapon == &gameItems[WEAPON_LIGHTNING] ||
       cl->weapon == &gameItems[WEAPON_QUAKE_THUNDERBOLT]) &&
      impact > g_balanceLightningLength->value) {
    return false;
  }

  return !(cl->weapon->def.flags & WF_EXPLOSIVE) || impact >= AI_SELF_SPLASH_DISTANCE;
}

/**
 * @brief Fires at the bot's shoot target, a button that is pressed by a shot, once the view is on
 * it, with no grenade, and with no splash near the bot.
 */
static void G_Ai_Weaponry_shoot(GameClient *cl, PMoveCmd *cmd) {

  const GameEntity *target = cl->ai->shootTarget;

  if (!cl->weapon || G_Ai_IsLobbed(cl->weapon)) {
    return;
  }

  Vec3 angles;
  G_Ai_ShootViewAngles(cl, &angles);

  if (fabsf(G_Ai_AngleDelta(cl->angles.x, angles.x)) < AI_SHOOT_AIM_DEGREES &&
      fabsf(G_Ai_AngleDelta(cl->angles.y, angles.y)) < AI_SHOOT_AIM_DEGREES &&
      G_Ai_ClearToFire(cl, target)) {
    cmd->buttons |= BUTTON_ATTACK;
  }
}

/**
 * @brief Funcgoal that controls the AI's weaponry. A bot committed to a climb
 * (`G_Ai_IsCommittedToClimb`) does not fire, spam or change its weapon. It keeps a grenade that it
 * holds for its usual hold time, so that the grenade does not go off in its hand, and then throws
 * it where it faces.
 */
uint32_t G_Ai_Weaponry(GameClient *cl, PMoveCmd *cmd) {

  // if we're dead, just keep clicking so we respawn.
  if (cl->entity->dead) {

    if (gameLevel.frameNum & 1) {
      cmd->buttons = BUTTON_ATTACK;
    }
    return 1;
  }

  if (G_Ai_IsCommittedToClimb(cl)) {
    if (G_Ai_IsSpamming(cl)) {
      G_Ai_EndSpam(cl);
    }

    const uint32_t holdMillis = cl->ai->aimHoldMillis ?: RandomRangeu(1500, 2500);
    if (cl->grenadeHoldTime && gameLevel.time - cl->grenadeHoldTime < holdMillis) {
      cmd->buttons |= BUTTON_ATTACK;
    }
    return 1;
  }

  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY || cl->ai->shootTarget) {
    if (G_Ai_IsSpamming(cl)) {
      G_Ai_EndSpam(cl);
    }
  } else {
    G_Ai_Weaponry_spam(cl, cmd);
  }

  if (cl->ai->shootTarget) {
    G_Ai_Weaponry_shoot(cl, cmd);
  }

  if (!G_Ai_IsSpamming(cl) && cl->ai->weaponCheckTime < gameLevel.time) {
    G_Ai_PickWeapon(cl);
  }

  // we're alive - if we're aiming at an enemy, and not at a button, start-a-firin
  if (cl->ai->combatTarget.type == AI_GOAL_ENTITY && !cl->ai->shootTarget) {
    if (cl->ai->combatTarget.entity.lockOnTime < gameLevel.time &&
        gameLevel.time - cl->ai->combatTargetSeenTime <= AI_TARGET_SEEN_MILLIS) {

      const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
      Vec3 toEnemy = Vec3_Normalize(Vec3_Subtract(
          Box3_Center(cl->ai->combatTarget.entity.ent->absBounds), eyeOrigin));

      if (G_Ai_IsLobbed(cl->weapon)) {
        Vec3_Vectors(cl->ai->aimAngles, &toEnemy, NULL, NULL);
      }

      // skilled bots fire with tighter aim cone (10° to 25°)
      const float fireCone = Mixf(25.f, 10.f, cl->ai->personality.skill);
      if (Vec3_Dot(cl->forward, toEnemy) > cosf(Radians(fireCone))) {
        const uint32_t grenadeHoldTime = cl->grenadeHoldTime;
        if (grenadeHoldTime) {
          const uint32_t holdMillis = cl->ai->aimHoldMillis ?: RandomRangeu(1500, 2500);
          if (gameLevel.time - grenadeHoldTime < holdMillis) {
            cmd->buttons |= BUTTON_ATTACK;
          }
        } else if (G_Ai_ClearToFire(cl, cl->ai->combatTarget.entity.ent)) {
          cmd->buttons |= BUTTON_ATTACK;
        }
      }
    }
  }

  return 1;
}

/**
 * @brief The distance around a bot that must have floor, for it to jump to dodge.
 */
#define AI_DODGE_FLOOR_DISTANCE 96.f

/**
 * @brief The height above the bot's origin at which it checks for floor around it, about the top of
 * the arc of a jump.
 */
#define AI_DODGE_HEIGHT 48.f

/**
 * @return True if each of eight directions around the bot has floor within
 * `AI_DODGE_FLOOR_DISTANCE`, or before a wall that stands above the arc of a jump. A jump to dodge
 * checks only its own arc, but the bot keeps moving while in the air, so it MUST NOT dodge near a
 * ledge, on a bridge, or beside a rail that it would jump over.
 */
static bool G_Ai_HasFloorAround(const GameEntity *ent) {

  const Vec3 start = Vec3_Add(ent->s.origin, MakeVec3(0.f, 0.f, AI_DODGE_HEIGHT));

  for (int32_t i = 0; i < 8; i++) {
    const float angle = Radians(i * 45.f);
    const Vec3 end = Vec3_Add(start, MakeVec3(cosf(angle) * AI_DODGE_FLOOR_DISTANCE,
                                              sinf(angle) * AI_DODGE_FLOOR_DISTANCE, 0.f));

    const CollisionTrace tr = gi.Trace(start, end, Box3_Zero(), ent, ent->clipMask);
    const Vec3 point = Vec3_Fmaf(tr.end, -1.f, Vec3_Normalize(Vec3_Subtract(end, start)));
    const Vec3 below = Vec3_Subtract(point,
                                     MakeVec3(0.f, 0.f, AI_DODGE_HEIGHT + PM_STEP_HEIGHT * 4.f));

    if (gi.Trace(point, below, Box3_Zero(), ent, CONTENTS_MASK_SOLID).fraction == 1.f) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Funcgoal that controls the AI's crouch/jumping while hunting.
 */
uint32_t G_Ai_Acrobatics(GameClient *cl, PMoveCmd *cmd) {

  if (G_IsMeat(cl->entity) && cl->entity->dead) {
    return 1;
  }


  if (cl->ai->combatTarget.type != AI_GOAL_ENTITY || G_Ai_IsClimbing(cl)) {
    return 200;
  }

  // do some acrobatics (aggressive bots dodge more)
  const uint32_t crouchFreq = (uint32_t) Mixf(48.f, 20.f, cl->ai->personality.aggression);
  const uint32_t jumpFreq = (uint32_t) Mixf(120.f, 50.f, cl->ai->personality.aggression);

  if (cl->entity->ground.ent) {

    if (cl->ps.pmState.flags & PMF_DUCKED) {

      if ((RandomRangeu(0, crouchFreq)) == 0) { // uncrouch eventually
        cmd->up = 0;
      } else {
        cmd->up = -PM_SPEED_JUMP;
      }
    } else {

      if ((RandomRangeu(0, crouchFreq)) == 0) { // randomly crouch
        cmd->up = -PM_SPEED_JUMP;
      } else if ((RandomRangeu(0, jumpFreq)) == 0 && G_Ai_HasFloorAround(cl->entity) &&
                 !G_Ai_FallsToHazard(cl, cl->entity->s.origin, G_Ai_JumpVelocity(cl))) {
        cmd->up = PM_SPEED_JUMP;
      }
    }
  } else {

    cmd->up = 0;
  }

  return 1;
}

/**
 * @brief The interval in seconds over which the rate of change of the aim angles is measured.
 */
#define AI_AIM_RATE_SECONDS .05f

/**
 * @brief The aim error scales of hitscan weapons. A hitscan weapon hits the instant that it fires,
 * with no lead to misjudge, so a bot aims it worse than a projectile weapon. A weapon made only for
 * long range, the railgun, does the most damage in one hit, so it scales most.
 */
#define AI_AIM_HITSCAN_SCALE 1.5f
#define AI_AIM_SNIPER_SCALE 3.f

/**
 * @return The aim error scale of the weapon.
 */
static float G_Ai_AimScale(const GameItem *weapon) {

  const uint32_t flags = weapon ? weapon->def.flags : 0;

  if (!(flags & WF_HITSCAN)) {
    return 1.f;
  }

  return (flags & (WF_SHORT_RANGE | WF_MED_RANGE)) ? AI_AIM_HITSCAN_SCALE : AI_AIM_SNIPER_SCALE;
}

/**
 * @brief Advances the aim error, a random walk toward zero that widens with the target's angular
 * rate and with the weapon's aim scale, and narrows with skill.
 * @param rate The angular rate of the aim point in degrees per second.
 */
static void G_Ai_UpdateAimError(GameClient *cl, const Vec3 rate, const float seconds) {

  const float skill = cl->ai->personality.skill;
  const float correction = .35f;

  const float angularSpeed = sqrtf(rate.x * rate.x + rate.y * rate.y);
  const float deviation = (Mixf(4.f, .4f, skill) + Mixf(.1f, .015f, skill) * angularSpeed) *
                          G_Ai_AimScale(cl->weapon);
  const float diffusion = deviation * sqrtf(2.f * seconds / correction);

  Vec2 *error = &cl->ai->aimError;

  for (int32_t i = 0; i < 2; i++) {
    const float noise = (Randomf() + Randomf() + Randomf() - 1.5f) * 2.f;
    const float scale = i == 0 ? .6f : 1.f;

    error->xy[i] += -error->xy[i] * seconds / correction + noise * diffusion * scale;
  }
}

/**
 * @brief Points the view at the grenade spam spot while the bot is in a burst of spam.
 */
void G_Ai_SpamViewAngles(const GameClient *cl, Vec3 *angles) {

  if (G_Ai_IsSpamming(cl) && cl->weapon == G_Ai_SpamWeapon(cl)) {
    uint32_t holdMillis;
    G_Ai_SpamAngles(cl, cl->weapon, cl->ai->spamPoint, angles, &holdMillis);
  }
}

/**
 * @see g_ai_combat.h
 */
void G_Ai_ShootViewAngles(const GameClient *cl, Vec3 *angles) {

  if (!cl->ai->shootTarget) {
    return;
  }

  const Vec3 eye = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);

  const Vec3 center = Box3_Center(cl->ai->shootTarget->absBounds);
  *angles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(center, eye)));
  angles->z = 0.f;
}

/**
 * @brief The skill above which a bot aims explosive weapons at the feet of a grounded target.
 */
#define AI_SPLASH_AIM_SKILL .5f

/**
 * @brief Resolves the view angles and their rate of change that aim at the combat target. The
 * aim point lags the target's changes in velocity by the bot's reaction time, plus the aim error.
 */
void G_Ai_CombatAngles(GameClient *cl, const float seconds, Vec3 *angles, Vec3 *rate) {

  const GameEntity *enemy = cl->ai->combatTarget.entity.ent;
  const Vec3 eyeOrigin = Vec3_Add(cl->entity->s.origin, cl->ps.pmState.viewOffset);
  const Vec3 enemyCenter = Box3_Center(enemy->absBounds);

  const float reaction = Mixf(.25f, .05f, cl->ai->personality.skill);
  cl->ai->perceivedVelocity = Vec3_Mix(cl->ai->perceivedVelocity, enemy->velocity,
                                       1.f - expf(-seconds / reaction));

  Vec3 aimPoint = Vec3_Fmaf(enemyCenter, reaction,
                            Vec3_Subtract(cl->ai->perceivedVelocity, enemy->velocity));

  const GameItem *const weapon = cl->weapon;

  if (weapon->def.flags & WF_PROJECTILE) {
    const float dist = Vec3_Distance(eyeOrigin, enemyCenter);
    const float speed = G_Ai_ProjectileSpeed(weapon);
    aimPoint = Vec3_Fmaf(aimPoint, dist / (speed * (1.f + cl->ai->leadError)),
                         cl->ai->perceivedVelocity);
  }

  if ((weapon->def.flags & WF_EXPLOSIVE) && enemy->ground.ent &&
      cl->ai->personality.skill > AI_SPLASH_AIM_SKILL) {
    const float footZ = aimPoint.z - (enemyCenter.z - enemy->absBounds.mins.z);
    const Vec3 feet = MakeVec3(aimPoint.x, aimPoint.y, footZ + 2.f);
    if (gi.Trace(eyeOrigin, feet, Box3_Zero(), cl->entity, CONTENTS_MASK_SOLID).fraction == 1.f) {
      aimPoint = feet;
    }
  }

  uint32_t holdMillis = 0;
  const bool lobbed = G_Ai_IsLobbed(weapon) &&
                      G_Ai_SpamAngles(cl, weapon, aimPoint, angles, &holdMillis);
  if (!lobbed) {
    *angles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(aimPoint, eyeOrigin)));
  }

  cl->ai->aimAngles = *angles;
  cl->ai->aimHoldMillis = holdMillis;

  const Vec3 relativeVelocity = Vec3_Subtract(cl->ai->perceivedVelocity, cl->entity->velocity);
  const Vec3 nextAimPoint = Vec3_Fmaf(aimPoint, AI_AIM_RATE_SECONDS, relativeVelocity);

  Vec3 nextAngles;
  uint32_t nextHoldMillis;
  if (!lobbed || !G_Ai_SpamAngles(cl, weapon, nextAimPoint, &nextAngles, &nextHoldMillis)) {
    nextAngles = Vec3_Euler(Vec3_Normalize(Vec3_Subtract(nextAimPoint, eyeOrigin)));
  }

  for (int32_t i = 0; i < 2; i++) {
    rate->xyz[i] = G_Ai_AngleDelta(angles->xyz[i], nextAngles.xyz[i]) / AI_AIM_RATE_SECONDS;
  }

  G_Ai_UpdateAimError(cl, *rate, seconds);

  angles->x = Clampf(G_Ai_AngleDelta(0.f, angles->x + cl->ai->aimError.x), -90.f, 90.f);
  angles->y += cl->ai->aimError.y;
}
