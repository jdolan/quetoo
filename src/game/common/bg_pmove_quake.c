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
 * You should have received a copy of the GNU General Above License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "bg_pmove.h"
#include "bg_pmove_internal.h"

/**
 * @file
 * @brief QuakeWorld's movement, `PM_KERNEL_QUAKE`.
 *
 * Ported from id's `QW/client/pmove.c`, which QuakeWorld shared between its
 * client and its server for exactly the reason we do. It is QuakeWorld's rather
 * than NetQuake's: the two differ, and it is QuakeWorld that people played and
 * that bunny hopping belongs to. NetQuake would be a separate kernel.
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
 * Being finished is the point: this file matches what it imitates and should
 * not acquire improvements. A change to how Quake moves is a new kernel.
 */

/**
 * @brief The parameters that make this kernel QuakeWorld rather than merely
 * QuakeWorld-shaped, from `sv_main.c`'s movement variables and `pmove.c`'s
 * player box. A server selecting this kernel takes these rather than its own
 * movement cvars; they reach the client inside `pm_state_t`, like any others.
 */
const pm_params_t pm_quake_params = {
  .gravity = 800,               // sv_gravity
  .gravity_water = 1.f,         // unused: this kernel applies no gravity in water
  .accel_ground = 10.f,         // sv_accelerate
  .accel_ground_slick = 10.f,   // unused: Quake has no slick surfaces
  .accel_air = 10.f,            // sv_accelerate again; PM_AirMove passes it to both
  .accel_water = 10.f,          // sv_wateraccelerate
  .accel_spectator = 10.f,
  .accel_ladder = 10.f,         // unused: Quake has no ladders
  .friction_ground = 4.f,       // sv_friction
  .friction_ground_slick = 0.f, // unused
  .friction_air = 0.f,          // Quake applies no friction while airborne
  .friction_water = 4.f,        // sv_waterfriction
  .friction_spectator = 6.f,    // sv_friction * 1.5, as SpectatorMove had it
  .friction_ladder = 0.f,       // unused
  .speed_ground = 320.f,        // sv_maxspeed
  .speed_air = 320.f,
  .speed_water = 320.f,         // the same cap; the water scale is applied below
  .speed_ladder = 0.f,          // unused
  .speed_spectator = 500.f,     // sv_spectatormaxspeed
  .speed_stop = 100.f,          // sv_stopspeed
  .speed_jump = 270.f,
  .speed_ducked = 320.f,        // unused: there is no ducking
  .speed_duck_stand = 320.f,    // unused
  .speed_water_jump = 310.f,
  .height = 32.f,               // player_maxs, against Quetoo's 36
  .height_ducked = 32.f         // never ducks, so never consulted
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

/**
 * @brief Slides `in` along `normal`, killing components that round to nothing.
 * @details QuakeWorld clips without overbounce, unlike Quake II.
 */
static vec3_t Pm_QuakeClipVelocity(const vec3_t in, const vec3_t normal) {

  vec3_t out = Vec3_Subtract(in, Vec3_Scale(normal, Vec3_Dot(in, normal)));

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

  const vec3_t primal_velocity = pm->s.velocity;
  vec3_t original_velocity = pm->s.velocity;

  cm_bsp_plane_t planes[PM_QUAKE_CLIP_PLANES];
  int32_t num_planes = 0;

  float time_left = pm_locals.time;

  for (int32_t bump = 0; bump < PM_QUAKE_BUMPS; bump++) {

    const vec3_t end = Vec3_Fmaf(pm->s.origin, time_left, pm->s.velocity);
    const cm_trace_t trace = Pm_Trace(pm->s.origin, end, pm->bounds);

    if (trace.start_solid || trace.all_solid) { // trapped in a solid
      pm->s.velocity = Vec3_Zero();
      return;
    }

    if (trace.fraction > 0.f) { // covered some distance
      pm->s.origin = trace.end;
      original_velocity = pm->s.velocity;
      num_planes = 0;
    }

    if (trace.fraction == 1.f) { // moved the entire distance
      break;
    }

    Pm_TouchEntity(&trace);

    time_left -= time_left * trace.fraction;

    if (num_planes >= PM_QUAKE_CLIP_PLANES) { // this should not happen
      pm->s.velocity = Vec3_Zero();
      break;
    }

    planes[num_planes++] = trace.plane;

    // slide along the first plane that the others do not immediately undo
    int32_t i;
    for (i = 0; i < num_planes; i++) {
      pm->s.velocity = Pm_QuakeClipVelocity(original_velocity, planes[i].normal);

      int32_t j;
      for (j = 0; j < num_planes; j++) {
        if (j != i && Vec3_Dot(pm->s.velocity, planes[j].normal) < 0.f) {
          break;
        }
      }

      if (j == num_planes) {
        break;
      }
    }

    if (i == num_planes) { // no such plane, so go along the crease
      if (num_planes != 2) {
        pm->s.velocity = Vec3_Zero();
        break;
      }

      const vec3_t dir = Vec3_Cross(planes[0].normal, planes[1].normal);
      pm->s.velocity = Vec3_Scale(dir, Vec3_Dot(dir, pm->s.velocity));
    }

    // stop dead rather than oscillate in a sloping corner
    if (Vec3_Dot(pm->s.velocity, primal_velocity) <= 0.f) {
      pm->s.velocity = Vec3_Zero();
      break;
    }
  }

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    pm->s.velocity = primal_velocity;
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
  const vec3_t dest = Vec3(pm->s.origin.x + pm->s.velocity.x * pm_locals.time,
                           pm->s.origin.y + pm->s.velocity.y * pm_locals.time,
                           pm->s.origin.z);

  cm_trace_t trace = Pm_Trace(pm->s.origin, dest, pm->bounds);
  if (trace.fraction == 1.f) {
    pm->s.origin = trace.end;
    return;
  }

  const vec3_t original = pm->s.origin;
  const vec3_t original_velocity = pm->s.velocity;

  Pm_QuakeFlyMove();

  const vec3_t down = pm->s.origin;
  const vec3_t down_velocity = pm->s.velocity;

  pm->s.origin = original;
  pm->s.velocity = original_velocity;

  // and again from a step height up
  trace = Pm_Trace(pm->s.origin, Vec3_Fmaf(pm->s.origin, PM_QUAKE_STEP_SIZE, Vec3_Up()),
                   pm->bounds);
  if (!trace.start_solid && !trace.all_solid) {
    pm->s.origin = trace.end;
  }

  Pm_QuakeFlyMove();

  // press back down the step height
  trace = Pm_Trace(pm->s.origin, Vec3_Fmaf(pm->s.origin, PM_QUAKE_STEP_SIZE, Vec3_Down()),
                   pm->bounds);

  bool use_down = trace.plane.normal.z < PM_QUAKE_GROUND_NORMAL;
  if (!use_down) {
    if (!trace.start_solid && !trace.all_solid) {
      pm->s.origin = trace.end;
    }

    const float down_dist = Vec2_DistanceSquared(Vec3_XY(down), Vec3_XY(original));
    const float up_dist = Vec2_DistanceSquared(Vec3_XY(pm->s.origin), Vec3_XY(original));

    use_down = down_dist > up_dist;
  }

  if (use_down) {
    pm->s.origin = down;
    pm->s.velocity = down_velocity;
  } else { // the stepped move went farther, but its vertical speed is the flat one's
    pm->s.velocity.z = down_velocity.z;
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

  float friction = pm->s.params.friction_ground;

  if (pm->s.flags & PMF_ON_GROUND) {

    // over a dropoff, friction doubles
    const vec3_t ahead = Vec3(pm->s.origin.x + pm->s.velocity.x / speed * PM_QUAKE_EDGE_PROBE,
                              pm->s.origin.y + pm->s.velocity.y / speed * PM_QUAKE_EDGE_PROBE,
                              pm->s.origin.z + pm->bounds.mins.z);
    const vec3_t below = Vec3(ahead.x, ahead.y, ahead.z - PM_QUAKE_EDGE_DROP);

    if (Pm_Trace(ahead, below, Box3_Zero()).fraction == 1.f) {
      friction *= PM_QUAKE_EDGE_FRICTION;
    }
  }

  float drop = 0.f;

  if (pm->water_level >= WATER_WAIST) {
    drop = speed * pm->s.params.friction_water * pm->water_level * pm_locals.time;
  } else if (pm->s.flags & PMF_ON_GROUND) {
    const float control = Maxf(speed, pm->s.params.speed_stop);
    drop = control * friction * pm_locals.time;
  }

  pm->s.velocity = Vec3_Scale(pm->s.velocity, Maxf(0.f, speed - drop) / speed);
}

/**
 * @brief Accelerates toward `dir`, up to `speed`.
 */
static void Pm_QuakeAccelerate(const vec3_t dir, float speed, float accel) {

  if (pm->s.type == PM_DEAD || (pm->s.flags & PMF_TIME_WATER_JUMP)) {
    return;
  }

  if (pm->Accelerate) {
    pm->Accelerate(pm, dir, speed, accel);
  }

  const float add_speed = speed - Vec3_Dot(pm->s.velocity, dir);
  if (add_speed <= 0.f) {
    return;
  }

  const float accel_speed = Minf(accel * pm_locals.time * speed, add_speed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accel_speed, dir);
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
static void Pm_QuakeAirAccelerate(const vec3_t dir, float speed, float accel) {

  if (pm->s.type == PM_DEAD || (pm->s.flags & PMF_TIME_WATER_JUMP)) {
    return;
  }

  if (pm->Accelerate) {
    pm->Accelerate(pm, dir, speed, accel);
  }

  const float wish_speed = Minf(speed, PM_QUAKE_AIR_WISH_SPEED);

  const float add_speed = wish_speed - Vec3_Dot(pm->s.velocity, dir);
  if (add_speed <= 0.f) {
    return;
  }

  const float accel_speed = Minf(accel * speed * pm_locals.time, add_speed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accel_speed, dir);
}

/**
 * @brief Swims, and steps up out of the water onto a ledge if the move allows.
 */
static void Pm_QuakeWaterMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  vec3_t wish_velocity = Vec3_Zero();
  wish_velocity = Vec3_Fmaf(wish_velocity, pm->cmd.forward, pm_locals.forward);
  wish_velocity = Vec3_Fmaf(wish_velocity, pm->cmd.right, pm_locals.right);

  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
    wish_velocity.z -= PM_QUAKE_WATER_SINK; // drift toward the bottom
  } else {
    wish_velocity.z += pm->cmd.up;
  }

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish_velocity, &speed);
  speed = Minf(speed, pm->s.params.speed_water) * PM_QUAKE_WATER_SCALE;

  Pm_QuakeAccelerate(dir, speed, pm->s.params.accel_water);

  // assume a stair or a slope, and press down from a step height above
  const vec3_t dest = Vec3_Fmaf(pm->s.origin, pm_locals.time, pm->s.velocity);
  const vec3_t start = Vec3(dest.x, dest.y, dest.z + PM_QUAKE_STEP_SIZE + 1.f);

  const cm_trace_t trace = Pm_Trace(start, dest, pm->bounds);
  if (!trace.start_solid && !trace.all_solid) { // walked up the step
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
  vec3_t forward = Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f);
  vec3_t right = Vec3(pm_locals.right.x, pm_locals.right.y, 0.f);

  forward = Vec3_Normalize(forward);
  right = Vec3_Normalize(right);

  vec3_t wish_velocity = Vec3_Zero();
  wish_velocity = Vec3_Fmaf(wish_velocity, pm->cmd.forward, forward);
  wish_velocity = Vec3_Fmaf(wish_velocity, pm->cmd.right, right);
  wish_velocity.z = 0.f;

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish_velocity, &speed);
  speed = Minf(speed, pm->s.params.speed_ground);

  const float gravity = pm->s.params.gravity * pm_locals.time;

  if (pm->s.flags & PMF_ON_GROUND) {
    pm->s.velocity.z = 0.f;
    Pm_QuakeAccelerate(dir, speed, pm->s.params.accel_ground);
    pm->s.velocity.z -= gravity;
    Pm_QuakeGroundMove();
  } else {
    Pm_QuakeAirAccelerate(dir, speed, pm->s.params.accel_air);
    pm->s.velocity.z -= gravity;
    Pm_QuakeFlyMove();
  }
}

/**
 * @brief Classifies the ground beneath the player and the water around them.
 */
static void Pm_QuakeCategorizePosition(void) {

  if (pm->s.velocity.z > PM_QUAKE_UP_SPEED) { // rising too fast to be standing
    pm->s.flags &= ~PMF_ON_GROUND;
    pm->ground.ent = NULL;
  } else {
    const vec3_t below = Vec3(pm->s.origin.x, pm->s.origin.y, pm->s.origin.z - 1.f);
    const cm_trace_t trace = Pm_Trace(pm->s.origin, below, pm->bounds);

    if (trace.plane.normal.z < PM_QUAKE_GROUND_NORMAL) { // too steep
      pm->s.flags &= ~PMF_ON_GROUND;
      pm->ground.ent = NULL;
    } else {
      pm->s.flags |= PMF_ON_GROUND;
      pm->ground = trace;
      pm_locals.ground = trace;

      pm->s.flags &= ~PMF_TIME_WATER_JUMP;

      if (!trace.start_solid && !trace.all_solid) {
        pm->s.origin = trace.end;
      }
    }

    if (trace.ent) {
      Pm_TouchEntity(&trace);
    }
  }

  pm->water_level = WATER_NONE;
  pm->water_type = 0;

  vec3_t point = Vec3(pm->s.origin.x, pm->s.origin.y,
                      pm->s.origin.z + pm->bounds.mins.z + 1.f);

  int32_t contents = pm->PointContents(point);
  if (contents & CONTENTS_MASK_LIQUID) {
    pm->water_type = contents;
    pm->water_level = WATER_FEET;

    point.z = pm->s.origin.z + (pm->bounds.mins.z + pm->bounds.maxs.z) * .5f;
    contents = pm->PointContents(point);
    if (contents & CONTENTS_MASK_LIQUID) {
      pm->water_level = WATER_WAIST;

      point.z = pm->s.origin.z + PM_QUAKE_WATER_UNDER;
      contents = pm->PointContents(point);
      if (contents & CONTENTS_MASK_LIQUID) {
        pm->water_level = WATER_UNDER;
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

  if (pm->water_level >= WATER_WAIST) { // swimming, not jumping
    pm->s.flags &= ~PMF_ON_GROUND;
    pm->ground.ent = NULL;

    if (pm->water_type & CONTENTS_LAVA) {
      pm->s.velocity.z = 50.f;
    } else if (pm->water_type & CONTENTS_SLIME) {
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
  pm->ground.ent = NULL;

  pm->s.velocity.z += pm->s.params.speed_jump;

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

  vec3_t forward = Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f);
  forward = Vec3_Normalize(forward);

  vec3_t spot = Vec3_Fmaf(pm->s.origin, PM_QUAKE_WATER_JUMP_DIST, forward);
  spot.z += 8.f;

  if (!(pm->PointContents(spot) & CONTENTS_SOLID)) {
    return;
  }

  spot.z += PM_QUAKE_WATER_JUMP_DIST;

  if (pm->PointContents(spot)) { // must be clear above the ledge
    return;
  }

  pm->s.velocity = Vec3_Scale(forward, PM_QUAKE_WATER_JUMP_PUSH);
  pm->s.velocity.z = pm->s.params.speed_water_jump;

  pm->s.flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
  pm->s.time = PM_QUAKE_WATER_JUMP_TIME;
}

/**
 * @brief Cuts the origin to the precision the network carries, then looks for a
 * free position nearby if that landed inside something.
 */
static void Pm_QuakeNudgePosition(void) {

  const vec3_t base = Vec3(truncf(pm->s.origin.x * PM_QUAKE_SNAP) / PM_QUAKE_SNAP,
                           truncf(pm->s.origin.y * PM_QUAKE_SNAP) / PM_QUAKE_SNAP,
                           truncf(pm->s.origin.z * PM_QUAKE_SNAP) / PM_QUAKE_SNAP);

  static const float offsets[] = { 0.f, -1.f / PM_QUAKE_SNAP, 1.f / PM_QUAKE_SNAP };

  pm->s.origin = base;

  for (size_t z = 0; z < lengthof(offsets); z++) {
    for (size_t x = 0; x < lengthof(offsets); x++) {
      for (size_t y = 0; y < lengthof(offsets); y++) {
        const vec3_t candidate = Vec3(base.x + offsets[x],
                                      base.y + offsets[y],
                                      base.z + offsets[z]);

        if (!Pm_Trace(candidate, candidate, pm->bounds).start_solid) {
          pm->s.origin = candidate;
          return;
        }
      }
    }
  }
}

/**
 * @brief QuakeWorld's movement, in the order `PlayerMove` ran it.
 */
void Pm_QuakeMove(void) {

  Pm_QuakeNudgePosition();

  Pm_QuakeCategorizePosition();

  if (pm->water_level == WATER_WAIST) {
    Pm_QuakeCheckWaterJump();
  }

  if (pm->s.velocity.z < 0.f) {
    pm->s.flags &= ~PMF_TIME_WATER_JUMP;
  }

  if (pm->cmd.up > 0) {
    Pm_QuakeJump();
  }

  Pm_QuakeFriction();

  if (pm->water_level >= WATER_WAIST) {
    Pm_QuakeWaterMove();
  } else {
    Pm_QuakeAirMove();
  }

  Pm_QuakeCategorizePosition();

  Pm_CheckViewStep();
}
