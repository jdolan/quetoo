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
 * @brief Quake III Arena's movement, `PM_MOVEMENT_QUAKE3`.
 *
 * Ported from id's `game/bg_pmove.c` and `game/bg_slidemove.c`, which Quake III
 * shared between its client and its server as we do. It is vanilla Quake III -
 * VQ3, not CPM: there is no air control and no double jump, because Quake III
 * has neither. What it does have is the uncapped wished speed Quake II
 * introduced, which is where strafe jumping comes from; `PM_Accelerate` is
 * literally Quake II's, and the source says so, the commented-out alternative
 * being labelled "proper way (avoids strafe jump maxspeed bug), but feels bad".
 *
 * It could not be seeded from the Quake II kernel, because three things are
 * structurally different:
 *
 * 1. **Ground is two concepts.** `PM_GroundTrace` decides `groundPlane` and
 *    `walking` independently: a slope steeper than 0.7 gives a plane to slide
 *    along while the player is not standing on it. Quetoo has one flag, which
 *    corresponds to `walking`, so the plane is a kernel-local bool alongside it.
 * 2. **Vertical speed is not zeroed on the ground.** The walk path projects the
 *    forward and right vectors onto the ground plane instead, so the wish itself
 *    follows the slope and its Z component is deliberately left alone. Quake II
 *    and QuakeWorld both zero it, and this is why Quake III walks up ramps the
 *    way it does.
 * 3. **Leaving the ground is a dot product, not a speed.** Quake II asks whether
 *    the player is rising faster than 180; Quake III asks whether they are
 *    rising at all and moving into the plane's normal by more than 10. On a
 *    slope that triggers far earlier, and by an amount the slope's angle
 *    decides, which is the mechanic ramp jumps actually run on.
 *
 * Gravity lives inside the slide rather than before it, and the slide runs two
 * velocities: half the gravity step on the one it moves by, the whole of it on
 * the one that survives. It seeds its plane list with the ground plane and with
 * the current velocity, so a move never turns back against either.
 *
 * `PM_CmdScale` is not ported. It normalizes an axial +/-127 command so that a
 * diagonal is not `sqrt(2)` faster, scaling the wish by
 * `speed * max / (127 * total)`; Quetoo's `pm_cmd_t` carries the intended speed
 * instead, so the equivalent is the wish-speed clamp the other kernels use. For
 * a command that fills its axes the two agree exactly, and they differ only for
 * partial input. Upstream's `PM_CheckJump` clearing `cmd.upmove` so that
 * `PM_CmdScale` would not count it goes with it, having nothing left to affect.
 *
 * Two things the plumbing owns rather than this file, both differing from
 * upstream by a hair. `Pm_Init` releases the jump latch below a command of 1
 * where `PmoveSingle` releases it below 10, so a partial jump command in
 * between would latch here and not there; Quetoo's client does not produce one.
 * And `Pm_Init` decrements the movement timer before the kernel runs, where
 * `PmoveSingle` drops it after the ground trace, so on the single move a water
 * jump's timer expires, upstream's ground trace still sees the flag and this
 * one does not.
 *
 * Two things a reader will look for and not find. `pm_wadeScale` is declared
 * upstream but never read: the wade clamp is built from `pm_swimScale`, so that
 * is what this uses. And `pml.impactSpeed`, which `PM_SlideMove` tracks, is
 * written and never read anywhere in id's tree, so it is not tracked here.
 *
 * The corpse is id's too, box and eye height both, since `pm_params_t` carries
 * the dead box as well. What stays Quetoo's is the giblet: no Quake has one.
 *
 * Being finished is the point: this file matches what it imitates and should
 * not acquire improvements. A change to how Quake III moves is a new movement.
 */

/**
 * @brief The parameters that make this Quake III, from `bg_pmove.c`'s movement
 * variables and `bg_public.h`'s player box.
 * @details `speed_ground` is `g_speed`, which Quake III hands the movement as
 * `ps->speed` and which bounds the wish everywhere. The wade scale is applied
 * in the kernel against `speed_ground`, as upstream applies it against
 * `ps->speed`; the duck scale is not, because `speed_ducked` is a parameter of
 * its own here. Upstream derives the ducked speed from `ps->speed`, so a
 * ruleset that moves `speed_ground` must move `speed_ducked` with it.
 */
#define PM_QUAKE3_BOUNDS { \
  .mins = { { -15.f, -15.f, -24.f } }, /* MINS_Z, and 30 across where the others */ \
  .maxs = { {  15.f,  15.f,  32.f } }  /* are 32; against Quetoo's 36 tall */ \
}

#define PM_QUAKE3_BOUNDS_DUCKED { \
  .mins = { { -15.f, -15.f, -24.f } }, \
  .maxs = { {  15.f,  15.f,  16.f } }  /* and 16, against Quake II's 4 */ \
}

#define PM_QUAKE3_BOUNDS_DEAD { \
  .mins = { { -15.f, -15.f, -24.f } }, \
  .maxs = { {  15.f,  15.f,  -8.f } }  /* PM_CheckDuck's corpse, against Quetoo's -4 */ \
}

const pm_params_t pm_quake3_params = {
  .gravity = 800,
  .accel_ground = 10.f,         // pm_accelerate
  .accel_ground_slick = 1.f,    // pm_airaccelerate, which is what slick ground gets
  .accel_air = 1.f,             // pm_airaccelerate
  .accel_water = 4.f,           // pm_wateraccelerate
  .accel_spectator = 8.f,       // pm_flyaccelerate
  .accel_ladder = 0.f,          // unused: Quake III has no ladders
  .friction_ground = 6.f,       // pm_friction
  .friction_ground_slick = 0.f, // slick surfaces are frictionless
  .friction_air = 0.f,          // no friction while airborne
  .friction_water = 1.f,        // pm_waterfriction
  .friction_spectator = 5.f,    // pm_spectatorfriction
  .friction_ladder = 0.f,       // unused
  .speed_ground = 320.f,        // g_speed
  .speed_air = 320.f,
  .speed_water = 320.f,         // g_speed again; the swim scale is applied below
  .speed_ladder = 0.f,          // unused
  .speed_spectator = 320.f,
  .speed_stop = 100.f,          // pm_stopspeed
  .speed_jump = 270.f,          // JUMP_VELOCITY, which a jump assigns
  .speed_ducked = 80.f,         // g_speed * pm_duckScale
  .speed_duck_stand = 0.f,      // unused: Quake III's duck is instant
  .speed_water_jump = 350.f,
  .bounds = PM_QUAKE3_BOUNDS,
  .bounds_ducked = PM_QUAKE3_BOUNDS_DUCKED,
  .bounds_dead = PM_QUAKE3_BOUNDS_DEAD
};

#define PM_QUAKE3_STEP_SIZE         18.f   // STEPSIZE
#define PM_QUAKE3_STEP_MIN          2.f    // the smallest rise upstream calls a step
#define PM_QUAKE3_CLIP_PLANES       5      // MAX_CLIP_PLANES
#define PM_QUAKE3_BUMPS             4      // numbumps
#define PM_QUAKE3_OVERCLIP          1.001f // OVERCLIP
#define PM_QUAKE3_WALK_NORMAL       .7f    // MIN_WALK_NORMAL
#define PM_QUAKE3_GROUND_PROBE      .25f   // how far down ground is looked for
#define PM_QUAKE3_KICKOFF           10.f   // the reach into the plane that unstands us
#define PM_QUAKE3_SAME_PLANE        .99f   // how aligned two planes are to be the one plane
#define PM_QUAKE3_INTO              .1f    // the reach into a plane that counts as entering it
#define PM_QUAKE3_SWIM_SCALE        .5f    // pm_swimScale
#define PM_QUAKE3_WATER_SINK        60.f   // how fast an idle swimmer drifts down
#define PM_QUAKE3_LAND_SPEED       -200.f  // landing harder than this starts the timer
#define PM_QUAKE3_LAND_TIME         250    // and runs it for this long
#define PM_QUAKE3_JUMP_UP_MIN       10     // how far the jump key must be down to count
#define PM_QUAKE3_WATER_JUMP_DIST   30.f   // how far ahead the ledge is looked for
#define PM_QUAKE3_WATER_JUMP_UP     4.f    // and how far up
#define PM_QUAKE3_WATER_JUMP_CLEAR  16.f   // and how much clear air it needs above
#define PM_QUAKE3_WATER_JUMP_PUSH   200.f  // how hard the player is pushed at it
#define PM_QUAKE3_WATER_JUMP_TIME   2000   // and for how long they have no control
#define PM_QUAKE3_VIEW_HEIGHT       26.f   // DEFAULT_VIEWHEIGHT, against Quetoo's 30
#define PM_QUAKE3_VIEW_HEIGHT_DUCK  12.f   // CROUCH_VIEWHEIGHT
#define PM_QUAKE3_VIEW_HEIGHT_DEAD -16.f   // DEAD_VIEWHEIGHT
#define PM_QUAKE3_VIEW_HEIGHT_GIB   8.f    // Quetoo's giblets, which Quake III has not
#define PM_QUAKE3_DEAD_FRICTION     20.f   // the speed a corpse sheds each move

/**
 * @brief Whether there is a plane beneath the player to slide along, which is
 * not the same as standing on it: a slope steeper than `PM_QUAKE3_WALK_NORMAL`
 * sets this while `PMF_ON_GROUND` stays clear. Decided by
 * `Pm_Quake3GroundTrace`, and reset at the top of every move, so nothing
 * survives one.
 */
static bool pm_quake3_ground_plane;

/**
 * @brief Slides `in` along `normal`. Unlike Quake II, a move already leaving the
 * plane is given back less rather than more, which is what the divide does.
 */
static vec3_t Pm_Quake3ClipVelocity(const vec3_t in, const vec3_t normal) {

  float backoff = Vec3_Dot(in, normal);

  if (backoff < 0.f) {
    backoff *= PM_QUAKE3_OVERCLIP;
  } else {
    backoff /= PM_QUAKE3_OVERCLIP;
  }

  return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

/**
 * @brief Slides through the world, clipping to every plane struck.
 * @details Gravity is applied here rather than before the move, and when it is,
 * two velocities are tracked through the same planes: the one the move is made
 * by, which takes half the gravity step, and the one that survives, which takes
 * all of it. The plane list is seeded with the ground plane and with the
 * velocity itself, so the move can never be turned back against either.
 * @return False only if the very first trace covered the whole move.
 * @remarks The traces here are `Pm_Trace` where upstream's are plain, as every
 * kernel in this directory has them: Quetoo's collision can hand back a move
 * that starts inside a solid where Quake III's did not, and `Pm_Trace` is
 * itself id's own escape from that, lifted out of `PM_CorrectAllSolid`.
 */
static bool Pm_Quake3SlideMove(const bool gravity) {

  vec3_t primal_velocity = pm->s.velocity;

  // this only survives the move when gravity was asked for; upstream leaves it
  // uninitialized otherwise and discards the clips it takes below
  vec3_t end_velocity = Vec3_Zero();

  if (gravity) {
    end_velocity = pm->s.velocity;
    end_velocity.z -= pm->s.params.gravity * pm_locals.time;

    pm->s.velocity.z = (pm->s.velocity.z + end_velocity.z) * .5f;
    primal_velocity.z = end_velocity.z;

    if (pm_quake3_ground_plane) {
      pm->s.velocity = Pm_Quake3ClipVelocity(pm->s.velocity, pm_locals.ground.plane.normal);
    }
  }

  vec3_t planes[PM_QUAKE3_CLIP_PLANES];
  int32_t num_planes = 0;

  if (pm_quake3_ground_plane) { // never turn against the ground plane
    planes[num_planes++] = pm_locals.ground.plane.normal;
  }

  planes[num_planes++] = Vec3_Normalize(pm->s.velocity); // nor against the velocity

  float time_left = pm_locals.time;

  int32_t bump;
  for (bump = 0; bump < PM_QUAKE3_BUMPS; bump++) {

    const vec3_t end = Vec3_Fmaf(pm->s.origin, time_left, pm->s.velocity);
    const cm_trace_t trace = Pm_Trace(pm->s.origin, end, pm->bounds);

    if (trace.all_solid) { // trapped in a solid
      pm->s.velocity.z = 0.f; // and do not build up falling damage
      return true;
    }

    if (trace.fraction > 0.f) { // covered some distance
      pm->s.origin = trace.end;
    }

    if (trace.fraction == 1.f) { // moved the entire distance
      break;
    }

    Pm_TouchEntity(&trace);

    time_left -= time_left * trace.fraction;

    if (num_planes >= PM_QUAKE3_CLIP_PLANES) { // this should not happen
      pm->s.velocity = Vec3_Zero();
      return true;
    }

    // striking a plane we have already struck is nudged out of rather than
    // clipped again, which settles the epsilon trouble non-axial planes cause
    int32_t i;
    for (i = 0; i < num_planes; i++) {
      if (Vec3_Dot(trace.plane.normal, planes[i]) > PM_QUAKE3_SAME_PLANE) {
        pm->s.velocity = Vec3_Add(trace.plane.normal, pm->s.velocity);
        break;
      }
    }

    if (i < num_planes) {
      continue;
    }

    planes[num_planes++] = trace.plane.normal;

    // and otherwise the velocity is made to parallel every plane at once
    for (i = 0; i < num_planes; i++) {

      if (Vec3_Dot(pm->s.velocity, planes[i]) >= PM_QUAKE3_INTO) {
        continue; // the move does not enter this plane
      }

      vec3_t clipped = Pm_Quake3ClipVelocity(pm->s.velocity, planes[i]);
      vec3_t end_clipped = Pm_Quake3ClipVelocity(end_velocity, planes[i]);

      for (int32_t j = 0; j < num_planes; j++) {

        if (j == i) {
          continue;
        }

        if (Vec3_Dot(clipped, planes[j]) >= PM_QUAKE3_INTO) {
          continue;
        }

        clipped = Pm_Quake3ClipVelocity(clipped, planes[j]);
        end_clipped = Pm_Quake3ClipVelocity(end_clipped, planes[j]);

        if (Vec3_Dot(clipped, planes[i]) >= 0.f) { // it no longer enters the first
          continue;
        }

        // so go along the crease the two of them make
        const vec3_t dir = Vec3_Normalize(Vec3_Cross(planes[i], planes[j]));

        clipped = Vec3_Scale(dir, Vec3_Dot(dir, pm->s.velocity));
        end_clipped = Vec3_Scale(dir, Vec3_Dot(dir, end_velocity));

        for (int32_t k = 0; k < num_planes; k++) {

          if (k == i || k == j) {
            continue;
          }

          if (Vec3_Dot(clipped, planes[k]) >= PM_QUAKE3_INTO) {
            continue;
          }

          pm->s.velocity = Vec3_Zero(); // stop dead in a three-plane corner
          return true;
        }
      }

      pm->s.velocity = clipped;
      end_velocity = end_clipped;
      break;
    }
  }

  if (gravity) {
    pm->s.velocity = end_velocity;
  }

  if (pm->s.time) { // a timed move keeps the velocity it was given
    pm->s.velocity = primal_velocity;
  }

  return bump != 0;
}

/**
 * @brief Slides, and if that struck anything, slides again from a step height
 * up.
 * @details Unlike Quake II, the two results are not compared: vanilla keeps the
 * flat one only under an `#if 0`, so the step move is simply taken whenever
 * there was room for it.
 */
static void Pm_Quake3StepSlideMove(const bool gravity) {

  const vec3_t start_origin = pm->s.origin;
  const vec3_t start_velocity = pm->s.velocity;

  if (!Pm_Quake3SlideMove(gravity)) {
    return; // went exactly where it wanted to on the first try
  }

  const vec3_t below = Vec3(start_origin.x, start_origin.y,
                            start_origin.z - PM_QUAKE3_STEP_SIZE);

  // pm->Trace and not Pm_Trace throughout this function, as upstream has it: the
  // latter jitters the start by up to a unit to escape a solid, so it cannot
  // answer a question about the position as it stands, and the origin it returns
  // would carry that jitter into the move
  cm_trace_t trace = pm->Trace(start_origin, below, pm->bounds);

  // never step up while still rising, unless there is floor right underneath
  if (pm->s.velocity.z > 0.f &&
      (trace.fraction == 1.f || trace.plane.normal.z < PM_QUAKE3_WALK_NORMAL)) {
    return;
  }

  const vec3_t above = Vec3(start_origin.x, start_origin.y,
                            start_origin.z + PM_QUAKE3_STEP_SIZE);

  trace = pm->Trace(start_origin, above, pm->bounds);
  if (trace.all_solid) {
    return; // no room to step up
  }

  const float step = trace.end.z - start_origin.z;

  pm->s.origin = trace.end;
  pm->s.velocity = start_velocity;

  Pm_Quake3SlideMove(gravity);

  // and press back down however far we came up
  const vec3_t back = Vec3(pm->s.origin.x, pm->s.origin.y, pm->s.origin.z - step);

  trace = pm->Trace(pm->s.origin, back, pm->bounds);
  if (!trace.all_solid) {
    pm->s.origin = trace.end;
  }

  if (trace.fraction < 1.f) {
    pm->s.velocity = Pm_Quake3ClipVelocity(pm->s.velocity, trace.plane.normal);
  }

  const float delta = pm->s.origin.z - start_origin.z;
  if (delta > PM_QUAKE3_STEP_MIN) { // below this upstream announces no step at all
    pm->step = delta;
  }
}

/**
 * @brief Bleeds speed off. Ground friction is skipped while the player is being
 * carried by a push, which is what keeps a knockback from being walked out of
 * the moment it lands.
 */
static void Pm_Quake3Friction(void) {

  vec3_t vel = pm->s.velocity;

  if (pm->s.flags & PMF_ON_GROUND) {
    vel.z = 0.f; // ignore slope movement
  }

  const float speed = Vec3_Length(vel);
  if (speed < 1.f) {
    pm->s.velocity.x = pm->s.velocity.y = 0.f; // and allow sinking under water
    return;
  }

  float drop = 0.f;

  if (pm->water_level <= WATER_FEET) {
    if ((pm->s.flags & PMF_ON_GROUND) && !(pm_locals.ground.surface & SURF_SLICK)) {
      if (!(pm->s.flags & PMF_TIME_PUSHED)) {
        const float control = Maxf(speed, pm->s.params.speed_stop);
        drop += control * pm->s.params.friction_ground * pm_locals.time;
      }
    }
  }

  if (pm->water_level) { // even if only wading
    drop += speed * pm->s.params.friction_water * (float) pm->water_level * pm_locals.time;
  }

  pm->s.velocity = Vec3_Scale(pm->s.velocity, Maxf(0.f, speed - drop) / speed);
}

/**
 * @brief Accelerates toward `dir`, up to `speed`. This is Quake II's, uncapped,
 * and strafe jumping is what comes of it.
 */
static void Pm_Quake3Accelerate(const vec3_t dir, float speed, float accel) {

  const float add_speed = speed - Vec3_Dot(pm->s.velocity, dir);
  if (add_speed <= 0.f) {
    return;
  }

  const float accel_speed = Minf(accel * pm_locals.time * speed, add_speed);

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accel_speed, dir);

  if (pm->Accelerate) {
    pm->Accelerate(pm, dir, speed, accel);
  }
}

/**
 * @brief Jumps. Quake III assigns the jump speed rather than adding it, so a
 * jump off a ramp or a lift does not stack, and it does not repeat while held.
 * @return True if the player left the ground.
 */
static bool Pm_Quake3CheckJump(void) {

  if (pm->cmd.up < PM_QUAKE3_JUMP_UP_MIN) { // not holding jump
    return false;
  }

  if (pm->s.flags & PMF_JUMP_HELD) { // must be released first
    return false;
  }

  pm_quake3_ground_plane = false; // jumping away
  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  pm->s.flags |= PMF_JUMP_HELD | PMF_JUMPED;

  pm->s.velocity.z = pm->s.params.speed_jump;

  return true;
}

/**
 * @brief Looks for a ledge to hop out of water onto.
 */
static bool Pm_Quake3CheckWaterJump(void) {

  if (pm->s.time) { // a timer is already running the move
    return false;
  }

  if (pm->water_level != WATER_WAIST) {
    return false;
  }

  const vec3_t forward = Vec3_Normalize(Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f));

  vec3_t spot = Vec3_Fmaf(pm->s.origin, PM_QUAKE3_WATER_JUMP_DIST, forward);
  spot.z += PM_QUAKE3_WATER_JUMP_UP;

  if (!(pm->PointContents(spot) & CONTENTS_SOLID)) {
    return false;
  }

  spot.z += PM_QUAKE3_WATER_JUMP_CLEAR;

  if (pm->PointContents(spot)) { // must be clear above the ledge
    return false;
  }

  // and this is the whole view vector, not the flattened one
  pm->s.velocity = Vec3_Scale(pm_locals.forward, PM_QUAKE3_WATER_JUMP_PUSH);
  pm->s.velocity.z = pm->s.params.speed_water_jump;

  pm->s.flags |= PMF_TIME_WATER_JUMP;
  pm->s.time = PM_QUAKE3_WATER_JUMP_TIME;

  return true;
}

/**
 * @brief Flies out of the water with no control.
 * @details Gravity is taken twice, once inside the slide and once here, which is
 * what vanilla does and so what a water jump falls at.
 */
static void Pm_Quake3WaterJumpMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Quake3StepSlideMove(true);

  pm->s.velocity.z -= pm->s.params.gravity * pm_locals.time;

  if (pm->s.velocity.z < 0.f) { // cancel as soon as we fall again
    // the whole mask, which is what upstream's PMF_ALL_TIMES is. Quetoo's timed
    // flags share one countdown and Pm_Init only clears them while it is still
    // running, so a flag left set behind a zeroed timer never expires again -
    // and PMF_TIME_PUSHED left that way is a player with no ground friction for
    // the rest of the map
    pm->s.flags &= ~PMF_TIME_MASK;
    pm->s.time = 0;
  }
}

/**
 * @brief Swims. No gravity and no step, and the swimmer is turned along the
 * ground plane rather than clipped to it, so speed is kept going up a slope.
 */
static void Pm_Quake3WaterMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  if (Pm_Quake3CheckWaterJump()) {
    Pm_Quake3WaterJumpMove();
    return;
  }

  Pm_Quake3Friction();

  vec3_t wish = Vec3_Zero();

  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
    wish.z = -PM_QUAKE3_WATER_SINK; // drift toward the bottom
  } else {
    wish = Vec3_Fmaf(wish, pm->cmd.forward, pm_locals.forward);
    wish = Vec3_Fmaf(wish, pm->cmd.right, pm_locals.right);
    wish.z += pm->cmd.up;
  }

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish, &speed);
  speed = Minf(speed, pm->s.params.speed_water * PM_QUAKE3_SWIM_SCALE);

  Pm_Quake3Accelerate(dir, speed, pm->s.params.accel_water);

  // make sure we can go up slopes easily under water
  if (pm_quake3_ground_plane &&
      Vec3_Dot(pm->s.velocity, pm_locals.ground.plane.normal) < 0.f) {

    const float length = Vec3_Length(pm->s.velocity);

    pm->s.velocity = Pm_Quake3ClipVelocity(pm->s.velocity, pm_locals.ground.plane.normal);
    pm->s.velocity = Vec3_Scale(Vec3_Normalize(pm->s.velocity), length);
  }

  Pm_Quake3SlideMove(false);
}

/**
 * @brief Falls, and steers a little while doing it.
 */
static void Pm_Quake3AirMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Quake3Friction();

  // the wish is flat, whatever the view is doing
  const vec3_t forward = Vec3_Normalize(Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f));
  const vec3_t right = Vec3_Normalize(Vec3(pm_locals.right.x, pm_locals.right.y, 0.f));

  vec3_t wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, right);
  wish.z = 0.f;

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish, &speed);
  speed = Minf(speed, pm->s.params.speed_air);

  // with no cap on the wished speed the headroom is the whole of it, which is
  // where strafe jumping comes from. Ducking does not slow this, only the walk
  Pm_Quake3Accelerate(dir, speed, pm->s.params.accel_air);

  // there may be a plane beneath us too steep to have stood on, and it is slid
  // along even though we are airborne
  if (pm_quake3_ground_plane) {
    pm->s.velocity = Pm_Quake3ClipVelocity(pm->s.velocity, pm_locals.ground.plane.normal);
  }

  Pm_Quake3StepSlideMove(true);
}

/**
 * @brief Walks. The vertical speed is never zeroed: the forward and right
 * vectors are projected onto the ground plane instead, so the wish follows the
 * slope, and the finished velocity is turned along that plane rather than
 * clipped to it, so nothing is scrubbed climbing or descending it.
 */
static void Pm_Quake3WalkMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  const vec3_t normal = pm_locals.ground.plane.normal;

  if (pm->water_level > WATER_WAIST && Vec3_Dot(pm_locals.forward, normal) > 0.f) {
    Pm_Quake3WaterMove(); // begin swimming
    return;
  }

  if (Pm_Quake3CheckJump()) { // jumped away
    if (pm->water_level > WATER_FEET) {
      Pm_Quake3WaterMove();
    } else {
      Pm_Quake3AirMove();
    }
    return;
  }

  Pm_Quake3Friction();

  vec3_t forward = Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f);
  vec3_t right = Vec3(pm_locals.right.x, pm_locals.right.y, 0.f);

  // project the forward and right directions onto the ground plane, which is
  // what leaves the wish a vertical component to climb the slope with
  forward = Vec3_Normalize(Pm_Quake3ClipVelocity(forward, normal));
  right = Vec3_Normalize(Pm_Quake3ClipVelocity(right, normal));

  vec3_t wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, right);

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish, &speed);
  speed = Minf(speed, pm->s.params.speed_ground);

  if (pm->s.flags & PMF_DUCKED) {
    speed = Minf(speed, pm->s.params.speed_ducked);
  }

  if (pm->water_level) { // wading, or walking on the bottom
    const float scale = 1.f - (1.f - PM_QUAKE3_SWIM_SCALE) * ((float) pm->water_level / 3.f);
    speed = Minf(speed, pm->s.params.speed_ground * scale);
  }

  // a player who has just been hit, or who is on ice, has air control rather
  // than ground control, which is what lets them be moved
  const bool slipping = (pm_locals.ground.surface & SURF_SLICK) ||
                        (pm->s.flags & PMF_TIME_PUSHED);

  Pm_Quake3Accelerate(dir, speed,
                      slipping ? pm->s.params.accel_ground_slick
                               : pm->s.params.accel_ground);

  if (slipping) {
    pm->s.velocity.z -= pm->s.params.gravity * pm_locals.time;
  } // and otherwise the vertical speed is left alone, for slopes

  const float length = Vec3_Length(pm->s.velocity);

  pm->s.velocity = Pm_Quake3ClipVelocity(pm->s.velocity, normal);

  // turned along the plane rather than clipped to it, so a slope costs nothing
  pm->s.velocity = Vec3_Scale(Vec3_Normalize(pm->s.velocity), length);

  if (pm->s.velocity.x == 0.f && pm->s.velocity.y == 0.f) {
    return; // standing still
  }

  Pm_Quake3StepSlideMove(false);
}

/**
 * @brief A corpse sheds speed on the ground rather than sliding.
 */
static void Pm_Quake3DeadMove(void) {

  if (!(pm->s.flags & PMF_ON_GROUND)) {
    return;
  }

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(pm->s.velocity, &speed);

  speed -= PM_QUAKE3_DEAD_FRICTION;

  pm->s.velocity = speed <= 0.f ? Vec3_Zero() : Vec3_Scale(dir, speed);
}

/**
 * @brief Finds the ground, and decides separately whether there is a plane to
 * slide along and whether the player is standing on it.
 */
static void Pm_Quake3GroundTrace(void) {

  // Quake III's own groundEntityNum persists between moves, so it can tell a
  // landing from merely standing. Quetoo clears PMF_ON_GROUND in Pm_Init, so the
  // ground the move was handed is what carries that edge - and it must be read
  // before anything below overwrites it
  const bool was_grounded = pm->ground.ent != NULL;

  const vec3_t below = Vec3(pm->s.origin.x, pm->s.origin.y,
                            pm->s.origin.z - PM_QUAKE3_GROUND_PROBE);

  // Pm_Trace is itself Quake III's PM_CorrectAllSolid, so the corrective case
  // upstream spells out here is already taken
  const cm_trace_t trace = Pm_Trace(pm->s.origin, below, pm->bounds);
  pm_locals.ground = trace;

  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  if (trace.all_solid) { // nowhere to be, so nothing to stand on
    pm_quake3_ground_plane = false;
    return;
  }

  if (trace.fraction == 1.f) { // in free fall
    pm_quake3_ground_plane = false;
    return;
  }

  // rising and reaching into the plane takes the ground away, however slowly:
  // on a slope this fires far earlier than a speed test would, and by an amount
  // the slope's angle decides. This is the kickoff, and ramp jumps run on it
  if (pm->s.velocity.z > 0.f &&
      Vec3_Dot(pm->s.velocity, trace.plane.normal) > PM_QUAKE3_KICKOFF) {
    Pm_Debug("kickoff\n");
    pm_quake3_ground_plane = false;
    return;
  }

  if (trace.plane.normal.z < PM_QUAKE3_WALK_NORMAL) { // too steep to stand on
    Pm_Debug("steep\n");
    pm_quake3_ground_plane = true; // but there is still a plane to slide along
    return;
  }

  pm_quake3_ground_plane = true;

  // unconditionally, because Pm_Init cleared it
  pm->s.flags |= PMF_ON_GROUND;

  if (pm->s.flags & PMF_TIME_WATER_JUMP) { // solid ground ends a water jump
    // upstream clears only these two here, and would strand its knockback flag
    // behind the zeroed timer exactly as described in Pm_Quake3WaterJumpMove.
    // Quetoo's PMF_TIME_PUSHED is reachable from a jump pad as well as a hit, so
    // this clears the mask rather than reproducing that
    pm->s.flags &= ~PMF_TIME_MASK;
    pm->s.time = 0;
  }

  if (!was_grounded) { // just landed
    Pm_Debug("land\n");

    // upstream's comment says this stops another jump for a little while, but
    // nothing tests the flag: what the timer actually does is hold the velocity
    // through the slide and refuse a water jump. Quake III lets a player jump
    // the instant they land, which is what chaining strafe jumps depends on
    if (pm_locals.previous_velocity.z < PM_QUAKE3_LAND_SPEED) {
      pm->s.flags |= PMF_TIME_LAND;
      pm->s.time = PM_QUAKE3_LAND_TIME;
    }
  }

  pm->ground = trace;

  Pm_TouchEntity(&trace);
}

/**
 * @brief Samples the water around the player.
 */
static void Pm_Quake3SetWaterLevel(void) {

  pm->water_level = WATER_NONE;
  pm->water_type = 0;

  // the samples follow the eye, so ducking changes what counts as submerged
  const float sample2 = pm->s.view_offset.z - pm->bounds.mins.z;
  const float sample1 = sample2 * .5f;

  vec3_t point = Vec3(pm->s.origin.x, pm->s.origin.y,
                      pm->s.origin.z + pm->bounds.mins.z + 1.f);

  int32_t contents = pm->PointContents(point);

  if (contents & CONTENTS_MASK_LIQUID) {
    pm->water_type = contents;
    pm->water_level = WATER_FEET;

    point.z = pm->s.origin.z + pm->bounds.mins.z + sample1;
    contents = pm->PointContents(point);

    if (contents & CONTENTS_MASK_LIQUID) {
      pm->water_level = WATER_WAIST;

      point.z = pm->s.origin.z + pm->bounds.mins.z + sample2;
      contents = pm->PointContents(point);

      if (contents & CONTENTS_MASK_LIQUID) {
        pm->water_level = WATER_UNDER;
        pm->s.flags |= PMF_UNDER_WATER;
      }
    }
  }
}

/**
 * @brief Ducks, which Quake III does instantly and, unlike Quake II, in mid-air
 * as well, and sets the eye height to match.
 */
static void Pm_Quake3CheckDuck(void) {

  if (pm->s.type == PM_DEAD) { // Pm_Init has already set the corpse box
    pm->s.view_offset.z = (pm->s.flags & PMF_GIBLET)
                          ? PM_QUAKE3_VIEW_HEIGHT_GIB
                          : PM_QUAKE3_VIEW_HEIGHT_DEAD;
    return;
  }

  if (pm->cmd.up < 0) {
    pm->s.flags |= PMF_DUCKED;
  } else if (pm->s.flags & PMF_DUCKED) { // stand up if there is room
    pm->bounds = Pm_Bounds(&pm->s.params, false);

    // again pm->Trace, so a ceiling cannot be jittered out from under us
    if (!pm->Trace(pm->s.origin, pm->s.origin, pm->bounds).all_solid) {
      pm->s.flags &= ~PMF_DUCKED;
    }
  }

  const bool ducked = pm->s.flags & PMF_DUCKED;

  pm->bounds = Pm_Bounds(&pm->s.params, ducked);
  pm->s.view_offset.z = ducked ? PM_QUAKE3_VIEW_HEIGHT_DUCK : PM_QUAKE3_VIEW_HEIGHT;
}

/**
 * @brief Rounds the velocity to whole units, as `trap_SnapVector` did.
 * @details `trap_SnapVector` is the engine's, and on the x86 build records were
 * set on it rounded to nearest through the FPU rather than truncating, which is
 * why this rounds where Quake II's quantization cut. `roundf` parts from that
 * only at an exact half, which the FPU took to even. It is observable in the
 * movement it produces, so it belongs to the ruleset rather than to the protocol
 * we actually use.
 */
static void Pm_Quake3SnapVelocity(void) {

  pm->s.velocity = Vec3(roundf(pm->s.velocity.x),
                        roundf(pm->s.velocity.y),
                        roundf(pm->s.velocity.z));
}

/**
 * @brief Quake III's movement, in the order `PmoveSingle` ran it.
 */
void Pm_Quake3Move(void) {

  pm_quake3_ground_plane = false;

  // upstream samples the water before it sizes the box, so these samples are
  // taken against the eye height the last move left. The pass at the end of the
  // move is the one that sees this move's
  Pm_Quake3SetWaterLevel();

  Pm_Quake3CheckDuck();

  Pm_Quake3GroundTrace();

  if (pm->s.type == PM_DEAD) {
    Pm_Quake3DeadMove();
  }

  if (pm->s.flags & PMF_TIME_TELEPORT) {
    // held in place briefly, as Quetoo's teleporters ask; Quake III has no
    // equivalent, clearing the velocity in the game module instead
  } else if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    Pm_Quake3WaterJumpMove();
  } else if (pm->water_level > WATER_FEET) {
    Pm_Quake3WaterMove();
  } else if (pm->s.flags & PMF_ON_GROUND) {
    Pm_Quake3WalkMove();
  } else {
    Pm_Quake3AirMove();
  }

  Pm_Quake3GroundTrace();

  Pm_Quake3SetWaterLevel();

  Pm_Quake3SnapVelocity();

  Pm_CheckViewStep();
}
