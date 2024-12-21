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

#include "shared/shared.h"
#include "collision/cm_types.h"

/**
 * @brief Acceleration constants.
 */
#define PM_ACCEL_AIR			2.f
#define PM_ACCEL_AIR_MOD_DUCKED	0.125f
#define PM_ACCEL_GROUND			10.f
#define PM_ACCEL_GROUND_SLICK	4.375f
#define PM_ACCEL_LADDER			16.f
#define PM_ACCEL_SPECTATOR		2.5f
#define PM_ACCEL_WATER			3.f

/**
 * @brief Bounce constant when clipping against solids.
 */
#define PM_CLIP_BOUNCE			1.0125f

/**
 * @brief Friction constants.
 */
#define PM_FRICT_AIR			0.125f
#define PM_FRICT_GROUND			6.f
#define PM_FRICT_GROUND_SLICK	2.f
#define PM_FRICT_LADDER			5.f
#define PM_FRICT_SPECTATOR		2.5f
#define PM_FRICT_WATER			2.f

/**
 * @brief Water gravity constant.
 */
#define PM_GRAVITY_WATER		0.33f

/**
 * @brief Distances traced when seeking ground.
 */
#define PM_GROUND_DIST			1.f
#define PM_GROUND_DIST_TRICK	16.f

/**
 * @brief Speed constants; intended velocities are clipped to these.
 */
#define PM_SPEED_AIR			350.f
#define PM_SPEED_CURRENT		100.f
#define PM_SPEED_DUCK_STAND		200.f
#define PM_SPEED_DUCKED			140.f
#define PM_SPEED_FALL			-700.f
#define PM_SPEED_FALL_FAR		-900.f
#define PM_SPEED_JUMP			270.f
#define PM_SPEED_LADDER			125.f
#define PM_SPEED_LAND			-280.f
#define PM_SPEED_RUN			300.f
#define PM_SPEED_SPECTATOR		500.f
#define PM_SPEED_STOP			100.f
#define PM_SPEED_UP				0.1f
#define PM_SPEED_TRICK_JUMP		0.f
#define PM_SPEED_WATER			140.f
#define PM_SPEED_WATER_JUMP		420.f
#define PM_SPEED_WATER_SINK		-16.f
#define PM_SPEED_STEP			150.f

/**
 * @brief The walk modifier slows all user-controlled speeds.
 */
#define PM_SPEED_MOD_WALK		0.66f

/**
 * @brief Water reduces jumping ability.
 */
#define PM_SPEED_JUMP_MOD_WATER	0.66f

/**
 * @brief The vertical distance afforded in step climbing.
 */
#define PM_STEP_HEIGHT			16.f

/**
 * @brief The smallest step that will be interpolated by the client.
 */
#define PM_STEP_HEIGHT_MIN		4.f

/**
 * @brief The minimum Z plane normal component required for standing.
 */
#define PM_STEP_NORMAL			0.7f

/**
 * @brief Velocity is cleared when less than this.
 */
#define PM_STOP_EPSILON			0.1f

/**
 * @brief Valid player positions are snapped a small distance away from planes.
 */
#define PM_SNAP_DISTANCE		PM_GROUND_DIST

/**
 * @brief Player bounding box scaling.
 */
#define PM_SCALE 1.f

extern const box3_t PM_BOUNDS, PM_CROUCHED_BOUNDS;

/**
 * @brief Game-specific button hits.
 */
#define BUTTON_ATTACK		(1 << 0)
#define BUTTON_WALK			(1 << 1)
#define BUTTON_HOOK			(1 << 2)
#define BUTTON_SCORE		(1 << 3)

/**
 * @brief Game-specific flags for pm_state_t.flags.
 */
#define PMF_DUCKED				(PMF_GAME << 0) // player is ducked
#define PMF_JUMPED				(PMF_GAME << 1) // player jumped
#define PMF_JUMP_HELD			(PMF_GAME << 2) // player's jump key is down
#define PMF_ON_GROUND			(PMF_GAME << 3) // player is on ground
#define PMF_ON_LADDER			(PMF_GAME << 4) // player is on ladder
#define PMF_UNDER_WATER			(PMF_GAME << 5) // player is under water
#define PMF_TIME_PUSHED			(PMF_GAME << 6) // time before can seek ground
#define PMF_TIME_TRICK_JUMP		(PMF_GAME << 7) // time eligible for trick jump
#define PMF_TIME_WATER_JUMP		(PMF_GAME << 8) // time before control
#define PMF_TIME_LAND			(PMF_GAME << 9) // time before jump eligible
#define PMF_TIME_TELEPORT		(PMF_GAME << 10) // time frozen in place
#define PMF_GIBLET				(PMF_GAME << 11) // player is a giblet
#define PMF_HOOK_RELEASED		(PMF_GAME << 12) // player's hook key was released
#define PMF_TIME_TRICK_START	(PMF_GAME << 13) // time until we can initiate a trick jump

/**
 * @brief The mask of pm_state_t.flags affecting pm_state_t.time.
 */
#define PMF_TIME_MASK ( \
                        PMF_TIME_PUSHED | \
                        PMF_TIME_TRICK_JUMP | \
                        PMF_TIME_WATER_JUMP | \
                        PMF_TIME_LAND | \
                        PMF_TIME_TELEPORT | \
						PMF_TIME_TRICK_START \
                      )

/**
 * @brief The maximum number of entities any single player movement can impact.
 */
#define PM_MAX_TOUCHS 32

/**
 * @brief The player movement structure provides context management between the
 * game modules and the player movement code.
 */
typedef struct {
	pm_cmd_t cmd; // movement command (in)

	pm_state_t s; // movement state (in / out)

	float hook_pull_speed; // hook pull speed (in)

	cm_trace_t touched[PM_MAX_TOUCHS]; // entities touched (out)
	int32_t num_touched;

	vec3_t angles; // clamped, and including kick and delta (out)
	box3_t bounds; // bounding box size (out)

	cm_trace_t ground; // (in / out)

	int32_t water_type; // water type and level (out)
	pm_water_level_t water_level;

	float step; // traversed step height (out)

	// contents checks with the world
	int32_t (*PointContents)(const vec3_t point);
	int32_t (*BoxContents)(const box3_t box);

	// collision with the world and solid entities
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const box3_t bounds);

	// print debug messages for development
	debug_t (*DebugMask)(void);
	void (*Debug)(const debug_t debug, const char *func, const char *fmt, ...);
	debug_t debug_mask;
} pm_move_t;

/**
 * @brief Performs one discrete movement of the player through the world.
 */
void Pm_Move(pm_move_t *pm_move);
