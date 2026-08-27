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
 * @brief The movement kernel's working state and the steps it is built from,
 * for a module that supplies its own kernel through `pm_move_t::Move`.
 *
 * `Pm_Move` sets `pm` and `pm_locals` before it dispatches, and every step here
 * reads them rather than taking the move as a parameter, so a kernel is written
 * exactly as Quetoo's is. Nothing in this header is for callers of `Pm_Move`.
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
vec3_t Pm_ClipVelocity(const vec3_t in, const vec3_t normal, float bounce);
void Pm_ClipMove(const cm_trace_t *trace);
float Pm_SlideMove(void);
bool Pm_CheckStep(const cm_trace_t *trace);
void Pm_StepDown(const cm_trace_t *trace);
void Pm_StepSlideMove(void);
void Pm_Friction(const bool flying);
void Pm_Accelerate(const vec3_t dir, float speed, float accel);
void Pm_Gravity(void);
void Pm_Currents(void);
bool Pm_CheckTrickJump(void);
bool Pm_CheckHookJump(void);
void Pm_CheckHook(void);
void Pm_CheckGround(void);
void Pm_CheckWater(void);
void Pm_CheckDuck(void);
bool Pm_CheckJump(void);
void Pm_CheckLadder(void);
bool Pm_CheckWaterJump(void);
void Pm_LadderMove(void);
void Pm_WaterJumpMove(void);
void Pm_WaterMove(void);
void Pm_AirMove(void);
void Pm_WalkMove(void);
void Pm_SpectatorMove(void);
void Pm_FreezeMove(void);
void Pm_CheckViewStep(void);
