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

/**
 * @brief PM_MINS and PM_MAXS are the default bounding box, scaled by PM_SCALE
 * in Pm_Init. They are referenced in a few other places e.g. to create effects
 * at a certain body position on the player model.
 */
const vec3_t PM_MINS = { -16.0, -16.0, -24.0 };
const vec3_t PM_MAXS = { 16.0, 16.0, 32.0 };

const vec3_t PM_DEAD_MINS = { -24.0, -24.0, -24.0 };
const vec3_t PM_DEAD_MAXS = { 24.0, 24.0, -4.0 };

const vec3_t PM_GIBLET_MINS = { -8.0, -8.0, -8.0 };
const vec3_t PM_GIBLET_MAXS = { 8.0, 8.0, 8.0 };

static pm_move_t *pm;

/**
 * @brief A structure containing full floating point precision copies of all
 * movement variables. This is initialized with the player's last movement
 * at each call to PM_Move (this is obviously not thread-safe).
 */
typedef struct {

	// previous origin, in case movement fails
	vec3_t previous_origin;

	// previous velocity, for detecting landings
	vec3_t previous_velocity;

	// float point precision view offset
	vec3_t view_offset;

	vec3_t forward, right, up;
	vec_t time; // the command milliseconds in seconds

	// ground interactions
	cm_bsp_surface_t *ground_surface;
	cm_bsp_plane_t ground_plane;
	int32_t ground_contents;

} pm_locals_t;

static pm_locals_t pml;

#if PM_DEBUG

/**
 * @brief Handle printing of debugging messages for development.
 */
static void Pm_Debug_(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void Pm_Debug_(const char *func, const char *fmt, ...) {
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
}

#define Pm_Debug(...) Pm_Debug_(__func__, __VA_ARGS__)
#else
#define Pm_Debug(...)
#endif

/**
 * @brief Slide off of the impacted plane.
 */
static void Pm_ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, vec_t bounce) {

	vec_t backoff = DotProduct(in, normal);

	if (backoff < 0.0)
		backoff *= bounce;
	else
		backoff /= bounce;

	for (int32_t i = 0; i < 3; i++) {

		const vec_t change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
}

/**
 * @brief Mark the specified entity as touched. This enables the game module to
 * detect player -> entity interactions.
 */
static void Pm_TouchEntity(struct g_entity_s *ent) {

	if (ent == NULL) {
		return;
	}

	if (pm->num_touch_ents == PM_MAX_TOUCH_ENTS) {
		Pm_Debug("MAX_TOUCH_ENTS\n");
		return;
	}

	for (int32_t i = 0; i < pm->num_touch_ents; i++) {
		if (pm->touch_ents[i] == ent) {
			return;
		}
	}

	pm->touch_ents[pm->num_touch_ents++] = ent;
}

#define MAX_CLIP_PLANES	5

/**
 * @brief Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state. Returns true if not blocked.
 */
static _Bool Pm_SlideMove(void) {
	vec3_t planes[MAX_CLIP_PLANES];
	int32_t bump, num_bumps = MAX_CLIP_PLANES - 1;

	vec_t time_remaining = pml.time;
	int32_t num_planes = 0;

	// never turn against our ground plane
	if (pm->s.flags & PMF_ON_GROUND) {
		VectorCopy(pml.ground_plane.normal, planes[num_planes]);
		num_planes++;
	}

	// or our original velocity
	VectorNormalize2(pm->s.velocity, planes[num_planes]);
	num_planes++;

	for (bump = 0; bump < num_bumps; bump++) {
		vec3_t pos;

		if (time_remaining <= 0.0) // out of time
			break;

		// project desired destination
		VectorMA(pm->s.origin, time_remaining, pm->s.velocity, pos);

		// trace to it
		const cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

		// if the player is trapped in a solid, don't build up Z
		if (trace.all_solid) {
			pm->s.velocity[2] = 0.0;
			return true;
		}

		// if the trace succeeded, move some distance
		if (trace.fraction > 0.0) {

			VectorCopy(trace.end, pm->s.origin);

			// if the trace didn't hit anything, we're done
			if (trace.fraction == 1.0)
				break;

			// update the movement time remaining
			time_remaining -= (time_remaining * trace.fraction);
		}

		// store a reference to the entity for firing game events
		Pm_TouchEntity(trace.ent);

		// record the impacted plane
		VectorCopy(trace.plane.normal, planes[num_planes]);
		num_planes++;

		// and modify velocity, clipping to all impacted planes
		for (int32_t i = 0; i < num_planes; i++) {
			vec3_t vel;

			if (DotProduct(pm->s.velocity, planes[i]) >= 0.0) {
				continue;
			}

			// slide along the plane
			Pm_ClipVelocity(pm->s.velocity, planes[i], vel, PM_CLIP_BOUNCE);

			// see if there is a second plane that the new move enters
			for (int32_t j = 0; j < num_planes; j++) {
				vec3_t cross;

				if (j == i) {
					continue;
				}

				// if the clipped velocity doesn't impact this plane, skip it
				if (DotProduct(vel, planes[j]) >= 0.0) {
					continue;
				}

				// we are now intersecting a second plane
				Pm_ClipVelocity(vel, planes[j], vel, PM_CLIP_BOUNCE);

				// but if we clip against it without being deflected back, we're okay
				if (DotProduct(vel, planes[i]) >= 0.0) {
					continue;
				}

				// we must now slide along the crease (cross product of the planes)
				CrossProduct(planes[i], planes[j], cross);
				VectorNormalize(cross);

				const vec_t scale = DotProduct(cross, pm->s.velocity);
				VectorScale(cross, scale, vel);

				// see if there is a third plane the the new move enters
				for (int32_t k = 0; k < num_planes; k++) {

					if (k == i || k == j) {
						continue;
					}

					if (DotProduct(vel, planes[k]) >= 0.0) {
						continue;
					}

					// stop dead at a triple plane interaction
					VectorClear(pm->s.velocity);
					return true;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy(vel, pm->s.velocity);
			break;
		}
	}

	return bump == 0;
}

/**
 * @return True if the downward trace yielded a step, false otherwise.
 */
static _Bool Pm_CheckStep(const cm_trace_t *trace) {

	if (!trace->all_solid) {
		if (trace->ent && trace->plane.normal[2] >= PM_STEP_NORMAL) {
			if (trace->ent != pm->ground_entity || trace->plane.num != pml.ground_plane.num) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief
 */
static void Pm_StepDown(const cm_trace_t *trace) {

	VectorCopy(trace->end, pm->s.origin);
	pm->s.origin[2] += PM_STOP_EPSILON;

	Pm_ClipVelocity(pm->s.velocity, trace->plane.normal, pm->s.velocity, PM_CLIP_BOUNCE);

	pm->step = pm->s.origin[2] - pml.previous_origin[2];
	if (fabs(pm->step) >= 4.0) {
		Pm_Debug("step %3.2f\n", pm->step);
		pm->s.flags |= PMF_ON_STAIRS;
	} else {
		pm->step = 0.0;
	}
}

/**
 * @brief
 */
static void Pm_StepSlideMove(void) {

#if PM_QUAKE3

	vec3_t org, vel;
	vec3_t down, up;

	// save our initial position and velocity to step from
	VectorCopy(pm->s.origin, org);
	VectorCopy(pm->s.velocity, vel);

	if (Pm_SlideMove()) { // we weren't blocked, we're done
		return;
	}

	if (pm->s.flags & PMF_ON_LADDER) { // we're on a ladder, we're done
		return;
	}

	// don't step up if we still have upward velocity, and there's no ground

	VectorMA(org, PM_STEP_HEIGHT + PM_GROUND_DIST, vec3_down, down);
	cm_trace_t trace = pm->Trace(org, down, pm->mins, pm->maxs);
	if (pm->s.velocity[2] > PM_SPEED_UP && (trace.ent == NULL || trace.plane.normal[2] < PM_STEP_NORMAL)) {
		return;
	}

	// try to step up

	VectorMA(org, PM_STEP_HEIGHT, vec3_up, up);
	trace = pm->Trace(org, up, pm->mins, pm->maxs);
	if (trace.all_solid) {
		return;
	}

	VectorCopy(trace.end, pm->s.origin);
	VectorCopy(vel, pm->s.velocity);

	Pm_SlideMove();

	// settle to the new ground

	VectorMA(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, vec3_down, down);
	trace = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);
	if (!trace.all_solid) {
		VectorCopy(trace.end, pm->s.origin);
	}

	if (trace.fraction < 1.0) {
		Pm_ClipVelocity(pm->s.velocity, trace.plane.normal, pm->s.velocity, PM_CLIP_BOUNCE);
	}

	// set the step for interpolation

	pm->step = pm->s.origin[2] - org[2];
	if (fabs(pm->step) >= 4.0) {
		pm->s.flags |= PMF_ON_STAIRS;
	} else {
		pm->step = 0.0;
	}

#else

	vec3_t org0, vel0;
	vec3_t org1, vel1;
	vec3_t down, up;

	VectorCopy(pm->s.origin, org0);
	VectorCopy(pm->s.velocity, vel0);

	// attempt to move; if nothing blocks us, we're done
	if (Pm_SlideMove()) {

		// attempt to step down to remain on ground
		if ((pm->s.flags & PMF_ON_GROUND) && pm->cmd.up <= 0) {

			VectorMA(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, vec3_down, down);
			const cm_trace_t step_down = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);

			if (Pm_CheckStep(&step_down)) {
				Pm_StepDown(&step_down);
			}
		}

		return;
	}

	// we were blocked, so try to step over the obstacle

	VectorCopy(pm->s.origin, org1);
	VectorCopy(pm->s.velocity, vel1);

	VectorMA(org0, PM_STEP_HEIGHT, vec3_up, up);
	const cm_trace_t step_up = pm->Trace(org0, up, pm->mins, pm->maxs);
	if (!step_up.all_solid) {

		// step from the higher position, with the original velocity
		VectorCopy(step_up.end, pm->s.origin);
		VectorCopy(vel0, pm->s.velocity);

		Pm_SlideMove();

		// settle to the new ground, keeping the step if and only if it was successful
		VectorMA(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, vec3_down, down);
		const cm_trace_t step_down = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);

		if (Pm_CheckStep(&step_down)) {
			// Quake2 trick jump secret sauce
			if ((pm->s.flags & PMF_ON_GROUND) || vel0[2] < PM_SPEED_UP) {
				Pm_StepDown(&step_down);
			} else {
				pm->step = pm->s.origin[2] - pml.previous_origin[2];
				pm->s.flags |= PMF_ON_STAIRS;
			}

			return;
		}
	}

	VectorCopy(org1, pm->s.origin);
	VectorCopy(vel1, pm->s.velocity);

#endif
}

/**
 * @brief Handles friction against user intentions, and based on contents.
 */
static void Pm_Friction(void) {
	vec3_t vel;

	VectorCopy(pm->s.velocity, vel);

	if (pm->s.flags & PMF_ON_GROUND) {
		vel[2] = 0.0;
	}

	const vec_t speed = VectorLength(vel);

	if (speed < 1.0) {
		pm->s.velocity[0] = 0.0;
		pm->s.velocity[1] = 0.0;
		return;
	}

	const vec_t control = MAX(PM_SPEED_STOP, speed);

	vec_t friction = 0.0;

	if (pm->s.type == PM_SPECTATOR) { // spectator friction
		friction = PM_FRICT_SPECTATOR;
	} else { // ladder, ground, water, air and friction
		if (pm->s.flags & PMF_ON_LADDER) {
			friction = PM_FRICT_LADDER;
		} else {
			if (pm->water_level > 1) {
				friction = PM_FRICT_WATER;
			} else {
				if (pm->s.flags & PMF_ON_GROUND) {
					if (pml.ground_surface && (pml.ground_surface->flags & SURF_SLICK)) {
						friction = PM_FRICT_GROUND_SLICK;
					} else {
						friction = PM_FRICT_GROUND;
					}
				} else {
					friction = PM_FRICT_AIR;
				}
			}
		}
	}

	// scale the velocity, taking care to not reverse direction
	vec_t scale = MAX(0.0, speed - (friction * control * pml.time)) / speed;

	VectorScale(pm->s.velocity, scale, pm->s.velocity);
}

/**
 * @brief Handles user intended acceleration.
 */
static void Pm_Accelerate(vec3_t dir, vec_t speed, vec_t accel) {

	const vec_t current_speed = DotProduct(pm->s.velocity, dir);
	const vec_t add_speed = speed - current_speed;

	if (add_speed <= 0.0)
		return;

	vec_t accel_speed = accel * pml.time * speed;

	if (accel_speed > add_speed)
		accel_speed = add_speed;

	VectorMA(pm->s.velocity, accel_speed, dir, pm->s.velocity);
}

/**
 * @brief Applies gravity to the current movement.
 */
static void Pm_Gravity(void) {
	vec_t gravity = pm->s.gravity;

	if (pm->water_level > 2) {
		gravity *= PM_GRAVITY_WATER;
	}

	pm->s.velocity[2] -= gravity * pml.time;
}

/**
 * @brief
 */
static void Pm_Currents(vec3_t vel) {
	vec3_t current;

	VectorClear(current);

	// add water currents
	if (pm->water_level) {
		if (pm->water_type & CONTENTS_CURRENT_0)
			current[0] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_90)
			current[1] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_180)
			current[0] -= 1.0;
		if (pm->water_type & CONTENTS_CURRENT_270)
			current[1] -= 1.0;
		if (pm->water_type & CONTENTS_CURRENT_UP)
			current[2] += 1.0;
		if (pm->water_type & CONTENTS_CURRENT_DOWN)
			current[2] -= 1.0;
	}

	// add conveyer belt velocities
	if (pm->ground_entity) {
		if (pml.ground_contents & CONTENTS_CURRENT_0)
			current[0] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_90)
			current[1] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_180)
			current[0] -= 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_270)
			current[1] -= 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_UP)
			current[2] += 1.0;
		if (pml.ground_contents & CONTENTS_CURRENT_DOWN)
			current[2] -= 1.0;
	}

	VectorMA(vel, PM_SPEED_CURRENT, current, vel);
}

/**
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

	if (pm->s.flags & PMF_JUMP_HELD)
		return false;

	if (pm->s.flags & PMF_TIME_MASK)
		return false;

	return true;
}

/**
 * @brief Shift around the current origin to find a valid position.
 */
static void Pm_CorrectPosition(void) {

	const cm_trace_t tr = pm->Trace(pm->s.origin, pm->s.origin, pm->mins, pm->maxs);
	if (tr.all_solid) {

		Pm_Debug("all solid %s\n", vtos(pm->s.origin));

		for (int32_t i = -1; i <= 1; i++) {
			for (int32_t j = -1; j <= 1; j++) {
				for (int32_t k = -1; k <= 1; k++) {
					vec3_t pos;
					VectorCopy(pm->s.origin, pos);

					pos[0] += i * PM_NUDGE_DIST;
					pos[1] += j * PM_NUDGE_DIST;
					pos[2] += k * PM_NUDGE_DIST;

					const cm_trace_t tr = pm->Trace(pos, pos, pm->mins, pm->maxs);
					if (!tr.all_solid) {
						VectorCopy(pos, pm->s.origin);
						return;
					}
				}
			}
		}

		VectorCopy(pml.previous_origin, pm->s.origin);

		Pm_Debug("still solid, reverted to %s\n", vtos(pm->s.origin));
	}
}

/**
 * @brief Checks for ground interaction, enabling trick jumping and dealing with landings.
 */
static void Pm_CheckGround(void) {
	vec3_t pos;

	// if we jumped, or been pushed, do not attempt to seek ground
	if (pm->s.flags & (PMF_JUMPED | PMF_TIME_PUSHED)) {
		return;
	}

	// seek ground eagerly if the player wishes to trick jump
	const _Bool trick_jump = Pm_CheckTrickJump();
	if (trick_jump) {
		VectorMA(pm->s.origin, pml.time, pm->s.velocity, pos);
		pos[2] -= PM_GROUND_DIST_TRICK;
	} else {
		VectorCopy(pm->s.origin, pos);
		pos[2] -= PM_GROUND_DIST;
	}

	// seek the ground
	cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	pml.ground_plane = trace.plane;
	pml.ground_surface = trace.surface;
	pml.ground_contents = trace.contents;

	// if we hit an upward facing plane, make it our ground
	if (trace.ent && trace.plane.normal[2] >= PM_STEP_NORMAL) {

		// if we had no ground, then handle landing events
		if (!pm->ground_entity) {

			// any landing terminates the water jump
			if (pm->s.flags & PMF_TIME_WATER_JUMP) {
				pm->s.flags &= ~PMF_TIME_WATER_JUMP;
				pm->s.time = 0;
			}

			// hard landings disable jumping briefly
			if (pml.previous_velocity[2] <= PM_SPEED_LAND) {
				pm->s.flags |= PMF_TIME_LAND;
				pm->s.time = 1;

				if (pml.previous_velocity[2] <= PM_SPEED_FALL) {
					pm->s.time = 16;

					if (pml.previous_velocity[2] <= PM_SPEED_FALL_FAR) {
						pm->s.time = 256;
					}
				}
			} else { // soft landings with upward momentum grant trick jumps
				if (trick_jump) {
					pm->s.flags |= PMF_TIME_TRICK_JUMP;
					pm->s.time = 32;
				}
			}
		}

		// save a reference to the ground
		pm->s.flags |= PMF_ON_GROUND;
		pm->ground_entity = trace.ent;

		// and sink down to it if not trick jumping
		if (!(pm->s.flags & PMF_TIME_TRICK_JUMP)) {
			Pm_StepDown(&trace);
		}
	} else {
		pm->s.flags &= ~PMF_ON_GROUND;
		pm->ground_entity = NULL;
	}

	// always touch the entity, even if we couldn't stand on it
	Pm_TouchEntity(trace.ent);
}

/**
 * @brief Checks for water interaction, accounting for player ducking, etc.
 */
static void Pm_CheckWater(void) {
	vec3_t pos;

	pm->water_level = pm->water_type = 0;

	VectorCopy(pm->s.origin, pos);
	pos[2] = pm->s.origin[2] + pm->mins[2] + PM_GROUND_DIST;

	int32_t contents = pm->PointContents(pos);
	if (contents & MASK_LIQUID) {

		pm->water_type = contents;
		pm->water_level = 1;

		pos[2] = pm->s.origin[2];

		contents = pm->PointContents(pos);

		if (contents & MASK_LIQUID) {

			pm->water_type |= contents;
			pm->water_level = 2;

			pos[2] = pm->s.origin[2] + pml.view_offset[2] + 1.0;

			contents = pm->PointContents(pos);

			if (contents & MASK_LIQUID) {
				pm->water_type |= contents;
				pm->water_level = 3;

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
		pml.view_offset[2] = 0.0;
	} else {

		if (pm->cmd.up < 0) {
			pm->s.flags |= PMF_DUCKED;
		}

		const vec_t height = pm->maxs[2] - pm->mins[2];

		if (pm->s.flags & PMF_DUCKED) { // ducked, reduce height
			vec_t target = pm->mins[2] + height * 0.5;

			if (pml.view_offset[2] > target) // go down
				pml.view_offset[2] -= pml.time * PM_SPEED_DUCK_STAND;

			if (pml.view_offset[2] < target)
				pml.view_offset[2] = target;

			// change the bounding box to reflect ducking
			pm->maxs[2] = pm->maxs[2] + pm->mins[2] * 0.5;
		} else {
			const vec_t target = pm->mins[2] + height * 0.9;

			if (pml.view_offset[2] < target) // go up
				pml.view_offset[2] += pml.time * PM_SPEED_DUCK_STAND;

			if (pml.view_offset[2] > target)
				pml.view_offset[2] = target;
		}
	}

	PackVector(pml.view_offset, pm->s.view_offset);
}

/**
 * @brief Check for jumping and trick jumping.
 *
 * @return True if a jump occurs, false otherwise.
 */
static _Bool Pm_CheckJump(void) {

	// must wait for landing damage to subside
	if (pm->s.flags & PMF_TIME_LAND)
		return false;

	// must wait for jump key to be released
	if (pm->s.flags & PMF_JUMP_HELD)
		return false;

	// didn't ask to jump
	if (pm->cmd.up < 1)
		return false;

	// finally, do the jump
	vec_t jump = PM_SPEED_JUMP;

	// factoring in water level
	if (pm->water_level > 1) {
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

	if (pm->s.velocity[2] < 0.0) {
		pm->s.velocity[2] = jump;
	} else {
		pm->s.velocity[2] += jump;
	}

	// indicate that jump is currently held
	pm->s.flags |= (PMF_JUMPED | PMF_JUMP_HELD);

	// clear the ground indicators
	pm->s.flags &= ~PMF_ON_GROUND;
	pm->ground_entity = NULL;

	return true;
}

/**
 * @brief Check for ladder interaction.
 *
 * @return True if the player is on a ladder, false otherwise.
 */
static void Pm_CheckLadder(void) {
	vec3_t forward, pos;

	if (pm->s.flags & PMF_TIME_MASK)
		return;

	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);

	VectorMA(pm->s.origin, 1.0, forward, pos);

	const cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER)) {
		pm->s.flags |= PMF_ON_LADDER;

		pm->ground_entity = NULL;
		pm->s.flags &= ~PMF_ON_GROUND;
	}
}

/**
 * @brief Checks for water exit. The player may exit the water when they can
 * see a usable step out of the water.
 *
 * @return True if a water jump has occurred, false otherwise.
 */
static _Bool Pm_CheckWaterJump(void) {
	vec3_t pos, pos2;

	if (pm->s.flags & PMF_TIME_WATER_JUMP)
		return false;

	if (pm->water_level != 2)
		return false;

	if (pm->cmd.up < 1 && pm->cmd.forward < 1)
		return false;

	VectorMA(pm->s.origin, 16.0, pml.forward, pos);

	cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & MASK_SOLID)) {

		pos[2] += PM_STEP_HEIGHT + pm->maxs[2] - pm->mins[2];

		trace = pm->Trace(pos, pos, pm->mins, pm->maxs);

		if (trace.start_solid) {
			Pm_Debug("Can't exit water: blocked\n");
			return false;
		}

		VectorSet(pos2, pos[0], pos[1], pm->s.origin[2]);

		trace = pm->Trace(pos, pos2, pm->mins, pm->maxs);

		if (!(trace.ent && trace.plane.normal[2] >= PM_STEP_NORMAL)) {
			Pm_Debug("Can't exit water: not a step\n");
			return false;
		}

		// jump out of water
		pm->s.velocity[2] = PM_SPEED_WATER_JUMP;

		pm->s.flags |= PMF_TIME_WATER_JUMP | PMF_JUMP_HELD;
		pm->s.time = 2000;

		return true;
	}

	return false;
}

/**
 * @brief
 */
static void Pm_LadderMove(void) {
	vec3_t vel, dir;

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();

	// user intentions in X/Y
	for (int32_t i = 0; i < 2; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	const vec_t s = PM_SPEED_LADDER * 0.125;

	// limit horizontal speed when on a ladder
	vel[0] = Clamp(vel[0], -s, s);
	vel[1] = Clamp(vel[1], -s, s);
	vel[2] = 0.0;

	// handle Z intentions differently
	if (fabsf(pm->s.velocity[2]) < PM_SPEED_LADDER) {

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
	}

	if (pm->cmd.up > 0) { // avoid jumps when exiting ladders
		pm->s.flags |= PMF_JUMP_HELD;
	}

	Pm_Currents(vel);

	VectorCopy(vel, dir);
	vec_t speed = VectorNormalize(dir);

	speed = Clamp(speed, 0.0, PM_SPEED_LADDER);

	if (speed < PM_STOP_EPSILON)
		speed = 0.0;

	Pm_Accelerate(dir, speed, PM_ACCEL_LADDER);

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_WaterJumpMove(void) {
	vec3_t forward;

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();

	Pm_Gravity();

	// check for a usable spot directly in front of us
	VectorCopy(pml.forward, forward);
	forward[2] = 0.0;

	VectorNormalize(forward);
	VectorMA(pm->s.origin, 30.0, forward, forward);

	// if we've reached a usable spot, clamp the jump to avoid launching
	if (pm->Trace(pm->s.origin, forward, pm->mins, pm->maxs).fraction == 1.0) {
		pm->s.velocity[2] = Clamp(pm->s.velocity[2], 0.0, PM_SPEED_JUMP);
	}

	// if we're falling back down, clear the timer to regain control
	if (pm->s.velocity[2] <= 0.0) {
		pm->s.flags &= ~PMF_TIME_MASK;
		pm->s.time = 0;
	}

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_WaterMove(void) {
	vec3_t vel, dir;
	vec_t speed;

	if (Pm_CheckWaterJump()) {
		Pm_WaterJumpMove();
		return;
	}

	Pm_Debug("%s\n", vtos(pm->s.origin));

	// apply friction, slowing rapidly when first entering the water
	VectorCopy(pm->s.velocity, vel);
	speed = VectorLength(vel);

	for (int32_t i = speed / PM_SPEED_WATER; i >= 0; i--) {
		Pm_Friction();
	}

	// and sink if idle
	if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up) {
		if (pm->s.velocity[2] > PM_SPEED_WATER_SINK) {
			Pm_Gravity();
		}
	}

	// user intentions on X/Y/Z
	for (int32_t i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	// add explicit Z
	vel[2] += pm->cmd.up;

	// disable water skiing
	if (pm->water_level == 2) {
		vec3_t view;

		VectorAdd(pm->s.origin, pml.view_offset, view);
		view[2] -= 4.0;

		if (!(pm->PointContents(view) & MASK_LIQUID)) {
			pm->s.velocity[2] = MIN(pm->s.velocity[2], 0.0);
			vel[2] = MIN(vel[2], 0.0);
		}

	}

	Pm_Currents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	speed = Clamp(speed, 0, PM_SPEED_WATER);

	if (speed < PM_STOP_EPSILON)
		speed = 0.0;

	Pm_Accelerate(dir, speed, PM_ACCEL_WATER);

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_AirMove(void) {
	vec3_t vel, dir;
	vec_t speed;

//	Pm_Debug("%s\n", vtos(pm->s.velocity));

	Pm_Friction();

	Pm_Gravity();

	pml.forward[2] = 0.0;
	pml.right[2] = 0.0;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (int32_t i = 0; i < 2; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	vel[2] = 0.0;

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	speed = Clamp(speed, 0.0, PM_SPEED_AIR);

	if (speed < PM_STOP_EPSILON)
		speed = 0.0;

	Pm_Accelerate(dir, speed, PM_ACCEL_AIR);

	Pm_StepSlideMove();
}

/**
 * @brief Called for movements where player is on ground, regardless of water level.
 */
static void Pm_WalkMove(void) {
	vec_t speed, max_speed, accel;
	vec3_t angles, vel, dir;

	if (Pm_CheckJump()) { // jumped away
		if (pm->water_level > 1) {
			Pm_WaterMove();
		} else {
			Pm_AirMove();
		}
		return;
	}

//	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();
	
	// project the desired movement into the X/Y plane
	VectorCopy(pm->angles, angles);
	angles[PITCH] = 0.0;
	
	AngleVectors(angles, pml.forward, pml.right, NULL);
	
	Pm_ClipVelocity(pml.forward, pml.ground_plane.normal, pml.forward, PM_CLIP_BOUNCE);
	Pm_ClipVelocity(pml.right, pml.ground_plane.normal, pml.right, PM_CLIP_BOUNCE);

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (int32_t i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	Pm_Currents(vel);

	VectorCopy(vel, dir);
	speed = VectorNormalize(dir);

	// clamp to max speed
	if (pm->water_level > 1) {
		max_speed = PM_SPEED_WATER;
	} else if (pm->s.flags & PMF_DUCKED) {
		max_speed = PM_SPEED_DUCKED;
	} else {
		max_speed = PM_SPEED_RUN;
	}

	// accounting for walk modulus
	if (pm->cmd.buttons & BUTTON_WALK) {
		max_speed *= PM_SPEED_MOD_WALK;
	}

	// clamp the speed to min/max speed
	speed = Clamp(speed, 0.0, max_speed);

	if (speed < PM_STOP_EPSILON)
		speed = 0.0;

	// accelerate based on slickness of ground surface
	accel = (pml.ground_surface->flags & SURF_SLICK) ? PM_ACCEL_GROUND_SLICK : PM_ACCEL_GROUND;

	Pm_Accelerate(dir, speed, accel);

	// determine the speed after acceleration
	speed = VectorLength(pm->s.velocity);

	// clip to the ground
	Pm_ClipVelocity(pm->s.velocity, pml.ground_plane.normal, pm->s.velocity, PM_CLIP_BOUNCE);

	// and now scale by the speed to avoid slowing down on slopes
	VectorNormalize(pm->s.velocity);
	VectorScale(pm->s.velocity, speed, pm->s.velocity);

	// and finally, step if moving in X/Y
	if (pm->s.velocity[0] || pm->s.velocity[1]) {
		Pm_StepSlideMove();
	}
}

/**
 * @brief
 */
static void Pm_ClampAngles(void) {

	// copy the command angles into the outgoing state
	VectorCopy(pm->cmd.angles, pm->s.view_angles);

	// circularly clamp the angles with kick and deltas
	for (int32_t i = 0; i < 3; i++) {

		const int16_t c = pm->cmd.angles[i];
		const int16_t k = pm->s.kick_angles[i];
		const int16_t d = pm->s.delta_angles[i];

		pm->angles[i] = UnpackAngle(c + k + d);
	}

	// clamp pitch to prevent the player from looking up or down more than 90
	if (pm->angles[PITCH] > 90.0 && pm->angles[PITCH] < 270.0) {
		pm->angles[PITCH] = 90.0;
	} else if (pm->angles[PITCH] <= 360.0 && pm->angles[PITCH] >= 270.0) {
		pm->angles[PITCH] -= 360.0;
	}

	// calculate the directional vectors for this move
	AngleVectors(pm->angles, pml.forward, pml.right, pml.up);
}

/**
 * @brief
 */
static void Pm_SpectatorMove() {
	vec3_t vel;

	Pm_Friction();

	// user intentions on X/Y/Z
	for (int32_t i = 0; i < 3; i++) {
		vel[i] = pml.forward[i] * pm->cmd.forward + pml.right[i] * pm->cmd.right;
	}

	// add explicit Z
	vel[2] += pm->cmd.up;

	vec_t speed = VectorNormalize(vel);
	speed = Clamp(speed, 0.0, PM_SPEED_SPECTATOR);

	if (speed < PM_STOP_EPSILON)
		speed = 0.0;

	// accelerate
	Pm_Accelerate(vel, speed, PM_ACCEL_SPECTATOR);

	// do the move
	VectorMA(pm->s.origin, pml.time, pm->s.velocity, pm->s.origin);
}

/**
 * @brief
 */
static void Pm_Init(void) {

	// set the default bounding box
	if (pm->s.type == PM_DEAD) {

		if (pm->s.flags & PMF_GIBLET) {
			VectorCopy(PM_GIBLET_MINS, pm->mins);
			VectorCopy(PM_GIBLET_MAXS, pm->maxs);
		} else {
			VectorScale(PM_DEAD_MINS, PM_SCALE, pm->mins);
			VectorScale(PM_DEAD_MAXS, PM_SCALE, pm->maxs);
		}
	}
	else {
		VectorScale(PM_MINS, PM_SCALE, pm->mins);
		VectorScale(PM_MAXS, PM_SCALE, pm->maxs);
	}

	VectorClear(pm->angles);

	pm->num_touch_ents = 0;
	pm->water_type = pm->water_level = 0;

	pm->step = 0.0;

	// reset flags that we test each move
	pm->s.flags &= ~(PMF_NO_PREDICTION);
	pm->s.flags &= ~(PMF_ON_GROUND | PMF_ON_STAIRS | PMF_ON_LADDER);
	pm->s.flags &= ~(PMF_DUCKED | PMF_JUMPED | PMF_UNDER_WATER);

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
 * @brief
 */
static void Pm_InitLocal(void) {

	memset(&pml, 0, sizeof(pml));

	// save previous values in case move fails, and to detect landings
	VectorCopy(pm->s.origin, pml.previous_origin);
	VectorCopy(pm->s.velocity, pml.previous_velocity);

	// convert view offset to float point
	UnpackVector(pm->s.view_offset, pml.view_offset);

	// convert from milliseconds to seconds
	pml.time = pm->cmd.msec * 0.001;
}

/**
 * @brief Called by the game and the client game to update the player's
 * authoritative or predicted movement state, respectively.
 */
void Pm_Move(pm_move_t *pm_move) {
	pm = pm_move;

	Pm_Init();

	Pm_InitLocal();

	Pm_ClampAngles();

	if (pm->s.type == PM_FREEZE) { // no movement
		return;
	}

	if (pm->s.type == PM_SPECTATOR) { // no interaction
		Pm_SpectatorMove();
		return;
	}

	if (pm->s.type == PM_DEAD) { // no control
		pm->cmd.forward = pm->cmd.right = pm->cmd.up = 0;
	}

	// check for ducking
	Pm_CheckDuck();

	// check for water level, water type
	Pm_CheckWater();

	// check for ground
	Pm_CheckGround();

	// check for ladders
	Pm_CheckLadder();

	if (pm->s.flags & PMF_TIME_TELEPORT) {
		// pause in place briefly
	} else if (pm->s.flags & PMF_TIME_WATER_JUMP) {
		Pm_WaterJumpMove();
	} else if (pm->s.flags & PMF_ON_LADDER) {
		Pm_LadderMove();
	} else if (pm->s.flags & PMF_ON_GROUND) {
		Pm_WalkMove();
	} else if (pm->water_level > 1) {
		Pm_WaterMove();
	} else {
		Pm_AirMove();
	}

	// ensure we're in a valid spot, or revert
	Pm_CorrectPosition();

	// check for ground at new spot
	Pm_CheckGround();

	// check for water level, water type at new spot
	Pm_CheckWater();
}

