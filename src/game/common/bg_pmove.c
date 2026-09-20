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
 * @brief The default bounding boxes: what the movement parameters default to,
 * and what code with no parameters to hand, such as the client's model setup,
 * may use.
 */
const Box3 PM_BOUNDS = {
  .mins = { { -16.f, -16.f, -24.f } },
  .maxs = { {  16.f,  16.f,  36.f } }
};

const Box3 PM_CROUCHED_BOUNDS = {
  .mins = { { -16.f, -16.f, -24.f } },
  .maxs = { {  16.f,  16.f,  6.f } }
};

const Box3 PM_DEAD_BOUNDS = {
  .mins = { { -16.f, -16.f, -24.f } },
  .maxs = { {  16.f,  16.f,  -4.f } }
};

static const Box3 PM_GIBLET_BOUNDS = {
  .mins = { { -8.f, -8.f, -8.f } },
  .maxs = { {  8.f,  8.f,  8.f } }
};

/**
 * @see bg_pmove.h
 */
Box3 Pm_Bounds(const PMoveParams *params, bool ducked) {

  // the box arrives whole, so the parameters are the only thing that decides it.
  // A movement that wants a bigger player declares a bigger box
  return ducked ? params->boundsDucked : params->bounds;
}

/**
 * @brief Keyed by `PMovement` so that the ids and this table cannot drift
 * apart. Quetoo's carries no parameters of its own: it is the one that follows
 * the server's movement cvars, which is what makes it the default.
 */
static const PMovementInfo pmMovements[] = {
  [PM_MOVEMENT_QUETOO] = { .name = "quetoo", .label = "Quetoo",      .params = NULL, .hook = true },
  [PM_MOVEMENT_RACE]   = { .name = "race",   .label = "Quetoo Race", .params = &pmRaceParams },
  [PM_MOVEMENT_QUAKE]  = { .name = "quake",  .label = "Quake",       .params = &pmQuakeParams },
  [PM_MOVEMENT_QUAKE2] = { .name = "quake2", .label = "Quake2",      .params = &pmQuake2Params },
  [PM_MOVEMENT_QUAKE3] = { .name = "quake3", .label = "Quake3",      .params = &pmQuake3Params },
};

/**
 * @see bg_pmove.h
 */
const PMovementInfo *Pm_Movement(PMovement movement) {

  if ((size_t) movement >= lengthof(pmMovements)) {
    return NULL;
  }

  return &pmMovements[movement];
}

/**
 * @see bg_pmove.h
 */
size_t Pm_MovementCount(void) {
  return lengthof(pmMovements);
}

/**
 * @see bg_pmove.h
 */
bool Pm_MovementByName(const char *name, PMovement *movement) {

  assert(movement);

  if (!name || !*name) {
    return false;
  }

  for (size_t i = 0; i < lengthof(pmMovements); i++) {
    if (!q_strcasecmp(pmMovements[i].name, name)) {
      *movement = (PMovement) i;
      return true;
    }
  }

  return false;
}

PMove *pm;

PMoveLocals pmLocals;

/**
 * @brief Mark the specified entity as touched. This enables the game module to
 * detect player -> entity interactions.
 */
void Pm_TouchEntity(const CmTrace *trace) {

  if (trace->ent == NULL) {
    return;
  }

  if (pm->numTouched == PM_MAX_TOUCHS) {
    Pm_Debug("MAX_TOUCH_ENTS\n");
    return;
  }

  for (int32_t i = 0; i < pm->numTouched; i++) {
    if (pm->touched[i].ent == trace->ent) {
      return;
    }
  }

  pm->touched[pm->numTouched++] = *trace;
}

/**
 * Adapted from Quake III, this function adjusts a trace so that if it starts inside of a wall,
 * it is adjusted so that the trace begins outside of the solid it impacts.
 * @return The actual trace.
 */
CmTrace Pm_Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {

  const float offsets[] = { 0.f, 1.f, -1.f };

  // jitter around
  for (uint32_t i = 0; i < lengthof(offsets); i++) {
    for (uint32_t j = 0; j < lengthof(offsets); j++) {
      for (uint32_t k = 0; k < lengthof(offsets); k++) {
        const Vec3 point = Vec3_Add(start, MakeVec3(offsets[i], offsets[j], offsets[k]));
        const CmTrace trace = pm->Trace(point, end, bounds);
        
        if (!trace.allSolid) {

          if (i != 0 || j != 0 || k != 0) {
            Pm_Debug("Fixed all-solid\n");
          }

          return trace;
        }
      }
    }
  }
  
  Pm_Debug("No good position\n");
  return pm->Trace(start, end, bounds);
}

/**
 * @brief Handles friction against user intentions, and based on contents.
 * @param flying Whether we should clear Z velocity as well if we are going to stop
 */
void Pm_Friction(const bool flying) {
  Vec3 vel = pm->s.velocity;

  if (pm->s.flags & PMF_ON_GROUND) {
    vel.z = 0.f;
  }

  const float speed = Vec3_Length(vel);

  if (speed < 1.f) {
    pm->s.velocity.x = pm->s.velocity.y = 0.f;

    if (flying) {
      pm->s.velocity.z = 0.f;
    }

    return;
  }

  const float control = Maxf(pm->s.params.speedStop, speed);

  float friction = 0.f;

  if (pm->s.type == PM_SPECTATOR) { // spectator friction
    friction = pm->s.params.frictionSpectator;
  } else if (pm->s.flags & PMF_ON_LADDER) { // ladder friction
    friction = pm->s.params.frictionLadder;
  } else if (pm->waterLevel > WATER_FEET) { // water friction
    friction = pm->s.params.frictionWater;
  } else if (pm->s.flags & PMF_ON_GROUND) { // ground friction
    if (pmLocals.ground.ent && (pmLocals.ground.surface & SURF_SLICK)) {
      friction = pm->s.params.frictionGroundSlick;
    } else {
      friction = pm->s.params.frictionGround;
    }
  } else { // everything else friction
    friction = pm->s.params.frictionAir;
  }

  friction = Maxf(0.f, friction); // never reverse direction

  // scale the velocity, taking care to not reverse direction
  const float scale = Maxf(0.f, speed - (friction * control * pmLocals.time)) / speed;

  pm->s.velocity = Vec3_Scale(pm->s.velocity, scale);
}

/**
 * @brief Handles user intended acceleration.
 */
void Pm_Accelerate(const Vec3 dir, float speed, float accel) {
  const float currentSpeed = Vec3_Dot(pm->s.velocity, dir);
  const float addSpeed = speed - currentSpeed;

  if (addSpeed > 0.f) {
    float accelSpeed = accel * pmLocals.time * speed;

    if (accelSpeed > addSpeed) {
      accelSpeed = addSpeed;
    }

    pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accelSpeed, dir);
  }
}

/**
 * @brief Applies gravity to the current movement.
 */
void Pm_Gravity(void) {

  if (pm->s.type == PM_HOOK_PULL) {
    return;
  }

  pm->s.velocity.z -= pm->s.params.gravity * pmLocals.time;
}

/**
 * @brief Handles spectator movement, allowing free-fly navigation through the world.
 */
static void Pm_SpectatorMove(void) {

  Pm_Friction(true);

  // user intentions on X/Y/Z
  Vec3 vel = Vec3_Zero();
  vel = Vec3_Fmaf(vel, pm->cmd.forward, pmLocals.forward);
  vel = Vec3_Fmaf(vel, pm->cmd.right, pmLocals.right);

  // add explicit Z
  vel.z += pm->cmd.up;

  float speed;
  vel = Vec3_NormalizeLength(vel, &speed);
  speed = Clampf(speed, 0.f, Maxf(0.f, pm->s.params.speedSpectator));

  if (speed < PM_STOP_EPSILON) {
    speed = 0.f;
  }

  // accelerate
  Pm_Accelerate(vel, speed, Maxf(0.f, pm->s.params.accelSpectator));

  // do the move
  pm->s.origin = Vec3_Fmaf(pm->s.origin, pmLocals.time, pm->s.velocity);
}

/**
 * @brief Handles movement for a frozen or dead player, suppressing all movement.
 */
static void Pm_FreezeMove(void) {

  Pm_Debug("%s\n", vtos(pm->s.origin));
}

/**
 * @brief Initializes outgoing player movement state for a new move frame.
 */
static void Pm_Init(void) {

  // set the default bounding box
  if (pm->s.type == PM_DEAD) {

    if (pm->s.flags & PMF_GIBLET) {
      pm->bounds = PM_GIBLET_BOUNDS;
    } else {
      pm->bounds = pm->s.params.boundsDead;
    }
  } else {
    pm->bounds = Pm_Bounds(&pm->s.params, false);
  }

  pm->angles = Vec3_Zero();

  pm->numTouched = 0;
  pm->waterLevel = WATER_NONE;
  pm->waterType = 0;

  pm->step = 0.f;

  // reset flags that we test each move
  pm->s.flags &= ~(PMF_ON_GROUND | PMF_ON_LADDER);
  pm->s.flags &= ~(PMF_JUMPED | PMF_UNDER_WATER);

  if (pm->cmd.up < 1) { // jump key released
    pm->s.flags &= ~PMF_JUMP_HELD;
  }

  // decrement the movement timer by the duration of the command
  if (pm->s.time) {
    if (pm->cmd.msec >= pm->s.time) { // clear the timer and timed flags
      pm->s.flags &= ~PMF_TIME_MASK;
      pm->s.time = 0;
    } else { // or just decrement the timer
      pm->s.time -= pm->cmd.msec;
    }
  }
}

/**
 * @brief Copies command angles into view state and clamps pitch to prevent inversion.
 */
static void Pm_ClampAngles(void) {

  // copy the command angles into the outgoing state
  pm->s.viewAngles = pm->cmd.angles;

  // add the delta angles
  pm->angles = Vec3_Add(pm->cmd.angles, pm->s.deltaAngles);

  // clamp pitch to prevent the player from looking up or down more than 90º
  if (pm->angles.x > 90.f && pm->angles.x < 270.f) {
    pm->angles.x = 90.f;
  } else if (pm->angles.x <= 360.f && pm->angles.x >= 270.f) {
    pm->angles.x -= 360.f;
  }
}

/**
 * @brief Initializes local movement state, computing directional vectors and frame timing.
 */
static void Pm_InitLocal(void) {

  memset(&pmLocals, 0, sizeof(pmLocals));

  // save previous values in case move fails, and to detect landings
  pmLocals.previousOrigin = pm->s.origin;
  pmLocals.previousVelocity = pm->s.velocity;

  // convert from milliseconds to seconds
  pmLocals.time = pm->cmd.msec * .001f;

  // calculate the directional vectors for this move
  Vec3_Vectors(pm->angles, &pmLocals.forward, &pmLocals.right, &pmLocals.up);

  // and calculate the directional vectors in the XY plane
  Vec3_Vectors(MakeVec3(0.f, pm->angles.y, 0.f), &pmLocals.forwardXy, &pmLocals.rightXy, NULL);
}

/**
 * @brief Updates the view step offset to smoothly interpolate the camera over stair steps.
 */
void Pm_CheckViewStep(void) {

  // add the step offset we've made on this frame
  if (pm->step) {
    pm->s.stepOffset += pm->step;
  }

  // calculate change to the step offset
  if (pm->s.stepOffset) {

    const float stepSpeed = pmLocals.time * (PM_SPEED_STEP * (Maxf(1.f, fabsf(pm->s.stepOffset) / PM_STEP_HEIGHT)));

    if (pm->s.stepOffset > 0) {
      pm->s.stepOffset = Maxf(0.f, pm->s.stepOffset - stepSpeed);
    } else {
      pm->s.stepOffset = Minf(0.f, pm->s.stepOffset + stepSpeed);
    }
  }
}

/**
 * @brief Called by the game and the client game to update the player's
 * authoritative or predicted movement state, respectively.
 */
void Pm_Move(PMove *pmMove) {
  pm = pmMove;

  Pm_Init();

  Pm_ClampAngles();

  Pm_InitLocal();

  if (pm->s.type == PM_FREEZE) { // no movement
    Pm_FreezeMove();
    return;
  }

  if (pm->s.type == PM_SPECTATOR) { // no interaction
    Pm_SpectatorMove();
    return;
  }

  if (pm->s.type == PM_DEAD) { // no control
    pm->cmd.forward = pm->cmd.right = pm->cmd.up = 0;
  }

  switch (pm->s.params.movement) {
    case PM_MOVEMENT_QUETOO:
      Pm_QuetooMove();
      break;
    case PM_MOVEMENT_QUAKE:
      Pm_QuakeMove();
      break;
    case PM_MOVEMENT_QUAKE2:
      Pm_Quake2Move();
      break;
    case PM_MOVEMENT_RACE:
      Pm_RaceMove();
      break;
    case PM_MOVEMENT_QUAKE3:
      Pm_Quake3Move();
      break;
    default:
      // the value arrives over the network, so it is clamped rather than
      // trusted; both sides clamp alike, so prediction stays consistent even
      // when a client and a server disagree about what ids exist
      Pm_Debug("Unknown movement %u\n", pm->s.params.movement);
      Pm_QuetooMove();
      break;
  }
}

