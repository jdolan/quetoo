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

#include "bg_pmove.h"

static pm_move_t *pm;

/*
 * @brief A structure containing full floating point precision copies of all
 * movement variables. This is initialized with the player's last movement
 * at each call to Pmove (this is obviously not thread-safe).
 */
typedef struct {
	vec3_t origin;
	vec3_t previous_origin; // in case movement fails

	vec3_t velocity;
	vec3_t previous_velocity; // for detecting landings

	vec3_t view_offset; // eye position in floating point

	vec3_t forward, right, up;
	vec_t time; // in seconds

	c_bsp_surface_t *ground_surface;
	c_bsp_plane_t ground_plane;
	int32_t ground_contents;

} pm_locals_t;

static pm_locals_t pml;

#define PM_ACCEL_GROUND			10.0
#define PM_ACCEL_GROUND_SLICK	4.375
#define PM_ACCEL_NO_GROUND		1.15
#define PM_ACCEL_SPECTATOR		4.5
#define PM_ACCEL_WATER			4.0

#define PM_CLIP_BOUNCE			1.001

#define PM_DEBUG 1

#define PM_FRICT_GROUND			6.0
#define PM_FRICT_GROUND_SLICK	2.0
#define PM_FRICT_LADDER			10.0
#define PM_FRICT_NO_GROUND		0.5
#define PM_FRICT_SPECTATOR		3.0
#define PM_FRICT_WATER			1.0

#define PM_GRAVITY_WATER		0.55

#define PM_GROUND_DIST			0.25
#define PM_GROUND_DIST_TRICK	16.0

#define PM_SPEED_AIR			450.0
#define PM_SPEED_CURRENT		100.0
#define PM_SPEED_DUCK_STAND		225.0
#define PM_SPEED_DUCKED			140.0
#define PM_SPEED_FALL			-420.0
#define PM_SPEED_FALL_FAR		-560.0
#define PM_SPEED_JUMP			262.5
#define PM_SPEED_LADDER			125.0
#define PM_SPEED_LAND			-300.0
#define PM_SPEED_RUN			300.0
#define PM_SPEED_SPECTATOR		350.0
#define PM_SPEED_STOP			100.0
#define PM_SPEED_UP				0.1
#define PM_SPEED_TRICK_JUMP		85.0
#define PM_SPEED_WATER			125.0
#define PM_SPEED_WATER_JUMP		450.0
#define PM_SPEED_WATER_SINK		50.0

#define PM_STEP_HEIGHT			16.0
#define PM_STEP_NORMAL			0.7

#define PM_STOP_EPSILON			0.1

/*
 * @brief Handle printing of debugging messages for development.
 */
static void Pm_Debug_(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Pm_Debug_(const char *func, const char *fmt, ...) {
#if PM_DEBUG
	char msg[MAX_STRING_CHARS];

	g_snprintf(msg, sizeof(msg), "%s: ", func);

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (pm->Debug) {
		pm->Debug(msg);
	} else {
		fputs(msg, stdout);
	}
#else
	if (func || fmt) {
		// silence compiler warnings
	}
#endif
}

#define Pm_Debug(...) Pm_Debug_(__func__, __VA_ARGS__)

/*
 * @brief Slide off of the impacted plane.
 */
static void Pm_ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, vec_t bounce) {
	int32_t i;

	vec_t backoff = DotProduct(in, normal);

	if (backoff < 0.0)
		backoff *= bounce;
	else
		backoff /= bounce;

	for (i = 0; i < 3; i++) {

		const vec_t change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] < PM_STOP_EPSILON && out[i] > -PM_STOP_EPSILON)
			out[i] = 0.0;
	}
}

/*
 * @brief Mark the specified entity as touched. This enables the game module to
 * detect player -> entity interactions.
 */
static void Pm_TouchEnt(struct g_edict_s *ent) {
	int32_t i;

	if (ent == NULL) {
		return;
	}

	if (pm->num_touch == MAX_TOUCH_ENTS) {
		Pm_Debug("MAX_TOUCH_ENTS\n");
		return;
	}

	for (i = 0; i < pm->num_touch; i++) {
		if (pm->touch_ents[i] == ent) {
			return;
		}
	}

	pm->touch_ents[pm->num_touch++] = ent;
}

#define MAX_CLIP_PLANES	4

/*
 * @brief Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state. Returns true if not blocked.
 */
static _Bool Pm_SlideMove(void) {
	int32_t i, num_planes = 0;

	vec3_t vel;
	VectorCopy(pml.velocity, vel);

	vec_t time = pml.time;

	for (i = 0; i < MAX_CLIP_PLANES; i++) {
		vec3_t pos;

		if (time <= 0.0) // out of time
			break;

		// project desired destination
		VectorMA(pml.origin, time, pml.velocity, pos);

		// trace to it
		c_trace_t trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

		// store a reference to the entity for firing game events
		Pm_TouchEnt(trace.ent);

		if (trace.all_solid) { // player is trapped in a solid
			VectorClear(pml.velocity);
			break;
		}

		// update the origin
		VectorCopy(trace.end, pml.origin);

		if (trace.fraction == 1.0) // moved the entire distance
			break;

		// update the movement time remaining
		time -= (time * trace.fraction);

		// and increase the number of planes intersected
		num_planes++;

		// now slide along the plane
		Pm_ClipVelocity(pml.velocity, trace.plane.normal, pml.velocity, PM_CLIP_BOUNCE);

		// if we've been deflected backwards, settle to prevent oscillations
		if (DotProduct(pml.velocity, vel) <= 0.0) {
			VectorClear(pml.velocity);
			break;
		}
	}

	return num_planes == 0;
}

/*
 * @brief Performs the step portion of step-slide-move.
 *
 * @return True if the step was successful, false otherwise.
 */
static _Bool Pm_StepMove(_Bool up) {
	vec3_t org, vel, pos;
	c_trace_t trace;

	VectorCopy(pml.origin, org);
	VectorCopy(pml.velocity, vel);

	if (up) { // try sliding from a higher position

		// check if the upward position is available
		VectorCopy(pml.origin, pos);
		pos[2] += PM_STEP_HEIGHT;

		// reaching even higher if trying to climb out of the water
		if (pm->s.pm_flags & PMF_TIME_WATER_JUMP) {
			pos[2] += PM_STEP_HEIGHT;
		}

		trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

		if (trace.all_solid) { // it's not
			Pm_Debug("Can't step up: %s\n", vtos(pml.origin));
			return false;
		}

		// an upward position is available, try to slide from there
		VectorCopy(trace.end, pml.origin);

		Pm_SlideMove(); // slide from the higher position
	}

	// if stepping down, or if we've just stepped up, settle to the floor
	VectorCopy(pml.origin, pos);
	pos[2] -= (PM_STEP_HEIGHT + PM_GROUND_DIST);

	// by tracing down to it
	trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

	// check if the floor was found
	if (trace.ent && trace.plane.normal[2] >= PM_STEP_NORMAL) {

		// check if the floor is new; if so, we've likely stepped
		if (memcmp(&trace.plane, &pml.ground_plane, sizeof(c_bsp_plane_t))) {

			if (up) {
				pml.origin[2] = MAX(org[2], trace.end[2]);
				pml.velocity[2] = MAX(vel[2], pml.velocity[2]);
			} else {
				pml.origin[2] = MIN(org[2], trace.end[2]);
				pml.velocity[2] = MIN(vel[2], pml.velocity[2]);
			}

			// calculate the step so that the client may interpolate
			pm->step = pml.origin[2] - org[2];
			pm->s.pm_flags |= PMF_ON_STAIRS;

			return true;
		}
	}

	return false;
}

/*
 * @brief
 */
static void Pm_StepSlideMove(void) {
	vec3_t org, vel;

	// save our initial position and velocity to step from
	VectorCopy(pml.origin, org);
	VectorCopy(pml.velocity, vel);

	// if something blocked us, try to step over it
	if (!Pm_SlideMove() && !(pm->s.pm_flags & PMF_ON_LADDER)) {
		vec3_t org0, vel0;

		// save the initial results in case stepping up fails
		VectorCopy(pml.origin, org0);
		VectorCopy(pml.velocity, vel0);

		// and step with the original position and velocity
		VectorCopy(org, pml.origin);
		VectorCopy(vel, pml.velocity);

		// if the step succeeds, select the more productive of the two moves
		if (Pm_StepMove(true)) {
			vec3_t org1, vel1;

			VectorCopy(pml.origin, org1);
			VectorCopy(pml.velocity, vel1);

			vec3_t delta0, delta1;

			VectorSubtract(org0, org, delta0);
			VectorSubtract(org1, org, delta1);

			delta0[2] = delta1[2] = 0.0;

			// if the step wasn't productive, revert it
			if (VectorLength(delta1) <= VectorLength(delta0)) {
				Pm_Debug("Reverting step %2.1f\n", pm->step);

				VectorCopy(org0, pml.origin);
				VectorCopy(vel0, pml.velocity);

				pm->s.pm_flags &= ~PMF_ON_STAIRS;
				pm->step = 0.0;
			}
		} else {
			VectorCopy(org0, pml.origin);
			VectorCopy(vel0, pml.velocity);
		}
	}

	// try to step down to remain on the ground
	if ((pm->s.pm_flags & PMF_ON_GROUND) && !(pm->s.pm_flags & PMF_TIME_TRICK_JUMP)) {

		if (pml.velocity[2] < PM_SPEED_UP) {
			vec3_t org0, vel0;

			// save these initial results in case stepping down fails
			VectorCopy(pml.origin, org0);
			VectorCopy(pml.velocity, vel0);

			if (!Pm_StepMove(false)) {
				VectorCopy(org0, pml.origin);
				VectorCopy(vel0, pml.velocity);
			}
		}
	}
}

/*
 * @brief Handles friction against user intentions, and based on contents.
 */
static void Pm_Friction(void) {

	const vec_t speed = VectorLength(pml.velocity);

	if (speed < 2.0) {
		VectorClear(pml.velocity);
		return;
	}

	const vec_t control = MAX(PM_SPEED_STOP, speed);

	vec_t friction = 0.0;

	if (pm->s.pm_type == PM_SPECTATOR) { // spectator friction
		friction = PM_FRICT_SPECTATOR;
	} else { // ladder, ground, water, air and friction
		if (pm->s.pm_flags & PMF_ON_LADDER) {
			friction = PM_FRICT_LADDER;
		} else {
			if (pm->s.pm_flags & PMF_ON_GROUND) {
				if (pml.ground_surface && (pml.ground_surface->flags & SURF_SLICK)) {
					friction = PM_FRICT_GROUND_SLICK;
				} else {
					friction = PM_FRICT_GROUND;
				}
			} else {
				friction = PM_FRICT_NO_GROUND;
			}

			if (pm->water_level) {
				friction += PM_FRICT_WATER * pm->water_level;
			}
		}
	}

	friction *= control * pml.time;

	// scale the velocity
	vec_t scale = speed - friction;

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
static void Pm_Accelerate(vec3_t dir, vec_t speed, vec_t accel) {
	vec_t add_speed, accel_speed, current_speed;

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
 * @brief Applies gravity to the current movement.
 */
static void Pm_Gravity(void) {
	vec_t gravity = pm->s.gravity;

	if (pm->water_level > 2) {
		gravity *= PM_GRAVITY_WATER;
	}

	pml.velocity[2] -= gravity * pml.time;
}

/*
 * @brief
 */
static void Pm_Currents(vec3_t vel) {
	vec3_t v;
	vec_t s;

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

	// add conveyer belt velocities
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
 * @return True if the player will be eligible for trick jumping should they
 * impact the ground on this frame, false otherwise.
 */
static _Bool Pm_CheckTrickJump(void) {

	if (pm->ground_entity)
		return false;

	if (pml.previous_velocity[2] < PM_SPEED_UP)
		return false;

	if (pm->cmd.up < 1)
		return false;

	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return false;

	if (pm->s.pm_flags & PMF_TIME_MASK)
		return false;

	return true;
}

/*
 * @brief Determine state for the current position. This involves resolving the
 * ground entity, water level, and water type.
 */
static void Pm_CategorizePosition(void) {
	vec3_t pos;

	// seek ground eagerly if the player wishes to trick jump
	const _Bool trick_jump = Pm_CheckTrickJump();

	if (trick_jump) {
		VectorMA(pml.origin, pml.time, pml.velocity, pos);
		pos[2] -= PM_GROUND_DIST_TRICK;
	} else {
		VectorCopy(pml.origin, pos);
		pos[2] -= PM_GROUND_DIST;
	}

	c_trace_t trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

	pml.ground_plane = trace.plane;
	pml.ground_surface = trace.surface;
	pml.ground_contents = trace.contents;

	// if we hit an upward facing plane, make it our ground
	if (trace.ent && trace.plane.normal[2] >= PM_STEP_NORMAL) {

		// if we had no ground, then handle landing events
		if (!pm->ground_entity) {

			// any landing terminates the water jump
			if (pm->s.pm_flags & PMF_TIME_WATER_JUMP) {
				pm->s.pm_flags &= ~PMF_TIME_WATER_JUMP;
				pm->s.pm_time = 0;
			}

			// hard landings disable jumping briefly
			if (pml.previous_velocity[2] <= PM_SPEED_LAND) {
				pm->s.pm_flags |= PMF_TIME_LAND;
				pm->s.pm_time = 32;

				if (pml.previous_velocity[2] <= PM_SPEED_FALL) {
					pm->s.pm_time = 250;

					if (pml.previous_velocity[2] <= PM_SPEED_FALL_FAR) {
						pm->s.pm_time = 500;
					}
				}
			} else { // soft landings with upward momentum grant trick jumps
				if (trick_jump) {
					pm->s.pm_flags |= PMF_TIME_TRICK_JUMP;
					pm->s.pm_time = 32;
				}
			}
		}

		// save a reference to the ground
		pm->s.pm_flags |= PMF_ON_GROUND;
		pm->ground_entity = trace.ent;
	} else {
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->ground_entity = NULL;
	}

	// always touch the entity, even if we couldn't stand on it
	Pm_TouchEnt(trace.ent);

	// get water level, accounting for ducking
	pm->water_level = pm->water_type = 0;

	VectorCopy(pml.origin, pos);
	pos[2] = pml.origin[2] + pm->mins[2] + 1.0;

	int32_t contents = pm->PointContents(pos);
	if (contents & MASK_WATER) {

		pm->water_type = contents;
		pm->water_level = 1;

		pos[2] = pml.origin[2];

		contents = pm->PointContents(pos);

		if (contents & MASK_WATER) {

			pm->water_level = 2;

			pos[2] = pml.origin[2] + pml.view_offset[2] + 1.0;

			contents = pm->PointContents(pos);

			if (contents & MASK_WATER) {
				pm->water_level = 3;
				pm->s.pm_flags |= PMF_UNDER_WATER;
			}
		}
	}
}

/*
 * @brief Handles ducking, adjusting both the player's bounding box and view
 * offset accordingly. Players must be on the ground in order to duck.
 */
static void Pm_CheckDuck(void) {

	const vec_t height = pm->maxs[2] - pm->mins[2];

	if (pm->s.pm_type == PM_DEAD) {
		pm->s.pm_flags |= PMF_DUCKED;
	} else {
		// if on the ground and requesting to crouch, duck
		if (pm->s.pm_flags & PMF_ON_GROUND && pm->cmd.up < 0) {
			pm->s.pm_flags |= PMF_DUCKED;
		} else { // stand up if possible
			c_trace_t trace = pm->Trace(pml.origin, pml.origin, pm->mins, pm->maxs);
			if (trace.all_solid) {
				pm->s.pm_flags |= PMF_DUCKED;
			}
		}
	}

	if (pm->s.pm_flags & PMF_DUCKED) { // ducked, reduce height
		vec_t target = pm->mins[2];

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
		const vec_t target = pm->mins[2] + height * 0.75;

		if (pml.view_offset[2] < target) // go up
			pml.view_offset[2] += pml.time * PM_SPEED_DUCK_STAND;

		if (pml.view_offset[2] > target)
			pml.view_offset[2] = target;
	}
}

/*
 * @brief Check for jumping and trick jumping.
 *
 * @return True if a jump occurs, false otherwise.
 */
static _Bool Pm_CheckJump(void) {

	// not on ground yet
	if (!(pm->s.pm_flags & PMF_ON_GROUND))
		return false;

	// must wait for landing damage to subside
	if (pm->s.pm_flags & PMF_TIME_LAND)
		return false;

	// must wait for jump key to be released
	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return false;

	// didn't ask to jump
	if (pm->cmd.up < 1)
		return false;

	// finally, do the jump
	vec_t jump = PM_SPEED_JUMP;

	// adding the double jump if eligible
	if (pm->s.pm_flags & PMF_TIME_TRICK_JUMP) {
		jump += PM_SPEED_TRICK_JUMP;

		pm->s.pm_flags &= ~PMF_TIME_TRICK_JUMP;
		pm->s.pm_time = 0;

		Pm_Debug("Trick jump: %d\n", pm->cmd.up);
	} else {
		Pm_Debug("Jump: %d\n", pm->cmd.up);
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
 * @brief Check for push interactions.
 *
 * @return True if the player was pushed by an entity, false otherwise.
 */
static _Bool Pm_CheckPush(void) {

	if (!(pm->s.pm_flags & PMF_PUSHED))
		return false;

	// clear the ground indicators
	pm->s.pm_flags &= ~PMF_ON_GROUND;
	pm->ground_entity = NULL;

	return true;
}

/*
 * @brief Check for ladder interaction.
 *
 * @return True if the player is on a ladder, false otherwise.
 */
static _Bool Pm_CheckLadder(void) {
	vec3_t forward, pos;

	if (pm->s.pm_flags & PMF_TIME_MASK)
		return false;

	// check for ladder
	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);

	VectorMA(pml.origin, 1.0, forward, pos);
	pos[2] += pml.view_offset[2];

	const c_trace_t trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER)) {
		pm->s.pm_flags |= PMF_ON_LADDER;
		return true;
	}

	return false;
}

/*
 * @brief Checks for water exit. The player may exit the water when they can
 * see a usable step out of the water.
 *
 * @return True if a water jump has occurred, false otherwise.
 */
static _Bool Pm_CheckWaterJump(void) {
	vec3_t pos;

	if (pm->s.pm_flags & PMF_TIME_WATER_JUMP)
		return false;

	if (pm->water_level != 2)
		return false;

	if (pm->cmd.up < 1 && pm->cmd.forward < 1)
		return false;

	VectorAdd(pml.origin, pml.view_offset, pos);
	VectorMA(pos, 24.0, pml.forward, pos);

	c_trace_t trace = pm->Trace(pml.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_SOLID)) {

		pos[2] += PM_STEP_HEIGHT + pm->maxs[2] - pm->mins[2];

		trace = pm->Trace(pos, pos, pm->mins, pm->maxs);

		if (trace.start_solid)
			return false;

		// jump out of water
		pml.velocity[2] = PM_SPEED_WATER_JUMP;

		pm->s.pm_flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
		pm->s.pm_time = 2000;

		Pm_Debug("%s\n", vtos(pml.origin));
		return true;
	}

	return false;
}

/*
 * @brief
 */
static void Pm_LadderMove(void) {
	vec3_t vel, dir;
	int32_t i;

	Pm_Debug("%s\n", vtos(pml.origin));

	Pm_Friction();

	// user intentions in X/Y
	for (i = 0; i < 3; i++) {
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

		const vec_t s = PM_SPEED_LADDER * 0.125;

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

	Pm_Currents(vel);

	VectorCopy(vel, dir);
	vec_t speed = VectorNormalize(dir);
	speed = Clamp(speed, 0.0, PM_SPEED_LADDER);

	Pm_Accelerate(dir, speed, PM_ACCEL_GROUND);

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static void Pm_WaterJumpMove(void) {
	vec3_t forward;

	Pm_Debug("%s\n", vtos(pml.origin));

	Pm_Friction();

	Pm_Gravity();

	// check for a usable spot directly in front of us
	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);
	VectorMA(pml.origin, 30.0, forward, forward);

	// if we've reached a usable spot, clamp the jump to avoid launching
	if (pm->Trace(pml.origin, forward, pm->mins, pm->maxs).fraction == 1.0) {
		pml.velocity[2] = Clamp(pml.velocity[2], 0.0, PM_SPEED_JUMP);
	}

	// if we're falling back down, clear the timer to regain control
	if (pml.velocity[2] <= 0.0) {
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
	vec_t speed;
	int32_t i;

	if (Pm_CheckWaterJump()) {
		Pm_WaterJumpMove();
		return;
	}

	Pm_Debug("%s\n", vtos(pml.origin));

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
			Pm_Gravity();
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

	Pm_Currents(vel);

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
	vec_t speed;
	int32_t i;

	// Pm_Debug("%s\n", vtos(pml.velocity));

	Pm_Friction();

	Pm_Gravity();

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

	speed = Clamp(speed, 0.0, PM_SPEED_AIR);

	Pm_Accelerate(dir, speed, PM_ACCEL_NO_GROUND);

	Pm_StepSlideMove();
}

/*
 * @brief Called for movements where player is on ground, regardless of water level.
 */
static void Pm_WalkMove(void) {
	vec_t speed, max_speed, accel;
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

	// Pm_Debug("%s\n", vtos(pml.origin));

	Pm_Friction();

	pml.forward[2] = 0.0;
	pml.right[2] = 0.0;

	Pm_ClipVelocity(pml.forward, pml.ground_plane.normal, pml.forward, PM_CLIP_BOUNCE);
	Pm_ClipVelocity(pml.right, pml.ground_plane.normal, pml.right, PM_CLIP_BOUNCE);

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	Pm_Currents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	max_speed = (pm->s.pm_flags & PMF_DUCKED) ? PM_SPEED_DUCKED : PM_SPEED_RUN;

	// accounting for water level
	max_speed = (pm->water_level > 1) ? max_speed / (pm->water_level * 0.66) : max_speed;

	// and accounting for speed modulus
	max_speed = (pm->cmd.buttons & BUTTON_WALK) ? max_speed * 0.66 : max_speed;

	// clamp the speed to max speed
	speed = Clamp(speed, 0.0, max_speed);

	// accelerate based on slickness of ground surface
	accel = (pml.ground_surface->flags & SURF_SLICK) ? PM_ACCEL_GROUND_SLICK : PM_ACCEL_GROUND;

	Pm_Accelerate(dir, speed, accel);

	// determine the speed after acceleration
	speed = VectorLength(pml.velocity);

	// clip to the ground
	Pm_ClipVelocity(pml.velocity, pml.ground_plane.normal, pml.velocity, PM_CLIP_BOUNCE);

	// and now scale by the speed to avoid slowing down on slopes
	VectorNormalize(pml.velocity);
	VectorScale(pml.velocity, speed, pml.velocity);

	Pm_StepSlideMove();
}

/*
 * @brief
 */
static _Bool Pm_GoodPosition(void) {
	c_trace_t trace;
	vec3_t pos;

	if (pm->s.pm_type == PM_SPECTATOR)
		return true;

	UnpackPosition(pm->s.origin, pos);

	trace = pm->Trace(pos, pos, pm->mins, pm->maxs);

	return !trace.start_solid;
}

/*
 * @brief On exit, the origin will have a value that is pre-quantized to the 0.125
 * precision of the network channel and in a valid position.
 */
static void Pm_SnapPosition(void) {
	static const int16_t jitter_bits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };
	int16_t i, sign[3], base[3];
	size_t j;

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
	Pm_Debug("Failed to snap to good position: %s.\n", vtos(pml.origin));
	PackPosition(pml.previous_origin, pm->s.origin);
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
	vec_t speed;
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

	pm->step = 0.0;

	// reset flags that we test each move
	pm->s.pm_flags &= ~(PMF_DUCKED | PMF_JUMPED);
	pm->s.pm_flags &= ~(PMF_ON_GROUND | PMF_ON_STAIRS | PMF_ON_LADDER);
	pm->s.pm_flags &= ~(PMF_UNDER_WATER);

	if (pm->cmd.up < 1) { // jump key released
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
	}

	// decrement the movement timer by the duration of the command
	if (pm->s.pm_time) {
		if (pm->cmd.msec >= pm->s.pm_time) { // clear the timer and timed flags
			pm->s.pm_flags &= ~PMF_TIME_MASK;
			pm->s.pm_time = 0;
		} else { // or just decrement the timer
			pm->s.pm_time -= pm->cmd.msec;
		}
	}
}

/*
 * @brief
 */
static void Pm_InitLocal(void) {

	// clear all move local vars
	memset(&pml, 0, sizeof(pml));

	// convert origin, velocity and view offset to floating point
	UnpackPosition(pm->s.origin, pml.origin);
	UnpackPosition(pm->s.velocity, pml.velocity);
	UnpackPosition(pm->s.view_offset, pml.view_offset);

	// save previous origin in case move fails entirely
	VectorCopy(pml.origin, pml.previous_origin);

	// save previous velocity for detecting landings
	VectorCopy(pml.velocity, pml.previous_velocity);

	// convert from milliseconds to seconds
	pml.time = pm->cmd.msec * 0.001;
}

/*
 * @brief Can be called by either the game or the client game to update prediction.
 */
void Pm_Move(pm_move_t *pm_move) {
	pm = pm_move;

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

	// check for ducking
	Pm_CheckDuck();

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

	// set ground_entity, water_type, and water_level for final spot
	Pm_CategorizePosition();

	// ensure we're at a good point
	Pm_SnapPosition();
}

