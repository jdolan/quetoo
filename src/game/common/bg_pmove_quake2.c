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

#include "bg_pmove.h"
#include "bg_pmove_local.h"

/**
 * @file
 * @brief Quake II's movement, `PM_MOVEMENT_QUAKE2`.
 *
 * Ported from id's `qcommon/pmove.c`, which Quake II shared between its client
 * and its server as we do. It is vanilla Quake II: `pm_airaccelerate` was zero
 * out of the box, so the airborne case accelerates plainly at 1 rather than
 * taking QuakeWorld's capped-wish path.
 *
 * That is not the same as having no air control. Quake II strafe jumping is
 * real, and this is where it comes from: with no cap on the wished speed, the
 * headroom is the whole of it, so a command earns its `accel * dt * wishspeed`
 * over a far wider range of angles than QuakeWorld's 30-unit window allows.
 * QuakeWorld gains nothing 45 degrees off its velocity, where Quake II still
 * gains; setting `pm_airaccelerate` would have swapped Quake II onto the
 * narrow window, which is a different ruleset rather than a better Quake II.
 *
 * What separates it from QuakeWorld is more than the numbers: it clips with an
 * overbounce of 1.01 and against the velocity as it stands rather than the one
 * the move began with, probes a quarter of a unit for ground instead of a whole
 * one, refuses to jump for a moment after a hard landing, ducks, climbs
 * ladders, and quantizes both origin and velocity to eighths of a unit on the
 * way out - which is observable in the movement and so belongs to the ruleset.
 *
 * Being finished is the point: this file matches what it imitates and should
 * not acquire improvements. A change to how Quake II moves is a new movement.
 */

/**
 * @brief The parameters that make this Quake II, from `pmove.c`'s movement
 * variables and player box.
 * @details `speed_ground` is Quake II's `pm_maxspeed`, which bounds the wish in
 * water as well; `speed_water` is its `pm_waterspeed`, which is the speed a
 * current pushes at. The two are different constants there and stay different
 * here.
 */
#define PM_QUAKE2_BOUNDS { \
  .mins = { { -16.f, -16.f, -24.f } }, \
  .maxs = { {  16.f,  16.f,  32.f } }  /* against Quetoo's 36 */ \
}

#define PM_QUAKE2_BOUNDS_DUCKED { \
  .mins = { { -16.f, -16.f, -24.f } }, \
  .maxs = { {  16.f,  16.f,   4.f } }  /* against Quetoo's 6 */ \
}

const PlayerMoveParams pmQuake2Params = {
  .gravity = 800,
  .accelGround = 10.f,         // pm_accelerate
  .accelGroundSlick = 10.f,   // Quake II does not accelerate differently on slick
  .accelAir = 1.f,             // the literal 1 the airborne case passes
  .accelWater = 10.f,          // pm_wateraccelerate
  .accelSpectator = 10.f,
  .accelLadder = 10.f,         // pm_accelerate again; ladders share it
  .frictionGround = 6.f,       // pm_friction
  .frictionGroundSlick = 0.f, // slick surfaces are frictionless
  .frictionAir = 0.f,          // no friction while airborne
  .frictionWater = 1.f,        // pm_waterfriction
  .frictionSpectator = 6.f,
  .frictionLadder = 6.f,       // a ladder gets ground friction
  .speedGround = 300.f,        // pm_maxspeed
  .speedAir = 300.f,
  .speedWater = 400.f,         // pm_waterspeed, the push of a current
  .speedLadder = 200.f,        // the vertical speed a ladder climbs at
  .speedSpectator = 500.f,
  .speedStop = 100.f,          // pm_stopspeed
  .speedJump = 270.f,
  .speedDucked = 100.f,        // pm_duckspeed
  .speedDuckStand = 0.f,      // unused: Quake II's duck is instant
  .speedWaterJump = 350.f,
  .bounds = PM_QUAKE2_BOUNDS,        // 32 tall, against Quetoo's 36
  .boundsDucked = PM_QUAKE2_BOUNDS_DUCKED, // and 4, against Quetoo's 6
  .boundsDead = PM_QUAKE2_BOUNDS_DUCKED    // a corpse is simply ducked here
};

#define PM_QUAKE2_STEP_SIZE        18.f  // STEPSIZE
#define PM_QUAKE2_CLIP_PLANES      5     // MAX_CLIP_PLANES
#define PM_QUAKE2_BUMPS            4     // numbumps
#define PM_QUAKE2_STOP_EPSILON     .1f   // STOP_EPSILON
#define PM_QUAKE2_OVERBOUNCE       1.01f // what a clip gives back, unlike QuakeWorld
#define PM_QUAKE2_STEP_NORMAL      .7f   // MIN_STEP_NORMAL
#define PM_QUAKE2_GROUND_NORMAL    .7f   // the steepest plane that is still floor
#define PM_QUAKE2_GROUND_PROBE     .25f  // how far down ground is looked for
#define PM_QUAKE2_UP_SPEED         180.f // rising faster than this is never grounded
#define PM_QUAKE2_WATER_SCALE      .5f   // wish speed is halved while swimming
#define PM_QUAKE2_WATER_SINK       60.f  // and drifts downward this fast with no input
#define PM_QUAKE2_LAND_SPEED      -200.f // landing harder than this locks the jump out
#define PM_QUAKE2_LAND_SPEED_HARD -400.f // and harder than this locks it out longer
#define PM_QUAKE2_LAND_TIME        144   // 18 of Quake II's eight-millisecond ticks
#define PM_QUAKE2_LAND_TIME_HARD   200   // and 25 of them
#define PM_QUAKE2_JUMP_UP_MIN      10    // how far the jump key must be down to count
#define PM_QUAKE2_SWIM_JUMP_MIN   -300.f // sinking faster than this cannot swim upward
#define PM_QUAKE2_LADDER_PROBE     1.f   // how far ahead a ladder is looked for
#define PM_QUAKE2_LADDER_SPEED     25.f  // the horizontal speed a ladder allows
#define PM_QUAKE2_LADDER_HOLD      200.f // the vertical speed at which it holds still
#define PM_QUAKE2_WATER_JUMP_DIST  30.f  // how far ahead the ledge is looked for
#define PM_QUAKE2_WATER_JUMP_UP    4.f   // and how far up
#define PM_QUAKE2_WATER_JUMP_CLEAR 16.f  // and how much clear air it needs above
#define PM_QUAKE2_WATER_JUMP_PUSH  50.f  // how hard the player is pushed at it
#define PM_QUAKE2_WATER_JUMP_TIME  2040  // 255 ticks of no control
#define PM_QUAKE2_CURRENT_SPEED    100.f // the push of a conveyor
#define PM_QUAKE2_SNAP             8.f   // the precision origin and velocity are cut to
#define PM_QUAKE2_VIEW_HEIGHT      22.f  // the eye, against Quetoo's 30
#define PM_QUAKE2_VIEW_HEIGHT_DUCK -2.f  // and ducked
#define PM_QUAKE2_DEAD_FRICTION    20.f  // the speed a corpse sheds each move

/**
 * @brief Slides `in` along `normal`, giving a little back.
 */
static Vec3 Pm_Quake2ClipVelocity(const Vec3 in, const Vec3 normal) {

  const float backoff = Vec3_Dot(in, normal) * PM_QUAKE2_OVERBOUNCE;

  Vec3 out = Vec3_Subtract(in, Vec3_Scale(normal, backoff));

  if (out.x > -PM_QUAKE2_STOP_EPSILON && out.x < PM_QUAKE2_STOP_EPSILON) {
    out.x = 0.f;
  }
  if (out.y > -PM_QUAKE2_STOP_EPSILON && out.y < PM_QUAKE2_STOP_EPSILON) {
    out.y = 0.f;
  }
  if (out.z > -PM_QUAKE2_STOP_EPSILON && out.z < PM_QUAKE2_STOP_EPSILON) {
    out.z = 0.f;
  }

  return out;
}

/**
 * @brief Slides through the world, clipping to every plane struck.
 * @details Unlike QuakeWorld, each plane clips the velocity as it stands rather
 * than the one the move began with, so the clips compound.
 */
static void Pm_Quake2SlideMove(void) {

  const Vec3 primalVelocity = pm->s.velocity;

  CmBspPlane planes[PM_QUAKE2_CLIP_PLANES];
  int32_t numPlanes = 0;

  float timeLeft = pmLocals.time;

  for (int32_t bump = 0; bump < PM_QUAKE2_BUMPS; bump++) {

    const Vec3 end = Vec3_Fmaf(pm->s.origin, timeLeft, pm->s.velocity);
    const CmTrace trace = Pm_Trace(pm->s.origin, end, pm->bounds);

    if (trace.allSolid) { // trapped in a solid
      pm->s.velocity.z = 0.f; // and do not build up falling damage
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

    if (numPlanes >= PM_QUAKE2_CLIP_PLANES) { // this should not happen
      pm->s.velocity = Vec3_Zero();
      break;
    }

    planes[numPlanes++] = trace.plane;

    // slide along the first plane that the others do not immediately undo
    int32_t i;
    for (i = 0; i < numPlanes; i++) {
      pm->s.velocity = Pm_Quake2ClipVelocity(pm->s.velocity, planes[i].normal);

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

  if (pm->s.time) { // a timed move keeps the velocity it was given
    pm->s.velocity = primalVelocity;
  }
}

/**
 * @brief Slides, and again from a step height up, keeping whichever went
 * farther across the ground.
 */
static void Pm_Quake2StepSlideMove(void) {

  const Vec3 startOrigin = pm->s.origin;
  const Vec3 startVelocity = pm->s.velocity;

  Pm_Quake2SlideMove();

  const Vec3 downOrigin = pm->s.origin;
  const Vec3 downVelocity = pm->s.velocity;

  const Vec3 up = MakeVec3(startOrigin.x, startOrigin.y,
                         startOrigin.z + PM_QUAKE2_STEP_SIZE);

  // pm->Trace and not Pm_Trace: the latter jitters the start by up to a unit to
  // escape a solid, so it cannot answer whether the box fits where it is
  if (pm->Trace(up, up, pm->bounds).allSolid) {
    return; // no room to step up
  }

  pm->s.origin = up;
  pm->s.velocity = startVelocity;

  Pm_Quake2SlideMove();

  // and press back down the step height
  const Vec3 down = MakeVec3(pm->s.origin.x, pm->s.origin.y,
                           pm->s.origin.z - PM_QUAKE2_STEP_SIZE);

  const CmTrace trace = Pm_Trace(pm->s.origin, down, pm->bounds);
  if (!trace.allSolid) {
    pm->s.origin = trace.end;
  }

  const float downDist = Vec2_DistanceSquared(Vec3_XY(downOrigin), Vec3_XY(startOrigin));
  const float upDist = Vec2_DistanceSquared(Vec3_XY(pm->s.origin), Vec3_XY(startOrigin));

  if (downDist > upDist || trace.plane.normal.z < PM_QUAKE2_STEP_NORMAL) {
    pm->s.origin = downOrigin;
    pm->s.velocity = downVelocity;
    return;
  }

  // walking along a plane keeps the vertical speed the flat move ended with
  pm->s.velocity.z = downVelocity.z;

  pm->step = pm->s.origin.z - pmLocals.previousOrigin.z;
}

/**
 * @brief Bleeds speed off. Ground and water friction are additive here, unlike
 * QuakeWorld, where the one excludes the other.
 */
static void Pm_Quake2Friction(void) {

  const float speed = Vec3_Length(pm->s.velocity);
  if (speed < 1.f) {
    pm->s.velocity.x = pm->s.velocity.y = 0.f;
    return;
  }

  float drop = 0.f;

  const bool slick = pmLocals.ground.surface & SURF_SLICK;

  if (((pm->s.flags & PMF_ON_GROUND) && !slick) || (pm->s.flags & PMF_ON_LADDER)) {
    const float control = Maxf(speed, pm->s.params.speedStop);
    drop += control * pm->s.params.frictionGround * pmLocals.time;
  }

  if (pm->waterLevel && !(pm->s.flags & PMF_ON_LADDER)) {
    drop += speed * pm->s.params.frictionWater * (float) pm->waterLevel * pmLocals.time;
  }

  pm->s.velocity = Vec3_Scale(pm->s.velocity, Maxf(0.f, speed - drop) / speed);
}

/**
 * @brief Accelerates toward `dir`, up to `speed`.
 */
static void Pm_Quake2Accelerate(const Vec3 dir, float speed, float accel) {

  const float addSpeed = speed - Vec3_Dot(pm->s.velocity, dir);
  if (addSpeed <= 0.f) {
    return;
  }

  const float accelSpeed = Minf(accel * pmLocals.time * speed, addSpeed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accelSpeed, dir);
}

/**
 * @brief Adds ladder, water and conveyor movement to the wish.
 */
static Vec3 Pm_Quake2AddCurrents(Vec3 wish) {

  if ((pm->s.flags & PMF_ON_LADDER) &&
      fabsf(pm->s.velocity.z) <= PM_QUAKE2_LADDER_HOLD) {

    const float speed = pm->s.params.speedLadder;

    if (pm->angles.x <= -15.f && pm->cmd.forward > 0) {
      wish.z = speed;
    } else if (pm->angles.x >= 15.f && pm->cmd.forward > 0) {
      wish.z = -speed;
    } else if (pm->cmd.up > 0) {
      wish.z = speed;
    } else if (pm->cmd.up < 0) {
      wish.z = -speed;
    } else {
      wish.z = 0.f;
    }

    wish.x = Clampf(wish.x, -PM_QUAKE2_LADDER_SPEED, PM_QUAKE2_LADDER_SPEED);
    wish.y = Clampf(wish.y, -PM_QUAKE2_LADDER_SPEED, PM_QUAKE2_LADDER_SPEED);
  }

  if (pm->waterType & CONTENTS_MASK_CURRENT) {
    Vec3 current = Vec3_Zero();

    if (pm->waterType & CONTENTS_CURRENT_0) {
      current.x += 1.f;
    }
    if (pm->waterType & CONTENTS_CURRENT_90) {
      current.y += 1.f;
    }
    if (pm->waterType & CONTENTS_CURRENT_180) {
      current.x -= 1.f;
    }
    if (pm->waterType & CONTENTS_CURRENT_270) {
      current.y -= 1.f;
    }
    if (pm->waterType & CONTENTS_CURRENT_UP) {
      current.z += 1.f;
    }
    if (pm->waterType & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.f;
    }

    float speed = pm->s.params.speedWater;
    if (pm->waterLevel == WATER_FEET && (pm->s.flags & PMF_ON_GROUND)) {
      speed *= .5f;
    }

    wish = Vec3_Fmaf(wish, speed, current);
  }

  if (pm->s.flags & PMF_ON_GROUND) {
    Vec3 current = Vec3_Zero();

    if (pmLocals.ground.contents & CONTENTS_CURRENT_0) {
      current.x += 1.f;
    }
    if (pmLocals.ground.contents & CONTENTS_CURRENT_90) {
      current.y += 1.f;
    }
    if (pmLocals.ground.contents & CONTENTS_CURRENT_180) {
      current.x -= 1.f;
    }
    if (pmLocals.ground.contents & CONTENTS_CURRENT_270) {
      current.y -= 1.f;
    }
    if (pmLocals.ground.contents & CONTENTS_CURRENT_UP) {
      current.z += 1.f;
    }
    if (pmLocals.ground.contents & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.f;
    }

    wish = Vec3_Fmaf(wish, PM_QUAKE2_CURRENT_SPEED, current);
  }

  return wish;
}

/**
 * @brief Swims.
 */
static void Pm_Quake2WaterMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Vec3 wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, pmLocals.forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, pmLocals.right);

  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
    wish.z -= PM_QUAKE2_WATER_SINK; // drift toward the bottom
  } else {
    wish.z += pm->cmd.up;
  }

  wish = Pm_Quake2AddCurrents(wish);

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(wish, &speed);
  speed = Minf(speed, pm->s.params.speedGround) * PM_QUAKE2_WATER_SCALE;

  Pm_Quake2Accelerate(dir, speed, pm->s.params.accelWater);

  Pm_Quake2StepSlideMove();
}

/**
 * @brief Walks, climbs and falls. Gravity is applied here, in each case that
 * needs it.
 */
static void Pm_Quake2AirMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  // Quake II asks for a third of the pitch here, and only here: swimming uses
  // the whole of it. The comment upstream wonders whether this is needed; it is
  // what the movement does, so it is what this does
  Vec3 angles = pm->angles;
  angles.x /= 3.f;

  Vec3 forward, right;
  Vec3_Vectors(angles, &forward, &right, NULL);

  Vec3 wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, right);
  wish.z = 0.f;

  wish = Pm_Quake2AddCurrents(wish);

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(wish, &speed);

  const float maxSpeed = (pm->s.flags & PMF_DUCKED)
                          ? pm->s.params.speedDucked
                          : pm->s.params.speedGround;
  speed = Minf(speed, maxSpeed);

  const float gravity = pm->s.params.gravity * pmLocals.time;

  if (pm->s.flags & PMF_ON_LADDER) {

    Pm_Quake2Accelerate(dir, speed, pm->s.params.accelLadder);

    if (wish.z == 0.f) { // hold still against gravity
      if (pm->s.velocity.z > 0.f) {
        pm->s.velocity.z = Maxf(0.f, pm->s.velocity.z - gravity);
      } else {
        pm->s.velocity.z = Minf(0.f, pm->s.velocity.z + gravity);
      }
    }

    Pm_Quake2StepSlideMove();

  } else if (pm->s.flags & PMF_ON_GROUND) {

    pm->s.velocity.z = 0.f; // before the acceleration, as upstream has it
    Pm_Quake2Accelerate(dir, speed, pm->s.params.accelGround);

    if (pm->s.params.gravity > 0) {
      pm->s.velocity.z = 0.f;
    } else { // a negative gravity field lifts instead
      pm->s.velocity.z -= gravity;
    }

    if (pm->s.velocity.x || pm->s.velocity.y) {
      Pm_Quake2StepSlideMove();
    }

  } else {

    // vanilla Quake II leaves pm_airaccelerate at zero, so this is a plain
    // acceleration at 1 rather than QuakeWorld's capped-wish path. That is not
    // the absence of air control: with no cap on the wished speed the headroom
    // is the whole of it, which is where Quake II strafe jumping comes from
    Pm_Quake2Accelerate(dir, speed, pm->s.params.accelAir);

    pm->s.velocity.z -= gravity;

    Pm_Quake2StepSlideMove();
  }
}

/**
 * @brief Classifies the ground beneath the player and the water around them.
 */
static void Pm_Quake2CategorizePosition(void) {

  // Quake II's own ON_GROUND flag persists between moves, so it can tell a
  // landing from merely standing. Quetoo clears that flag in Pm_Init, so the
  // ground the move was handed is what carries the edge - and it must be read
  // before this function overwrites it, or every grounded frame looks like a
  // landing and the jump lockout never lifts
  const bool wasGrounded = pm->ground.ent != NULL;

  if (pm->s.flags & PMF_TIME_PUSHED) { // the plumbing asks us not to seek ground
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else if (pm->s.velocity.z > PM_QUAKE2_UP_SPEED) { // rising too fast to be standing
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else {
    const Vec3 below = MakeVec3(pm->s.origin.x, pm->s.origin.y,
                              pm->s.origin.z - PM_QUAKE2_GROUND_PROBE);

    const CmTrace trace = Pm_Trace(pm->s.origin, below, pm->bounds);
    pmLocals.ground = trace;

    // a steep plane is still ground if we started inside it
    if (!trace.ent || (trace.plane.normal.z < PM_QUAKE2_GROUND_NORMAL && !trace.startSolid)) {
      pm->s.flags &= ~PMF_ON_GROUND;
      memset(&pm->ground, 0, sizeof(pm->ground));
    } else {
      pm->ground = trace;

      // unconditionally, because Pm_Init cleared it: Quake II only ever adds it
      // here, its own copy having survived the frame
      pm->s.flags |= PMF_ON_GROUND;

      if (pm->s.flags & PMF_TIME_WATER_JUMP) { // solid ground ends a water jump
        pm->s.flags &= ~(PMF_TIME_WATER_JUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
        pm->s.time = 0;
      }

      if (!wasGrounded) { // just landed

        // a hard landing locks the jump out briefly, which is why Quake II
        // cannot be hopped down a staircase
        if (pm->s.velocity.z < PM_QUAKE2_LAND_SPEED) {
          pm->s.flags |= PMF_TIME_LAND;
          pm->s.time = pm->s.velocity.z < PM_QUAKE2_LAND_SPEED_HARD
                       ? PM_QUAKE2_LAND_TIME_HARD
                       : PM_QUAKE2_LAND_TIME;
        }
      }
    }

    Pm_TouchEntity(&trace);
  }

  pm->waterLevel = WATER_NONE;
  pm->waterType = 0;

  // the samples follow the eye, so ducking changes what counts as submerged
  const float sample2 = pm->s.viewOffset.z - pm->bounds.mins.z;
  const float sample1 = sample2 * .5f;

  Vec3 point = MakeVec3(pm->s.origin.x, pm->s.origin.y,
                      pm->s.origin.z + pm->bounds.mins.z + 1.f);

  int32_t contents = pm->PointContents(point);

  if (contents & CONTENTS_MASK_LIQUID) {
    pm->waterType = contents;
    pm->waterLevel = WATER_FEET;

    point.z = pm->s.origin.z + pm->bounds.mins.z + sample1;
    contents = pm->PointContents(point);

    if (contents & CONTENTS_MASK_LIQUID) {
      pm->waterLevel = WATER_WAIST;

      point.z = pm->s.origin.z + pm->bounds.mins.z + sample2;
      contents = pm->PointContents(point);

      if (contents & CONTENTS_MASK_LIQUID) {
        pm->waterLevel = WATER_UNDER;
        pm->s.flags |= PMF_UNDER_WATER;
      }
    }
  }
}

/**
 * @brief Jumps, or swims upward. Quake II adds to the vertical speed and then
 * insists on at least the jump speed, so a jump while falling is neither
 * weakened nor strengthened.
 */
static void Pm_Quake2CheckJump(void) {

  if (pm->s.flags & PMF_TIME_LAND) { // too soon after landing
    return;
  }

  if (pm->cmd.up < PM_QUAKE2_JUMP_UP_MIN) { // not holding jump
    pm->s.flags &= ~PMF_JUMP_HELD;
    return;
  }

  if (pm->s.flags & PMF_JUMP_HELD) { // must be released first
    return;
  }

  if (pm->s.type == PM_DEAD) {
    return;
  }

  if (pm->waterLevel >= WATER_WAIST) { // swimming, not jumping
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));

    if (pm->s.velocity.z <= PM_QUAKE2_SWIM_JUMP_MIN) { // sinking too fast
      return;
    }

    // by exact equality, as upstream has it: a water brush carrying any other
    // contents bit - a current, most often - falls through to the slowest of
    // these rather than the fastest. That is Quake II, quirk and all
    if (pm->waterType == CONTENTS_WATER) {
      pm->s.velocity.z = 100.f;
    } else if (pm->waterType == CONTENTS_SLIME) {
      pm->s.velocity.z = 80.f;
    } else {
      pm->s.velocity.z = 50.f;
    }
    return;
  }

  if (!(pm->s.flags & PMF_ON_GROUND)) {
    return;
  }

  pm->s.flags |= PMF_JUMP_HELD | PMF_JUMPED;
  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  pm->s.velocity.z += pm->s.params.speedJump;
  pm->s.velocity.z = Maxf(pm->s.velocity.z, pm->s.params.speedJump);
}

/**
 * @brief Looks for a ladder to hold, and for a ledge to hop out of water onto.
 */
static void Pm_Quake2CheckSpecialMovement(void) {

  if (pm->s.time) { // a timer is already running the move
    return;
  }

  pm->s.flags &= ~PMF_ON_LADDER;

  Vec3 forward = MakeVec3(pmLocals.forward.x, pmLocals.forward.y, 0.f);
  forward = Vec3_Normalize(forward);

  const Vec3 ahead = Vec3_Fmaf(pm->s.origin, PM_QUAKE2_LADDER_PROBE, forward);

  const CmTrace trace = Pm_Trace(pm->s.origin, ahead, pm->bounds);
  if (trace.fraction < 1.f && (trace.contents & CONTENTS_LADDER)) {
    pm->s.flags |= PMF_ON_LADDER;
  }

  if (pm->waterLevel != WATER_WAIST) {
    return;
  }

  Vec3 spot = Vec3_Fmaf(pm->s.origin, PM_QUAKE2_WATER_JUMP_DIST, forward);
  spot.z += PM_QUAKE2_WATER_JUMP_UP;

  if (!(pm->PointContents(spot) & CONTENTS_SOLID)) {
    return;
  }

  spot.z += PM_QUAKE2_WATER_JUMP_CLEAR;

  if (pm->PointContents(spot)) { // must be clear above the ledge
    return;
  }

  pm->s.velocity = Vec3_Scale(forward, PM_QUAKE2_WATER_JUMP_PUSH);
  pm->s.velocity.z = pm->s.params.speedWaterJump;

  pm->s.flags |= PMF_TIME_WATER_JUMP;
  pm->s.time = PM_QUAKE2_WATER_JUMP_TIME;
}

/**
 * @brief Ducks, which Quake II does instantly and only on the ground, and sets
 * the eye height to match.
 */
static void Pm_Quake2CheckDuck(void) {

  if (pm->s.type == PM_DEAD) {
    if (pm->s.flags & PMF_GIBLET) {
      pm->s.viewOffset.z = 8.f;
      return;
    }

    // Quake II has no corpse box of its own: a dead player is simply ducked,
    // so its bounds_dead is the ducked box. Setting the flag and stopping here
    // leaves the box Pm_Init took from the parameters, which is the same box and
    // is the one a ruleset can actually change
    pm->s.flags |= PMF_DUCKED;
    pm->s.viewOffset.z = PM_QUAKE2_VIEW_HEIGHT_DUCK;
    return;
  } else if (pm->cmd.up < 0 && pm->ground.ent) {
    // Quake II reads its own ON_GROUND flag here, which persists between moves;
    // Quetoo clears that flag in Pm_Init, so the ground this move was handed is
    // what carries the same meaning. Ducking is still refused in mid-air
    pm->s.flags |= PMF_DUCKED;
  } else if (pm->s.flags & PMF_DUCKED) { // stand up if there is room
    pm->bounds = Pm_Bounds(&pm->s.params, false);

    // again pm->Trace, so a ceiling cannot be jittered out from under us
    if (!pm->Trace(pm->s.origin, pm->s.origin, pm->bounds).allSolid) {
      pm->s.flags &= ~PMF_DUCKED;
    }
  }

  const bool ducked = pm->s.flags & PMF_DUCKED;

  pm->bounds = Pm_Bounds(&pm->s.params, ducked);
  pm->s.viewOffset.z = ducked ? PM_QUAKE2_VIEW_HEIGHT_DUCK : PM_QUAKE2_VIEW_HEIGHT;
}

/**
 * @brief A corpse sheds speed on the ground rather than sliding.
 */
static void Pm_Quake2DeadMove(void) {

  if (!(pm->s.flags & PMF_ON_GROUND)) {
    return;
  }

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(pm->s.velocity, &speed);

  speed -= PM_QUAKE2_DEAD_FRICTION;

  pm->s.velocity = speed <= 0.f ? Vec3_Zero() : Vec3_Scale(dir, speed);
}

/**
 * @brief Cuts the origin and the velocity to the precision Quake II's network
 * channel carried, and looks for a free position if that landed inside
 * something.
 * @details The quantization is observable in the movement it produces, which is
 * why it belongs to the ruleset rather than to the protocol we actually use.
 */
static void Pm_Quake2SnapPosition(void) {

  pm->s.velocity = MakeVec3(truncf(pm->s.velocity.x * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP,
                        truncf(pm->s.velocity.y * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP,
                        truncf(pm->s.velocity.z * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP);

  const Vec3 wanted = pm->s.origin;

  Vec3 base = MakeVec3(truncf(wanted.x * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP,
                     truncf(wanted.y * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP,
                     truncf(wanted.z * PM_QUAKE2_SNAP) / PM_QUAKE2_SNAP);

  // which way each axis was rounded, so that the jitter tries putting it back
  Vec3 sign = MakeVec3(wanted.x >= 0.f ? 1.f : -1.f,
                     wanted.y >= 0.f ? 1.f : -1.f,
                     wanted.z >= 0.f ? 1.f : -1.f);

  if (base.x == wanted.x) {
    sign.x = 0.f;
  }
  if (base.y == wanted.y) {
    sign.y = 0.f;
  }
  if (base.z == wanted.z) {
    sign.z = 0.f;
  }

  // single axes first, as upstream orders them
  static const int32_t jitter[] = { 0, 4, 1, 2, 3, 5, 6, 7 };

  for (size_t i = 0; i < lengthof(jitter); i++) {
    const int32_t bits = jitter[i];

    Vec3 candidate = base;

    if (bits & 1) {
      candidate.x += sign.x / PM_QUAKE2_SNAP;
    }
    if (bits & 2) {
      candidate.y += sign.y / PM_QUAKE2_SNAP;
    }
    if (bits & 4) {
      candidate.z += sign.z / PM_QUAKE2_SNAP;
    }

    if (!pm->Trace(candidate, candidate, pm->bounds).allSolid) {
      pm->s.origin = candidate;
      return;
    }
  }

  pm->s.origin = pmLocals.previousOrigin; // nowhere to be, so stay put
}

/**
 * @brief Quake II's movement, in the order `Pmove` ran it.
 */
void Pm_Quake2Move(void) {

  Pm_Quake2CheckDuck();

  Pm_Quake2CategorizePosition();

  if (pm->s.type == PM_DEAD) {
    Pm_Quake2DeadMove();
  }

  Pm_Quake2CheckSpecialMovement();

  if (pm->s.flags & PMF_TIME_TELEPORT) {
    // stay exactly in place
  } else if (pm->s.flags & PMF_TIME_WATER_JUMP) {

    pm->s.velocity.z -= pm->s.params.gravity * pmLocals.time;

    if (pm->s.velocity.z < 0.f) { // cancel as soon as we fall again
      pm->s.flags &= ~(PMF_TIME_WATER_JUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
      pm->s.time = 0;
    }

    Pm_Quake2StepSlideMove();

  } else {

    Pm_Quake2CheckJump();

    Pm_Quake2Friction();

    if (pm->waterLevel >= WATER_WAIST) {
      Pm_Quake2WaterMove();
    } else {
      Pm_Quake2AirMove();
    }
  }

  Pm_Quake2CategorizePosition();

  Pm_Quake2SnapPosition();

  Pm_CheckViewStep();
}
