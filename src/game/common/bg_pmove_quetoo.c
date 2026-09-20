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
 * @brief Quetoo's own movement kernel, `PM_MOVEMENT_QUETOO`.
 *
 * Everything here was `bg_pmove.c` before the kernel became a selection rather
 * than the only option; the move it performs is unchanged. What stayed behind
 * is the plumbing every kernel shares, which `bg_pmove_local.h` declares.
 */

/**
 * @brief Slide off of the impacted plane.
 */
static Vec3 Pm_ClipVelocity(const Vec3 in, const Vec3 normal, float bounce) {

  float backoff = Vec3_Dot(in, normal);

  if (backoff < 0.f) {
    backoff *= bounce;
  } else {
    backoff /= bounce;
  }

  return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

/**
 * @brief Collide with the results of the trace, clipping our velocity along the normal.
 */
static void Pm_ClipMove(const CmTrace *trace) {

  if (trace->ent == NULL) {
    return;
  }

  if (pmLocals.numClipPlanes == MAX_CLIP_PLANES) {
    Pm_Debug("MAX_CLIP_PLANES\n");
    return;
  }

  // determine if this plane is new to this move
  for (int32_t i = 0; i < pmLocals.numClipPlanes; i++) {
    if (Vec3_Dot(trace->plane.normal, pmLocals.clipPlanes[i].normal) > 1.f - ON_EPSILON) {
      return;
    }
  }

  pmLocals.clipPlanes[pmLocals.numClipPlanes++] = trace->plane;

  // it is, so clip to it, and nudge out along the normal
  pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, trace->plane.normal, PM_CLIP_BOUNCE);
  pm->s.origin = Vec3_Fmaf(pm->s.origin, TRACE_EPSILON, trace->plane.normal);

  // re-clip to all previously intersected planes, too
  for (int32_t i = 0; i < pmLocals.numClipPlanes - 1; i++) {
    pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, pmLocals.clipPlanes[i].normal, PM_CLIP_BOUNCE);
  }
}

/**
 * @brief Slide through the world, clipping to impacted planes.
 */
static float Pm_SlideMove(void) {

  const Vec3 org0 = pm->s.origin;

  memset(pmLocals.clipPlanes, 0, sizeof(pmLocals.clipPlanes));
  pmLocals.numClipPlanes = 0;

  float time = pmLocals.time;
  while (time > 0.f) {

    // project desired destination
    const Vec3 pos = Vec3_Fmaf(pm->s.origin, time, pm->s.velocity);

    // and move distance
    const float dist0 = Vec3_Distance(pos, org0);

    // trace to it
    const CmTrace trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

    // move to the end position
    pm->s.origin = trace.end;

    // store a reference to the entity for firing game events
    Pm_TouchEntity(&trace);

    // clip along the plane
    Pm_ClipMove(&trace);

    // calculate the actual move distance, which includes nudging along the normal
    const float dist1 = Vec3_Distance(pm->s.origin, org0);

    // calculate the trace fraction based on actual distance moved
    float fraction = Maxf(trace.fraction, dist1 / dist0);

    // if we didn't move at all, we're done
    if (fraction == 0.f || isnan(fraction)) {
      break;
    }

    // and update the movement time remaining
    time -= time * fraction;
  }

  const Vec3 org1 = pm->s.origin;

  return fabsf(Vec2_Distance(Vec3_XY(org0), Vec3_XY(org1)));
}

/**
 * @return True if the downward trace yielded a step, false otherwise.
 */
static bool Pm_CheckStep(const CmTrace *trace) {

  if (!trace->allSolid) {
    if (trace->ent && trace->plane.normal.z >= PM_STEP_NORMAL) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Moves the player origin to the end of a step-down trace and records the step height.
 */
static void Pm_StepDown(const CmTrace *trace) {

  pm->s.origin = trace->end;
  
  const float stepHeight = pm->s.origin.z - pmLocals.previousOrigin.z;

  if (fabsf(stepHeight) >= PM_STEP_HEIGHT_MIN) {
    pm->step = stepHeight;
  }
}

/**
 * @brief Performs a slide move with stair stepping, attempting to step up over obstacles.
 */
static void Pm_StepSlideMove(void) {

  // store pre-move parameters
  const Vec3 org0 = pm->s.origin;
  const Vec3 vel0 = pm->s.velocity;

  // attempt to move
  float dist0 = Pm_SlideMove();

  // attempt to step down to remain on ground
  if ((pm->s.flags & PMF_ON_GROUND) && pm->cmd.up <= 0) {

    const Vec3 down = Vec3_Fmaf(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, Vec3_Down());
    const CmTrace stepDown = Pm_Trace(pm->s.origin, down, pm->bounds);

    if (Pm_CheckStep(&stepDown)) {
      Pm_StepDown(&stepDown);
    }
  }

  // now that we're on the ground, try to step over any obstacles
  const Vec3 org1 = pm->s.origin;
  const Vec3 vel1 = pm->s.velocity;

  const Vec3 up = Vec3_Fmaf(org0, PM_STEP_HEIGHT, Vec3_Up());
  const CmTrace stepUp = Pm_Trace(org0, up, pm->bounds);

  if (stepUp.fraction == 1.f) {

    // step from the higher position, with the original velocity
    pm->s.origin = stepUp.end;
    pm->s.velocity = vel0;

    const float dist1 = Pm_SlideMove();
    if (dist1 > dist0) {

      // settle to the new ground, keeping the step if and only if it was successful
      const Vec3 down = Vec3_Fmaf(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, Vec3_Down());
      const CmTrace stepDown = Pm_Trace(pm->s.origin, down, pm->bounds);

      if (Pm_CheckStep(&stepDown)) {
        // Quake2 trick jump secret sauce
        if ((pm->s.flags & PMF_ON_GROUND) || vel0.z < PM_SPEED_UP) {
          Pm_StepDown(&stepDown);
        } else {
          pm->step = pm->s.origin.z - pmLocals.previousOrigin.z;
        }

        return;
      }
    }
  }

  // stepping up was not helpful, so take the lower movement
  pm->s.origin = org1;
  pm->s.velocity = vel1;
}

/**
 * @brief Applies water and conveyor belt current velocities to the player.
 */
static void Pm_Currents(void) {
  Vec3 current = Vec3_Zero();

  // add water currents
  if (pm->waterLevel) {
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
  }

  // add conveyer belt velocities
  if (pm->ground.ent) {
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
  }

  if (!Vec3_Equal(current, Vec3_Zero())) {
    current = Vec3_Normalize(current);
  }

  pm->s.velocity = Vec3_Fmaf(pm->s.velocity, PM_SPEED_CURRENT, current);
}

/**
 * @return True if the player will be eligible for trick jumping should they
 * impact the ground on this frame, false otherwise.
 */
static bool Pm_CheckTrickJump(void) {

  if (pm->ground.ent) {
    return false;
  }

  if (pmLocals.previousVelocity.z < PM_SPEED_UP) {
    return false;
  }

  if (pm->cmd.up < 1) {
    return false;
  }

  if (pm->s.flags & PMF_JUMP_HELD) {
    return false;
  }

  if (pm->s.flags & PMF_TIME_MASK) {
    return false;
  }

  return true;
}

/**
 * @return True if the player is attempting to leave the ground via grappling hook.
 */
static bool Pm_CheckHookJump(void) {

  if ((pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) && (pm->s.velocity.z > 1.f)) {

    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));

    return true;
  }

  return false;
}

/**
 * @brief Validates and processes grappling hook state, updating movement type as needed.
 */
static void Pm_CheckHook(void) {

  // hookers only
  if (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO) {
    pm->s.flags &= ~PMF_HOOK_RELEASED;
    return;
  }

  // if we let go of hook, just go back to normal
  if ((pm->s.type == PM_HOOK_PULL || pm->s.type == PM_HOOK_SWING_AUTO) && !(pm->cmd.buttons & BUTTON_HOOK)) {
    pm->s.type = PM_NORMAL;
    return;
  }

  // get chain length
  if (pm->s.type == PM_HOOK_PULL) {

    pm->cmd.forward = pm->cmd.right = 0;

    // pull physics
    const float dist = Vec3_DistanceDir(pm->s.hookPosition, pm->s.origin, &pm->s.velocity);
    if (dist > PM_HOOK_MIN_DIST && !Pm_CheckHookJump()) {
      pm->s.velocity = Vec3_Scale(pm->s.velocity, pm->hookPullSpeed);
    } else {
      pm->s.velocity = Vec3_Zero();
    }
  } else {

    // check for disable
    if (!(pm->s.flags & PMF_HOOK_RELEASED)) {

      if (!(pm->cmd.buttons & BUTTON_HOOK)) {
        pm->s.flags |= PMF_HOOK_RELEASED;
      }
    } else {

      // if we let go of hook, just go back to normal.
      if (pm->cmd.buttons & BUTTON_HOOK) {
        pm->s.type = PM_NORMAL;
        pm->s.flags &= ~PMF_HOOK_RELEASED;
        return;
      }
    }

    const float hookRate = (pm->hookPullSpeed / 1.5f) * pmLocals.time;

    // chain physics
    // grow/shrink chain based on input
    if ((pm->cmd.up > 0 || !(pm->s.flags & PMF_HOOK_RELEASED)) && (pm->s.hookLength > PM_HOOK_MIN_DIST)) {
      pm->s.hookLength = Maxf(pm->s.hookLength - hookRate, PM_HOOK_MIN_DIST);
    } else if ((pm->cmd.up < 0) && (pm->s.hookLength < PM_HOOK_MAX_DIST)) {
      pm->s.hookLength = Minf(pm->s.hookLength + hookRate, PM_HOOK_MAX_DIST);
    }

    Vec3 chainVec = Vec3_Subtract(pm->s.hookPosition, pm->s.origin);
    float chainLen = Vec3_Length(chainVec);

    // if player's location is already within the chain's reach
    if (chainLen <= pm->s.hookLength) {
      return;
    }

    // reel us in!
    Vec3 velPart;

    // determine player's velocity component of chain vector
    velPart = Vec3_Scale(chainVec, Vec3_Dot(pm->s.velocity, chainVec) / Vec3_Dot(chainVec, chainVec));

    // restrainment default force
    float force = (chainLen - pm->s.hookLength) * 5.f;

    // if player's velocity heading is away from the hook
    if (Vec3_Dot(pm->s.velocity, chainVec) < 0.f) {

      // if chain has streched for PM_HOOK_MIN_DIST units
      if (chainLen > pm->s.hookLength + PM_HOOK_MIN_DIST) {

        // remove player's velocity component moving away from hook
        pm->s.velocity = Vec3_Subtract(pm->s.velocity, velPart);
      }
    } else { // if player's velocity heading is towards the hook

      if (Vec3_Length(velPart) < force) {
        force -= Vec3_Length(velPart);
      } else {
        force = 0.f;
      }
    }

    if (force) {
      // applies chain restrainment
      chainVec = Vec3_Normalize(chainVec);
      pm->s.velocity = Vec3_Fmaf(pm->s.velocity, force, chainVec);
    }
  }
}

/**
 * @brief Checks for ground interaction, enabling trick jumping and dealing with landings.
 */
static void Pm_CheckGround(void) {

  if (Pm_CheckHookJump()) {
    return;
  }

  // if we jumped, or been pushed, do not attempt to seek ground
  if (pm->s.flags & (PMF_JUMPED | PMF_TIME_PUSHED | PMF_ON_LADDER)) {
    return;
  }

  // seek ground eagerly if the player wishes to trick jump
  const bool trickJump = Pm_CheckTrickJump();
  Vec3 pos;

  if (trickJump) {
    pos = Vec3_Fmaf(pm->s.origin, pmLocals.time, pm->s.velocity);
    pos.z -= PM_GROUND_DIST_TRICK;
  } else {
    pos = pm->s.origin;
    pos.z -= PM_GROUND_DIST;
  }

  // seek the ground
  CmTrace trace = pmLocals.ground = Pm_Trace(pm->s.origin, pos, pm->bounds);

  // if we hit an upward facing plane, make it our ground
  if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {

    // if we had no ground, then handle landing events
    if (!pm->ground.ent) {

      // any landing terminates the water jump
      if (pm->s.flags & PMF_TIME_WATER_JUMP) {
        pm->s.flags &= ~PMF_TIME_WATER_JUMP;
        pm->s.time = 0;
      }

      // hard landings disable jumping briefly
      if (pmLocals.previousVelocity.z <= PM_SPEED_LAND) {
        pm->s.flags |= PMF_TIME_LAND;
        pm->s.time = 1;

        if (pmLocals.previousVelocity.z <= PM_SPEED_FALL) {
          pm->s.time = 16;

          if (pmLocals.previousVelocity.z <= PM_SPEED_FALL_FAR) {
            pm->s.time = 256;
          }
        }
      } else { // soft landings with upward momentum grant trick jumps
        if (trickJump) {
          pm->s.flags |= PMF_TIME_TRICK_JUMP;
          pm->s.time = 32;
        }
      }
    }

    // save a reference to the ground
    pm->s.flags |= PMF_ON_GROUND;
    pm->ground = trace;

    // and sink down to it if not trick jumping
    if (!(pm->s.flags & PMF_TIME_TRICK_JUMP)) {
      pm->s.origin = trace.end;
    }
  } else {
    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));
  }

  // always touch the entity, even if we couldn't stand on it
  Pm_TouchEntity(&trace);
}

/**
 * @brief Checks for water interaction, accounting for player ducking, etc.
 */
static void Pm_CheckWater(void) {

  pm->waterLevel = WATER_NONE;
  pm->waterType = 0;

  Vec3 pos = pm->s.origin;
  pos.z = pm->s.origin.z + pm->bounds.mins.z + PM_GROUND_DIST;

  int32_t contents = pm->PointContents(pos);
  if (contents & CONTENTS_MASK_LIQUID) {

    pm->waterType = contents;
    pm->waterLevel = WATER_FEET;

    pos.z = pm->s.origin.z;

    contents = pm->PointContents(pos);

    if (contents & CONTENTS_MASK_LIQUID) {

      pm->waterType |= contents;
      pm->waterLevel = WATER_WAIST;

      pos.z = pm->s.origin.z + pm->s.viewOffset.z + 1.f;

      contents = pm->PointContents(pos);

      if (contents & CONTENTS_MASK_LIQUID) {
        pm->waterType |= contents;
        pm->waterLevel = WATER_UNDER;

        pm->s.flags |= PMF_UNDER_WATER;
      }
    }
  }
}

/**
 * @brief Handles ducking, adjusting both the player's bounding box and view
 * offset accordingly. Players must be on the ground in order to duck.
 */
static void Pm_CheckDuck(void) {

  if (pm->s.type == PM_DEAD) {
    if (pm->s.flags & PMF_GIBLET) {
      pm->s.viewOffset.z = 0.f;
    } else {
      pm->s.viewOffset.z = -16.f;
    }
  } else {

    const bool isDucking = pm->s.flags & PMF_DUCKED;
    const bool wantsDucking = (pm->cmd.up < 0) && !(pm->s.flags & PMF_ON_LADDER);

    if (!isDucking && wantsDucking) {
      pm->s.flags |= PMF_DUCKED;
    } else if (isDucking && !wantsDucking) {
      const CmTrace trace = Pm_Trace(pm->s.origin, pm->s.origin, pm->bounds);

      if (!trace.allSolid && !trace.startSolid) {
        pm->s.flags &= ~PMF_DUCKED;
      }
    }

    const float height = Box3_Size(pm->bounds).z;
    const float duckStandSpeed = Maxf(0.f, pm->s.params.speedDuckStand); // never reverse the transition

    if (pm->s.flags & PMF_DUCKED) { // ducked, reduce height
      const float target = pm->bounds.mins.z + height * 0.5f;

      if (pm->s.viewOffset.z > target) { // go down
        pm->s.viewOffset.z -= pmLocals.time * duckStandSpeed;
      }

      if (pm->s.viewOffset.z < target) {
        pm->s.viewOffset.z = target;
      }

      // change the bounding box to reflect ducking
      pm->bounds = Pm_Bounds(&pm->s.params, true);
    } else {
      const float target = pm->bounds.mins.z + height * 0.9f;

      if (pm->s.viewOffset.z < target) { // go up
        pm->s.viewOffset.z += pmLocals.time * duckStandSpeed;
      }

      if (pm->s.viewOffset.z > target) {
        pm->s.viewOffset.z = target;
      }
    }
  }

  pm->s.viewOffset = pm->s.viewOffset;
}

/**
 * @brief Check for jumping and trick jumping.
 *
 * @return True if a jump occurs, false otherwise.
 */
static bool Pm_CheckJump(void) {

  if (Pm_CheckHookJump()) {
    return true;
  }

  // must wait for landing damage to subside
  if (pm->s.flags & PMF_TIME_LAND) {
    return false;
  }

  // must wait for jump key to be released
  if (pm->s.flags & PMF_JUMP_HELD) {
    return false;
  }

  // didn't ask to jump
  if (pm->cmd.up < 1) {
    return false;
  }

  // finally, do the jump
  float jump = Maxf(0.f, pm->s.params.speedJump);

  // factoring in water level
  if (pm->waterLevel > WATER_FEET) {
    jump *= PM_SPEED_JUMP_MOD_WATER;
  }

  // adding the trick jump if eligible
  if (pm->s.flags & PMF_TIME_TRICK_JUMP) {
    jump += PM_SPEED_TRICK_JUMP;

    pm->s.flags &= ~PMF_TIME_TRICK_JUMP;
    pm->s.time = 0;

    Pm_Debug("Trick jump: %d\n", pm->cmd.up);
  } else {
    Pm_Debug("Jump: %d\n", pm->cmd.up);
  }

  if (pm->s.velocity.z < 0.f) {
    pm->s.velocity.z = jump;
  } else {
    pm->s.velocity.z += jump;
  }

  // indicate that jump is currently held
  pm->s.flags |= (PMF_JUMPED | PMF_JUMP_HELD);

  // clear the ground indicators
  pm->s.flags &= ~PMF_ON_GROUND;
  memset(&pm->ground, 0, sizeof(pm->ground));

  // we can trick jump soon
  pm->s.flags |= PMF_TIME_TRICK_START;
  pm->s.time = 100;

  return true;
}

/**
 * @brief Check for ladder interaction, setting `PMF_ON_LADDER` when the player is on one.
 */
static void Pm_CheckLadder(void) {

  if (pm->s.flags & PMF_TIME_MASK) {
    return;
  }

  if (pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) {
    return;
  }

  const Vec3 pos = Vec3_Fmaf(pm->s.origin, 4.f, pmLocals.forwardXy);
  const CmTrace trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

  if (trace.contents & CONTENTS_LADDER) {
    pm->s.flags |= PMF_ON_LADDER;

    memset(&pm->ground, 0, sizeof(pm->ground));
    pm->s.flags &= ~(PMF_ON_GROUND | PMF_DUCKED);
  }
}

/**
 * @brief Checks for water exit. The player may exit the water when they can
 * see a usable step out of the water.
 *
 * @return True if a water jump has occurred, false otherwise.
 */
static bool Pm_CheckWaterJump(void) {

  if (pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) {
    return false;
  }

  if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    return false;
  }

  if (pm->waterLevel != WATER_WAIST) {
    return false;
  }

  if (pm->cmd.up < 1 && pm->cmd.forward < 1) {
    return false;
  }

  Vec3 pos = Vec3_Fmaf(pm->s.origin, 16.f, pmLocals.forward);
  CmTrace trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

  if (trace.contents & CONTENTS_MASK_SOLID) {

    pos.z += PM_STEP_HEIGHT + Box3_Size(pm->bounds).z;

    trace = Pm_Trace(pos, pos, pm->bounds);

    if (trace.startSolid) {
      Pm_Debug("Can't exit water: blocked\n");
      return false;
    }

    Vec3 pos2 = MakeVec3(pos.x, pos.y, pm->s.origin.z);

    trace = Pm_Trace(pos, pos2, pm->bounds);

    if (!(trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL)) {
      Pm_Debug("Can't exit water: not a step\n");
      return false;
    }

    // jump out of water
    pm->s.velocity.z = Maxf(0.f, pm->s.params.speedWaterJump);

    pm->s.flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
    pm->s.time = 2000;

    return true;
  }

  return false;
}

/**
 * @brief Handles player movement while climbing a ladder.
 */
static void Pm_LadderMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Friction(false);

  Pm_Currents();

  const float ladderSpeed = Maxf(0.f, pm->s.params.speedLadder);
  const float ladderAccel = Maxf(0.f, pm->s.params.accelLadder);

  // user intentions in X/Y
  Vec3 vel = Vec3_Zero();
  vel = Vec3_Fmaf(vel, pm->cmd.forward, pmLocals.forwardXy);
  vel = Vec3_Fmaf(vel, pm->cmd.right, pmLocals.rightXy);

  const float s = ladderSpeed * 0.125f;

  // limit horizontal speed when on a ladder
  vel.x = Clampf(vel.x, -s, s);
  vel.y = Clampf(vel.y, -s, s);
  vel.z = 0.f;

  // handle Z intentions differently
  if (fabsf(pm->s.velocity.z) < ladderSpeed) {

    if ((pm->angles.x <= -15.f) && (pm->cmd.forward > 0)) {
      vel.z = ladderSpeed;
    } else if ((pm->angles.x >= 15.f) && (pm->cmd.forward > 0)) {
      vel.z = -ladderSpeed;
    } else if (pm->cmd.up > 0) {
      vel.z = ladderSpeed;
    } else if (pm->cmd.up < 0) {
      vel.z = -ladderSpeed;
    } else {
      vel.z = 0.f;
    }
  }

  if (pm->cmd.up > 0) { // avoid jumps when exiting ladders
    pm->s.flags |= PMF_JUMP_HELD;
  }

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(vel, &speed);
  speed = Clampf(speed, 0.f, ladderSpeed);

  if (speed < PM_STOP_EPSILON) {
    speed = 0.f;
  }

  Pm_Accelerate(dir, speed, ladderAccel);

  Pm_StepSlideMove();
}

/**
 * @brief Handles player movement during a water jump, propelling the player out of the water.
 */
static void Pm_WaterJumpMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Friction(false);

  Pm_Gravity();

  // check for a usable spot directly in front of us
  const Vec3 pos = Vec3_Fmaf(pm->s.origin, 30.f, pmLocals.forwardXy);

  // if we've reached a usable spot, clamp the jump to avoid launching
  if (Pm_Trace(pm->s.origin, pos, pm->bounds).fraction == 1.f) {
    pm->s.velocity.z = Clampf(pm->s.velocity.z, 0.f, Maxf(0.f, pm->s.params.speedJump));
  }

  // if we're falling back down, clear the timer to regain control
  if (pm->s.velocity.z <= 0.f) {
    pm->s.flags &= ~PMF_TIME_MASK;
    pm->s.time = 0;
  }

  Pm_StepSlideMove();
}

/**
 * @brief Handles player movement while submerged or wading in water.
 */
static void Pm_WaterMove(void) {

  if (Pm_CheckWaterJump()) {
    Pm_WaterJumpMove();
    return;
  }

  Pm_Debug("%s\n", vtos(pm->s.origin));

  const float waterSpeed = Maxf(1.f, pm->s.params.speedWater); // also a loop divisor below

  // apply friction, slowing rapidly when first entering the water
  float speed = Vec3_Length(pm->s.velocity);

  for (int32_t i = speed / waterSpeed; i >= 0; i--) {
    Pm_Friction(true);
  }

  // and sink
  if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up && (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO)) {
    if (pm->s.velocity.z > PM_SPEED_WATER_SINK) {
      Pm_Gravity();
    }
  }

  Pm_Currents();

  // user intentions on X/Y/Z
  Vec3 vel = Vec3_Zero();
  vel = Vec3_Fmaf(vel, pm->cmd.forward, pmLocals.forward);
  vel = Vec3_Fmaf(vel, pm->cmd.right, pmLocals.right);

  // add explicit Z
  vel.z += pm->cmd.up;

  // disable water skiing
  if (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO) {
    if (pm->waterLevel == WATER_WAIST) {
      Vec3 view = Vec3_Add(pm->s.origin, pm->s.viewOffset);
      view.z -= 4.f;

      if (!(pm->PointContents(view) & CONTENTS_MASK_LIQUID)) {
        pm->s.velocity.z = Minf(pm->s.velocity.z, 0.f);
        vel.z = Minf(vel.z, 0.f);
      }
    }
  }

  const Vec3 dir = Vec3_NormalizeLength(vel, &speed);
  speed = Clampf(speed, 0, waterSpeed);

  if (speed < PM_STOP_EPSILON) {
    speed = 0.f;
  }

  Pm_Accelerate(dir, speed, Maxf(0.f, pm->s.params.accelWater));

  if (pm->cmd.up > 0) {
    Pm_SlideMove();
  } else {
    Pm_StepSlideMove();
  }
}

/**
 * @brief Handles player movement while airborne, applying friction, gravity, and air acceleration.
 */
static void Pm_AirMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Friction(false);

  Pm_Gravity();

  Vec3 vel = Vec3_Zero();
  vel = Vec3_Fmaf(vel, pm->cmd.forward, pmLocals.forwardXy);
  vel = Vec3_Fmaf(vel, pm->cmd.right, pmLocals.rightXy);
  vel.z = 0.f;

  float maxSpeed = Maxf(1.f, pm->s.params.speedAir); // air_speed must stay positive to bound the wish-speed

  // accounting for walk modulus
  if (pm->cmd.buttons & BUTTON_WALK) {
    maxSpeed *= PM_SPEED_MOD_WALK;
  }

  float speed;
  const Vec3 dir = Vec3_NormalizeLength(vel, &speed);
  speed = Clampf(speed, 0.f, maxSpeed);

  if (speed < PM_STOP_EPSILON) {
    speed = 0.f;
  }

  float accel = Maxf(0.f, pm->s.params.accelAir);

  if (pm->s.flags & PMF_DUCKED) {
    accel *= PM_ACCEL_AIR_MOD_DUCKED;
  }

  Pm_Accelerate(dir, speed, accel);

  Pm_StepSlideMove();
}

/**
 * @brief Called for movements where player is on ground, regardless of water level.
 */
static void Pm_WalkMove(void) {

  // check for beginning of a jump
  if (Pm_CheckJump()) {
    Pm_AirMove();
    return;
  }

  Pm_Debug("%s\n", vtos(pm->s.origin));

  Pm_Friction(false);

  Pm_Currents();

  // if the player is walking on the sea floor and wishes to swim, let them

  if (pm->waterLevel == WATER_UNDER && pmLocals.forward.z > 0.f) {

    pm->s.flags &= ~PMF_ON_GROUND;
    memset(&pm->ground, 0, sizeof(pm->ground));

    Pm_WaterMove();
    return;
  }

  // project the desired movement into the X/Y plane

  Vec3 vel = Vec3_Zero();
  vel = Vec3_Fmaf(vel, pm->cmd.forward, pmLocals.forwardXy);
  vel = Vec3_Fmaf(vel, pm->cmd.right, pmLocals.rightXy);

  // clip XY velocity to ground to enable ramp jumps
  vel = Pm_ClipVelocity(vel, pmLocals.ground.plane.normal, PM_CLIP_BOUNCE);

  float maxSpeed;

  // clamp to max speed
  if (pm->waterLevel > WATER_FEET) {
    maxSpeed = pm->s.params.speedWater;
  } else if (pm->s.flags & PMF_DUCKED) {
    maxSpeed = pm->s.params.speedDucked;
  } else {
    maxSpeed = pm->s.params.speedGround;
  }

  maxSpeed = Maxf(0.f, maxSpeed); // keep the Clampf range valid

  // accounting for walk modulus
  if (pm->cmd.buttons & BUTTON_WALK) {
    maxSpeed *= PM_SPEED_MOD_WALK;
  }

  // clamp the speed to min/max speed
  float speed;
  const Vec3 dir = Vec3_NormalizeLength(vel, &speed);
  speed = Clampf(speed, 0.f, maxSpeed);

  if (speed < PM_STOP_EPSILON) {
    speed = 0.f;
  }

  // accelerate based on slickness of ground surface
  const float accel = Maxf(0.f, (pmLocals.ground.surface & SURF_SLICK)
      ? pm->s.params.accelGroundSlick : pm->s.params.accelGround);

  Pm_Accelerate(dir, speed, accel);

  // determine the speed after acceleration
  speed = Vec3_Length(pm->s.velocity);

  // and now scale by the speed to avoid slowing down on slopes
  pm->s.velocity = Vec3_Normalize(pm->s.velocity);
  pm->s.velocity = Vec3_Scale(pm->s.velocity, speed);

  // and finally, step if moving in X/Y
  if (pm->s.velocity.x || pm->s.velocity.y) {
    Pm_StepSlideMove();
  }
}

/**
 * @brief Quetoo's movement, from the checks that classify the player through
 * the move itself.
 */
void Pm_QuetooMove(void) {

  // check for ladders
  Pm_CheckLadder();

  // check for grapple hook
  Pm_CheckHook();

  // check for ducking
  Pm_CheckDuck();

  // check for water level, water type
  Pm_CheckWater();

  // check for ground
  Pm_CheckGround();

  if (pm->s.flags & PMF_TIME_TELEPORT) {
    // pause in place briefly
  } else if (pm->s.flags & PMF_TIME_WATER_JUMP) {
    Pm_WaterJumpMove();
  } else if (pm->s.flags & PMF_ON_LADDER) {
    Pm_LadderMove();
  } else if (pm->s.flags & PMF_ON_GROUND) {
    Pm_WalkMove();
  } else if (pm->waterLevel > WATER_FEET) {
    Pm_WaterMove();
  } else {
    Pm_AirMove();
  }

  // check for ground at new spot
  Pm_CheckGround();

  // check for water level, water type at new spot
  Pm_CheckWater();

  // check for offset changes for our view
  Pm_CheckViewStep();
}
