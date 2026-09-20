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
 * @see Pm_CheckGround
 */
static void G_CheckGround(GameEntity *ent) {
  Vec3 pos;

  if (ent->moveType == MOVE_TYPE_WALK) {
    return;
  }

  // check for ground interaction
  if (ent->moveType == MOVE_TYPE_BOUNCE) {

    pos = ent->s.origin;
    pos.z -= PM_GROUND_DIST;

    CmTrace trace = gi.Trace(ent->s.origin, pos, ent->bounds, ent, ent->clipMask ? : CONTENTS_MASK_SOLID);

    if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {
      if (ent->ground.ent == NULL) {
        G_Debug("%s meeting ground %s\n", etos(ent), etos((GameEntity *) trace.ent));
      }
      ent->ground = trace;
    } else {
      if (ent->ground.ent) {
        G_Debug("%s leaving ground %s\n", etos(ent), etos((GameEntity *) ent->ground.ent));
      }
      memset(&ent->ground, 0, sizeof(ent->ground));
    }
  } else {
    memset(&ent->ground, 0, sizeof(ent->ground));
  }
}

/**
 * @brief Checks and updates the entity's water level, dispatching entry and exit sound effects.
 */
static void G_CheckWater(GameEntity *ent) {
  Vec3 pos;

  if (ent->moveType == MOVE_TYPE_WALK) {
    return;
  }

  if (ent->solid == SOLID_NOT) {
    return;
  }

  // check for water interaction
  const PMoveWaterLevel oldWaterLevel = ent->waterLevel;
  const int32_t oldWaterType = ent->waterType;

  const int32_t water = gi.BoxContents(ent->absBounds) & CONTENTS_MASK_LIQUID;

  ent->waterType = water;
  ent->waterLevel = ent->waterType ? WATER_UNDER : WATER_NONE;

  if (ent->solid == SOLID_BSP) {
    pos = Box3_Center(ent->absBounds);
  } else {
    pos = ent->s.origin;
  }

  const Vec3 top = MakeVec3(pos.x, pos.y, ent->absBounds.maxs.z);
  const Vec3 bot = MakeVec3(pos.x, pos.y, ent->absBounds.mins.z);

  if (oldWaterLevel == WATER_NONE && ent->waterLevel == WATER_UNDER) {

    if (ent->moveType == MOVE_TYPE_BOUNCE) {
      ent->velocity = Vec3_Scale(ent->velocity, 0.66);
    }

    if (!(ent->svFlags & SVF_NO_CLIENT)) {
      const int8_t pitch = ent->waterType & (CONTENTS_LAVA | CONTENTS_SLIME) ? -32 : 0;
      const float gain = Clampf(sqrtf(ent->mass / 200.f), 0.f, 1.f);

      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.waterIn,
        .origin = &pos,
        .pitch = pitch,
        .gain = gain
      }, MULTICAST_PHS);

      if (ent->moveType != MOVE_TYPE_NO_CLIP) {

        const Vec3 pos1 = Vec3_Fmaf(top, -QUETOO_TICK_SECONDS, ent->velocity);
        const Vec3 pos2 = Vec3_Fmaf(bot,  QUETOO_TICK_SECONDS, ent->velocity);

        G_Ripple(ent, pos1, pos2, 0.f, Vec3_Length(ent->velocity) > 100.f);
      }
    }

  } else if (oldWaterLevel == WATER_UNDER && ent->waterLevel == WATER_NONE) {

    if (!(ent->svFlags & SVF_NO_CLIENT)) {
      const int8_t pitch = oldWaterType & (CONTENTS_LAVA | CONTENTS_SLIME) ? -32 : 0;
      const float gain = Clampf(sqrtf(ent->mass / 200.f), 0.f, 1.f);

      G_MulticastSound(&(const GamePlaySound) {
        .index = gMedia.sounds.waterOut,
        .origin = &pos,
        .pitch = pitch,
        .gain = gain
      }, MULTICAST_PHS);

      if (ent->moveType != MOVE_TYPE_NO_CLIP) {

        const Vec3 pos1 = Vec3_Fmaf(top,  QUETOO_TICK_SECONDS, ent->velocity);
        const Vec3 pos2 = Vec3_Fmaf(bot, -QUETOO_TICK_SECONDS, ent->velocity);

        G_Ripple(ent, pos1, pos2, 0.f, false);
      }
    }
  }

  G_CheckItemHazard(ent);
}

/**
 * @brief Runs thinking code for this frame if necessary
 */
void G_RunThink(GameEntity *ent) {

  if (ent->nextThink == 0) {
    return;
  }

  if (ent->nextThink > gLevel.time + 1) {
    return;
  }

  ent->nextThink = 0;

  if (!ent->Think) {
    G_Error("%s has no Think function\n", etos(ent));
  }

  ent->Think(ent);
}

/**
 * @return True if the entity is in a valid position, false otherwise.
 */
static bool G_GoodPosition(const GameEntity *ent) {

  const int32_t mask = ent->clipMask ? : CONTENTS_MASK_SOLID;

  const CmTrace tr = gi.Trace(ent->s.origin, ent->s.origin, ent->bounds, ent, mask);

  return tr.startSolid == false && tr.allSolid == false;
}

/**
 * @return True if the entity is in, or could be moved to, a valid position, false otherwise.
 */
static bool G_CorrectPosition(GameEntity *ent) {

  const int32_t offsets[] = { 0, 1, -1 };

  Vec3 pos = ent->s.origin;

  for (size_t i = 0; i < lengthof(offsets); i++) {
    for (size_t j = 0; j < lengthof(offsets); j++) {
      for (size_t k = 0; k < lengthof(offsets); k++) {

        ent->s.origin = Vec3_Add(pos, MakeVec3(offsets[i], offsets[j], offsets[k]));
        gi.LinkEntity(ent);

        if (G_GoodPosition(ent)) {
          return true;
        }
      }
    }
  }

  ent->s.origin = pos;
  gi.LinkEntity(ent);
  G_Debug("still solid, reverting %s\n", etos(ent));

  return false;
}

#define MAX_SPEED 2400.0
#define STOP_EPSILON PM_STOP_EPSILON

/**
 * @brief Clamps entity velocity to `MAX_SPEED`, or zeroes it when below `STOP_EPSILON`.
 */
static void G_ClampVelocity(GameEntity *ent) {

  const float speed = Vec3_Length(ent->velocity);

  if (speed > MAX_SPEED) {
    ent->velocity = Vec3_Scale(ent->velocity, MAX_SPEED / speed);
  } else if (speed < STOP_EPSILON) {
    ent->velocity = Vec3_Zero();
  }
}

/**
 * @brief Slide off of the impacted plane.
 */
static Vec3 G_ClipVelocity(const Vec3 in, const Vec3 normal, float bounce) {

  float backoff = Vec3_Dot(in, normal);

  if (backoff < 0.0) {
    backoff *= bounce;
  } else {
    backoff /= bounce;
  }

  return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

#define SPEED_STOP 150.0

/**
 * @see Pm_Friction
 */
static void G_Friction(GameEntity *ent) {

  Vec3 vel = ent->velocity;

  if (ent->ground.contents & CONTENTS_MASK_SOLID) {
    vel.z = 0.0;
  }

  const float speed = Vec3_Length(vel);

  if (speed < 1.0) {
    ent->velocity = Vec3_Zero();
    return;
  }

  const float control = Maxf(SPEED_STOP, speed);

  float friction = 0.0;

  if (ent->ground.contents & CONTENTS_MASK_SOLID) {
    if (ent->ground.surface & SURF_SLICK) {
      friction = PM_FRICT_GROUND_SLICK;
    } else {
      friction = PM_FRICT_GROUND;
    }
  } else {
    friction = PM_FRICT_AIR;
  }

  if (ent->waterType) {
    friction += PM_FRICT_WATER;
  }

  float scale = Maxf(0.0, speed - (friction * control * QUETOO_TICK_SECONDS)) / speed;

  ent->velocity = Vec3_Scale(ent->velocity, scale);
  ent->avelocity = Vec3_Scale(ent->avelocity, scale);
}

/**
 * @see Pm_Accelerate
 */
static void G_Accelerate(GameEntity *ent, const Vec3 dir, float speed, float accel) {

  const float currentSpeed = Vec3_Dot(ent->velocity, dir);
  const float addSpeed = speed - currentSpeed;

  if (addSpeed <= 0.0) {
    return;
  }

  float accelSpeed = accel * QUETOO_TICK_SECONDS * speed;

  if (accelSpeed > addSpeed) {
    accelSpeed = addSpeed;
  }

  ent->velocity = Vec3_Fmaf(ent->velocity, accelSpeed, dir);
}

/**
 * @see Pm_Gravity
 */
static void G_Gravity(GameEntity *ent) {

  if (ent->ground.ent == NULL) {
    float gravity = G_LevelGravity();

    if (ent->waterLevel) {
      gravity *= PM_GRAVITY_WATER;
    }

    ent->velocity.z -= gravity * QUETOO_TICK_SECONDS;
  }
}

/**
 * @see Pm_Currents
 */
static void G_Currents(GameEntity *ent) {
  Vec3 current = Vec3_Zero();

  if (ent->waterLevel) {

    if (ent->waterType & CONTENTS_CURRENT_0) {
      current.x += 1.0;
    }
    if (ent->waterType & CONTENTS_CURRENT_90) {
      current.y += 1.0;
    }
    if (ent->waterType & CONTENTS_CURRENT_180) {
      current.x -= 1.0;
    }
    if (ent->waterType & CONTENTS_CURRENT_270) {
      current.y -= 1.0;
    }
    if (ent->waterType & CONTENTS_CURRENT_UP) {
      current.z += 1.0;
    }
    if (ent->waterType & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.0;
    }
  }

  if (ent->ground.ent) {

    if (ent->ground.contents & CONTENTS_CURRENT_0) {
      current.x += 1.0;
    }
    if (ent->ground.contents & CONTENTS_CURRENT_90) {
      current.y += 1.0;
    }
    if (ent->ground.contents & CONTENTS_CURRENT_180) {
      current.x -= 1.0;
    }
    if (ent->ground.contents & CONTENTS_CURRENT_270) {
      current.y -= 1.0;
    }
    if (ent->ground.contents & CONTENTS_CURRENT_UP) {
      current.z += 1.0;
    }
    if (ent->ground.contents & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.0;
    }
  }

  current = Vec3_Scale(current, PM_SPEED_CURRENT);

  const float speed = Vec3_Length(current);

  if (speed == 0.0) {
    return;
  }

  current = Vec3_Normalize(current);

  G_Accelerate(ent, current, speed, PM_ACCEL_GROUND);
}

/**
 * @brief Interact with `BOX_OCCUPY` objects after moving.
 */
void G_TouchOccupy(GameEntity *ent) {
  GameEntity *ents[MAX_ENTITIES];

  switch (ent->solid) {
    case SOLID_PROJECTILE:
    case SOLID_DEAD:
    case SOLID_BOX:
      break;
    default:
      return;
  }

  const size_t len = gi.BoxEntities(ent->absBounds, ents, lengthof(ents), BOX_OCCUPY);
  for (size_t i = 0; i < len; i++) {

    GameEntity *occupied = ents[i];

    if (occupied == ent) {
      continue;
    }

    if (occupied->solid == SOLID_PROJECTILE && occupied->owner == ent) {
      continue;
    }

    if (ent->solid == SOLID_PROJECTILE && occupied->solid == SOLID_PROJECTILE) {
      continue;
    }

    G_Debug("%s occupying %s\n", etos(ent), etos(occupied));

    if (occupied->Touch) {
      occupied->Touch(occupied, ent, NULL);
    }

    if (!ent->inUse) {
      break;
    }
  }
}

/**
 * @brief A moving object that doesn't obey physics
 */
static void G_Physics_NoClip(GameEntity *ent) {

  ent->s.angles = Vec3_Fmaf(ent->s.angles, QUETOO_TICK_SECONDS, ent->avelocity);
  ent->s.origin = Vec3_Fmaf(ent->s.origin, QUETOO_TICK_SECONDS, ent->velocity);

  gi.LinkEntity(ent);
}

/**
 * @brief Maintain a record of all pushed entities for each move, in case that
 * move needs to be reverted.
 */
typedef struct {
  GameEntity *ent;
  Vec3 origin;
  Vec3 angles;
  int16_t deltaYaw;
} GamePush;

static GamePush g_pushes[MAX_ENTITIES], *g_push_p;

/**
 * @brief Records the current origin, angles, and client delta-yaw of the entity
 * on the push stack to allow the move to be reverted if blocked.
 */
static void G_Physics_Push_Impact(GameEntity *ent) {

  if (g_push_p - g_pushes == MAX_ENTITIES) {
    G_Error("MAX_ENTITIES\n");
  }

  g_push_p->ent = ent;

  g_push_p->origin = ent->s.origin;
  g_push_p->angles = ent->s.angles;

  if (ent->client) {
    g_push_p->deltaYaw = ent->client->ps.pmState.deltaAngles.y;
  } else {
    g_push_p->deltaYaw = 0;
  }

  g_push_p++;
}

/**
 * @brief Reverts an entity to its saved origin and angles from the push stack entry.
 */
static void G_Physics_Push_Revert(const GamePush *p) {

  if (!p->ent->inUse) {
    return;
  }

  p->ent->s.origin = p->origin;
  p->ent->s.angles = p->angles;

  if (p->ent->client) {
    p->ent->client->ps.pmState.deltaAngles.y = p->deltaYaw;
  }

  gi.LinkEntity(p->ent);
}

/**
 * @brief When items ride pushers, they rotate along with them. For clients,
 * this requires incrementing their delta angles.
 */
static void G_Physics_Push_Rotate_Entity(GameEntity *self, GameEntity *ent, float yaw) {

  if (ent->ground.ent == self) {
    if (ent->client) {
      ent->client->ps.pmState.deltaAngles.y += yaw;
    } else {
      ent->s.angles.y += yaw;
    }
  }
}

/**
 * @brief Translates a BSP pusher entity by @p move, pushing or carrying any entities in its path.
 * @return The first entity that blocked the move, or `NULL` if the move succeeded.
 */
static GameEntity *G_Physics_Push_Translate(GameEntity *ent, const Vec3 move) {
  GameEntity *ents[MAX_ENTITIES];

  G_Physics_Push_Impact(ent);

  // calculate bounds for the entire move
  Box3 totalBounds = ent->absBounds;

  // unlink the pusher so we don't get it in the entity list
  gi.UnlinkEntity(ent);

  // store original position
  const Vec3 originalPosition = ent->s.origin;

  // move the pusher to it's intended position
  const Vec3 finalPosition = Vec3_Add(originalPosition, move);

  ent->s.origin = finalPosition;

  gi.LinkEntity(ent);

  totalBounds = Box3_Union(totalBounds, ent->absBounds);

  const size_t len = gi.BoxEntities(totalBounds, ents, lengthof(ents), BOX_ALL);

  // see if any solid entities are inside the final position
  for (size_t i = 0; i < len; i++) {

    GameEntity *other = ents[i];

    if (other->solid == SOLID_BSP) {
      continue;
    }

    // Dead entities are not pushed; if the mover has moved into one, obliterate it.
    if (other->solid == SOLID_DEAD) {
      if (!G_GoodPosition(other) && ent->Blocked) {
        ent->Blocked(ent, other);
      }
      continue;
    }

    if (other->moveType < MOVE_TYPE_WALK) {
      continue;
    }

    // if the entity is in a good position and not riding us, we can skip them
    if (G_GoodPosition(other) && other->ground.ent != ent) {
      continue;
    }

    // if we are a pusher, or someone is riding us, try to move them
    if ((ent->moveType == MOVE_TYPE_PUSH) || (other->ground.ent == ent)) {

      G_Physics_Push_Impact(other);

      if (other->ground.ent == ent) {
        // we can only ride a bmodel if we're in a good position
        // on top of it already; to make things simpler, we assume
        // that we're not going to self-intersect with the pusher.

        // move us by the full translation, clipping to the rest of the world,
        // and clip us to where we end up.
        gi.UnlinkEntity(ent);

        const CmTrace tr = gi.Trace(other->s.origin, Vec3_Add(other->s.origin, move), other->bounds, other, other->clipMask ? : CONTENTS_MASK_SOLID);

        gi.LinkEntity(ent);

        other->s.origin = tr.end;

        // if we're good here, we can stop
        if (G_CorrectPosition(other)) {
          continue;
        }

        // we intersected with the mover. we may have been pushed off of us by the world,
        // so try it's original position, which may now be valid.
        G_Physics_Push_Revert(--g_push_p);

        if (G_CorrectPosition(other)) {
          continue;
        }
        
        // no good, we're blocking!
      } else {
        // we're not riding the bmodel, so we must be being pushed by it.
        // trace backwards to find our TOI with the pusher.
        gi.UnlinkEntity(other);

        // restore original position to calculate our hit with it
        ent->s.origin = originalPosition;

        gi.LinkEntity(ent);

        CmTrace tr = gi.Clip(other->s.origin, Vec3_Subtract(other->s.origin, move), other->bounds, ent, other->clipMask ? : CONTENTS_MASK_SOLID);

        // move back to final position
        ent->s.origin = finalPosition;

        gi.LinkEntity(ent);

        // did we even collide with it?
        if (tr.fraction >= 1.0) {
          G_Debug("%s false positive clip\n", etos(ent));
          continue; // was a false positive?
        }

        // didn't hit the mover??
        if (tr.ent == ent) {
          // we did; clip us against the world with the full movement that
          // we need to do
          const float remainingDist = 1.0f - tr.fraction;

          const Vec3 newPosition = Vec3_Fmaf(other->s.origin, remainingDist, Vec3_Multiply(move, Vec3_Fabsf(tr.plane.normal)));

          tr = gi.Trace(other->s.origin, newPosition, other->s.bounds, ent, other->clipMask ? : CONTENTS_MASK_SOLID);
        
          other->s.origin = tr.end;

          // in theory this should never be a bad spot, but who knows
          if (G_CorrectPosition(other)) {
            G_Debug("%s: pushed %s into correct position\n", etos(ent), etos(other));
            continue;
          }
          
          G_Debug("%s: pushed %s into bad position\n", etos(ent), etos(other));
        } else {
          G_Debug("%s -> %s didn't clip?\n", etos(ent), etos(other));
        }
      }
    }

    // try to destroy the impeding entity by calling our Blocked function

    if (ent->Blocked) {
      ent->Blocked(ent, other);
      if (!other->inUse || other->dead) {
        continue;
      }
    }

    G_Debug("%s blocked by %s\n", etos(ent), etos(other));

    // if we've reached this point, we were G_MOVE_TYPE_STOP, or we were
    // blocked: revert any moves we may have made and return our obstacle

    while (g_push_p > g_pushes) {
      G_Physics_Push_Revert(--g_push_p);
    }

    return other;
  }

  // set us in the new position
  ent->s.origin = finalPosition;

  // the move was successful, so re-link all pushed entities
  for (GamePush *p = g_push_p - 1; p >= g_pushes; p--) {
    if (p->ent->inUse) {

      gi.LinkEntity(p->ent);

      G_CheckGround(p->ent);

      G_CheckWater(p->ent);

      G_TouchOccupy(p->ent);
    }
  }

  return NULL;
}

/**
 * @brief Rotates the mover to `angles` and clips `ent` against it there.
 */
static CmTrace G_Physics_Push_Rotate_And_Trace(GameEntity *ent, GameEntity *mover, const Vec3 angles) {
  mover->s.angles = angles;
  
  gi.LinkEntity(mover);

  return gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, mover, ent->clipMask);
}

/**
 * @brief The smallest fraction we care about in rotational
 * TOI precision checking
 */
#define TOI_MIN_FRACTION  0.06f

/**
 * @return The time-of-impact fraction in [0, 1] for the entity against the rotating mover.
 */
static float G_Physics_Push_Calculate_Rotational_TOI(GameEntity *ent, GameEntity *mover, const Vec3 originalAngles, const Vec3 finalAngles, const float left, const float right) {
  const CmTrace leftTr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(originalAngles, finalAngles, left));
  
  const float half = Mixf(left, right, 0.5f);

  const CmTrace halfTr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(originalAngles, finalAngles, half));

  if (leftTr.fraction == 1.f && halfTr.fraction < 1.f) {

    if (half - left < TOI_MIN_FRACTION) {
      return left;
    }

    return G_Physics_Push_Calculate_Rotational_TOI(ent, mover, originalAngles, finalAngles, left, half);
  }

  const CmTrace rightTr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(originalAngles, finalAngles, right));

  if (halfTr.fraction == 1.f && rightTr.fraction < 1.f) {

    if (half - left < TOI_MIN_FRACTION) {
      return half;
    }

    return G_Physics_Push_Calculate_Rotational_TOI(ent, mover, originalAngles, finalAngles, half, right);
  }

  // this is an edge case where both positions are occupied by the mover.
  // should never be possible on any iteration other than the first.
  return 0.f;
}

/**
 * @brief Rotates a BSP pusher entity by @p amove, pushing or carrying any entities in its path.
 * @return The first entity that blocked the move, or `NULL` if the move succeeded.
 */
static GameEntity *G_Physics_Push_Rotate(GameEntity *self, const Vec3 amove) {
  GameEntity *ents[MAX_ENTITIES];

  G_Physics_Push_Impact(self);

  // calculate bounds for the entire move
  Box3 totalBounds = self->absBounds;

  // unlink the pusher so we don't get it in the entity list
  gi.UnlinkEntity(self);

  const Vec3 originalAngles = self->s.angles;

  const Vec3 finalAngles = Vec3_Add(originalAngles, amove);

  // move the pusher to it's intended position
  self->s.angles = finalAngles;

  gi.LinkEntity(self);

  totalBounds = Box3_Union(totalBounds, self->absBounds);

  const size_t len = gi.BoxEntities(totalBounds, ents, lengthof(ents), BOX_ALL);

  // see if any solid entities are inside the final position
  for (size_t i = 0; i < len; i++) {

    GameEntity *ent = ents[i];

    if (ent->solid == SOLID_BSP) {
      continue;
    }

    // Dead entities are not pushed; if the mover has moved into one, obliterate it.
    if (ent->solid == SOLID_DEAD) {
      if (!G_GoodPosition(ent) && self->Blocked) {
        self->Blocked(self, ent);
      }
      continue;
    }

    if (ent->moveType < MOVE_TYPE_WALK) {
      continue;
    }

    // if the entity is in a good position and not riding us, we can skip them
    if (G_GoodPosition(ent) && ent->ground.ent != self) {
      continue;
    }

    // if we are a pusher, or someone is riding us, try to move them
    if ((self->moveType == MOVE_TYPE_PUSH) || (ent->ground.ent == self)) {

      G_Physics_Push_Impact(ent);

      // are we going to collide with the mover?
      // put us to final angles and check for intersection.
      // FIXME: this won't work for larger rotations of thin objects
      // that rotate beyond the bounds of an object.
      self->s.angles = finalAngles;

      gi.LinkEntity(self);

      CmTrace tr = gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, self, ent->clipMask ? : CONTENTS_MASK_SOLID);
      float remainingMove = 1.0f;

      if (tr.fraction < 1.f) {
        // we intersect with the final position, so we're gonna be
        // pushed by the rotator. calculate approximate TOI
        remainingMove = 1.0f - G_Physics_Push_Calculate_Rotational_TOI(ent, self, originalAngles, finalAngles, 0.f, 1.f);

        // put us back to final position
        self->s.angles = finalAngles;

        gi.LinkEntity(self);
      }

      // calculate the rotational matrix for the rotation around the origin
      Vec3 originalEntPosition = ent->s.origin;

      // try a few movements, taking the one that doesn't clip with the mover.
      const int32_t totalMovements = 55;
      int32_t k;

      for (k = 0; k < totalMovements; k++) {
        int32_t offset = (int32_t) ceilf(k * 0.5f);
        if (k & 1) {
          offset = -offset;
        }
        Mat4 m = Mat4_FromTranslation(self->s.origin);
        m = Mat4_ConcatRotation3(m, Vec3_Scale(MakeVec3(amove.z, amove.x, amove.y), remainingMove + (remainingMove * offset * 0.5f)));
        m = Mat4_ConcatTranslation(m, Vec3_Negate(self->s.origin));

        ent->s.origin = Mat4_Transform(m, originalEntPosition);

        if (gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, self, ent->clipMask ? : CONTENTS_MASK_SOLID).fraction == 1.0f) {
          G_Debug("%s rotated %s @ %i, good position\n", etos(self), etos(ent), k);
          break;
        }
      }

      if (k == totalMovements) {
        G_Debug("%s rotated %s, rotational fit failed; %f remaining, trying positional correction\n", etos(self), etos(ent), remainingMove);
      }

      // clip rest of the movement.
      tr = gi.Trace(originalEntPosition, ent->s.origin, ent->bounds, ent, ent->clipMask ? : CONTENTS_MASK_SOLID);

      ent->s.origin = tr.end;

      // if the move has separated us, finish up by rotating the entity
      if (G_CorrectPosition(ent)) {
        G_Physics_Push_Rotate_Entity(self, ent, amove.y);
        continue;
      }

      if (ent->ground.ent == self) {

        // an entity riding us may have been pushed off of us by the world, so try
        // it's original position, which may now be valid

        G_Physics_Push_Revert(--g_push_p);

        // but in this case, don't rotate
        if (G_CorrectPosition(ent)) {
          continue;
        }
      }

      G_Debug("%s rotated %s, but couldn't fit after positional correction; %f was remaining\n", etos(self), etos(ent), remainingMove);

      // Entity is completely stuck inside the pusher; apply lethal crush damage
      // immediately rather than waiting for the throttled G_MoveType_Push_Blocked path.
      if (ent->takeDamage) {
        G_Damage(&(GameDamage) {
          .target = ent,
          .inflictor = self,
          .attacker = self,
          .dir = Vec3_Zero(),
          .point = ent->s.origin,
          .normal = Vec3_Zero(),
          .damage = 999,
          .knockback = 0,
          .mod = MOD_CRUSH
        });
      }
    }

    // try to destroy the impeding entity by calling our Blocked function

    if (self->Blocked) {
      self->Blocked(self, ent);
      if (!ent->inUse || ent->dead) {
        continue;
      }
    }

    G_Debug("%s blocked by %s\n", etos(self), etos(ent));

    // if we've reached this point, we were G_MOVE_TYPE_STOP, or we were
    // blocked: revert any moves we may have made and return our obstacle

    while (g_push_p > g_pushes) {
      G_Physics_Push_Revert(--g_push_p);
    }

    return ent;
  }

  // the move was successful, so re-link all pushed entities
  for (GamePush *p = g_push_p - 1; p >= g_pushes; p--) {
    if (p->ent->inUse) {

      gi.LinkEntity(p->ent);

      G_CheckGround(p->ent);

      G_CheckWater(p->ent);

      G_TouchOccupy(p->ent);
    }
  }

  return NULL;
}

/**
 * @brief For `G_MOVE_TYPE_PUSH`, push all box entities intersected while moving.
 * Generally speaking, only inline BSP models are pushers.
 */
static void G_Physics_Push(GameEntity *ent) {
  GameEntity *obstacle = NULL;

  // for teamed entities, the master must initiate all moves
  if (ent->flags & FL_TEAM_SLAVE) {
    return;
  }

  // reset the pushed array
  g_push_p = g_pushes;

  // make sure all team slaves can move before committing any moves
  for (GameEntity *part = ent; part; part = part->teamNext) {
    if (!Vec3_Equal(part->velocity, Vec3_Zero())) { // object is translating
      const Vec3 move = Vec3_Scale(part->velocity, QUETOO_TICK_SECONDS);

      if ((obstacle = G_Physics_Push_Translate(part, move))) {
        break; // move was blocked
      }
    }

    if (!Vec3_Equal(part->avelocity, Vec3_Zero())) { // object is rotating
      const Vec3 amove = Vec3_Scale(part->avelocity, QUETOO_TICK_SECONDS);

      if ((obstacle = G_Physics_Push_Rotate(part, amove))) {
        break; // move was blocked
      }
    }
  }

  if (!obstacle) { // the move succeeded, so call all think functions
    for (GameEntity *part = ent; part; part = part->teamNext) {
      G_RunThink(part);
    }
  }
}

#define MAX_CLIP_PLANES 4

typedef struct {
  GameEntity *entities[MAX_CLIP_PLANES];
  int32_t numEntities;
} GameTouch;

static GameTouch gTouch;

/**
 * @brief Runs the `Touch` functions of each object.
 */
static void G_TouchEntity(GameEntity *ent, const CmTrace *trace) {

  // ensure that we only impact an entity once per frame

  for (int32_t i = 0; i < gTouch.numEntities; i++) {
    if (gTouch.entities[i] == trace->ent) {
      return;
    }
  }

  gTouch.entities[gTouch.numEntities++] = trace->ent;

  // run the interaction

  GameEntity *other = trace->ent;
  if (ent->Touch) {
    G_Debug("%s touching %s\n", etos(ent), etos(other));
    ent->Touch(ent, other, trace);
  }

  if (ent->inUse && other->inUse) {

    if (other->Touch) {
      G_Debug("%s touching %s\n", etos(other), etos(ent));
      other->Touch(other, ent, trace);
    }
  }
}

/**
 * @see Pm_SlideMove
 */
static bool G_Physics_Fly_Move(GameEntity *ent, const float bounce) {
  Vec3 planes[MAX_CLIP_PLANES];
  Vec3 origin, angles;

  memset(&gTouch, 0, sizeof(gTouch));

  origin = ent->s.origin;
  angles = ent->s.angles;

  const int32_t mask = ent->clipMask ? : CONTENTS_MASK_SOLID;

  float timeRemaining = QUETOO_TICK_SECONDS;
  int32_t numPlanes = 0;

  for (int32_t bump = 0; bump < MAX_CLIP_PLANES; bump++) {
    Vec3 pos;

    if (timeRemaining <= 0.0) {
      break;
    }

    // project desired destination
    pos = Vec3_Fmaf(ent->s.origin, timeRemaining, ent->velocity);

    // trace to it
    const CmTrace trace = gi.Trace(ent->s.origin, pos, ent->bounds, ent, mask);

    // if the entity is trapped in a solid, don't build up Z
    if (trace.allSolid) {
      ent->velocity.z = 0;
      return true;
    }

    const float time = trace.fraction * timeRemaining;

    ent->s.origin = Vec3_Fmaf(ent->s.origin, time, ent->velocity);
    ent->s.angles = Vec3_Fmaf(ent->s.angles, time, ent->avelocity);

    timeRemaining -= time;

    GameEntity *other = trace.ent;
    if (other && other->solid > SOLID_TRIGGER) {

      G_TouchEntity(ent, &trace);

      if (!ent->inUse) {
        return true;
      }

      if (!other->inUse) {
        continue;
      }

      // if both entities remain, clip this entity to the trace entity

      planes[numPlanes] = trace.plane.normal;
      numPlanes++;

      for (int32_t i = 0; i < numPlanes; i++) {

        if (Vec3_Dot(ent->velocity, planes[i]) >= 0.0) {
          continue;
        }

        // slide along the plane
        Vec3 vel = G_ClipVelocity(ent->velocity, planes[i], bounce);

        // see if there is a second plane that the new move enters
        for (int32_t j = 0; j < numPlanes; j++) {
          Vec3 cross;

          if (j == i) {
            continue;
          }

          if (Vec3_Dot(vel, planes[j]) >= 0.0) {
            continue;
          }

          // try clipping the move to the plane
          vel = G_ClipVelocity(vel, planes[j], PM_CLIP_BOUNCE);

          // see if it goes back into the first clip plane
          if (Vec3_Dot(vel, planes[i]) >= 0.0) {
            continue;
          }

          // slide the original velocity along the crease
          cross = Vec3_Cross(planes[i], planes[j]);
          cross = Vec3_Normalize(cross);

          const float scale = Vec3_Dot(cross, ent->velocity);
          vel = Vec3_Scale(cross, scale);

          // see if there is a third plane the the new move enters
          for (int32_t k = 0; k < numPlanes; k++) {

            if (k == i || k == j) {
              continue;
            }

            if (Vec3_Dot(vel, planes[k]) >= 0.0) {
              continue;
            }

            // stop dead at a triple plane interaction
            ent->velocity = Vec3_Zero();
            return true;
          }
        }

        // if we have fixed all interactions, try another move
        ent->velocity = vel;
        break;
      }
    }
  }

  if (!G_CorrectPosition(ent)) {
    G_Debug("reverting %s\n", etos(ent));

    ent->s.origin = origin;
    ent->s.angles = angles;

    ent->velocity = Vec3_Zero();
    ent->avelocity = Vec3_Zero();
  }

  gi.LinkEntity(ent);

  return numPlanes == 0;
}

/**
 * @brief Fly through the world, interacting with other solids.
 */
static void G_Physics_Fly(GameEntity *ent) {

  G_Physics_Fly_Move(ent, 1.0);

  G_CheckGround(ent);

  G_CheckWater(ent);

  G_TouchOccupy(ent);
}

/**
 * @brief Bounce movement. When on ground, do nothing.
 */
static void G_Physics_Bounce(GameEntity *ent) {

  if (ent->ground.ent == NULL || !Vec3_Equal(ent->velocity, Vec3_Zero())) {

    G_Friction(ent);

    G_Gravity(ent);

    G_Currents(ent);

    G_Physics_Fly_Move(ent, 1.33);
  }

  G_CheckGround(ent);

  G_CheckWater(ent);

  G_TouchOccupy(ent);
}

/**
 * @brief Dispatches thinking and physics routines for the specified entity.
 */
void G_RunEntity(GameEntity *ent) {

  G_ClampVelocity(ent);

  G_RunThink(ent);

  switch (ent->moveType) {
    case MOVE_TYPE_NONE:
      break;
    case MOVE_TYPE_NO_CLIP:
      G_Physics_NoClip(ent);
      break;
    case MOVE_TYPE_PUSH:
    case MOVE_TYPE_STOP:
      G_Physics_Push(ent);
      break;
    case MOVE_TYPE_FLY:
      G_Physics_Fly(ent);
      break;
    case MOVE_TYPE_BOUNCE:
      G_Physics_Bounce(ent);
      break;
    default:
      G_Error("Bad move type %i\n", ent->moveType);
  }

  // update BSP sub-model animations based on move state
  if (ent->solid == SOLID_BSP) {
    ent->s.animation1 = ent->moveInfo.state;
  }
}
