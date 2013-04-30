/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "pmove.h"

static pm_move_t *pm;

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
	vec3_t origin; // full float precision
	vec3_t velocity; // full float precision
	vec3_t view_offset; // full float precision

	vec3_t forward, right, up;
	float time; // in seconds

	c_bsp_surface_t *ground_surface;
	c_bsp_plane_t ground_plane;
	int32_t ground_contents;

	int16_t previous_origin[3];
} pm_locals_t;

static pm_locals_t pml;

#define PM_ACCEL_GROUND			10.0
#define PM_ACCEL_NO_GROUND		1.33
#define PM_ACCEL_SPECTATOR		4.5
#define PM_ACCEL_WATER			4.0

#define PM_FRICT_GROUND			8.0
#define PM_FRICT_GROUND_SLICK	2.0
#define PM_FRICT_LADDER			12.0
#define PM_FRICT_NO_GROUND		0.125
#define PM_FRICT_SPECTATOR		3.0
#define PM_FRICT_SPEED_CLAMP	0.1
#define PM_FRICT_WATER			1.0

#define PM_SPEED_CURRENT		100.0
#define PM_SPEED_DOUBLE_JUMP	75.0
#define PM_SPEED_DUCK_STAND		225.0
#define PM_SPEED_DUCKED			150.0
#define PM_SPEED_FALL			450.0
#define PM_SPEED_FALL_FAR		600.0
#define PM_SPEED_JUMP			265.0
#define PM_SPEED_LADDER			125.0
#define PM_SPEED_LAND			300.0
#define PM_SPEED_MAX			450.0
#define PM_SPEED_RUN			300.0
#define PM_SPEED_SPECTATOR		350.0
#define PM_SPEED_STAIRS			30.0
#define PM_SPEED_STOP			100.0
#define PM_SPEED_WATER			125.0
#define PM_SPEED_WATER_JUMP		450.0
#define PM_SPEED_WATER_SINK		50.0

#define PM_GRAVITY_WATER		0.55

#define PM_CLIP_WALL			1.005
#define PM_CLIP_FLOOR			1.01

#define PM_STOP_EPSILON			0.1

/*
 * @brief Slide off of the impacted plane.
 */
static void Pm_ClipVelocity(vec3_t in, const vec3_t normal, vec3_t out, vec_t bounce) {
	float backoff, change;
	int32_t i;

	backoff = DotProduct(in, normal);

	if (backoff < 0.0)
		backoff *= bounce;
	else
		backoff /= bounce;

	for (i = 0; i < 3; i++) {

		change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] < PM_STOP_EPSILON && out[i] > -PM_STOP_EPSILON)
			out[i] = 0.0;
	}
}

/*
 * @brief Clamp horizontal velocity over PM_SPEED_MAX.
 */
static void Pm_ClampVelocity(void) {
	vec3_t tmp;
	float speed, scale;

	if (pm->s.pm_flags & PMF_PUSHED) // don't clamp jump pad movement
		return;

	VectorCopy(pml.velocity, tmp);
	tmp[2] = 0.0;

	speed = VectorLength(tmp);

	if (speed <= PM_SPEED_MAX) // or slower movement
		return;

	scale = (speed - (speed * pml.time * PM_FRICT_SPEED_CLAMP)) / speed;

	pml.velocity[0] *= scale;
	pml.velocity[1] *= scale;
}

#define MAX_CLIP_PLANES	4

/*
 * @brief Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state. Returns the number of planes intersected.
 */
static int32_t Pm_StepSlideMove_(void) {
	vec3_t vel, end;
	c_trace_t trace;
	int32_t k, num_planes;
	float time;

	VectorCopy(pml.velocity, vel);

	num_planes = 0;
	time = pml.time;

	for (k = 0; k < MAX_CLIP_PLANES; k++) {

		if (time <= 0.0) // out of time
			break;

		// project desired destination
		VectorMA(pml.origin, time, pml.velocity, end);

		// trace to it
		trace = pm->Trace(pml.origin, pm->mins, pm->maxs, end);

		// store a reference to the entity for firing game events
		if (pm->num_touch < MAX_TOUCH_ENTS && trace.ent) {
			pm->touch_ents[pm->num_touch] = trace.ent;
			pm->num_touch++;
		}

		if (trace.all_solid) { // player is trapped in a solid
			VectorClear(pml.velocity);
			break;
		}

		// update the origin
		VectorCopy(trace.end, pml.origin);

		if (trace.fraction == 1.0) // moved the entire distance
			break;

		// now slide along the plane
		Pm_ClipVelocity(pml.velocity, trace.plane.normal, pml.velocity, PM_CLIP_WALL);

		// update the movement time remaining
		time -= time * trace.fraction;

		// and increase the number of planes intersected
		num_planes++;

		// if we've been deflected backwards, settle to prevent oscillations
		if (DotProduct(pml.velocity, vel) <= 0.0) {
			VectorClear(pml.velocity);
			break;
		}
	}

	return num_planes;
}

/*
 * @brief
 */
static void Pm_StepSlideMove(void) {
	vec3_t org, vel, clipped_org;
	vec3_t up, down;
	c_trace_t trace;

	// save our initial position and velocity to step from
	VectorCopy(pml.origin, org);
	VectorCopy(pml.velocity, vel);

	// if nothing blocked us, try to step down to remain on ground
	if (!Pm_StepSlideMove_()) {

		if ((pm->s.pm_flags & PMF_ON_GROUND) && pm->cmd.up < 1) {

			VectorCopy(pml.origin, down);
			down[2] -= (PM_STAIR_HEIGHT + PM_STOP_EPSILON);

			trace = pm->Trace(pml.origin, pm->mins, pm->maxs, down);

			if (trace.fraction > PM_STOP_EPSILON && trace.fraction < 1.0) {
				VectorCopy(trace.end, pml.origin);

				if (org[2] - pml.origin[2] >= 4.0) { // we are in fact on stairs
					pm->s.pm_flags |= PMF_ON_STAIRS;
				}
			}
		}

		return;
	}

	// something blocked us, try to step over it

	// in order to step up, we must be on the ground or jumping upward
	if (!(pm->s.pm_flags & PMF_ON_GROUND) && pml.velocity[2] < -PM_SPEED_STAIRS)
		return;

	if (pm->s.pm_flags & PMF_ON_LADDER) // don't bother stepping up on ladders
		return;

	//Com_Debug("%d step up\n", quake2world.time);

	// save the clipped results in case stepping fails
	VectorCopy(pml.origin, clipped_org);

	// see if the upward position is available
	VectorCopy(org, up);
	up[2] += PM_STAIR_HEIGHT;

	// reaching even higher if trying to climb out of the water
	if (pm->s.pm_flags & PMF_TIME_WATER_JUMP) {
		up[2] += PM_STAIR_HEIGHT;
	}

	trace = pm->Trace(org, pm->mins, pm->maxs, up);

	if (trace.all_solid) { // it's not
		return;
	}

	// an upward position is available, try to step from there
	VectorCopy(trace.end, pml.origin);
	VectorCopy(vel, pml.velocity);

	Pm_StepSlideMove_(); // step up

	VectorCopy(pml.origin, down);
	down[2] = clipped_org[2];

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, down);

	if (!trace.all_solid) {
		VectorCopy(trace.end, pml.origin);
	}

	if (trace.fraction < 1.0 && pm->cmd.up < 1) { // clip to the new floor
		Pm_ClipVelocity(pml.velocity, trace.plane.normal, pml.velocity, PM_CLIP_FLOOR);

		if (pml.velocity[2] < vel[2] - PM_SPEED_STAIRS) { // but don't slow down on Z
			pml.velocity[2] = vel[2] - PM_SPEED_STAIRS;
		}
	}

	if (pml.origin[2] - clipped_org[2] >= 4.0) { // we are in fact on stairs
		pm->s.pm_flags |= PMF_ON_STAIRS;
	}
}

/*
 * @brief Handles friction against user intentions, and based on contents.
 */
static void Pm_Friction(void) {
	float scale, friction;

	const float speed = VectorLength(pml.velocity);

	if (speed < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	const float control = speed < PM_SPEED_STOP ? PM_SPEED_STOP : speed;

	friction = 0.0;

	if (pm->s.pm_type == PM_SPECTATOR) { // spectator friction
		friction = PM_FRICT_SPECTATOR;
	} else { // ladder, ground, water, air and friction
		if (pm->s.pm_flags & PMF_ON_LADDER) {
			friction = PM_FRICT_LADDER;
		} else {
			if (pm->ground_entity) {
				if (pml.ground_surface && (pml.ground_surface->flags & SURF_SLICK)) {
					friction = PM_FRICT_GROUND_SLICK;
				} else {
					friction = PM_FRICT_GROUND;
				}
			}

			if (pm->water_level) {
				friction += PM_FRICT_WATER * pm->water_level;
			}

			if (!friction) {
				friction = PM_FRICT_NO_GROUND;
			}
		}
	}

	friction *= control * pml.time;

	// scale the velocity
	scale = speed - friction;

	if (scale < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	scale /= speed;

	VectorScale(pml.velocity, scale, pml.velocity);
}

/*
 * @brief Handles user intended acceleration.
 */
static void Pm_Accelerate(vec3_t dir, float speed, float accel) {
	float add_speed, accel_speed, current_speed;

	current_speed = DotProduct(pml.velocity, dir);
	add_speed = speed - current_speed;

	if (add_speed <= 0.0)
		return;

	accel_speed = accel * pml.time * speed;

	if (accel_speed > add_speed)
		accel_speed = add_speed;

	VectorMA(pml.velocity, accel_speed, dir, pml.velocity);
}

/*
 * @brief
 */
static void Pm_AddCurrents(vec3_t vel) {
	vec3_t v;
	float s;

	// add water currents
	if (pm->water_type & MASK_CURRENT) {
		VectorClear(v);

		if (pm->water_type & CONTENTS_CURRENT_0)
			v[0] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_90)
			v[1] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_180)
			v[0] -= 1.0;
		if (pm->water_type & CONTENTS_CURRENT_270)
			v[1] -= 1.0;
		if (pm->water_type & CONTENTS_CURRENT_UP)
			v[2] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_DOWN)
			v[2] -= 1.0;

		s = PM_SPEED_RUN;
		if ((pm->water_level == 1) && pm->ground_entity)
			s = PM_SPEED_WATER;

		VectorMA(vel, s, v, vel);
	}

	// add conveyor belt velocities
	if (pm->ground_entity) {
		VectorClear(v);

		if (pml.ground_contents & CONTENTS_CURRENT_0)
			v[0] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_90)
			v[1] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_180)
			v[0] -= 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_270)
			v[1] -= 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_UP)
			v[2] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1.0;

		VectorMA(vel, PM_SPEED_CURRENT, v, vel);
	}
}

/*
 * @brief Determine state for the current position. This involves resolving the
 * ground entity, water level, and water type.
 */
static void Pm_CategorizePosition(void) {
	vec3_t point;
	int32_t contents;
	c_trace_t trace;

	// if the client wishes to trick-jump, seek ground eagerly
	if (!pm->ground_entity && (pml.velocity[2] > 0.0 && pm->cmd.up > 0) && !(pm->s.pm_flags
			& PMF_JUMP_HELD)) {
		VectorCopy(pml.origin, point);
		point[2] -= PM_STAIR_HEIGHT * 0.5;
	} else { // otherwise, seek the ground beneath the next origin
		VectorMA(pml.origin, pml.time, pml.velocity, point);
		if (pm->ground_entity) { // try to stay on the ground rather than float away
			point[2] -= PM_STAIR_HEIGHT * 0.25;
		} else {
			point[2] -= PM_STOP_EPSILON;
		}
	}

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, point);

	pml.ground_plane = trace.plane;
	pml.ground_surface = trace.surface;
	pml.ground_contents = trace.contents;

	// if we did not hit anything, or we hit an upward pusher, or we hit a
	// world surface that is too steep to stand on, then we have no ground
	if (!trace.ent || trace.plane.normal[2] < PM_STAIR_NORMAL) {
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->ground_entity = NULL;
	} else {
		if (!pm->ground_entity) {
			// landing hard disables jumping briefly
			if (pml.velocity[2] <= -PM_SPEED_LAND) {
				pm->s.pm_flags |= PMF_TIME_LAND;
				pm->s.pm_time = 1;

				if (pml.velocity[2] <= -PM_SPEED_FALL) {
					pm->s.pm_time = 16;

					if (pml.velocity[2] <= -PM_SPEED_FALL_FAR) {
						pm->s.pm_time = 64;
					}
				}
			} else if (pml.velocity[2] > 0.0) {
				pm->s.pm_flags |= PMF_TIME_DOUBLE_JUMP;
				pm->s.pm_time = 8;
			}

			// we're done being pushed
			pm->s.pm_flags &= ~PMF_PUSHED;
			//Com_Debug("%d landed\n", quake2world.time);
		}

		VectorCopy(trace.end, pml.origin);

		pm->s.pm_flags |= PMF_ON_GROUND;
		pm->ground_entity = trace.ent;
	}

	// save a reference to the entity touched for game events
	if (pm->num_touch < MAX_TOUCH_ENTS && trace.ent) {
		pm->touch_ents[pm->num_touch] = trace.ent;
		pm->num_touch++;
	}

	// get water_level, accounting for ducking
	pm->water_level = pm->water_type = 0;

	point[2] = pml.origin[2] + pm->mins[2] + 1.0;
	contents = pm->PointContents(point);

	if (contents & MASK_WATER) {

		pm->water_type = contents;
		pm->water_level = 1;

		point[2] = pml.origin[2];

		contents = pm->PointContents(point);

		if (contents & MASK_WATER) {

			pm->water_level = 2;

			point[2] = pml.origin[2] + pml.view_offset[2] + 1.0;

			contents = pm->PointContents(point);

			if (contents & MASK_WATER) {
				pm->water_level = 3;
				pm->s.pm_flags |= PMF_UNDER_WATER;
			}
		}
	}
}

/*
 * @brief
 */
static void Pm_CheckDuck(void) {
	float height;
	c_trace_t trace;

	height = pm->maxs[2] - pm->mins[2];

	if (pm->s.pm_type == PM_DEAD) {
		pm->s.pm_flags |= PMF_DUCKED;
	} else if (pm->cmd.up < 0) { // duck
		pm->s.pm_flags |= PMF_DUCKED;
	} else { // stand up if possible
		trace = pm->Trace(pml.origin, pm->mins, pm->maxs, pml.origin);
		if (trace.all_solid)
			pm->s.pm_flags |= PMF_DUCKED;
	}

	if (pm->s.pm_flags & PMF_DUCKED) { // ducked, reduce height
		float target = pm->mins[2];

		if (pm->s.pm_type == PM_DEAD)
			target += height * 0.15;
		else
			target += height * 0.5;

		if (pml.view_offset[2] > target) // go down
			pml.view_offset[2] -= pml.time * PM_SPEED_DUCK_STAND;

		if (pml.view_offset[2] < target)
			pml.view_offset[2] = target;

		// change the bounding box to reflect ducking and jumping
		pm->maxs[2] = pm->maxs[2] + pm->mins[2] * 0.5;
	} else {
		const float target = pm->mins[2] + height * 0.75;

		if (pml.view_offset[2] < target) // go up
			pml.view_offset[2] += pml.time * PM_SPEED_DUCK_STAND;

		if (pml.view_offset[2] > target)
			pml.view_offset[2] = target;
	}
}

/*
 * @brief
 */
static bool Pm_CheckJump(void) {
	float jump;

	// can't jump yet
	if (pm->s.pm_flags & PMF_TIME_LAND)
		return false;

	// must wait for jump key to be released
	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return false;

	// not on ground yet
	if (!(pm->s.pm_flags & PMF_ON_GROUND))
		return false;

	// didn't ask to jump
	if (pm->cmd.up < 1)
		return false;

	//Com_Debug("%d jump %d\n", quake2world.time, pm->cmd.up);

	// finally, do the jump
	jump = PM_SPEED_JUMP;

	// adding the double jump if eligible
	if (pm->s.pm_flags & PMF_TIME_DOUBLE_JUMP) {
		jump += PM_SPEED_DOUBLE_JUMP;
		//Com_Debug("%d double_jump %3.2f\n", quake2world.time, jump);
	}

	if (pm->water_level > 1) {
		jump *= 0.66;
		if (pm->water_level > 2) {
			jump *= 0.66;
		}
	}

	if (pml.velocity[2] < 0.0) {
		pml.velocity[2] = jump;
	} else {
		pml.velocity[2] += jump;
	}

	// indicate that jump is currently held
	pm->s.pm_flags |= (PMF_JUMPED | PMF_JUMP_HELD);

	// clear the ground indicators
	pm->s.pm_flags &= ~PMF_ON_GROUND;
	pm->ground_entity = NULL;

	return true;
}

/*
 * @brief
 */
static bool Pm_CheckPush(void) {

	if (!(pm->s.pm_flags & PMF_PUSHED))
		return false;

	// clear the ground indicators
	pm->s.pm_flags &= ~PMF_ON_GROUND;
	pm->ground_entity = NULL;

	return true;
}

/*
 * @brief Check for ladder interaction.
 */
static bool Pm_CheckLadder(void) {
	vec3_t forward, spot;
	c_trace_t trace;

	if (pm->s.pm_time)
		return false;

	// check for ladder
	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);

	VectorMA(pml.origin, 1.0, forward, spot);
	spot[2] += pml.view_offset[2];

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, spot);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER)) {
		pm->s.pm_flags |= PMF_ON_LADDER;
		return true;
	}

	return false;
}

/*
 * @brief Checks for the water jump condition, where we can see a usable step out of
 * the water.
 */
static bool Pm_CheckWaterJump(void) {
	vec3_t point;
	c_trace_t trace;

	if (pm->s.pm_time || pm->water_level != 2)
		return false;

	if (pm->cmd.up < 1 && pm->cmd.forward < 1)
		return false;

	VectorAdd(pml.origin, pml.view_offset, point);
	VectorMA(point, 24.0, pml.forward, point);

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, point);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_SOLID)) {

		point[2] += PM_STAIR_HEIGHT + pm->maxs[2] - pm->mins[2];

		trace = pm->Trace(point, pm->mins, pm->maxs, point);

		if (trace.start_solid)
			return false;

		// jump out of water
		pml.velocity[2] = PM_SPEED_WATER_JUMP;

		pm->s.pm_flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
		pm->s.pm_time = 255;

		//Com_Debug("%d water jump\n", quake2world.time);
		return true;
	}

	return false;
}

/*
 * @brief
 */
static void Pm_LadderMove(void) {
	vec3_t vel, dir;
	float speed;
	int32_t i;

	//Com_Debug("%d ladder move\n", quake2world.time);

	Pm_Friction();

	// user intentions in X/Y
	for (i = 0; i < 2; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	vel[2] = 0.0;

	// handle Z intentions differently
	if (fabsf(pml.velocity[2]) < PM_SPEED_LADDER) {

		if ((pm->angles[PITCH] <= -15.0) && (pm->cmd.forward > 0)) {
			vel[2] = PM_SPEED_LADDER;
		} else if ((pm->angles[PITCH] >= 15.0) && (pm->cmd.forward > 0)) {
			vel[2] = -PM_SPEED_LADDER;
		} else if (pm->cmd.up > 0) {
			vel[2] = PM_SPEED_LADDER;
		} else if (pm->cmd.up < 0) {
			vel[2] = -PM_SPEED_LADDER;
		} else {
			vel[2] = 0.0;
		}

		const float s = PM_SPEED_LADDER * 0.125;

		// limit horizontal speed when on a ladder
		if (vel[0] < -s) {
			vel[0] = -s;
		} else if (vel[0] > s) {
			vel[0] = s;
		}

		if (vel[1] < -s) {
			vel[1] = -s;
		} else if (vel[1] > s) {
			vel[1] = s;
		}
	}

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);
	speed = Clamp(speed, 0.0, PM_SPEED_LADDER);

	Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static void Pm_WaterJumpMove(void) {
	vec3_t forward;

	//Com_Debug("%d water jump move\n", quake2world.time);

	Pm_Friction();

	// add gravity
	pml.velocity[2] -= pm->s.gravity * pml.time;

	// check for a usable spot directly in front of us
	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);
	VectorMA(pml.origin, 30.0, forward, forward);

	// if we've reached a usable spot, clamp the jump to avoid launching
	if (pm->Trace(pml.origin, pm->mins, pm->maxs, forward).fraction == 1.0) {
		pml.velocity[2] = Clamp(pml.velocity[2], 0.0, PM_SPEED_JUMP);
	}

	// if we're falling back down, clear the timer to regain control
	if (pml.velocity[2] < PM_SPEED_STAIRS) {
		pm->s.pm_flags &= ~PMF_TIME_MASK;
		pm->s.pm_time = 0;
	}

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static void Pm_WaterMove(void) {
	vec3_t vel, dir;
	float speed;
	int32_t i;

	if (Pm_CheckWaterJump()) {
		Pm_WaterJumpMove();
		return;
	}

	//Com_Debug("%d water move\n", quake2world.time);

	Pm_Friction();

	// slow down if we've hit the water at a high velocity
	VectorCopy(pml.velocity, vel);
	speed = VectorLength(vel);

	if (speed > PM_SPEED_WATER) { // use additional friction rather than a hard clamp
		Pm_Friction();
	}

	// and sink if idle
	if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
		if (pml.velocity[2] > -PM_SPEED_WATER_SINK) {
			pml.velocity[2] -= pm->s.gravity * PM_GRAVITY_WATER * pml.time;
		}
	}

	// user intentions on X/Y
	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	// handle Z independently
	vel[2] += pm->cmd.up;

	// disable water skiing
	if (pm->water_level == 2 && pml.velocity[2] >= 0.0 && vel[2] > 0.0) {
		vec3_t view;

		VectorAdd(pml.origin, pml.view_offset, view);
		view[2] -= 4.0;

		if (!(pm->PointContents(view) & CONTENTS_WATER)) {
			pml.velocity[2] = 0.0;
			vel[2] = 0.0;
		}
	}

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	if (speed > PM_SPEED_WATER)
		speed = PM_SPEED_WATER;

	Pm_Accelerate(dir, speed, PM_ACCEL_WATER);

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static void Pm_AirMove(void) {
	vec3_t vel, dir;
	float speed, accel;
	int32_t i;

	//Com_Debug("%d air move\n", quake2world.time);
	Pm_CheckDuck();
	Pm_Friction();

	// add gravity
	pml.velocity[2] -= pm->s.gravity * pml.time;

	pml.forward[2] = 0.0;
	pml.right[2] = 0.0;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 2; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	vel[2] = 0.0;

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	if (speed < PM_SPEED_RUN)
		accel = PM_ACCEL_NO_GROUND * (PM_SPEED_RUN / speed);
	else
		accel = PM_ACCEL_NO_GROUND;

	speed = Clamp(speed, 0.0, PM_SPEED_MAX);

	Pm_Accelerate(dir, speed, accel);

	Pm_StepSlideMove();
}

/*
 * @brief Called for movements where player is on ground, regardless of water level.
 */
static void Pm_WalkMove(void) {
	float speed, max_speed, accel;
	vec3_t vel, dir;
	int32_t i;

	if (Pm_CheckJump() || Pm_CheckPush()) {
		// jumped or pushed away
		if (pm->water_level > 1) {
			Pm_WaterMove();
		} else {
			Pm_AirMove();
		}
		return;
	}

	//Com_Debug("%d walk move\n", quake2world.time);

	Pm_CheckDuck();

	Pm_Friction();

	pml.forward[2] = 0.0;
	pml.right[2] = 0.0;

	Pm_ClipVelocity(pml.forward, pml.ground_plane.normal, pml.forward, PM_CLIP_FLOOR);
	Pm_ClipVelocity(pml.right, pml.ground_plane.normal, pml.right, PM_CLIP_FLOOR);

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	max_speed = (pm->s.pm_flags & PMF_DUCKED) ? PM_SPEED_DUCKED : PM_SPEED_RUN;

	// accounting for water level
	max_speed = (pm->water_level > 1) ? max_speed / (pm->water_level * 0.66) : max_speed;

	// and accounting for speed modulus
	max_speed = (pm->cmd.buttons & BUTTON_WALK) ? max_speed * 0.66 : max_speed;

	speed = Clamp(speed, 0.0, max_speed);

	// accelerate based on slickness of ground surface
	accel = (pml.ground_surface->flags & SURF_SLICK) ? PM_ACCEL_NO_GROUND : PM_ACCEL_GROUND;

	Pm_Accelerate(dir, speed, accel);

	// clip to the ground
	Pm_ClipVelocity(pml.velocity, pml.ground_plane.normal, pml.velocity, PM_CLIP_FLOOR);

	if (!pml.velocity[0] && !pml.velocity[1]) { // don't bother stepping
		return;
	}

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static bool Pm_GoodPosition(void) {
	c_trace_t trace;
	vec3_t pos;

	if (pm->s.pm_type == PM_SPECTATOR)
		return true;

	UnpackPosition(pm->s.origin, pos);

	trace = pm->Trace(pos, pm->mins, pm->maxs, pos);

	return !trace.all_solid;
}

/*
 * @brief On exit, the origin will have a value that is pre-quantized to the 0.125
 * precision of the network channel and in a valid position.
 */
static void Pm_SnapPosition(void) {
	static const int16_t jitter_bits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };
	int16_t sign[3], base[3];
	size_t i, j;

	// pack velocity for network transmission
	PackPosition(pml.velocity, pm->s.velocity);

	// snap the origin, but be prepared to try nearby locations
	for (i = 0; i < 3; i++) {

		if (pml.origin[i] >= 0.0)
			sign[i] = 1;
		else
			sign[i] = -1;

		pm->s.origin[i] = (int16_t) (pml.origin[i] * 8.0);

		if (pm->s.origin[i] * 0.125 == pml.origin[i])
			sign[i] = 0;
	}

	// pack view offset for network transmission
	PackPosition(pml.view_offset, pm->s.view_offset);

	VectorCopy(pm->s.origin, base);

	// try all combinations
	for (j = 0; j < lengthof(jitter_bits); j++) {

		const int16_t bit = jitter_bits[j];

		VectorCopy(base, pm->s.origin);

		for (i = 0; i < 3; i++) { // shift the origin

			if (bit & (1 << i))
				pm->s.origin[i] += sign[i];
		}

		if (Pm_GoodPosition())
			return;
	}

	// go back to the last position
	Com_Debug("Pm_SnapPosition: Failed to snap to good position: %s.\n", vtos(pml.origin));
	VectorCopy(pml.previous_origin, pm->s.origin);
}

/*
 * @brief
 */
static void Pm_ClampAngles(void) {
	vec3_t angles;
	int32_t i;

	// copy the command angles into the outgoing state
	VectorCopy(pm->cmd.angles, pm->s.view_angles);

	// circularly clamp the angles with kick and deltas
	for (i = 0; i < 3; i++) {

		const int16_t c = pm->cmd.angles[i];
		const int16_t k = pm->s.kick_angles[i];
		const int16_t d = pm->s.delta_angles[i];

		pm->angles[i] = UnpackAngle(c + k + d);
	}

	// clamp angles to prevent the player from looking up or down more than 90'
	ClampAngles(pm->angles);

	// update the local angles responsible for velocity calculations
	VectorCopy(pm->angles, angles);

	// for most movements, kill pitch to keep the player moving forward
	if (pm->water_level < 3 && !(pm->s.pm_flags & PMF_ON_LADDER) && pm->s.pm_type != PM_SPECTATOR)
		angles[PITCH] = 0.0;

	// finally calculate the directional vectors for this move
	AngleVectors(angles, pml.forward, pml.right, pml.up);
}

/*
 * @brief
 */
static void Pm_SpectatorMove() {
	vec3_t vel;
	float speed;
	int32_t i;

	Pm_Friction();

	// user intentions on X/Y/Z
	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right + pml.up[i]
				* pm->cmd.up;
	}

	speed = VectorNormalize(vel);
	speed = Clamp(speed, 0.0, PM_SPEED_SPECTATOR);

	// accelerate
	Pm_Accelerate(vel, speed, PM_ACCEL_SPECTATOR);

	// do the move
	VectorMA(pml.origin, pml.time, pml.velocity, pml.origin);
}

/*
 * @brief
 */
static void Pm_Init(void) {

	VectorScale(PM_MINS, PM_SCALE, pm->mins);
	VectorScale(PM_MAXS, PM_SCALE, pm->maxs);

	VectorClear(pm->angles);

	pm->num_touch = 0;
	pm->water_type = pm->water_level = 0;

	// reset flags that we test each move
	pm->s.pm_flags &= ~(PMF_DUCKED | PMF_JUMPED);
	pm->s.pm_flags &= ~(PMF_ON_GROUND | PMF_ON_STAIRS | PMF_ON_LADDER);
	pm->s.pm_flags &= ~(PMF_UNDER_WATER);

	if (pm->cmd.up < 1) { // jump key released
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
	}

	// decrement the movement timer
	if (pm->s.pm_time) {
		// each pm_time is worth 8ms
		uint32_t time = pm->cmd.msec >> 3;

		if (!time)
			time = 1;

		if (time >= pm->s.pm_time) { // clear the timer and timed flags
			pm->s.pm_flags &= ~PMF_TIME_MASK;
			pm->s.pm_time = 0;
		} else { // or just decrement the timer
			pm->s.pm_time -= time;
		}
	}
}

/*
 * @brief
 */
static void Pm_InitLocal(void) {

	// clear all pmove local vars
	memset(&pml, 0, sizeof(pml));

	// save previous origin in case move fails entirely
	VectorCopy(pm->s.origin, pml.previous_origin);

	// convert origin and velocity to float values
	UnpackPosition(pm->s.origin, pml.origin);
	UnpackPosition(pm->s.velocity, pml.velocity);
	UnpackPosition(pm->s.view_offset, pml.view_offset);

	pml.time = pm->cmd.msec * 0.001;
}

/*
 * @brief Can be called by either the server or the client to update prediction.
 */
void Pmove(pm_move_t *pmove) {
	pm = pmove;

	Pm_Init();

	Pm_InitLocal();

	if (pm->s.pm_type == PM_SPECTATOR) { // fly around without world interaction

		Pm_ClampAngles();

		Pm_SpectatorMove();

		Pm_SnapPosition();

		return;
	}

	if (pm->s.pm_type == PM_DEAD || pm->s.pm_type == PM_FREEZE) { // no control
		pm->cmd.forward = pm->cmd.right = pm->cmd.up = 0;

		if (pm->s.pm_type == PM_FREEZE) // no movement
			return;
	}

	// set ground_entity, water_type, and water_level
	Pm_CategorizePosition();

	// clamp angles based on current position
	Pm_ClampAngles();

	// set ladder interaction, valid for all other states
	Pm_CheckLadder();

	if (pm->s.pm_flags & PMF_TIME_TELEPORT) {
		// pause in place briefly
	} else if (pm->s.pm_flags & PMF_TIME_WATER_JUMP) {
		Pm_WaterJumpMove();
	} else if (pm->s.pm_flags & PMF_ON_LADDER) {
		Pm_LadderMove();
	} else if (pm->s.pm_flags & PMF_ON_GROUND) {
		Pm_WalkMove();
	} else if (pm->water_level > 1) {
		Pm_WaterMove();
	} else {
		Pm_AirMove();
	}

	// clamp horizontal velocity to reasonable bounds
	Pm_ClampVelocity();

	// set ground_entity, water_type, and water_level for final spot
	Pm_CategorizePosition();

	// ensure we're at a good point
	Pm_SnapPosition();
}

