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

#pragma once

#include "bg_pmove.h"

/**
 * @file
 * @brief The movement plumbing, and the working state it keeps, for the movement
 * kernels that `Pm_Move` dispatches to.
 *
 * `Pm_Move` initializes the move, clamps the angles, handles the frozen,
 * spectator and dead cases, and then hands everything else to the kernel named
 * by `pm_params_t.kernel`. A kernel owns the ground, water, ladder and duck
 * checks, the move itself, and `Pm_CheckViewStep` if it wants step smoothing.
 *
 * `pm` and `pm_locals` are set before the dispatch, and every step here reads
 * them rather than taking the move as a parameter, so a kernel is written the
 * way Quetoo's is.
 *
 * A kernel MUST be a pure function of `pm_move_t` and `pm_params_t`. It MUST
 * NOT read a cvar, keep state between moves, consult the clock, or use a random
 * number: the server and the client both run it, and anything else desynchronizes
 * prediction. Whatever a ruleset needs in order to vary MUST travel in
 * `pm_params_t`, which is networked per-player, or be a constant in the kernel's
 * own file.
 *
 * A kernel SHOULD be finished rather than maintained. A record is only
 * comparable to another record set under the same movement, so changing what a
 * kernel does is a new kernel, appended to `pm_movement_t`, not an edit to an
 * existing one. Fixes to this file are the exception: it is shared, and a fault
 * in the plumbing is a fault in every ruleset.
 */

#define MAX_CLIP_PLANES  6

/**
 * @brief A structure containing full floating point precision copies of all
 * movement variables. This is initialized with the player's last movement
 * at each call to `Pm_Move` (this is obviously not thread-safe).
 */
typedef struct {

  /**
   * @brief Previous (incoming) origin, in case movement fails and must be reverted.
   */
  vec3_t previous_origin;

  /**
   * @brief Previous (incoming) velocity, used for detecting landings.
   */
  vec3_t previous_velocity;

  /**
   * @brief Directional vectors based on command angles, with Z component.
   */
  vec3_t forward, right, up;

  /**
   * @brief Directional vectors without Z component, for air and ground movement.
   */
  vec3_t forward_xy, right_xy;

  /**
   * @brief The current movement command duration, in seconds.
   */
  float time;

  /**
   * @brief The player's ground interaction.
   */
  cm_trace_t ground;

  /**
   * @brief The clipping planes per slide-move.
   */
  cm_bsp_plane_t clip_planes[MAX_CLIP_PLANES];

  /**
   * @brief The number of clipping planes per slide-move.
   */
  int32_t num_clip_planes;

} pm_locals_t;

extern pm_move_t *pm;
extern pm_locals_t pm_locals;

/**
 * @brief Unlike the game and the client game, this keeps its own mask test: it is
 * called per move, from the movement loop, where the arguments it would otherwise
 * format are worth skipping. `do while` rather than a statement expression, so it
 * is standard C.
 */
#define Pm_Debug(...) \
  do { \
    if (pm->DebugMask() & pm->debug_mask) { \
      pm->Debug(pm->debug_mask, __func__, __VA_ARGS__); \
    } \
  } while (0)

void Pm_TouchEntity(const cm_trace_t *trace);
cm_trace_t Pm_Trace(const vec3_t start, const vec3_t end, const box3_t bounds);
void Pm_Friction(const bool flying);
void Pm_Accelerate(const vec3_t dir, float speed, float accel);
void Pm_Gravity(void);
void Pm_CheckViewStep(void);

/**
 * @brief The kernels, one per `pm_movement_t`. `Pm_Move` calls exactly one.
 */
void Pm_QuetooMove(void);
void Pm_QuakeMove(void);
void Pm_Quake2Move(void);
void Pm_RaceMove(void);
void Pm_Quake3Move(void);

/**
 * @brief The parameters each movement that has its own is defined by, exported
 * by the kernel that implements it.
 */
extern const pm_params_t pm_quake_params;
extern const pm_params_t pm_quake2_params;
extern const pm_params_t pm_race_params;
extern const pm_params_t pm_quake3_params;
