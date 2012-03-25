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
	int ground_contents;

	vec3_t previous_origin;
} pm_locals_t;

static pm_locals_t pml;

#define PM_ACCEL_GROUND			10.0
#define PM_ACCEL_NO_GROUND		1.2
#define PM_ACCEL_SPECTATOR		4.5
#define PM_ACCEL_WATER			4.0

#define PM_FRICT_GROUND			10.0
#define PM_FRICT_GROUND_SLICK	2.0
#define PM_FRICT_LADDER			12.0
#define PM_FRICT_NO_GROUND		0.4
#define PM_FRICT_SPECTATOR		3.0
#define PM_FRICT_SPEED_CLAMP	0.5
#define PM_FRICT_WATER			1.5

#define PM_SPEED_CURRENT		100.0
#define PM_SPEED_DUCK_STAND		225.0
#define PM_SPEED_DUCKED			150.0
#define PM_SPEED_FALL			600.0
#define PM_SPEED_JUMP			275.0
#define PM_SPEED_JUMP_WATER		375.0
#define PM_SPEED_LADDER			240.0
#define PM_SPEED_LAND			300.0
#define PM_SPEED_MAX			450.0
#define PM_SPEED_RUN			300.0
#define PM_SPEED_SPECTATOR		350.0
#define PM_SPEED_STAIRS			30.0
#define PM_SPEED_STOP			40.0
#define PM_SPEED_WATER			175.0
#define PM_SPEED_WATER_SINK		30.0
#define PM_SPEED_WATER_BREACH	110.0

#define CLIP_BOUNCE 1.05
#define CLIP_BOUNCE_WATER 0.95

#define STOP_EPSILON 0.1

/**
 * Pm_ClipVelocity
 *
 * Slide off of the impacted plane.
 */
static void Pm_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out) {
	float bounce, backoff, change;
	int i;

	bounce = pm->water_level > 1 ? CLIP_BOUNCE_WATER : CLIP_BOUNCE;
	backoff = DotProduct(in, normal) * bounce;

	if (backoff < 0.0)
		backoff *= bounce;
	else
		backoff /= bounce;

	for (i = 0; i < 3; i++) {

		change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] < STOP_EPSILON && out[i] > -STOP_EPSILON)
			out[i] = 0.0;
	}
}

/**
 * Pm_ClampVelocity
 *
 * Clamp horizontal velocity over PM_SPEED_MAX.
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

/**
 * Pm_StepSlideMove_
 *
 * Each intersection will try to step over the obstruction instead of
 * sliding along it.
 *
 * Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state.  Returns the number of planes intersected.
 */
static int Pm_StepSlideMove_(void) {
	vec3_t vel, end;
	c_trace_t trace;
	int k, num_planes;
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
		Pm_ClipVelocity(pml.velocity, trace.plane.normal, pml.velocity);

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
 * Pm_StepSlideMove
 */
static void Pm_StepSlideMove(void) {
	vec3_t org, vel, clipped_org, clipped_vel;
	vec3_t up, down;
	c_trace_t trace;

	// save our initial position and velocity to step from
	VectorCopy(pml.origin, org);
	VectorCopy(pml.velocity, vel);

	if (!Pm_StepSlideMove_()) { // nothing blocked us, try to step down

		if ((pm->s.pm_flags & PMF_ON_GROUND) && pm->cmd.up < 1) {

			VectorCopy(pml.origin, down);
			down[2] -= (PM_STAIR_HEIGHT + STOP_EPSILON);

			trace = pm->Trace(pml.origin, pm->mins, pm->maxs, down);

			if (trace.fraction > STOP_EPSILON && trace.fraction < 1.0) {
				VectorCopy(trace.end, pml.origin);

				if (clipped_org[2] - pml.origin[2] >= 4.0) // we are in fact on stairs
					pm->s.pm_flags |= PMF_ON_STAIRS;
			}
		}

		return;
	}

	if (!(pm->s.pm_flags & PMF_ON_GROUND))
		return;

	Com_Debug("%d step up\n", quake2world.time);

	// something blocked us, try to step up

	// save the clipped results in case stepping fails
	VectorCopy(pml.origin, clipped_org);
	VectorCopy(pml.velocity, clipped_vel);

	// see if the upward position is available
	VectorCopy(org, up);
	up[2] += PM_STAIR_HEIGHT;

	trace = pm->Trace(org, pm->mins, pm->maxs, up);

	if (trace.fraction == 0.0) // it's not
		return;

	// an upward position is available, try to step from there
	VectorCopy(trace.end, pml.origin);

	VectorCopy(vel, pml.velocity);
	pml.velocity[2] += PM_SPEED_STAIRS;

	Pm_StepSlideMove_(); // step up

	VectorCopy(pml.origin, down);
	down[2] = clipped_org[2];

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, down);
	VectorCopy(trace.end, pml.origin);

	// ensure that what we stepped upon was actually step-able
	if (trace.plane.normal[2] < PM_STAIR_NORMAL) {
		VectorCopy(clipped_org, pml.origin);
		VectorCopy(clipped_vel, pml.velocity);
	} else { // the step was successful
		if (pml.origin[2] - clipped_org[2] >= 4.0) // we are in fact on stairs
			pm->s.pm_flags |= PMF_ON_STAIRS;

		if (pml.velocity[2] < 0.0) // clamp to positive velocity
			pml.velocity[2] = 0.0;

		if (pml.velocity[2] > clipped_vel[2]) // but don't launch
			pml.velocity[2] = clipped_vel[2];
	}
}

/**
 * Pm_Friction
 *
 * Handles friction against user intentions, and based on contents.
 */
static void Pm_Friction(void) {
	float scale, drop;

	const float speed = VectorLength(pml.velocity);

	if (speed < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	const float control = speed < PM_SPEED_STOP ? PM_SPEED_STOP : speed;

	drop = 0.0;

	if (pm->s.pm_type == PM_SPECTATOR) { // spectator friction
		drop = PM_FRICT_SPECTATOR;
	} else { // ladder, water, ground, and air friction

		if (pm->water_level) {
			drop += PM_FRICT_WATER * pm->water_level;
		}

		if (pm->s.pm_flags & PMF_ON_LADDER) {
			drop += PM_FRICT_LADDER;
		}

		if (pm->ground_entity) {
			if (pml.ground_surface && (pml.ground_surface->flags & SURF_SLICK)) {
				drop += PM_FRICT_GROUND_SLICK;
			} else {
				drop += PM_FRICT_GROUND;
			}
		} else if (!pm->water_level) {
			drop += PM_FRICT_NO_GROUND;
		}
	}

	drop *= control * pml.time;

	// scale the velocity
	scale = speed - drop;

	if (scale < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	scale /= speed;

	pml.velocity[0] *= scale;
	pml.velocity[1] *= scale;

	if (pm->water_level > 1 || pm->s.pm_type == PM_SPECTATOR) {
		pml.velocity[2] *= scale;
	}
}

/**
 * Pm_Accelerate
 *
 * Handles user intended acceleration.
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
 * Pm_AddCurrents
 */
static void Pm_AddCurrents(vec3_t vel) {
	vec3_t v;
	float s;

	// account for ladders
	if ((pm->s.pm_flags & PMF_ON_LADDER) && fabsf(pml.velocity[2]) <= PM_SPEED_LADDER) {
		if ((pm->angles[PITCH] <= -15) && (pm->cmd.forward > 0))
			vel[2] = PM_SPEED_LADDER;
		else if ((pm->angles[PITCH] >= 15) && (pm->cmd.forward > 0))
			vel[2] = -PM_SPEED_LADDER;
		else if (pm->cmd.up > 0)
			vel[2] = PM_SPEED_LADDER;
		else if (pm->cmd.up < 0)
			vel[2] = -PM_SPEED_LADDER;
		else
			vel[2] = 0.0;

		s = PM_SPEED_LADDER / 8.0;

		// limit horizontal speed when on a ladder
		if (vel[0] < -s)
			vel[0] = -s;
		else if (vel[0] > s)
			vel[0] = s;

		if (vel[1] < -s)
			vel[1] = -s;
		else if (vel[1] > s)
			vel[1] = s;
	}

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

/**
 * Pm_CategorizePosition
 *
 * Determine state for the current position. This involves resolving the
 * ground entity and water level.
 */
static void Pm_CategorizePosition(void) {
	vec3_t point;
	int contents;
	c_trace_t trace;

	// see if we're standing on something solid
	VectorCopy(pml.origin, point);

	// if the client wishes to double-jump, seek ground eagerly
	if (pml.velocity[2] >= 0.0 && pm->cmd.up > 0 && !(pm->s.pm_flags & PMF_JUMP_HELD)) {
		point[2] -= 2.0;
	} else { // otherwise, seek ground just beneath next origin
		point[2] += pml.velocity[2] * pml.time - 2.0;
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
			if (pml.velocity[2] <= -PM_SPEED_LAND || pm->s.pm_flags & PMF_PUSHED) {
				pm->s.pm_flags |= PMF_TIME_LAND;
				pm->s.pm_time = 16;

				if (pml.velocity[2] <= -PM_SPEED_FALL) {
					pm->s.pm_time = 64;
				}
			}

			// we're done being pushed
			pm->s.pm_flags &= ~PMF_PUSHED;
			//Com_Debug("%d landed\n", quake2world.time);
		} else {
			// remaining on the ground without intention to jump kills Z velocity
			if (pm->cmd.up < 1) {
				pml.velocity[2] = 0.0;
			}
		}

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
 * Pm_CheckDuck
 */
static void Pm_CheckDuck(void) {
	float height;
	c_trace_t trace;

	height = pm->maxs[2] - pm->mins[2];

	if (pm->s.pm_type == PM_DEAD) {
		pm->s.pm_flags |= PMF_DUCKED;
	} else if (pm->cmd.up < 0 && (pm->s.pm_flags & PMF_ON_GROUND)) { // duck
		pm->s.pm_flags |= PMF_DUCKED;
	} else { // stand up if possible
		if (pm->s.pm_flags & PMF_DUCKED) { // try to stand up
			trace = pm->Trace(pml.origin, pm->mins, pm->maxs, pml.origin);
			if (!trace.all_solid)
				pm->s.pm_flags &= ~PMF_DUCKED;
		}
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
 * Pm_CheckJump
 */
static bool Pm_CheckJump(void) {
	float jump;

	if (pm->s.pm_type == PM_DEAD)
		return false;

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

	if (pm->water_level > 1) {
		jump *= 0.75;
		if (pm->water_level > 2) {
			jump *= 0.75;
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
 * Pm_CheckPush
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
 * Pm_CheckSpecialMovement
 */
static void Pm_CheckSpecialMovement(void) {
	vec3_t forward_flat, spot;
	c_trace_t trace;

	if (pm->s.pm_time)
		return;

	pm->s.pm_flags &= ~PMF_ON_LADDER;

	// check for ladder
	VectorCopy(pml.forward, forward_flat);
	forward_flat[2] = 0.0;

	VectorNormalize(forward_flat);

	// TODO: project further maybe? q2dm3 ladder?
	VectorMA(pml.origin, 1.0, forward_flat, spot);
	spot[2] += pml.view_offset[2];

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, spot);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER)) {
		pm->s.pm_flags |= PMF_ON_LADDER;
	}

	// check for water jump
	if (pm->water_level != 2 || (pm->cmd.up < 1 && pm->cmd.forward < 1))
		return;

	VectorMA(pml.origin, 30.0, forward_flat, spot);

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, spot);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_SOLID)) {

		spot[2] += pml.view_offset[2];

		if (pm->PointContents(spot))
			return;

		// jump out of water
		VectorScale(forward_flat, PM_SPEED_RUN, pml.velocity);
		pml.velocity[2] = PM_SPEED_JUMP_WATER;

		pm->s.pm_flags |= PMF_TIME_WATERJUMP;
		pm->s.pm_time = 255;

		Com_Debug("%d water jump\n", quake2world.time);
	}
}

/*
 * Pm_WaterJumpMove
 */
static void Pm_WaterJumpMove(void) {

	pml.velocity[2] -= pm->s.gravity * pml.time;

	if (pml.velocity[2] < PM_SPEED_STAIRS) { // clear the timer
		pm->s.pm_flags &= ~PMF_TIME_MASK;
		pm->s.pm_time = 0;
	}

	Pm_StepSlideMove();
}

/*
 * Pm_LadderMove
 */
static void Pm_LadderMove(void) {
	vec3_t vel, dir;
	float speed;
	int i;

	// user intentions in X/Y/Z
	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right + pml.up[i]
				* pm->cmd.up;
	}

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	if (speed > PM_SPEED_LADDER)
		speed = PM_SPEED_LADDER;

	Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

	// FIXME: This can't possibly be right
	if (!vel[2]) { // we're not being pushed down by a current
		if (pml.velocity[2] > 0.0) {
			pml.velocity[2] -= pm->s.gravity * pml.time;
			if (pml.velocity[2] < 0.0)
				pml.velocity[2] = 0.0;
		} else {
			pml.velocity[2] += pm->s.gravity * pml.time;
			if (pml.velocity[2] > 0.0)
				pml.velocity[2] = 0.0;
		}
	}

	Pm_StepSlideMove();
}

/*
 * Pm_WaterMove
 */
static void Pm_WaterMove(void) {
	vec3_t vel, dir;
	float speed;
	int i;

	Com_Debug("%d water\n", quake2world.time);

	// first slow down if we've hit the water at a high velocity
	VectorCopy(pml.velocity, vel);
	speed = VectorLength(vel);

	if (speed > PM_SPEED_WATER) {
		VectorScale(pml.velocity, PM_SPEED_WATER / speed, pml.velocity);
	}

	// add gravity as we break the surface
	pml.velocity[2] -= (pm->s.gravity / pm->water_level) * pml.time;

	// user intentions in X/Y
	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	// constant sinking
	vel[2] = pm->cmd.up - PM_SPEED_WATER_SINK;

	// and clamped for water skiers
	if (pm->water_level == 2 && pm->cmd.up > 0) {
		if (vel[2] > PM_SPEED_WATER_BREACH) {
			vel[2] = PM_SPEED_WATER_BREACH;
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
 * Pm_AirMove
 */
static void Pm_AirMove(void) {
	vec3_t vel, dir;
	float speed, accel;
	int i;

	Com_Debug("%d air\n", quake2world.time);

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

	// clamp to max speed
	if (speed > PM_SPEED_MAX)
		speed = PM_SPEED_MAX;

	Pm_Accelerate(dir, speed, accel);

	Pm_StepSlideMove();
}

/**
 * Pm_WalkMove
 *
 * Called for movements where player is on ground and water level <= 1.
 */
static void Pm_WalkMove(void) {
	float speed, max_speed;
	vec3_t vel, dir;
	int i;

	if (Pm_CheckJump() || Pm_CheckPush()) {
		// jumped or pushed away
		if (pm->water_level > 1) {
			Pm_WaterMove();
		} else {
			Pm_AirMove();
		}
		return;
	}

	Pm_CheckDuck();

	pml.forward[2] = 0.0;
	pml.right[2] = 0.0;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 2; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	vel[2] = 0.0;

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	max_speed = (pm->s.pm_flags & PMF_DUCKED) ? PM_SPEED_DUCKED : PM_SPEED_RUN;

	// accounting for water level
	max_speed = (pm->water_level > 1) ? max_speed / (1.25 * pm->water_level) : max_speed;

	// and accounting for speed modulus
	max_speed = (pm->cmd.buttons & BUTTON_WALK) ? max_speed * 0.66 : max_speed;

	if (speed > max_speed) // clamp it to max speed
		speed = max_speed;

	Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

	Pm_StepSlideMove();
}

/*
 * Pm_GoodPosition
 */
static bool Pm_GoodPosition(void) {
	c_trace_t trace;
	vec3_t pos;

	if (pm->s.pm_type == PM_SPECTATOR)
		return true;

	VectorScale(pm->s.origin, 0.125, pos);

	trace = pm->Trace(pos, pm->mins, pm->maxs, pos);

	return !trace.all_solid;
}

/*
 * Pm_SnapPosition
 *
 * On exit, the origin will have a value that is pre-quantized to the 0.125
 * precision of the network channel and in a valid position.
 */
static void Pm_SnapPosition(void) {
	static int jitter_bits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };
	int i, j, bits, sign[3];
	short base[3];

	// snap velocity to eights
	for (i = 0; i < 3; i++)
		pm->s.velocity[i] = (int) (pml.velocity[i] * 8.0);

	for (i = 0; i < 3; i++) { // resolve signedness

		if (pml.origin[i] >= 0)
			sign[i] = 1;
		else
			sign[i] = -1;

		pm->s.origin[i] = (int) (pml.origin[i] * 8.0);

		if (pm->s.origin[i] * 0.125 == pml.origin[i])
			sign[i] = 0;
	}

	for (i = 0; i < 3; i++)
		pm->s.view_offset[i] = (int) (pml.view_offset[i] * 8.0);

	VectorCopy(pm->s.origin, base);

	// try all combinations
	for (j = 0; j < 8; j++) {

		bits = jitter_bits[j];

		VectorCopy(base, pm->s.origin);

		for (i = 0; i < 3; i++) { // shift the origin

			if (bits & (1 << i))
				pm->s.origin[i] += sign[i];
		}

		if (Pm_GoodPosition())
			return;
	}

	// go back to the last position
	Com_Debug("Pm_SnapPosition: Failed to snap to good position.\n");
	VectorCopy(pml.previous_origin, pm->s.origin);
}

/*
 * Pm_ClampAngles
 */
static void Pm_ClampAngles(void) {
	vec3_t angles;
	int i;

	// circularly clamp the angles with deltas
	for (i = 0; i < 3; i++) {
		pm->angles[i] = SHORT2ANGLE(pm->cmd.angles[i] + pm->s.delta_angles[i]);

		if (pm->angles[i] > 360.0)
			pm->angles[i] -= 360.0;

		if (pm->angles[i] < 0)
			pm->angles[i] += 360.0;
	}

	// make sure that looking up allows you to walk in the right direction
	if (pm->angles[PITCH] > 271.0)
		pm->angles[PITCH] -= 360.0;

	// update the local angles responsible for velocity calculations
	VectorCopy(pm->angles, angles);

	// for most movements, kill pitch to keep the player moving forward
	if (pm->water_level < 3 && !(pm->s.pm_flags & PMF_ON_LADDER))
		angles[PITCH] = 0.0;

	AngleVectors(angles, pml.forward, pml.right, pml.up);
}

/*
 * Pm_SpectatorMove
 */
static void Pm_SpectatorMove() {
	vec3_t vel, dir;
	float speed;
	int i;

	VectorSet(pml.view_offset, 0.0, 0.0, 22.0);

	// user intentions on X/Y/Z
	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right + pml.up[i]
				* pm->cmd.up;
	}

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	if (speed > PM_SPEED_SPECTATOR)
		speed = PM_SPEED_SPECTATOR;

	// accelerate
	Pm_Accelerate(dir, speed, PM_ACCEL_SPECTATOR);

	// do the move
	VectorMA(pml.origin, pml.time, pml.velocity, pml.origin);
}

/*
 * Pm_Init
 */
static void Pm_Init(void) {

	VectorScale(PM_MINS, PM_SCALE, pm->mins);
	VectorScale(PM_MAXS, PM_SCALE, pm->maxs);

	VectorClear(pm->angles);

	pm->num_touch = 0;
	pm->water_type = pm->water_level = 0;

	// reset flags that we test each move
	pm->s.pm_flags &= ~(PMF_DUCKED | PMF_JUMPED | PMF_ON_STAIRS | PMF_UNDER_WATER);

	if (pm->cmd.up < 1) { // jump key released
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
	}

	// decrement the movement timer
	if (pm->s.pm_time) {
		// each pm_time is worth 8ms
		unsigned int time = pm->cmd.msec >> 3;

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
 * Pm_InitLocal
 */
static void Pm_InitLocal(void) {

	// clear all pmove local vars
	memset(&pml, 0, sizeof(pml));

	// save previous origin in case move fails entirely
	VectorCopy(pm->s.origin, pml.previous_origin);

	// convert origin and velocity to float values
	VectorScale(pm->s.origin, 0.125, pml.origin);
	VectorScale(pm->s.velocity, 0.125, pml.velocity);
	VectorScale(pm->s.view_offset, 0.125, pml.view_offset);

	pml.time = pm->cmd.msec * 0.001;
}

/**
 * Pmove
 *
 * Can be called by either the server or the client to update prediction.
 */
void Pmove(pm_move_t *pmove) {
	pm = pmove;

	Pm_Init();

	Pm_InitLocal();

	Pm_ClampAngles();

	if (pm->s.pm_type == PM_SPECTATOR) { // fly around

		Pm_Friction();

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

	// attach to a ladder or jump out of the water
	Pm_CheckSpecialMovement();

	// add friction to desired movement
	Pm_Friction();

	if (pm->s.pm_flags & PMF_TIME_TELEPORT) {
		// pause in place briefly
	} else if (pm->s.pm_flags & PMF_TIME_WATERJUMP) {
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

