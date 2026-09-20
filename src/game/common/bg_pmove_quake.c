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

#include "bg_pmove.h"
#include "bg_pmove_local.h"

/**
 * @file
 * @brief QuakeWorld's movement, `PM_MOVEMENT_QUAKE`.
 *
 * Ported from id's `QW/client/pmove.c`, which QuakeWorld shared between its
 * client and its server for exactly the reason we do. It is QuakeWorld's rather
 * than NetQuake's: the two differ, and it is QuakeWorld that people played and
 * that bunny hopping belongs to. NetQuake would be a separate movement.
 *
 * The bunny hop is not emulated, it falls out. `Pm_QuakeAirAccelerate` caps the
 * *wished* speed at 30 units, but scales the acceleration by the uncapped wish
 * speed, so a player already faster than 30 in the direction they are looking
 * can still gain the full 30 along a direction they are not - which is what
 * strafing while airborne does. Nothing here special-cases it.
 *
 * There is no ducking. Quake had none, and adding one would not be Quake, so
 * the kernel never raises `PMF_DUCKED` and the standing box is the only box.
 *
 * There is no grappling hook either, for the same reason, so this kernel does
 * not implement the `PM_HOOK_*` movement types. `G_AllowHook` declines the hook
 * for any movement but Quetoo's, which keeps a player from being handed a hook
 * that nothing here would honour.
 *
 * Being finished is the point: this file matches what it imitates and should
 * not acquire improvements. A change to how Quake moves is a new movement.
 */

/**
 * @brief The parameters that make this QuakeWorld rather than merely
 * QuakeWorld-shaped, from `sv_main.c`'s movement variables and `pmove.c`'s
 * player box. A server selecting this movement takes these rather than its own
 * movement cvars; they reach the client inside `PMoveState`, like any others.
 */
#define PM_QUAKE_BOUNDS { \
  .mins = { { -16.f, -16.f, -24.f } }, /* player_mins */ \
  .maxs = { {  16.f,  16.f,  32.f } }  /* player_maxs, against Quetoo's 36 */ \
}

#define PM_QUAKE_BOUNDS_DEAD { \
  .mins = { { -16.f, -16.f, -24.f } }, \
  .maxs = { {  16.f,  16.f,  -4.f } }  /* Quetoo's corpse: QuakeWorld resized nothing on death */ \
}

const PMoveParams pmQuakeParams = {
  .gravity = 800,               // sv_gravity
  .accelGround = 10.f,         // sv_accelerate
  .accelGroundSlick = 10.f,   // unused: Quake has no slick surfaces
  .accelAir = 10.f,            // sv_accelerate again; PM_AirMove passes it to both
  .accelWater = 10.f,          // sv_wateraccelerate
  .accelSpectator = 10.f,
  .accelLadder = 10.f,         // unused: Quake has no ladders
  .frictionGround = 4.f,       // sv_friction
  .frictionGroundSlick = 0.f, // unused
  .frictionAir = 0.f,          // Quake applies no friction while airborne
  .frictionWater = 4.f,        // sv_waterfriction
  .frictionSpectator = 6.f,    // sv_friction * 1.5, as SpectatorMove had it
  .frictionLadder = 0.f,       // unused
  .speedGround = 320.f,        // sv_maxspeed
  .speedAir = 320.f,
  .speedWater = 320.f,         // the same cap; the water scale is applied below
  .speedLadder = 0.f,          // unused
  .speedSpectator = 500.f,     // sv_spectatormaxspeed
  .speedStop = 100.f,          // sv_stopspeed
  .speedJump = 270.f,
  .speedDucked = 320.f,        // unused: there is no ducking
  .speedDuckStand = 320.f,    // unused
  .speedWaterJump = 310.f,
  .bounds = PM_QUAKE_BOUNDS,
  .boundsDucked = PM_QUAKE_BOUNDS, // never ducks, so never consulted
  .boundsDead = PM_QUAKE_BOUNDS_DEAD // a corpse is Quetoo's; QuakeWorld had none
};

#define PM_QUAKE_STEP_SIZE       18.f  // STEPSIZE
#define PM_QUAKE_CLIP_PLANES     5     // MAX_CLIP_PLANES
#define PM_QUAKE_BUMPS           4     // numbumps
#define PM_QUAKE_STOP_EPSILON    .1f   // STOP_EPSILON
#define PM_QUAKE_GROUND_NORMAL   .7f   // the steepest plane that is still floor
#define PM_QUAKE_EDGE_FRICTION   2.f   // the multiplier over a dropoff
#define PM_QUAKE_EDGE_PROBE      16.f  // how far ahead the dropoff is looked for
#define PM_QUAKE_EDGE_DROP       34.f  // and how far down
#define PM_QUAKE_UP_SPEED        180.f // rising faster than this is never grounded
#define PM_QUAKE_AIR_WISH_SPEED  30.f  // the air wish-speed cap the bunny hop lives on
#define PM_QUAKE_WATER_SCALE     .7f   // wish speed is scaled by this while swimming
#define PM_QUAKE_WATER_SINK      60.f  // and drifts downward this fast with no input
#define PM_QUAKE_WATER_UNDER     22.f  // the height at which the view is submerged
#define PM_QUAKE_WATER_JUMP_TIME 2000  // how long a water jump holds control, in ms
#define PM_QUAKE_WATER_JUMP_DIST 24.f  // how far ahead the ledge is looked for
#define PM_QUAKE_WATER_JUMP_PUSH 50.f  // and how hard the player is pushed at it
#define PM_QUAKE_SNAP            8.f   // the network precision origins are cut to
#define PM_QUAKE_VIEW_HEIGHT     22.f  // Quake's eye, against Quetoo's 30

/**
 * @brief Slides `in` along `normal`, killing components that round to nothing.
 * @details QuakeWorld clips without overbounce, unlike Quake II.
 */
static Vec3 Pm_QuakeClipVelocity(const Vec3 in, const Vec3 normal) {

  Vec3 out = Vec3_Subtract(in, Vec3_Scale(normal, Vec3_Dot(in, normal)));

  if (out.x > -PM_QUAKE_STOP_EPSILON && out.x < PM_QUAKE_STOP_EPSILON) {
    out.x = 0.f;
  }
  if (out.y > -PM_QUAKE_STOP_EPSILON && out.y < PM_QUAKE_STOP_EPSILON) {
    out.y = 0.f;
  }
  if (out.z > -PM_QUAKE_STOP_EPSILON && out.z < PM_QUAKE_STOP_EPSILON) {
    out.z = 0.f;
  }

  return out;
}

/**
 * @brief Slides through the world, clipping to every plane struck.
 */
static void Pm_QuakeFlyMove(void) {

  // both are the velocity the move began with, and neither is re-based inside
  // the loop: each bump re-derives the slide from it rather than from the
  // already-clipped velocity, which is what lets a second plane give back speed
  // the first one took
  const Vec3 primalVelocity = pm->s.velocity;
  const Vec3 originalVelocity = pm->s.velocity;

  CmBspPlane planes[PM_QUAKE_CLIP_PLANES];
  int32_t numPlanes = 0;

  float timeLeft = pmLocals.time;

  for (int32_t bump = 0; bump < PM_QUAKE_BUMPS; bump++) {

    const Vec3 end = Vec3_Fmaf(pm->s.origin, timeLeft, pm->s.velocity);
    const CmTrace trace = Pm_Trace(pm->s.origin, end, pm->bounds);

    if (trace.startSolid || trace.allSolid) { // trapped in a solid
      pm->s.velocity = Vec3_Zero();
      return;
    }

    if (trace.fraction > 0.f) { // covered some distance
      pm->s.origin = trace.end;
      numPlanes = 0;
    }

    if (trace.fraction == 1.f) { // moved the entire distance
      break;
    }

    Pm_TouchEntity(&trace);

    timeLeft -= timeLeft * trace.fraction;

    if (numPlanes >= PM_QUAKE_CLIP_PLANES) { // this should not happen
      pm->s.velocity = Vec3_Zero();
      break;
    }

    planes[numPlanes++] = trace.plane;

    // slide along the first plane that the others do not immediately undo
    int32_t i;
    for (i = 0; i < numPlanes; i++) {
      pm->s.velocity = Pm_QuakeClipVelocity(originalVelocity, planes[i].normal);

      int32_t j;
      for (j = 0; j < numPlanes; j++) {
        if (j != i && Vec3_Dot(pm->s.velocity, planes[j].normal) < 0.f) {
          break;
        }
      }

      if (j == numPlanes) {
        break;
      }
    }

    if (i == numPlanes) { // no such plane, so go along the crease
      if (numPlanes != 2) {
        pm->s.velocity = Vec3_Zero();
        break;
      }

      const Vec3 dir = Vec3_Cross(planes[0].normal, planes[1].normal);
      pm->s.velocity = Vec3_Scale(dir, Vec3_Dot(dir, pm->s.velocity));
    }

    // stop dead rather than oscillate in a sloping corner
    if (Vec3_Dot(pm->s.velocity, primalVelocity) <= 0.f) {
      pm->s.velocity = Vec3_Zero();
      break;
    }
  }

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    pm->s.velocity = primalVelocity;
  }
}

/**
 * @brief Moves along the ground, taking whichever of the flat and the stepped
 * candidate travels farther.
 */
static void Pm_QuakeGroundMove(void) {

  pm->s.velocity.z = 0.f;

  if (Vec3_Equal(pm->s.velocity, Vec3_Zero())) {
    return;
  }

  // try moving straight there first
  const Vec3 dest = MakeVec3(pm->s.origin.x + pm->s.velocity.x * pmLocals.time,
                           pm->s.origin.y + pm->s.velocity.y * pmLocals.time,
                           pm->s.origin.z);

  CmTrace trace = Pm_Trace(pm->s.origin, dest, pm->bounds);
  if (trace.fraction == 1.f) {
    pm->s.origin = trace.end;
    return;
  }

  const Vec3 original = pm->s.origin;
  const Vec3 originalVelocity = pm->s.velocity;

  Pm_QuakeFlyMove();

  const Vec3 down = pm->s.origin;
  const Vec3 downVelocity = pm->s.velocity;

  pm->s.origin = original;
  pm->s.velocity = originalVelocity;

  // and again from a step height up
  trace = Pm_Trace(pm->s.origin, Vec3_Fmaf(pm->s.origin, PM_QUAKE_STEP_SIZE, Vec3_Up()),
                   pm->bounds);
  if (!trace.startSolid && !trace.allSolid) {
    pm->s.origin = trace.end;
  }

  Pm_QuakeFlyMove();

  // press back down the step height
  trace = Pm_Trace(pm->s.origin, Vec3_Fmaf(pm->s.origin, PM_QUAKE_STEP_SIZE, Vec3_Down()),
                   pm->bounds);

  bool useDown = trace.plane.normal.z < PM_QUAKE_GROUND_NORMAL;
  if (!useDown) {
    if (!trace.startSolid && !trace.allSolid) {
      pm->s.origin = trace.end;
    }

    const float downDist = Vec2_DistanceSquared(Vec3_XY(down), Vec3_XY(original));
    const float upDist = Vec2_DistanceSquared(Vec3_XY(pm->s.origin), Vec3_XY(original));

    useDown = downDist > upDist;
  }

  if (useDown) {
    pm->s.origin = down;
    pm->s.velocity = downVelocity;
  } else { // the stepped move went farther, but its vertical speed is the flat one's
    pm->s.velocity.z = downVelocity.z;

    // tell the view how far it climbed, or stairs snap the camera
    pm->step = pm->s.origin.z - pmLocals.previousOrigin.z;
  }
}

/**
 * @brief Bleeds speed off, on the ground and in water. Nothing is bled while
 * airborne, which is the other half of why a bunny hop keeps its speed.
 */
static void Pm_QuakeFriction(void) {

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    return;
  }

  const float speed = Vec3_Length(pm->s.velocity);
  if (speed < 1.f) {
    pm->s.velocity.x = pm->s.velocity.y = 0.f;
    return;
  }

  float friction = pm->s.params.frictionGround;

  if (pm->s.flags & PMF_ON_GROUND) {

    // over a dropoff, friction doubles
    const Vec3 ahead = MakeVec3(pm->s.origin.x + pm->s.velocity.x / speed * PM_QUAKE_EDGE_PROBE,
                              pm->s.origin.y + pm->s.velocity.y / speed * PM_QUAKE_EDGE_PROBE,
                              pm->s.origin.z + pm->bounds.mins.z);
    const Vec3 below = MakeVec3(ahead.x, ahead.y, ahead.z - PM_QUAKE_EDGE_DROP);

    // the hull, as upstream traces it: a point here would find a dropoff far
    // more often than QuakeWorld does, since a box at foot level usually starts
    // solid and so never reports a clean miss
    if (Pm_Trace(ahead, below, pm->bounds).fraction == 1.f) {
      friction *= PM_QUAKE_EDGE_FRICTION;
    }
  }

  float drop = 0.f;

  if (pm->waterLevel >= WATER_WAIST) {
    drop = speed * pm->s.params.frictionWater * (float) pm->waterLevel * pmLocals.time;
  } else if (pm->s.flags & PMF_ON_GROUND) {
    const float control = Maxf(speed, pm->s.params.speedStop);
    drop = control * friction * pmLocals.time;
  }

  pm->s.velocity = Vec3_Scale(pm->s.velocity, Maxf(0.f, speed - drop) / speed);
}

/**
 * @brief Accelerates toward `dir`, up to `speed`.
 */
static void Pm_QuakeAccelerate(const Vec3 dir, float speed, float accel) {

  if (pm->s.type == PM_DEAD || (pm->s.flags & PMF_TIME_WATER_JUMP)) {
    return;
  }

  const float addSpeed = speed - Vec3_Dot(pm->s.velocity, dir);
  if (addSpeed <= 0.f) {
    return;
  }

  const float accelSpeed = Minf(accel * pmLocals.time * speed, addSpeed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accelSpeed, dir);
}

/**
 * @brief Accelerates while airborne, where the wish speed is capped but the
 * acceleration derived from it is not.
 * @details This is the bunny hop. A player moving faster than the cap along
 * `dir` gains nothing, but one moving fast *across* it still has the full cap
 * available, so turning while strafing converts direction into speed. Passing
 * the uncapped `speed` to the acceleration and the capped one to the headroom
 * is what makes that true, and it is why the two are not the same variable.
 */
static void Pm_QuakeAirAccelerate(const Vec3 dir, float speed, float accel) {

  if (pm->s.type == PM_DEAD || (pm->s.flags & PMF_TIME_WATER_JUMP)) {
    return;
  }

  const float wishSpeed = Minf(speed, PM_QUAKE_AIR_WISH_SPEED);

  const float addSpeed = wishSpeed - Vec3_Dot(pm->s.velocity, dir);
  if (addSpeed <= 0.f) {
    return;
  }

  const float accelSpeed = Minf(accel * speed * pmLocals.time, addSpeed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accelSpeed, dir);
}

/**
 * @brief Swims, and steps up out of the water onto a ledge if the move allows.
 */
static void Pm_QuakeWaterMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Vec3 wishVelocity = Vec3_Zero();
  wishVelocity = Vec3_Fmaf(wishVelocity, pm->cmd.forward, pmLocals.forward);
  wishVelocity = Vec3_Fmaf(wishVelocity, pm->cmd.right, pmLocals.right);

  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
    wishVelocity.z -= PM_QUAKE_WATER_SINK; // drift toward the bottom
  } else {
    wishVelocity.z += pm->cmd.up;
  }

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(wishVelocity, &speed);
  speed = Minf(speed, pm->s.params.speedWater) * PM_QUAKE_WATER_SCALE;

  Pm_QuakeAccelerate(dir, speed, pm->s.params.accelWater);

  // assume a stair or a slope, and press down from a step height above
  const Vec3 dest = Vec3_Fmaf(pm->s.origin, pmLocals.time, pm->s.velocity);
  const Vec3 start = MakeVec3(dest.x, dest.y, dest.z + PM_QUAKE_STEP_SIZE + 1.f);

  const CmTrace trace = Pm_Trace(start, dest, pm->bounds);
  if (!trace.startSolid && !trace.allSolid) { // walked up the step
    pm->s.origin = trace.end;
    return;
  }

  Pm_QuakeFlyMove();
}

/**
 * @brief Walks and falls. Gravity is applied here, once, in both cases.
 */
static void Pm_QuakeAirMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  // the wish is horizontal, from a basis flattened rather than projected
  Vec3 forward = MakeVec3(pmLocals.forward.x, pmLocals.forward.y, 0.f);
  Vec3 right = MakeVec3(pmLocals.right.x, pmLocals.right.y, 0.f);

  forward = Vec3_Normalize(forward);
  right = Vec3_Normalize(right);

  Vec3 wishVelocity = Vec3_Zero();
  wishVelocity = Vec3_Fmaf(wishVelocity, pm->cmd.forward, forward);
  wishVelocity = Vec3_Fmaf(wishVelocity, pm->cmd.right, right);
  wishVelocity.z = 0.f;

  // the ground speed bounds the wish in the air as well, because upstream bounds
  // both with `movevars.maxspeed`; `speedAir` is deliberately unread here
  float speed;
  const Vec3 dir = Vec3_NormalizeLength(wishVelocity, &speed);
  speed = Minf(speed, pm->s.params.speedGround);

  const float gravity = pm->s.params.gravity * pmLocals.time;

  if (pm->s.flags & PMF_ON_GROUND) {
    pm->s.velocity.z = 0.f;
    Pm_QuakeAccelerate(dir, speed, pm->s.params.accelGround);
    pm->s.velocity.z -= gravity;
    Pm_QuakeGroundMove();
  } else {
    Pm_QuakeAirAccelerate(dir, speed, pm->s.params.accelAir);
    pm->s.velocity.z -= gravity;
    Pm_QuakeFlyMove();
  }
}

/**
 * @brief Classifies the ground beneath the player and the water around them.
 */
static void Pm_QuakeCategorizePosition(void) {

  // a push, a knockback or a fresh spawn asks the plumbing not to be re-grounded
  // for a moment; QuakeWorld had nothing of the kind, so it says nothing about
  // this, and honouring it is what keeps rocket knockback from being taken
  // straight back off by ground friction
  if (pm->s.flags & PMF_TIME_PUSHED) {
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else if (pm->s.velocity.z > PM_QUAKE_UP_SPEED) { // rising too fast to be standing
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else {
    const Vec3 below = MakeVec3(pm->s.origin.x, pm->s.origin.y, pm->s.origin.z - 1.f);
    const CmTrace trace = Pm_Trace(pm->s.origin, below, pm->bounds);

    if (trace.plane.normal.z < PM_QUAKE_GROUND_NORMAL) { // too steep
      pm->s.flags &= ~PMF_ON_GROUND;
      memset(&pm->ground, 0, sizeof(pm->ground));
    } else {
      pm->s.flags |= PMF_ON_GROUND;
      pm->ground = trace;
      pmLocals.ground = trace;

      if (pm->s.flags & PMF_TIME_WATER_JUMP) {
        pm->s.flags &= ~PMF_TIME_WATER_JUMP;
        pm->s.time = 0;
      }

      if (!trace.startSolid && !trace.allSolid) {
        pm->s.origin = trace.end;
      }
    }

    if (trace.ent) {
      Pm_TouchEntity(&trace);
    }
  }

  pm->waterLevel = WATER_NONE;
  pm->waterType = 0;

  Vec3 point = MakeVec3(pm->s.origin.x, pm->s.origin.y,
                      pm->s.origin.z + pm->bounds.mins.z + 1.f);

  int32_t contents = pm->PointContents(point);
  if (contents & CONTENTS_MASK_LIQUID) {
    pm->waterType = contents;
    pm->waterLevel = WATER_FEET;

    point.z = pm->s.origin.z + (pm->bounds.mins.z + pm->bounds.maxs.z) * .5f;
    contents = pm->PointContents(point);
    if (contents & CONTENTS_MASK_LIQUID) {
      pm->waterLevel = WATER_WAIST;

      point.z = pm->s.origin.z + PM_QUAKE_WATER_UNDER;
      contents = pm->PointContents(point);
      if (contents & CONTENTS_MASK_LIQUID) {
        pm->waterLevel = WATER_UNDER;
        pm->s.flags |= PMF_UNDER_WATER;
      }
    }
  }
}

/**
 * @brief Jumps, or swims upward. Quake's jump adds to the player's vertical
 * speed rather than replacing it, which is what a jump off a lift or a ramp
 * keeps.
 */
static void Pm_QuakeJump(void) {

  if (pm->s.type == PM_DEAD) {
    pm->s.flags |= PMF_JUMP_HELD;
    return;
  }

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    return;
  }

  if (pm->waterLevel >= WATER_WAIST) { // swimming, not jumping
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));

    if (pm->waterType & CONTENTS_LAVA) {
      pm->s.velocity.z = 50.f;
    } else if (pm->waterType & CONTENTS_SLIME) {
      pm->s.velocity.z = 80.f;
    } else {
      pm->s.velocity.z = 100.f;
    }
    return;
  }

  if (!(pm->s.flags & PMF_ON_GROUND)) {
    return;
  }

  if (pm->s.flags & PMF_JUMP_HELD) { // no pogo sticking
    return;
  }

  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  pm->s.velocity.z += pm->s.params.speedJump;

  pm->s.flags |= PMF_JUMPED | PMF_JUMP_HELD;
}

/**
 * @brief Hops out of water onto a ledge the player is swimming into.
 */
static void Pm_QuakeCheckWaterJump(void) {

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    return;
  }

  if (pm->s.velocity.z < -180.f) { // only hop out while moving up
    return;
  }

  Vec3 forward = MakeVec3(pmLocals.forward.x, pmLocals.forward.y, 0.f);
  forward = Vec3_Normalize(forward);

  Vec3 spot = Vec3_Fmaf(pm->s.origin, PM_QUAKE_WATER_JUMP_DIST, forward);
  spot.z += 8.f;

  if (!(pm->PointContents(spot) & CONTENTS_SOLID)) {
    return;
  }

  spot.z += PM_QUAKE_WATER_JUMP_DIST;

  if (pm->PointContents(spot)) { // must be clear above the ledge
    return;
  }

  pm->s.velocity = Vec3_Scale(forward, PM_QUAKE_WATER_JUMP_PUSH);
  pm->s.velocity.z = pm->s.params.speedWaterJump;

  pm->s.flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
  pm->s.time = PM_QUAKE_WATER_JUMP_TIME;
}

/**
 * @brief Looks for a free position within an eighth of a unit of where the
 * player is, in the order QuakeWorld looked.
 * @details Upstream appears to quantize the origin to eighths first, but does
 * not: its `base` is the unquantized origin, its first candidate is `base`
 * itself, and its fallback restores `base`, so the quantized value never
 * survives. This does what it does rather than what it looks like it does.
 *
 * The probe is `pm->Trace` and not `Pm_Trace`, because the latter jitters the
 * start by up to a whole unit to escape a solid - eight times the precision
 * being searched for here, which would report a blocked candidate as free.
 */
static void Pm_QuakeNudgePosition(void) {

  const Vec3 base = pm->s.origin;

  static const float offsets[] = { 0.f, -1.f / PM_QUAKE_SNAP, 1.f / PM_QUAKE_SNAP };

  for (size_t z = 0; z < lengthof(offsets); z++) {
    for (size_t x = 0; x < lengthof(offsets); x++) {
      for (size_t y = 0; y < lengthof(offsets); y++) {
        const Vec3 candidate = MakeVec3(base.x + offsets[x],
                                      base.y + offsets[y],
                                      base.z + offsets[z]);

        if (!pm->Trace(candidate, candidate, pm->bounds).startSolid) {
          pm->s.origin = candidate;
          return;
        }
      }
    }
  }

  pm->s.origin = base;
}

/**
 * @brief Quake's eye height, which is simply a height: there is no duck to lerp
 * between, so nothing here moves. The dead cases are Quetoo's, because a corpse
 * is presentation rather than movement.
 */
static void Pm_QuakeViewOffset(void) {

  if (pm->s.type == PM_DEAD) {
    pm->s.viewOffset.z = (pm->s.flags & PMF_GIBLET) ? 0.f : -16.f;
  } else {
    pm->s.viewOffset.z = PM_QUAKE_VIEW_HEIGHT;
  }
}

/**
 * @brief QuakeWorld's movement, in the order `PMove` ran it.
 */
void Pm_QuakeMove(void) {

  Pm_QuakeViewOffset();

  Pm_QuakeNudgePosition();

  Pm_QuakeCategorizePosition();

  if (pm->waterLevel == WATER_WAIST) {
    Pm_QuakeCheckWaterJump();
  }

  if (pm->s.velocity.z < 0.f && (pm->s.flags & PMF_TIME_WATER_JUMP)) {
    pm->s.flags &= ~PMF_TIME_WATER_JUMP;
    pm->s.time = 0;
  }

  if (pm->cmd.up > 0) {
    Pm_QuakeJump();
  }

  if (pm->s.flags & PMF_TIME_TELEPORT) { // held in place briefly, as Quetoo does
    Pm_QuakeCategorizePosition();
    return;
  }

  Pm_QuakeFriction();

  if (pm->waterLevel >= WATER_WAIST) {
    Pm_QuakeWaterMove();
  } else {
    Pm_QuakeAirMove();
  }

  Pm_QuakeCategorizePosition();

  Pm_CheckViewStep();
}
