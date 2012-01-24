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

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
	vec3_t origin; // full float precision
	vec3_t velocity; // full float precision

	vec3_t forward, right, up;
	float frame_time;

	c_bsp_surface_t *ground_surface;
	c_bsp_plane_t ground_plane;
	int ground_contents;

	vec3_t previous_origin;
	boolean_t ladder;
} pm_locals_t;

pm_move_t *pm;
pm_locals_t pml;

#define PM_ACCEL_GROUND			10.0
#define PM_ACCEL_NO_GROUND		2.0
#define PM_ACCEL_SPECTATOR		4.5
#define PM_ACCEL_WATER			4.0

#define PM_FRICT_GROUND			10.0
#define PM_FRICT_GROUND_SLICK	2.0
#define PM_FRICT_LADDER			12.0
#define PM_FRICT_NO_GROUND		0.1
#define PM_FRICT_SPECTATOR		3.0
#define PM_FRICT_SPEED_CLAMP	0.5
#define PM_FRICT_WATER			2.5

#define PM_SPEED_CURRENT		100.0
#define PM_SPEED_DUCKED			150.0
#define PM_SPEED_FALL			600.0
#define PM_SPEED_JUMP			275.0
#define PM_SPEED_JUMP_WATER		400.0
#define PM_SPEED_LADDER			240.0
#define PM_SPEED_MAX			450.0
#define PM_SPEED_RUN			300.0
#define PM_SPEED_SPECTATOR		350.0
#define PM_SPEED_STAIRS			20.0
#define PM_SPEED_STOP			40.0
#define PM_SPEED_WATER			200.0
#define PM_SPEED_WATER_DOWN		20.0

#define CLIP_BACKOFF 1.001
#define STOP_EPSILON 0.1

/*
 * Pm_ClipVelocity
 *
 * Slide off of the impacted plane.
 */
static void Pm_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out) {
	float backoff;
	float change;
	int i;

	backoff = DotProduct(in, normal) * CLIP_BACKOFF;

	if (backoff < 0.0)
		backoff *= CLIP_BACKOFF;
	else
		backoff /= CLIP_BACKOFF;

	for (i = 0; i < 3; i++) {

		change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] < STOP_EPSILON && out[i] > -STOP_EPSILON)
			out[i] = 0.0;
	}
}

/*
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

	if (speed < PM_SPEED_MAX) // or slower movement
		return;

	scale = (speed - (speed * pml.frame_time * PM_FRICT_SPEED_CLAMP)) / speed;

	pml.velocity[0] *= scale;
	pml.velocity[1] *= scale;
}

#define MAX_CLIP_PLANES	4

/*
 * Pm_StepSlideMove
 *
 * Each intersection will try to step over the obstruction instead of
 * sliding along it.
 *
 * Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state.  Returns the number of planes intersected.
 */
static int Pm_StepSlideMove_(void) {
	vec3_t vel, dir, end, planes[MAX_CLIP_PLANES];
	c_trace_t trace;
	int i, j, k, num_planes;
	float d, time;

	VectorCopy(pml.velocity, vel);

	num_planes = 0;
	time = pml.frame_time;

	for (k = 0; k < MAX_CLIP_PLANES; k++) {

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

		// update the movement time remaining
		time -= time * trace.fraction;

		// now slide along the plane
		VectorCopy(trace.plane.normal, planes[num_planes]);
		num_planes++;

		// clip the velocity to all impacted planes
		for (i = 0; i < num_planes; i++) {

			Pm_ClipVelocity(pml.velocity, planes[i], pml.velocity);

			for (j = 0; j < num_planes; j++) {
				if (j != i) {
					if (DotProduct(pml.velocity, planes[j]) < 0.0)
						break; // not ok
				}
			}

			if (j == num_planes)
				break;
		}

		if (i != num_planes) { // go along this plane
		} else { // go along the crease
			if (num_planes != 2) {
				VectorClear(pml.velocity);
				break;
			}
			CrossProduct(planes[0], planes[1], dir);
			d = DotProduct(dir, pml.velocity);
			VectorScale(dir, d, pml.velocity);
		}

		// if new velocity is against original velocity, clear it to avoid
		// tiny occilations in corners
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
	vec3_t org, vel;
	vec3_t neworg, newvel;
	vec3_t up, down;
	c_trace_t trace;
	int i;

	VectorCopy(pml.origin, org);
	VectorCopy(pml.velocity, vel);

	if (!Pm_StepSlideMove_()) // nothing blocked us, we're done
		return;

	// something blocked us, try to step over it with a few discrete steps

	// save the first move in case stepping fails
	VectorCopy(pml.origin, neworg);
	VectorCopy(pml.velocity, newvel);

	for (i = 0; i < 3; i++) {

		// see if the upward position is available
		VectorCopy(org, up);
		up[2] += PM_STAIR_HEIGHT;

		if (pm->Trace(up, pm->mins, pm->maxs, up).all_solid)
			break; // it's not

		// the upward position is available, try to step from there
		VectorCopy(up, pml.origin);
		VectorCopy(vel, pml.velocity);

		pml.velocity[2] += PM_SPEED_STAIRS;

		if (!Pm_StepSlideMove_()) // nothing blocked us, we're done
			break;
	}

	// settle back down atop the step
	VectorCopy(pml.origin, down);
	down[2] = org[2];

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, down);
	VectorCopy(trace.end, pml.origin);

	// ensure that what we stepped upon was actually step-able
	if (trace.plane.normal[2] < PM_STAIR_NORMAL) {
		VectorCopy(neworg, pml.origin);
		VectorCopy(newvel, pml.velocity);
	} else { // the step was successful

		if (pml.origin[2] - neworg[2] > 4.0) // we are in fact on stairs
			pm->s.pm_flags |= PMF_ON_STAIRS;

		if (pml.velocity[2] < 0.0) // clamp to positive velocity
			pml.velocity[2] = 0.0;
		if (pml.velocity[2] > newvel[2]) // but don't launch
			pml.velocity[2] = newvel[2];
	}
}

/*
 * Pm_Friction
 *
 * Handles both ground friction and water friction
 */
static void Pm_Friction(void) {
	float speed, newspeed, friction, control;
	float drop;

	speed = VectorLength(pml.velocity);

	if (speed < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	drop = 0.0;

	if (pm->s.pm_type == PM_SPECTATOR) { // spectator friction
		drop = speed * PM_FRICT_SPECTATOR * pml.frame_time;
	} else { // ladder, water, ground, and air friction

		if (pml.ladder) {
			control = speed < PM_SPEED_STOP ? PM_SPEED_STOP : speed;
			drop = control * PM_FRICT_LADDER * pml.frame_time;
		} else if (pm->ground_entity) {

			if (pm->s.pm_type == PM_DEAD) { // dead players slide on ground
				friction = PM_FRICT_SPECTATOR;
			} else { // this is our general case for slick or normal ground
				if (pml.ground_surface && (pml.ground_surface->flags
						& SURF_SLICK))
					friction = PM_FRICT_GROUND_SLICK;
				else
					friction = PM_FRICT_GROUND;
			}

			control = speed < PM_SPEED_STOP ? PM_SPEED_STOP : speed;
			drop = control * friction * pml.frame_time;
		} else if (pm->water_level) { // water friction
			drop = speed * PM_FRICT_WATER * pm->water_level * pml.frame_time;
		} else { // jumping or falling, add some friction
			drop = speed * PM_FRICT_NO_GROUND * pml.frame_time;
		}
	}

	// scale the velocity
	newspeed = speed - drop;

	if (newspeed < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	newspeed /= speed;
	VectorScale(pml.velocity, newspeed, pml.velocity);
}

/*
 * Pm_Accelerate
 *
 * Handles user intended acceleration
 */
static void Pm_Accelerate(vec3_t dir, float speed, float accel) {
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct(pml.velocity, dir);
	addspeed = speed - currentspeed;

	if (addspeed <= 0)
		return;

	accelspeed = accel * pml.frame_time * speed;

	if (accelspeed > addspeed)
		accelspeed = addspeed;

	VectorMA(pml.velocity, accelspeed, dir, pml.velocity);
}

/*
 * Pm_AddCurrents
 */
static void Pm_AddCurrents(vec3_t vel) {
	vec3_t v;
	float s;

	// account for ladders
	if (pml.ladder && fabsf(pml.velocity[2]) <= PM_SPEED_LADDER) {
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

/*
 * Pm_WaterMove
 */
static void Pm_WaterMove(void) {
	vec3_t org, vel, dir, tmp;
	float speed, accel;
	int i;

	VectorCopy(pml.origin, org);

	// user intentions
	for (i = 0; i < 3; i++)
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.side;

	if (!pm->cmd.forward && !pm->cmd.side && !pm->cmd.up)
		vel[2] -= PM_SPEED_WATER_DOWN; // sink
	else
		vel[2] += pm->cmd.up;

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	if (speed > PM_SPEED_WATER) {
		VectorScale(vel, PM_SPEED_WATER / speed, vel);
		speed = PM_SPEED_WATER;
	}

	accel = pml.ladder ? PM_ACCEL_GROUND : PM_ACCEL_WATER;
	Pm_Accelerate(dir, speed, accel);

	Pm_StepSlideMove();

	// we've swam upward, let's not go water skiing
	if (!pm->ground_entity && !pml.ladder && pml.velocity[2] > 0.0) {

		VectorCopy(pml.origin, tmp);
		tmp[2] += pm->view_height - 6.0;

		i = pm->PointContents(tmp);

		if (!(i & MASK_WATER)) // add some gravity
			pml.velocity[2] -= 2.0 * pm->s.gravity * pml.frame_time;
	}
}

/*
 * Pm_AirMove
 */
static void Pm_AirMove(void) {
	float speed, maxspeed, accel;
	float fmove, smove;
	vec3_t vel, dir;
	int i;

	fmove = pm->cmd.forward;
	smove = pm->cmd.side;

	for (i = 0; i < 2; i++)
		vel[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	vel[2] = 0.0;

	Pm_AddCurrents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to server defined max speed
	maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? PM_SPEED_DUCKED : PM_SPEED_RUN;

	// account for speed modulus
	maxspeed = (pm->cmd.buttons & BUTTON_WALK ? maxspeed * 0.66 : maxspeed);

	if (speed > maxspeed) { // clamp it to maxspeed
		VectorScale(vel, maxspeed / speed, vel);
		speed = maxspeed;
	}

	if (pml.ladder) { // climb up or down the ladder

		Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

		if (!vel[2]) { // we're not being pushed down by a current
			if (pml.velocity[2] > 0.0) {
				pml.velocity[2] -= pm->s.gravity * pml.frame_time;
				if (pml.velocity[2] < 0.0)
					pml.velocity[2] = 0.0;
			} else {
				pml.velocity[2] += pm->s.gravity * pml.frame_time;
				if (pml.velocity[2] > 0.0)
					pml.velocity[2] = 0.0;
			}
		}
	} else if (pm->ground_entity) { // walking on ground

		Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

		if (!pml.velocity[0] && !pml.velocity[1]) // optimization
			return;
	} else { // jumping, falling, etc..

		if (speed < PM_SPEED_RUN)
			accel = PM_ACCEL_NO_GROUND * (PM_SPEED_RUN / speed);
		else
			accel = PM_ACCEL_NO_GROUND;

		Pm_Accelerate(dir, speed, accel);

		// add gravity
		pml.velocity[2] -= pm->s.gravity * pml.frame_time;
	}

	Pm_StepSlideMove();
}

/*
 * Pm_CategorizePosition
 */
static void Pm_CategorizePosition(void) {
	vec3_t point;
	int cont;
	c_trace_t trace;
	int sample1;
	int sample2;

	// see if we're standing on something solid
	VectorCopy(pml.origin, point);
	point[2] -= 2.0;

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, point);

	pml.ground_plane = trace.plane;
	pml.ground_surface = trace.surface;
	pml.ground_contents = trace.contents;

	// if we did not hit anything, or we hit an upward pusher, or we hit a
	// world surface that is too steep to stand on, then we have no ground
	if (!trace.ent || ((pm->s.pm_flags & PMF_PUSHED) && pm->s.pm_time)
			|| trace.plane.normal[2] < PM_STAIR_NORMAL) {

		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->ground_entity = NULL;

	} else {
		pm->s.pm_flags |= PMF_ON_GROUND;
		pm->ground_entity = trace.ent;

		// falling a significant distance disables jumping for a moment
		if (pml.velocity[2] < -PM_SPEED_FALL) {
			pm->s.pm_flags |= PMF_TIME_LAND;
			pm->s.pm_time = 10;
		}

		// hitting the world means we're done being pushed
		if ((int) ((intptr_t) trace.ent) == 1)
			pm->s.pm_flags &= ~PMF_PUSHED;
	}

	// save a reference to the entity touched for game events
	if (pm->num_touch < MAX_TOUCH_ENTS && trace.ent) {
		pm->touch_ents[pm->num_touch] = trace.ent;
		pm->num_touch++;
	}

	// get water_level, accounting for ducking
	pm->water_level = pm->water_type = 0;

	// unset the underwater flagm will reset if still underwater
	pm->s.pm_flags &= ~PMF_UNDER_WATER;

	sample2 = pm->view_height - pm->mins[2];
	sample1 = sample2 / 2.0;

	point[2] = pml.origin[2] + pm->mins[2] + 1.0;
	cont = pm->PointContents(point);

	if (cont & MASK_WATER) {

		pm->water_type = cont;
		pm->water_level = 1;

		point[2] = pml.origin[2] + pm->mins[2] + sample1;

		cont = pm->PointContents(point);

		if (cont & MASK_WATER) {

			pm->water_level = 2;

			point[2] = pml.origin[2] + pm->mins[2] + sample2;

			cont = pm->PointContents(point);

			if (cont & MASK_WATER) {
				pm->water_level = 3;
				pm->s.pm_flags |= PMF_UNDER_WATER;
			}
		}
	}
}

/*
 * Pm_CheckJump
 */
static void Pm_CheckJump(void) {

	if (pm->s.pm_type == PM_DEAD)
		return;

	// can't jump yet
	if (pm->s.pm_flags & PMF_TIME_LAND)
		return;

	if (pm->cmd.up < 10) { // jump key released
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump key to be released
	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return;

	if (!(pm->s.pm_flags & PMF_ON_GROUND))
		return;

	if (pm->water_level >= 2) { // swimming, not jumping
		pm->ground_entity = NULL;

		if (pml.velocity[2] <= -PM_SPEED_JUMP)
			return;

		if (pm->water_type == CONTENTS_WATER)
			pml.velocity[2] = PM_SPEED_JUMP / 3.0;
		else if (pm->water_type == CONTENTS_SLIME)
			pml.velocity[2] = PM_SPEED_JUMP / 4.0;
		else
			pml.velocity[2] = PM_SPEED_JUMP / 5.0;
		return;
	}

	// indicate that jump is currently held
	pm->s.pm_flags |= PMF_JUMP_HELD;

	// disable jumping very briefly
	pm->s.pm_flags |= PMF_TIME_LAND;
	pm->s.pm_time = 4;

	// clear the ground indicators
	pm->s.pm_flags &= ~PMF_ON_GROUND;
	pm->ground_entity = NULL;

	// and do the jump
	pml.velocity[2] += PM_SPEED_JUMP;

	if (pml.velocity[2] < PM_SPEED_JUMP)
		pml.velocity[2] = PM_SPEED_JUMP;
}

/*
 * Pm_CheckSpecialMovement
 */
static void Pm_CheckSpecialMovement(void) {
	vec3_t spot;
	int cont;
	vec3_t flatforward;
	c_trace_t trace;

	if (pm->s.pm_time)
		return;

	pml.ladder = false;

	// check for ladder
	VectorCopy(pml.forward, flatforward);
	flatforward[2] = 0.0;

	VectorNormalize(flatforward);

	VectorMA(pml.origin, 1.0, flatforward, spot);
	spot[2] += pm->view_height;

	trace = pm->Trace(pml.origin, pm->mins, pm->maxs, spot);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER))
		pml.ladder = true;

	// check for water jump
	if (pm->water_level != 2 || pm->cmd.up <= 0)
		return;

	VectorMA(pml.origin, 30.0, flatforward, spot);
	spot[2] += pm->view_height;

	cont = pm->PointContents(spot);

	if (!(cont & CONTENTS_SOLID))
		return;

	spot[2] += PM_STAIR_HEIGHT;
	cont = pm->PointContents(spot);

	if (cont)
		return;

	// jump out of water
	VectorScale(flatforward, 100.0, pml.velocity);
	pml.velocity[2] = PM_SPEED_JUMP_WATER;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}

/*
 * Pm_WaterJumpMove
 */
static void Pm_WaterJumpMove(void) {

	pml.velocity[2] -= pm->s.gravity * pml.frame_time;

	if (pml.velocity[2] < PM_SPEED_STAIRS) { // clear the timer
		pm->s.pm_flags &= ~PMF_TIME_MASK;
		pm->s.pm_time = 0;
	}

	Pm_StepSlideMove();
}

/*
 * Pm_CheckDuck
 *
 * Sets mins, maxs, and pm->view_height
 */
static void Pm_CheckDuck(void) {
	float height;
	c_trace_t trace;

	VectorScale(PM_MINS, PM_SCALE, pm->mins);
	VectorScale(PM_MAXS, PM_SCALE, pm->maxs);

	height = pm->maxs[2] - pm->mins[2];
	pm->view_height = pm->mins[2] + (height * 0.75);

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
		pm->view_height = pm->mins[2] + (height * 0.375);
		pm->maxs[2] = pm->maxs[2] + pm->mins[2] / 2.0;
	}
}

/*
 * Pm_GoodPosition
 */
static boolean_t Pm_GoodPosition(void) {
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
	static int jitterbits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };
	int i, j, bits, sign[3];
	short base[3];

	// snap velocity to eigths
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

	VectorCopy(pm->s.origin, base);

	// try all combinations
	for (j = 0; j < 8; j++) {

		bits = jitterbits[j];

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

	// for ground movement, reduce pitch to keep the player moving forward
	if (pm->s.pm_flags & PMF_ON_GROUND)
		angles[PITCH] /= 3.0;

	AngleVectors(angles, pml.forward, pml.right, pml.up);
}

/*
 * Pm_SpectatorMove
 */
static void Pm_SpectatorMove() {
	vec3_t vel, dir;
	float fmove, smove, speed;
	int i;

	pm->view_height = 22.0;

	// apply friction
	Pm_Friction();

	// resolve desired movement
	fmove = pm->cmd.forward;
	smove = pm->cmd.side;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 3; i++)
		vel[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	vel[2] += pm->cmd.up;

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	if (speed > PM_SPEED_SPECTATOR)
		speed = PM_SPEED_SPECTATOR;

	// accelerate
	Pm_Accelerate(dir, speed, PM_ACCEL_SPECTATOR);

	// do the move
	VectorMA(pml.origin, pml.frame_time, pml.velocity, pml.origin);
}

/*
 * Pm_Init
 */
static void Pm_Init(void) {

	VectorClear(pm->angles);

	pm->view_height = 0.0;

	pm->num_touch = 0;
	pm->ground_entity = NULL;
	pm->water_type = pm->water_level = 0;

	// reset stair climbing flag on every move
	pm->s.pm_flags &= ~PMF_ON_STAIRS;

	// decrement the movement timer
	if (pm->s.pm_time) {
		int msec;

		// each pm_time is worth 8ms
		msec = pm->cmd.msec >> 3;

		if (!msec)
			msec = 1;

		if (msec >= pm->s.pm_time) { // clear the timer and timed flags
			pm->s.pm_flags &= ~PMF_TIME_MASK;
			pm->s.pm_time = 0;
		} else { // or just decrement the timer
			pm->s.pm_time -= msec;
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

	pml.frame_time = pm->cmd.msec * 0.001;
}

/*
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

		Pm_SpectatorMove();

		Pm_SnapPosition();
		return;
	}

	if (pm->s.pm_type == PM_DEAD || pm->s.pm_type == PM_FREEZE) { // no control
		pm->cmd.forward = pm->cmd.side = pm->cmd.up = 0;

		if (pm->s.pm_type == PM_FREEZE) // no movement
			return;
	}

	// set mins, maxs, and view_height
	Pm_CheckDuck();

	// set ground_entity, water_type, and water_level
	Pm_CategorizePosition();

	// attach to a ladder or jump out of the water
	Pm_CheckSpecialMovement();

	if (pm->s.pm_flags & PMF_TIME_TELEPORT) {
		// teleport pause stays in place
	} else if (pm->s.pm_flags & PMF_TIME_WATERJUMP) {
		// waterjump has no control, but falls
		Pm_WaterJumpMove();
	} else {
		// general case checks for jump, adds friction, and then moves
		Pm_CheckJump();

		Pm_Friction();

		if (pm->water_level > 1) // swim
			Pm_WaterMove();
		else
			// or walk
			Pm_AirMove();
	}

	// clamp horizontal velocity to reasonable bounds
	Pm_ClampVelocity();

	// set ground_entity, water_type, and water_level for final spot
	Pm_CategorizePosition();

	// ensure we're at a good point
	Pm_SnapPosition();
}

