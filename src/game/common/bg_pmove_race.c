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
 * @brief The race mod's movement, `PM_MOVEMENT_RACE`.
 *
 * Unlike the others here, this one imitates nothing. It began from Quake II,
 * because the ramp jumping the race mod is built on came to us through Digital
 * Paint: Paintball 2, which is a Quake II engine mod - but Digital Paint is
 * where these mechanics were found, not a standard they are held to. This is
 * Quetoo's own movement for racing, and it is meant to be tuned.
 *
 * That makes it the one movement in this directory that is not frozen, and the
 * exception is deliberate: a movement that imitates a game is finished when it
 * matches, while a movement we own is finished when it is fun. Racing is
 * entirely about movement, so this file is where that work happens.
 *
 * What it takes from Digital Paint, and what makes ramps worth racing on:
 *
 *  - Rising off a ramp does not take the ground away unless the jump is held,
 *    so a player who lets go keeps contact and keeps their vertical speed.
 *  - While that contact holds, the ground accelerates like air rather than like
 *    ground, so speed carries up and off a slope instead of being scrubbed.
 *  - A jump adds to the vertical speed already present, up to a ceiling, so
 *    jumps can be stacked off ramps and lifts rather than replacing each other.
 *  - Rising slower than that same threshold is given up rather than kept, so a
 *    bump does not launch a player where a ramp does.
 *
 * Tuning this changes what times mean, which will matter once anyone is racing
 * for them. It does not matter yet.
 */

/**
 * @brief Where racing starts from, which is Quake II's vector: the ramp work
 * above changes how the movement behaves rather than how fast it runs. These
 * are ours to tune, unlike the imitating movements' numbers.
 */
const pm_params_t pm_race_params = {
  .gravity = 800,
  .gravity_water = 1.f,
  .accel_ground = 10.f,
  .accel_ground_slick = 10.f,
  .accel_air = 1.f,             // uncapped wish, so strafing gains over a wide arc
  .accel_water = 10.f,
  .accel_spectator = 10.f,
  .accel_ladder = 10.f,
  .friction_ground = 6.f,
  .friction_ground_slick = 0.f,
  .friction_air = 0.f,
  .friction_water = 1.f,
  .friction_spectator = 6.f,
  .friction_ladder = 6.f,
  .speed_ground = 300.f,
  .speed_air = 300.f,
  .speed_water = 400.f,
  .speed_ladder = 200.f,
  .speed_spectator = 500.f,
  .speed_stop = 100.f,
  .speed_jump = 270.f,          // what a jump adds, not what it sets
  .speed_ducked = 100.f,
  .speed_duck_stand = 0.f,
  .speed_water_jump = 350.f,
  .height = 32.f,
  .height_ducked = 4.f
};

#define PM_RACE_STEP_SIZE        18.f  // STEPSIZE
#define PM_RACE_CLIP_PLANES      5     // MAX_CLIP_PLANES
#define PM_RACE_BUMPS            4     // numbumps
#define PM_RACE_STOP_EPSILON     .1f   // STOP_EPSILON
#define PM_RACE_OVERBOUNCE       1.01f // what a clip gives back, unlike QuakeWorld
#define PM_RACE_STEP_NORMAL      .7f   // MIN_STEP_NORMAL
#define PM_RACE_GROUND_NORMAL    .7f   // the steepest plane that is still floor
#define PM_RACE_GROUND_PROBE     .25f  // how far down ground is looked for
#define PM_RACE_UP_SPEED         180.f // rising faster than this is never grounded
#define PM_RACE_WATER_SCALE      .5f   // wish speed is halved while swimming
#define PM_RACE_WATER_SINK       60.f  // and drifts downward this fast with no input
#define PM_RACE_LAND_SPEED      -200.f // landing harder than this locks the jump out
#define PM_RACE_LAND_SPEED_HARD -400.f // and harder than this locks it out longer
#define PM_RACE_LAND_TIME        144   // 18 of Quake II's eight-millisecond ticks
#define PM_RACE_LAND_TIME_HARD   200   // and 25 of them
#define PM_RACE_JUMP_UP_MIN      10    // how far the jump key must be down to count
#define PM_RACE_SWIM_JUMP_MIN   -300.f // sinking faster than this cannot swim upward
#define PM_RACE_LADDER_PROBE     1.f   // how far ahead a ladder is looked for
#define PM_RACE_LADDER_SPEED     25.f  // the horizontal speed a ladder allows
#define PM_RACE_LADDER_HOLD      200.f // the vertical speed at which it holds still
#define PM_RACE_WATER_JUMP_DIST  30.f  // how far ahead the ledge is looked for
#define PM_RACE_WATER_JUMP_UP    4.f   // and how far up
#define PM_RACE_WATER_JUMP_CLEAR 16.f  // and how much clear air it needs above
#define PM_RACE_WATER_JUMP_PUSH  50.f  // how hard the player is pushed at it
#define PM_RACE_WATER_JUMP_TIME  2040  // 255 ticks of no control
#define PM_RACE_CURRENT_SPEED    100.f // the push of a conveyor
#define PM_RACE_SNAP             8.f   // the precision origin and velocity are cut to
#define PM_RACE_VIEW_HEIGHT      22.f  // the eye, against Quetoo's 30
#define PM_RACE_VIEW_HEIGHT_DUCK -2.f  // and ducked
#define PM_RACE_DEAD_FRICTION    20.f  // the speed a corpse sheds each move

// what racing adds. These are the tuning surface: they are named for what they
// do rather than for where they came from, because they are expected to move
// PM_RACE_UP_SPEED has to stay below what a jump leaves after a command of
// gravity, or the second categorize re-grounds the player and the next move
// gives their climb away
#define PM_RACE_JUMP_CEILING     450.f // the most vertical speed a jump may leave
#define PM_RACE_SLIDE_ACCEL      1.f   // how a slope accelerates while contact holds
#define PM_RACE_SLIDE_ALIGNMENT  .01f  // how aligned the wish must be to keep sliding

/**
 * @brief Whether the player is riding a slope upward with the ground still under
 * them, decided each move by `Pm_RaceCategorizePosition` and spent by
 * `Pm_RaceAirMove`. Reset at the top of every move, so nothing survives one.
 */
static bool pm_race_sliding;

/**
 * @brief Slides `in` along `normal`, giving a little back.
 */
static vec3_t Pm_RaceClipVelocity(const vec3_t in, const vec3_t normal) {

  const float backoff = Vec3_Dot(in, normal) * PM_RACE_OVERBOUNCE;

  vec3_t out = Vec3_Subtract(in, Vec3_Scale(normal, backoff));

  if (out.x > -PM_RACE_STOP_EPSILON && out.x < PM_RACE_STOP_EPSILON) {
    out.x = 0.f;
  }
  if (out.y > -PM_RACE_STOP_EPSILON && out.y < PM_RACE_STOP_EPSILON) {
    out.y = 0.f;
  }
  if (out.z > -PM_RACE_STOP_EPSILON && out.z < PM_RACE_STOP_EPSILON) {
    out.z = 0.f;
  }

  return out;
}

/**
 * @brief Slides through the world, clipping to every plane struck.
 * @details Unlike QuakeWorld, each plane clips the velocity as it stands rather
 * than the one the move began with, so the clips compound.
 */
static void Pm_RaceSlideMove(void) {

  const vec3_t primal_velocity = pm->s.velocity;

  cm_bsp_plane_t planes[PM_RACE_CLIP_PLANES];
  int32_t num_planes = 0;

  float time_left = pm_locals.time;

  for (int32_t bump = 0; bump < PM_RACE_BUMPS; bump++) {

    const vec3_t end = Vec3_Fmaf(pm->s.origin, time_left, pm->s.velocity);
    const cm_trace_t trace = Pm_Trace(pm->s.origin, end, pm->bounds);

    if (trace.all_solid) { // trapped in a solid
      pm->s.velocity.z = 0.f; // and do not build up falling damage
      return;
    }

    if (trace.fraction > 0.f) { // covered some distance
      pm->s.origin = trace.end;
      num_planes = 0;
    }

    if (trace.fraction == 1.f) { // moved the entire distance
      break;
    }

    Pm_TouchEntity(&trace);

    time_left -= time_left * trace.fraction;

    if (num_planes >= PM_RACE_CLIP_PLANES) { // this should not happen
      pm->s.velocity = Vec3_Zero();
      break;
    }

    planes[num_planes++] = trace.plane;

    // slide along the first plane that the others do not immediately undo
    int32_t i;
    for (i = 0; i < num_planes; i++) {
      pm->s.velocity = Pm_RaceClipVelocity(pm->s.velocity, planes[i].normal);

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

  if (pm->s.time) { // a timed move keeps the velocity it was given
    pm->s.velocity = primal_velocity;
  }
}

/**
 * @brief Slides, and again from a step height up, keeping whichever went
 * farther across the ground.
 */
static void Pm_RaceStepSlideMove(void) {

  const vec3_t start_origin = pm->s.origin;
  const vec3_t start_velocity = pm->s.velocity;

  Pm_RaceSlideMove();

  const vec3_t down_origin = pm->s.origin;
  const vec3_t down_velocity = pm->s.velocity;

  const vec3_t up = Vec3(start_origin.x, start_origin.y,
                         start_origin.z + PM_RACE_STEP_SIZE);

  // pm->Trace and not Pm_Trace: the latter jitters the start by up to a unit to
  // escape a solid, so it cannot answer whether the box fits where it is
  if (pm->Trace(up, up, pm->bounds).all_solid) {
    return; // no room to step up
  }

  pm->s.origin = up;
  pm->s.velocity = start_velocity;

  Pm_RaceSlideMove();

  // and press back down the step height
  const vec3_t down = Vec3(pm->s.origin.x, pm->s.origin.y,
                           pm->s.origin.z - PM_RACE_STEP_SIZE);

  const cm_trace_t trace = Pm_Trace(pm->s.origin, down, pm->bounds);

  if (!trace.all_solid) {
    pm->s.origin = trace.end;
  }

  const float down_dist = Vec2_DistanceSquared(Vec3_XY(down_origin), Vec3_XY(start_origin));
  const float up_dist = Vec2_DistanceSquared(Vec3_XY(pm->s.origin), Vec3_XY(start_origin));

  if (down_dist > up_dist || trace.plane.normal.z < PM_RACE_STEP_NORMAL) {
    pm->s.origin = down_origin;
    pm->s.velocity = down_velocity;
    return;
  }

  // walking along a plane keeps the vertical speed the flat move ended with
  pm->s.velocity.z = down_velocity.z;

  pm->step = pm->s.origin.z - pm_locals.previous_origin.z;
}

/**
 * @brief Bleeds speed off. Ground and water friction are additive here, unlike
 * QuakeWorld, where the one excludes the other.
 */
static void Pm_RaceFriction(void) {

  const float speed = Vec3_Length(pm->s.velocity);
  if (speed < 1.f) {
    pm->s.velocity.x = pm->s.velocity.y = 0.f;
    return;
  }

  float drop = 0.f;

  const bool slick = pm_locals.ground.surface & SURF_SLICK;

  // not while riding a slope: the whole point of keeping the ground there is to
  // carry speed off it, and ground friction applies to all three axes, so it
  // would scrub the climb as well as the run
  if (((pm->s.flags & PMF_ON_GROUND) && !slick && !pm_race_sliding) ||
      (pm->s.flags & PMF_ON_LADDER)) {
    const float control = Maxf(speed, pm->s.params.speed_stop);
    drop += control * pm->s.params.friction_ground * pm_locals.time;
  }

  if (pm->water_level && !(pm->s.flags & PMF_ON_LADDER)) {
    drop += speed * pm->s.params.friction_water * (float) pm->water_level * pm_locals.time;
  }

  pm->s.velocity = Vec3_Scale(pm->s.velocity, Maxf(0.f, speed - drop) / speed);
}

/**
 * @brief Accelerates toward `dir`, up to `speed`.
 */
static void Pm_RaceAccelerate(const vec3_t dir, float speed, float accel) {

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
 * @brief Adds ladder, water and conveyor movement to the wish.
 */
static vec3_t Pm_RaceAddCurrents(vec3_t wish) {

  if ((pm->s.flags & PMF_ON_LADDER) &&
      fabsf(pm->s.velocity.z) <= PM_RACE_LADDER_HOLD) {

    const float speed = pm->s.params.speed_ladder;

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

    wish.x = Clampf(wish.x, -PM_RACE_LADDER_SPEED, PM_RACE_LADDER_SPEED);
    wish.y = Clampf(wish.y, -PM_RACE_LADDER_SPEED, PM_RACE_LADDER_SPEED);
  }

  if (pm->water_type & CONTENTS_MASK_CURRENT) {
    vec3_t current = Vec3_Zero();

    if (pm->water_type & CONTENTS_CURRENT_0) {
      current.x += 1.f;
    }
    if (pm->water_type & CONTENTS_CURRENT_90) {
      current.y += 1.f;
    }
    if (pm->water_type & CONTENTS_CURRENT_180) {
      current.x -= 1.f;
    }
    if (pm->water_type & CONTENTS_CURRENT_270) {
      current.y -= 1.f;
    }
    if (pm->water_type & CONTENTS_CURRENT_UP) {
      current.z += 1.f;
    }
    if (pm->water_type & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.f;
    }

    float speed = pm->s.params.speed_water;
    if (pm->water_level == WATER_FEET && (pm->s.flags & PMF_ON_GROUND)) {
      speed *= .5f;
    }

    wish = Vec3_Fmaf(wish, speed, current);
  }

  if (pm->s.flags & PMF_ON_GROUND) {
    vec3_t current = Vec3_Zero();

    if (pm_locals.ground.contents & CONTENTS_CURRENT_0) {
      current.x += 1.f;
    }
    if (pm_locals.ground.contents & CONTENTS_CURRENT_90) {
      current.y += 1.f;
    }
    if (pm_locals.ground.contents & CONTENTS_CURRENT_180) {
      current.x -= 1.f;
    }
    if (pm_locals.ground.contents & CONTENTS_CURRENT_270) {
      current.y -= 1.f;
    }
    if (pm_locals.ground.contents & CONTENTS_CURRENT_UP) {
      current.z += 1.f;
    }
    if (pm_locals.ground.contents & CONTENTS_CURRENT_DOWN) {
      current.z -= 1.f;
    }

    wish = Vec3_Fmaf(wish, PM_RACE_CURRENT_SPEED, current);
  }

  return wish;
}

/**
 * @brief Swims.
 */
static void Pm_RaceWaterMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  vec3_t wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, pm_locals.forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, pm_locals.right);

  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
    wish.z -= PM_RACE_WATER_SINK; // drift toward the bottom
  } else {
    wish.z += pm->cmd.up;
  }

  wish = Pm_RaceAddCurrents(wish);

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish, &speed);
  speed = Minf(speed, pm->s.params.speed_ground) * PM_RACE_WATER_SCALE;

  Pm_RaceAccelerate(dir, speed, pm->s.params.accel_water);

  Pm_RaceStepSlideMove();
}

/**
 * @brief Walks, climbs and falls. Gravity is applied here, in each case that
 * needs it.
 */
static void Pm_RaceAirMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  // Quake II asks for a third of the pitch here, and only here: swimming uses
  // the whole of it. The comment upstream wonders whether this is needed; it is
  // what the movement does, so it is what this does
  vec3_t angles = pm->angles;
  angles.x /= 3.f;

  vec3_t forward, right;
  Vec3_Vectors(angles, &forward, &right, NULL);

  vec3_t wish = Vec3_Zero();
  wish = Vec3_Fmaf(wish, pm->cmd.forward, forward);
  wish = Vec3_Fmaf(wish, pm->cmd.right, right);
  wish.z = 0.f;

  wish = Pm_RaceAddCurrents(wish);

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(wish, &speed);

  const float max_speed = (pm->s.flags & PMF_DUCKED)
                          ? pm->s.params.speed_ducked
                          : pm->s.params.speed_ground;
  speed = Minf(speed, max_speed);

  const float gravity = pm->s.params.gravity * pm_locals.time;

  if (pm->s.flags & PMF_ON_LADDER) {

    Pm_RaceAccelerate(dir, speed, pm->s.params.accel_ladder);

    if (wish.z == 0.f) { // hold still against gravity
      if (pm->s.velocity.z > 0.f) {
        pm->s.velocity.z = Maxf(0.f, pm->s.velocity.z - gravity);
      } else {
        pm->s.velocity.z = Minf(0.f, pm->s.velocity.z + gravity);
      }
    }

    Pm_RaceStepSlideMove();

  } else if (pm->s.flags & PMF_ON_GROUND) {

    float accel = pm->s.params.accel_ground;

    if (pm_race_sliding) {
      vec3_t along = pm->s.velocity;
      along.z = 0.f;
      along = Vec3_Normalize(along);

      // asking for roughly where we are already going keeps the slide, and the
      // slope then accelerates like air so the speed carries off it. Asking for
      // anything else - or for nothing, which normalizes to zero - gives it up
      if (Vec3_Dot(along, dir) > PM_RACE_SLIDE_ALIGNMENT) {
        accel = PM_RACE_SLIDE_ACCEL;
      } else {
        pm_race_sliding = false;
      }
    }

    // gravity applies with the ground under us, which is what lets a rising
    // slope contact arc rather than hold. Sliding keeps the vertical speed it
    // arrived with; not sliding gives it up, as ordinary ground does
    if (!pm_race_sliding) {
      pm->s.velocity.z = 0.f;
    }

    pm->s.velocity.z -= gravity;

    Pm_RaceAccelerate(dir, speed, accel);

    if (pm->s.velocity.x || pm->s.velocity.y) {
      Pm_RaceStepSlideMove();
    }

  } else {

    // a plain acceleration at 1, with no cap on the wished speed, so strafing
    // gains over a wide arc - which is what racing wants
    Pm_RaceAccelerate(dir, speed, pm->s.params.accel_air);

    pm->s.velocity.z -= gravity;

    Pm_RaceStepSlideMove();
  }
}

/**
 * @brief Classifies the ground beneath the player and the water around them.
 */
static void Pm_RaceCategorizePosition(void) {

  // Quake II's own ON_GROUND flag persists between moves, so it can tell a
  // landing from merely standing. Quetoo clears that flag in Pm_Init, so the
  // ground the move was handed is what carries the edge - and it must be read
  // before this function overwrites it, or every grounded frame looks like a
  // landing and the jump lockout never lifts
  const bool was_grounded = pm->ground.ent != NULL;

  if (pm->s.flags & PMF_TIME_PUSHED) { // the plumbing asks us not to seek ground
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else if (pm->s.velocity.z > PM_RACE_UP_SPEED && (pm->s.flags & PMF_JUMP_HELD)) {

    // rising fast takes the ground away only from a player who is holding jump.
    // Letting go keeps contact, which is what carries speed up and off a slope
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  } else {
    const vec3_t below = Vec3(pm->s.origin.x, pm->s.origin.y,
                              pm->s.origin.z - PM_RACE_GROUND_PROBE);

    const cm_trace_t trace = Pm_Trace(pm->s.origin, below, pm->bounds);
    pm_locals.ground = trace;

    // a steep plane is still ground if we started inside it
    if (!trace.ent || (trace.plane.normal.z < PM_RACE_GROUND_NORMAL && !trace.start_solid)) {
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

      // riding a slope upward, with the ground still under us
      if (pm->s.velocity.z > PM_RACE_UP_SPEED && !(pm->s.flags & PMF_JUMP_HELD)) {
        pm_race_sliding = true;
      }

      if (!was_grounded) { // just landed

        // a hard landing locks the jump out briefly
        if (pm->s.velocity.z < PM_RACE_LAND_SPEED) {
          pm->s.flags |= PMF_TIME_LAND;
          pm->s.time = pm->s.velocity.z < PM_RACE_LAND_SPEED_HARD
                       ? PM_RACE_LAND_TIME_HARD
                       : PM_RACE_LAND_TIME;
        }
      }
    }

    Pm_TouchEntity(&trace);
  }

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
 * @brief Jumps, or swims upward. Quake II adds to the vertical speed and then
 * insists on at least the jump speed, so a jump while falling is neither
 * weakened nor strengthened.
 */
static void Pm_RaceCheckJump(void) {

  if (pm->s.flags & PMF_TIME_LAND) { // too soon after landing
    return;
  }

  if (pm->cmd.up < PM_RACE_JUMP_UP_MIN) { // not holding jump
    pm->s.flags &= ~PMF_JUMP_HELD;
    return;
  }

  if (pm->s.flags & PMF_JUMP_HELD) { // must be released first
    return;
  }

  if (pm->s.type == PM_DEAD) {
    return;
  }

  if (pm->water_level >= WATER_WAIST) { // swimming, not jumping
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));

    if (pm->s.velocity.z <= PM_RACE_SWIM_JUMP_MIN) { // sinking too fast
      return;
    }

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

  // already above the ceiling, a jump does nothing at all - and must not take
  // the ground away or announce itself either, or it would cost the player the
  // slope contact it is refusing to add to
  if (pm->s.velocity.z >= PM_RACE_JUMP_CEILING) {
    pm->s.flags |= PMF_JUMP_HELD;
    return;
  }

  pm->s.flags |= PMF_JUMP_HELD | PMF_JUMPED;
  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  // a jump adds to the vertical speed already present rather than replacing it,
  // so jumps can be stacked off a ramp or a lift

  const float jump = Minf(pm->s.params.speed_jump,
                          PM_RACE_JUMP_CEILING - pm->s.velocity.z);

  pm->s.velocity.z += jump;
  pm->s.velocity.z = Maxf(pm->s.velocity.z, jump);
}

/**
 * @brief Looks for a ladder to hold, and for a ledge to hop out of water onto.
 */
static void Pm_RaceCheckSpecialMovement(void) {

  if (pm->s.time) { // a timer is already running the move
    return;
  }

  pm->s.flags &= ~PMF_ON_LADDER;

  vec3_t forward = Vec3(pm_locals.forward.x, pm_locals.forward.y, 0.f);
  forward = Vec3_Normalize(forward);

  const vec3_t ahead = Vec3_Fmaf(pm->s.origin, PM_RACE_LADDER_PROBE, forward);

  const cm_trace_t trace = Pm_Trace(pm->s.origin, ahead, pm->bounds);
  if (trace.fraction < 1.f && (trace.contents & CONTENTS_LADDER)) {
    pm->s.flags |= PMF_ON_LADDER;
  }

  if (pm->water_level != WATER_WAIST) {
    return;
  }

  vec3_t spot = Vec3_Fmaf(pm->s.origin, PM_RACE_WATER_JUMP_DIST, forward);
  spot.z += PM_RACE_WATER_JUMP_UP;

  if (!(pm->PointContents(spot) & CONTENTS_SOLID)) {
    return;
  }

  spot.z += PM_RACE_WATER_JUMP_CLEAR;

  if (pm->PointContents(spot)) { // must be clear above the ledge
    return;
  }

  pm->s.velocity = Vec3_Scale(forward, PM_RACE_WATER_JUMP_PUSH);
  pm->s.velocity.z = pm->s.params.speed_water_jump;

  pm->s.flags |= PMF_TIME_WATER_JUMP;
  pm->s.time = PM_RACE_WATER_JUMP_TIME;
}

/**
 * @brief Ducks, which Quake II does instantly and only on the ground, and sets
 * the eye height to match.
 */
static void Pm_RaceCheckDuck(void) {

  if (pm->s.type == PM_DEAD) {
    if (pm->s.flags & PMF_GIBLET) {
      pm->s.view_offset.z = 8.f;
      return;
    }

    pm->s.flags |= PMF_DUCKED;
  } else if (pm->cmd.up < 0 && pm->ground.ent) {
    // Quake II reads its own ON_GROUND flag here, which persists between moves;
    // Quetoo clears that flag in Pm_Init, so the ground this move was handed is
    // what carries the same meaning. Ducking is still refused in mid-air
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
  pm->s.view_offset.z = ducked ? PM_RACE_VIEW_HEIGHT_DUCK : PM_RACE_VIEW_HEIGHT;
}

/**
 * @brief A corpse sheds speed on the ground rather than sliding.
 */
static void Pm_RaceDeadMove(void) {

  if (!(pm->s.flags & PMF_ON_GROUND)) {
    return;
  }

  float speed;
  const vec3_t dir = Vec3_NormalizeLength(pm->s.velocity, &speed);

  speed -= PM_RACE_DEAD_FRICTION;

  pm->s.velocity = speed <= 0.f ? Vec3_Zero() : Vec3_Scale(dir, speed);
}

/**
 * @brief Cuts the origin and the velocity to the precision Quake II's network
 * channel carried, and looks for a free position if that landed inside
 * something.
 * @details The quantization is observable in the movement it produces, which is
 * why it belongs to the ruleset rather than to the protocol we actually use.
 */
static void Pm_RaceSnapPosition(void) {

  pm->s.velocity = Vec3(truncf(pm->s.velocity.x * PM_RACE_SNAP) / PM_RACE_SNAP,
                        truncf(pm->s.velocity.y * PM_RACE_SNAP) / PM_RACE_SNAP,
                        truncf(pm->s.velocity.z * PM_RACE_SNAP) / PM_RACE_SNAP);

  const vec3_t wanted = pm->s.origin;

  vec3_t base = Vec3(truncf(wanted.x * PM_RACE_SNAP) / PM_RACE_SNAP,
                     truncf(wanted.y * PM_RACE_SNAP) / PM_RACE_SNAP,
                     truncf(wanted.z * PM_RACE_SNAP) / PM_RACE_SNAP);

  // which way each axis was rounded, so that the jitter tries putting it back
  vec3_t sign = Vec3(wanted.x >= 0.f ? 1.f : -1.f,
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

    vec3_t candidate = base;

    if (bits & 1) {
      candidate.x += sign.x / PM_RACE_SNAP;
    }
    if (bits & 2) {
      candidate.y += sign.y / PM_RACE_SNAP;
    }
    if (bits & 4) {
      candidate.z += sign.z / PM_RACE_SNAP;
    }

    if (!pm->Trace(candidate, candidate, pm->bounds).all_solid) {
      pm->s.origin = candidate;
      return;
    }
  }

  pm->s.origin = pm_locals.previous_origin; // nowhere to be, so stay put
}

/**
 * @brief Quake II's movement, in the order `Pmove` ran it.
 */
void Pm_RaceMove(void) {

  pm_race_sliding = false;

  Pm_RaceCheckDuck();

  Pm_RaceCategorizePosition();

  if (pm->s.type == PM_DEAD) {
    Pm_RaceDeadMove();
  }

  Pm_RaceCheckSpecialMovement();

  if (pm->s.flags & PMF_TIME_TELEPORT) {
    // stay exactly in place
  } else if (pm->s.flags & PMF_TIME_WATER_JUMP) {

    pm->s.velocity.z -= pm->s.params.gravity * pm_locals.time;

    if (pm->s.velocity.z < 0.f) { // cancel as soon as we fall again
      pm->s.flags &= ~(PMF_TIME_WATER_JUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
      pm->s.time = 0;
    }

    Pm_RaceStepSlideMove();

  } else {

    Pm_RaceCheckJump();

    Pm_RaceFriction();

    if (pm->water_level >= WATER_WAIST) {
      Pm_RaceWaterMove();
    } else {
      Pm_RaceAirMove();
    }
  }

  Pm_RaceCategorizePosition();

  Pm_RaceSnapPosition();

  Pm_CheckViewStep();
}
