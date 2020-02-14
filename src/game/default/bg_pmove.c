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
const vec3_t PM_MINS = { { -16.f, -16.f, -24.f } };
const vec3_t PM_MAXS = { {  16.f,  16.f,  32.f } };

static const vec3_t PM_DEAD_MINS = { { -16.f, -16.f, -24.f } };
static const vec3_t PM_DEAD_MAXS = { {  16.f,  16.f,  -4.f } };

static const vec3_t PM_GIBLET_MINS = { { -8.f, -8.f, -8.f } };
static const vec3_t PM_GIBLET_MAXS = { {  8.f,  8.f,  8.f } };

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

	// directional vectors based on command angles
	vec3_t forward, right, up;

	// directional vectors without Z component
	vec3_t forward_xy, right_xy;

	float time; // the command duration in seconds

	// ground interactions
	cm_bsp_texinfo_t *ground_surface;
	cm_bsp_plane_t ground_plane;
	int32_t ground_contents;

} pm_locals_t;

static pm_locals_t pml;

#define Pm_Debug(...) pm->Debug(__func__, __VA_ARGS__)

/**
 * @brief Slide off of the impacted plane.
 */
static vec3_t Pm_ClipVelocity(const vec3_t in, const vec3_t normal, float bounce) {

	float backoff = Vec3_Dot(in, normal);

	if (backoff < 0.0) {
		backoff *= bounce;
	} else {
		backoff /= bounce;
	}

	return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
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

/**
 * @return True if `plane` is unique to `planes` and should be impacted, false otherwise.
 */
static _Bool Pm_ImpactPlane(vec3_t *planes, int32_t num_planes, const vec3_t plane) {

	for (int32_t i = 0 ; i < num_planes; i++) {
		if (Vec3_Dot(plane, planes[i]) > 1.0 - PM_STOP_EPSILON) {
			return false;
		}
	}

	return true;
}

#define MAX_CLIP_PLANES	6

/**
 * @brief Calculates a new origin, velocity, and contact entities based on the
 * movement command and world state. Returns true if not blocked.
 */
static _Bool Pm_SlideMove(void) {
	vec3_t planes[MAX_CLIP_PLANES];
	int32_t bump, num_bumps = MAX_CLIP_PLANES - 2;

	float time_remaining = pml.time;
	int32_t num_planes = 0;

	// never turn against our ground plane
	if (pm->s.flags & PMF_ON_GROUND) {
		planes[num_planes] = pml.ground_plane.normal;
		num_planes++;
	}

	// or our original velocity
	planes[num_planes] = Vec3_Normalize(pm->s.velocity);
	num_planes++;

	for (bump = 0; bump < num_bumps; bump++) {
		vec3_t pos;

		if (time_remaining <= 0.0) { // out of time
			break;
		}

		// project desired destination
		pos = Vec3_Add(pm->s.origin, Vec3_Scale(pm->s.velocity, time_remaining));

		// trace to it
		const cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

		// if the player is trapped in a solid, don't build up Z
		if (trace.all_solid) {
			pm->s.velocity.z = 0.0;
			return true;
		}

		// if the trace succeeded, move some distance
		if (trace.fraction > 0.0) {

			pm->s.origin = trace.end;

			// if the trace didn't hit anything, we're done
			if (trace.fraction == 1.0) {
				break;
			}

			// update the movement time remaining
			time_remaining -= (time_remaining * trace.fraction);
		}

		// store a reference to the entity for firing game events
		Pm_TouchEntity(trace.ent);

		// record the impacted plane, or nudge velocity out along it
		if (Pm_ImpactPlane(planes, num_planes, trace.plane.normal)) {
			planes[num_planes] = trace.plane.normal;
			num_planes++;
		} else {
			// if we've seen this plane before, nudge our velocity out along it
			pm->s.velocity = Vec3_Add(pm->s.velocity, trace.plane.normal);
			continue;
		}

		// and modify velocity, clipping to all impacted planes
		for (int32_t i = 0; i < num_planes; i++) {
			vec3_t vel;

			// if velocity doesn't impact this plane, skip it
			if (Vec3_Dot(pm->s.velocity, planes[i]) >= 0.0) {
				continue;
			}

			// slide along the plane
			vel = Pm_ClipVelocity(pm->s.velocity, planes[i], PM_CLIP_BOUNCE);

			// see if there is a second plane that the new move enters
			for (int32_t j = 0; j < num_planes; j++) {
				vec3_t cross;

				if (j == i) {
					continue;
				}

				// if the clipped velocity doesn't impact this plane, skip it
				if (Vec3_Dot(vel, planes[j]) >= 0.0) {
					continue;
				}

				// we are now intersecting a second plane
				vel = Pm_ClipVelocity(vel, planes[j], PM_CLIP_BOUNCE);

				// but if we clip against it without being deflected back, we're okay
				if (Vec3_Dot(vel, planes[i]) >= 0.0) {
					continue;
				}

				// we must now slide along the crease (cross product of the planes)
				cross = Vec3_Cross(planes[i], planes[j]);
				cross = Vec3_Normalize(cross);

				const float scale = Vec3_Dot(cross, pm->s.velocity);
				vel = Vec3_Scale(cross, scale);

				// see if there is a third plane the the new move enters
				for (int32_t k = 0; k < num_planes; k++) {

					if (k == i || k == j) {
						continue;
					}

					if (Vec3_Dot(vel, planes[k]) >= 0.0) {
						continue;
					}

					// stop dead at a triple plane interaction
					pm->s.velocity = Vec3_Zero();
					return true;
				}
			}

			// if we have fixed all interactions, try another move
			pm->s.velocity = vel;
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
		if (trace->ent && trace->plane.normal.z >= PM_STEP_NORMAL) {
			if (trace->ent != pm->ground_entity || trace->plane.dist != pml.ground_plane.dist) {
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

	pm->s.origin = trace->end;
	pm->s.origin.z += PM_STOP_EPSILON;

	pm->step = pm->s.origin.z - pml.previous_origin.z;

	if (pm->step >= PM_STEP_HEIGHT_MIN) {
		Pm_Debug("step up %3.2f\n", pm->step);
		pm->s.flags |= PMF_ON_STAIRS;
	} else if (pm->step <= -PM_STEP_HEIGHT_MIN && (pm->s.flags & PMF_ON_GROUND)) {
		Pm_Debug("step down %3.2f\n", pm->step);
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
	org = pm->s.origin;
	vel = pm->s.velocity;

	if (Pm_SlideMove()) { // we weren't blocked, we're done
		return;
	}

	if (pm->s.flags & PMF_ON_LADDER) { // we're on a ladder, we're done
		return;
	}

	// don't step up if we still have upward velocity, and there's no ground

	down = vec3_add(org, Vec3_Scale(vec3_down(), PM_STEP_HEIGHT + PM_GROUND_DIST));
	cm_trace_t trace = pm->Trace(org, down, pm->mins, pm->maxs);
	if (pm->s.velocity.z > PM_SPEED_UP && (trace.ent == NULL || trace.plane.normal.z < PM_STEP_NORMAL)) {
		return;
	}

	// try to step up

	up = vec3_add(org, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT));
	trace = pm->Trace(org, up, pm->mins, pm->maxs);
	if (trace.all_solid) {
		return;
	}

	pm->s.origin = trace.end;
	pm->s.velocity = vel;

	Pm_SlideMove();

	// settle to the new ground

	down = vec3_add(pm->s.origin, Vec3_Scale(vec3_down(), PM_STEP_HEIGHT + PM_GROUND_DIST));
	trace = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);
	if (!trace.all_solid) {
		pm->s.origin = trace.end;
	}

	if (trace.fraction < 1.0) {
		pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, trace.plane.normal, PM_CLIP_BOUNCE);
	}

	// set the step for interpolation

	pm->step = pm->s.origin.z - org.z;
	if (fabs(pm->step) >= PM_STEP_HEIGHT_MIN) {
		pm->s.flags |= PMF_ON_STAIRS;
	} else {
		pm->step = 0.0;
	}

#else

	vec3_t org0, vel0;
	vec3_t org1, vel1;
	vec3_t down, up;

	org0 = pm->s.origin;
	vel0 = pm->s.velocity;

	// attempt to move; if nothing blocks us, we're done
	if (Pm_SlideMove()) {

		// attempt to step down to remain on ground
		if ((pm->s.flags & PMF_ON_GROUND) && pm->cmd.up <= 0) {

			down = Vec3_Add(pm->s.origin, Vec3_Scale(Vec3_Down(), PM_STEP_HEIGHT + PM_GROUND_DIST));
			const cm_trace_t step_down = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);

			if (Pm_CheckStep(&step_down)) {
				Pm_StepDown(&step_down);
			}
		}

		return;
	}

	// we were blocked, so try to step over the obstacle

	org1 = pm->s.origin;
	vel1 = pm->s.velocity;

	up = Vec3_Add(org0, Vec3_Scale(Vec3_Up(), PM_STEP_HEIGHT));
	const cm_trace_t step_up = pm->Trace(org0, up, pm->mins, pm->maxs);
	if (!step_up.all_solid) {

		// step from the higher position, with the original velocity
		pm->s.origin = step_up.end;
		pm->s.velocity = vel0;

		Pm_SlideMove();

		// settle to the new ground, keeping the step if and only if it was successful
		down = Vec3_Add(pm->s.origin, Vec3_Scale(Vec3_Down(), PM_STEP_HEIGHT + PM_GROUND_DIST));
		const cm_trace_t step_down = pm->Trace(pm->s.origin, down, pm->mins, pm->maxs);

		if (Pm_CheckStep(&step_down)) {
			// Quake2 trick jump secret sauce
			if ((pm->s.flags & PMF_ON_GROUND) || vel0.z < PM_SPEED_UP) {
				Pm_StepDown(&step_down);
			} else {
				pm->step = pm->s.origin.z - pml.previous_origin.z;
				pm->s.flags |= PMF_ON_STAIRS;
			}

			return;
		}
	}

	pm->s.origin = org1;
	pm->s.velocity = vel1;

#endif
}

/**
 * @brief Handles friction against user intentions, and based on contents.
 */
static void Pm_Friction(void) {
	vec3_t vel;

	vel = pm->s.velocity;

	if (pm->s.flags & PMF_ON_GROUND) {
		vel.z = 0.0;
	}

	const float speed = Vec3_Length(vel);

	if (speed < 1.0) {
		pm->s.velocity.x = 0.0;
		pm->s.velocity.y = 0.0;
		return;
	}

	const float control = Maxf(PM_SPEED_STOP, speed);

	float friction = 0.0;

	if (pm->s.type == PM_SPECTATOR) { // spectator friction
		friction = PM_FRICT_SPECTATOR;
	} else { // ladder, ground, water, air and friction
		if (pm->s.flags & PMF_ON_LADDER) {
			friction = PM_FRICT_LADDER;
		} else {
			if (pm->water_level > WATER_FEET) {
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
	float scale = Maxf(0.0, speed - (friction * control * pml.time)) / speed;

	pm->s.velocity = Vec3_Scale(pm->s.velocity, scale);
}

/**
 * @brief Handles user intended acceleration.
 */
static void Pm_Accelerate(const vec3_t dir, float speed, float accel) {

	const float current_speed = Vec3_Dot(pm->s.velocity, dir);
	const float add_speed = speed - current_speed;

	if (add_speed <= 0.0) {
		return;
	}

	float accel_speed = accel * pml.time * speed;

	if (accel_speed > add_speed) {
		accel_speed = add_speed;
	}

	pm->s.velocity = Vec3_Add(pm->s.velocity, Vec3_Scale(dir, accel_speed));
}

/**
 * @brief Applies gravity to the current movement.
 */
static void Pm_Gravity(void) {

	if (pm->s.type == PM_HOOK_PULL) {
		return;
	}

	float gravity = pm->s.gravity;

	if (pm->water_level > WATER_WAIST) {
		gravity *= PM_GRAVITY_WATER;
	}

	pm->s.velocity.z -= gravity * pml.time;
}

/**
 * @brief
 */
static void Pm_Currents(void) {
	vec3_t current = Vec3_Zero();

	// add water currents
	if (pm->water_level) {
		if (pm->water_type & CONTENTS_CURRENT_0) {
			current.x += 1.0;
		}
		if (pm->water_type & CONTENTS_CURRENT_90) {
			current.y += 1.0;
		}
		if (pm->water_type & CONTENTS_CURRENT_180) {
			current.x -= 1.0;
		}
		if (pm->water_type & CONTENTS_CURRENT_270) {
			current.y -= 1.0;
		}
		if (pm->water_type & CONTENTS_CURRENT_UP) {
			current.z += 1.0;
		}
		if (pm->water_type & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.0;
		}
	}

	// add conveyer belt velocities
	if (pm->ground_entity) {
		if (pml.ground_contents & CONTENTS_CURRENT_0) {
			current.x += 1.0;
		}
		if (pml.ground_contents & CONTENTS_CURRENT_90) {
			current.y += 1.0;
		}
		if (pml.ground_contents & CONTENTS_CURRENT_180) {
			current.x -= 1.0;
		}
		if (pml.ground_contents & CONTENTS_CURRENT_270) {
			current.y -= 1.0;
		}
		if (pml.ground_contents & CONTENTS_CURRENT_UP) {
			current.z += 1.0;
		}
		if (pml.ground_contents & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.0;
		}
	}

	if (!Vec3_Equal(current, Vec3_Zero())) {
		current = Vec3_Normalize(current);
	}

	pm->s.velocity = Vec3_Add(pm->s.velocity, Vec3_Scale(current, PM_SPEED_CURRENT));
}

/**
 * @return True if the player will be eligible for trick jumping should they
 * impact the ground on this frame, false otherwise.
 */
static _Bool Pm_CheckTrickJump(void) {

	if (pm->ground_entity) {
		return false;
	}

	if (pml.previous_velocity.z < PM_SPEED_UP) {
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
 * @brief This function is designed to keep the player from getting too close to
 * other planes. If the player is close enough to another wall or box, it will adjust
 * the position by shoving them away from it at a certain distance.
 */
static void Pm_SnapToWalls(void) {

	const vec3_t dirs[] = {
		Vec3( 1.0,  0.0,  0.0 ),
		Vec3(-1.0,  0.0,  0.0 ),
		Vec3( 0.0,  1.0,  0.0 ),
		Vec3( 0.0, -1.0,  0.0 )
	};

	for (uint32_t i = 0; i < lengthof(dirs); i++) {

		vec3_t end;
		end = Vec3_Add(pm->s.origin, Vec3_Scale(dirs[i], PM_SNAP_DISTANCE));

		const cm_trace_t tr = pm->Trace(pm->s.origin, end, pm->mins, pm->maxs);
		if (tr.fraction < 1.0) {
			if (tr.plane.normal.z < PM_STEP_NORMAL &&
			        tr.plane.normal.z > -PM_STEP_NORMAL) {
				pm->s.origin = Vec3_Add(tr.end, Vec3_Scale(dirs[i], -PM_SNAP_DISTANCE));
			}
		}
	}
}

/**
 * @brief Shift around the current origin to find a valid position.
 */
static void Pm_CorrectPosition(void) {

	cm_trace_t tr = pm->Trace(pm->s.origin, pm->s.origin, pm->mins, pm->maxs);
	if (tr.all_solid) {

		Pm_Debug("all solid %s\n", vtos(pm->s.origin));

		for (int32_t i = -1; i <= 1; i++) {
			for (int32_t j = -1; j <= 1; j++) {
				for (int32_t k = -1; k <= 1; k++) {
					vec3_t pos;
					pos = pm->s.origin;

					pos.x += i * PM_NUDGE_DIST;
					pos.y += j * PM_NUDGE_DIST;
					pos.z += k * PM_NUDGE_DIST;

					tr = pm->Trace(pml.previous_origin, pos, pm->mins, pm->maxs);
					if (tr.fraction == 1.0) {
						pm->s.origin = pos;
						Pm_Debug("corrected %s\n", vtos(pm->s.origin));
						return;
					}
				}
			}
		}

		pm->s.origin = pml.previous_origin;

		Pm_Debug("still solid, reverted to %s\n", vtos(pm->s.origin));
	}
}

/**
 * @return True if the player is attempting to leave the ground via grappling hook.
 */
static bool Pm_CheckHookJump(void) {

	if ((pm->s.type == PM_HOOK_PULL || pm->s.type == PM_HOOK_SWING) && pm->s.velocity.z > 4.0) {

		pm->s.flags &= ~PMF_ON_GROUND;
		pm->ground_entity = NULL;

		return true;
	}

	return false;
}

/**
 * @brief
 */
static void Pm_CheckHook(void) {

	// hookers only
	if (pm->s.type != PM_HOOK_PULL && pm->s.type != PM_HOOK_SWING) {
		pm->s.flags &= ~PMF_HOOK_RELEASED;
		return;
	}

	// get chain length
	if (pm->s.type == PM_HOOK_PULL) {

		// if we let go of hook, just go back to normal
		if (!(pm->cmd.buttons & BUTTON_HOOK)) {
			pm->s.type = PM_NORMAL;
			return;
		}

		pm->cmd.forward = pm->cmd.right = 0;

		// pull physics
		const float dist = Vec3_DistanceDir(pm->s.hook_position, pm->s.origin, &pm->s.velocity);
		if (dist > 8.0 && !Pm_CheckHookJump()) {
			pm->s.velocity = Vec3_Scale(pm->s.velocity, pm->hook_pull_speed);
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

		const float hook_rate = (pm->hook_pull_speed / 1.5) * pml.time;

		// chain physics
		// grow/shrink chain based on input
		if ((pm->cmd.up > 0 || !(pm->s.flags & PMF_HOOK_RELEASED)) && (pm->s.hook_length > PM_HOOK_MIN_DIST)) {
			pm->s.hook_length = Maxf(pm->s.hook_length - hook_rate, PM_HOOK_MIN_DIST);
		} else if ((pm->cmd.up < 0) && (pm->s.hook_length < PM_HOOK_MAX_DIST)) {
			pm->s.hook_length = Minf(pm->s.hook_length + hook_rate, PM_HOOK_MAX_DIST);
		}

		vec3_t chain_vec;
		float chain_len, force = 0.0;

		chain_vec = Vec3_Subtract(pm->s.hook_position, pm->s.origin);
		chain_len = Vec3_Length(chain_vec);

		// if player's location is beyond the chain's reach
		if (chain_len > pm->s.hook_length) {
			vec3_t vel_part;

			// determine player's velocity component of chain vector
			vel_part = Vec3_Scale(chain_vec, Vec3_Dot(pm->s.velocity, chain_vec) / Vec3_Dot(chain_vec, chain_vec));

			// restrainment default force
			force = (chain_len - pm->s.hook_length) * 5.0;

			// if player's velocity heading is away from the hook
			if (Vec3_Dot(pm->s.velocity, chain_vec) < 0.0) {

				// if chain has streched for 24 units
				if (chain_len > pm->s.hook_length + 24.0) {

					// remove player's velocity component moving away from hook
					pm->s.velocity = Vec3_Subtract(pm->s.velocity, vel_part);
				}
			} else { // if player's velocity heading is towards the hook

				if (Vec3_Length(vel_part) < force) {
					force -= Vec3_Length(vel_part);
				} else {
					force = 0.0;
				}
			}
		}

		// applies chain restrainment
		chain_vec = Vec3_Normalize(chain_vec);
		pm->s.velocity = Vec3_Add(pm->s.velocity, Vec3_Scale(chain_vec, force));
	}
}

/**
 * @brief Checks for ground interaction, enabling trick jumping and dealing with landings.
 */
static void Pm_CheckGround(void) {
	vec3_t pos;

	if (Pm_CheckHookJump()) {
		return;
	}

	// if we jumped, or been pushed, do not attempt to seek ground
	if (pm->s.flags & (PMF_JUMPED | PMF_TIME_PUSHED | PMF_ON_LADDER)) {
		return;
	}

	// seek ground eagerly if the player wishes to trick jump
	const _Bool trick_jump = Pm_CheckTrickJump();
	if (trick_jump) {
		pos = Vec3_Add(pm->s.origin, Vec3_Scale(pm->s.velocity, pml.time));
		pos.z -= PM_GROUND_DIST_TRICK;
	} else {
		pos = pm->s.origin;
		pos.z -= PM_GROUND_DIST;
	}

	// seek the ground
	cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	pml.ground_plane = trace.plane;
	pml.ground_surface = trace.surface;
	pml.ground_contents = trace.contents;

	// if we hit an upward facing plane, make it our ground
	if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {

		// if we had no ground, then handle landing events
		if (!pm->ground_entity) {

			// any landing terminates the water jump
			if (pm->s.flags & PMF_TIME_WATER_JUMP) {
				pm->s.flags &= ~PMF_TIME_WATER_JUMP;
				pm->s.time = 0;
			}

			// hard landings disable jumping briefly
			if (pml.previous_velocity.z <= PM_SPEED_LAND) {
				pm->s.flags |= PMF_TIME_LAND;
				pm->s.time = 1;

				if (pml.previous_velocity.z <= PM_SPEED_FALL) {
					pm->s.time = 16;

					if (pml.previous_velocity.z <= PM_SPEED_FALL_FAR) {
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
			pm->s.origin = trace.end;
			pm->s.origin.z += PM_STOP_EPSILON;

			pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, trace.plane.normal, PM_CLIP_BOUNCE);
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

	pm->water_level = WATER_NONE;
	pm->water_type = 0;

	pos = pm->s.origin;
	pos.z = pm->s.origin.z + pm->mins.z + PM_GROUND_DIST;

	int32_t contents = pm->PointContents(pos);
	if (contents & MASK_LIQUID) {

		pm->water_type = contents;
		pm->water_level = WATER_FEET;

		pos.z = pm->s.origin.z;

		contents = pm->PointContents(pos);

		if (contents & MASK_LIQUID) {

			pm->water_type |= contents;
			pm->water_level = WATER_WAIST;

			pos.z = pm->s.origin.z + pm->s.view_offset.z + 1.0;

			contents = pm->PointContents(pos);

			if (contents & MASK_LIQUID) {
				pm->water_type |= contents;
				pm->water_level = WATER_UNDER;

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
			pm->s.view_offset.z = 0.0;
		} else {
			pm->s.view_offset.z = -16.0;
		}
	} else {

		_Bool is_ducking = pm->s.flags & PMF_DUCKED;
		_Bool wants_ducking = (pm->cmd.up < 0) && !(pm->s.flags & PMF_ON_LADDER);

		if (!is_ducking && wants_ducking) {
			pm->s.flags |= PMF_DUCKED;
		} else if (is_ducking && !wants_ducking) {
			const cm_trace_t trace = pm->Trace(pm->s.origin, pm->s.origin, pm->mins, pm->maxs);

			if (!trace.all_solid && !trace.start_solid) {
				pm->s.flags &= ~PMF_DUCKED;
			}
		}

		const float height = pm->maxs.z - pm->mins.z;

		if (pm->s.flags & PMF_DUCKED) { // ducked, reduce height
			float target = pm->mins.z + height * 0.5;

			if (pm->s.view_offset.z > target) { // go down
				pm->s.view_offset.z -= pml.time * PM_SPEED_DUCK_STAND;
			}

			if (pm->s.view_offset.z < target) {
				pm->s.view_offset.z = target;
			}

			// change the bounding box to reflect ducking
			pm->maxs.z = pm->maxs.z + pm->mins.z * 0.5;
		} else {
			const float target = pm->mins.z + height * 0.9;

			if (pm->s.view_offset.z < target) { // go up
				pm->s.view_offset.z += pml.time * PM_SPEED_DUCK_STAND;
			}

			if (pm->s.view_offset.z > target) {
				pm->s.view_offset.z = target;
			}
		}
	}

	pm->s.view_offset = pm->s.view_offset;
}

/**
 * @brief Check for jumping and trick jumping.
 *
 * @return True if a jump occurs, false otherwise.
 */
static _Bool Pm_CheckJump(void) {

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
	float jump = PM_SPEED_JUMP;

	// factoring in water level
	if (pm->water_level > WATER_FEET) {
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

	if (pm->s.velocity.z < 0.0) {
		pm->s.velocity.z = jump;
	} else {
		pm->s.velocity.z += jump;
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
	vec3_t pos;

	if (pm->s.flags & PMF_TIME_MASK) {
		return;
	}

	if (pm->s.type == PM_HOOK_PULL) {
		return;
	}

	pos = Vec3_Add(pm->s.origin, Vec3_Scale(pml.forward_xy, 4.0));

	const cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & CONTENTS_LADDER)) {
		pm->s.flags |= PMF_ON_LADDER;

		pm->ground_entity = NULL;
		pm->s.flags &= ~(PMF_ON_GROUND | PMF_DUCKED);
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

	if (pm->s.type == PM_HOOK_PULL ||
	        pm->s.type == PM_HOOK_SWING) {
		return false;
	}

	if (pm->s.flags & PMF_TIME_WATER_JUMP) {
		return false;
	}

	if (pm->water_level != WATER_WAIST) {
		return false;
	}

	if (pm->cmd.up < 1 && pm->cmd.forward < 1) {
		return false;
	}

	pos = Vec3_Add(pm->s.origin, Vec3_Scale(pml.forward, 16.0));

	cm_trace_t trace = pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs);

	if ((trace.fraction < 1.0) && (trace.contents & MASK_SOLID)) {

		pos.z += PM_STEP_HEIGHT + pm->maxs.z - pm->mins.z;

		trace = pm->Trace(pos, pos, pm->mins, pm->maxs);

		if (trace.start_solid) {
			Pm_Debug("Can't exit water: blocked\n");
			return false;
		}

		pos2 = Vec3(pos.x, pos.y, pm->s.origin.z);

		trace = pm->Trace(pos, pos2, pm->mins, pm->maxs);

		if (!(trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL)) {
			Pm_Debug("Can't exit water: not a step\n");
			return false;
		}

		// jump out of water
		pm->s.velocity.z = PM_SPEED_WATER_JUMP;

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

	Pm_Currents();

	// user intentions in X/Y
	vel = Vec3_Zero();
	vel = Vec3_Add(vel, Vec3_Scale(pml.forward_xy, pm->cmd.forward));
	vel = Vec3_Add(vel, Vec3_Scale(pml.right_xy, pm->cmd.right));

	const float s = PM_SPEED_LADDER * 0.125;

	// limit horizontal speed when on a ladder
	vel.x = Clampf(vel.x, -s, s);
	vel.y = Clampf(vel.y, -s, s);
	vel.z = 0.0;

	// handle Z intentions differently
	if (fabsf(pm->s.velocity.z) < PM_SPEED_LADDER) {

		if ((pm->angles.x <= -15.0) && (pm->cmd.forward > 0)) {
			vel.z = PM_SPEED_LADDER;
		} else if ((pm->angles.x >= 15.0) && (pm->cmd.forward > 0)) {
			vel.z = -PM_SPEED_LADDER;
		} else if (pm->cmd.up > 0) {
			vel.z = PM_SPEED_LADDER;
		} else if (pm->cmd.up < 0) {
			vel.z = -PM_SPEED_LADDER;
		} else {
			vel.z = 0.0;
		}
	}

	if (pm->cmd.up > 0) { // avoid jumps when exiting ladders
		pm->s.flags |= PMF_JUMP_HELD;
	}

	float speed;
	dir = Vec3_NormalizeLength(vel, &speed);

	speed = Clampf(speed, 0.0, PM_SPEED_LADDER);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.0;
	}

	Pm_Accelerate(dir, speed, PM_ACCEL_LADDER);

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_WaterJumpMove(void) {
	vec3_t pos;

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();

	Pm_Gravity();

	// check for a usable spot directly in front of us
	pos = Vec3_Add(pm->s.origin, Vec3_Scale(pml.forward_xy, 30.0));

	// if we've reached a usable spot, clamp the jump to avoid launching
	if (pm->Trace(pm->s.origin, pos, pm->mins, pm->maxs).fraction == 1.0) {
		pm->s.velocity.z = Clampf(pm->s.velocity.z, 0.0, PM_SPEED_JUMP);
	}

	// if we're falling back down, clear the timer to regain control
	if (pm->s.velocity.z <= 0.0) {
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
	float speed;

	if (Pm_CheckWaterJump()) {
		Pm_WaterJumpMove();
		return;
	}

	Pm_Debug("%s\n", vtos(pm->s.origin));

	// apply friction, slowing rapidly when first entering the water
	vel = pm->s.velocity;
	speed = Vec3_Length(vel);

	for (int32_t i = speed / PM_SPEED_WATER; i >= 0; i--) {
		Pm_Friction();
	}

	// and sink if idle
	if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up && pm->s.type != PM_HOOK_PULL && pm->s.type != PM_HOOK_SWING) {
		if (pm->s.velocity.z > PM_SPEED_WATER_SINK) {
			Pm_Gravity();
		}
	}

	Pm_Currents();

	// user intentions on X/Y/Z
	vel = Vec3_Add(vel, Vec3_Scale(pml.forward, pm->cmd.forward));
	vel = Vec3_Add(vel, Vec3_Scale(pml.right, pm->cmd.right));

	// add explicit Z
	vel.z += pm->cmd.up;

	// disable water skiing
	if (pm->s.type != PM_HOOK_PULL && pm->s.type != PM_HOOK_SWING) {
		if (pm->water_level == WATER_WAIST) {
			vec3_t view;

			view = Vec3_Add(pm->s.origin, pm->s.view_offset);
			view.z -= 4.0;

			if (!(pm->PointContents(view) & MASK_LIQUID)) {
				pm->s.velocity.z = Minf(pm->s.velocity.z, 0.0);
				vel.z = Minf(vel.z, 0.0);
			}
		}
	}

	dir = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0, PM_SPEED_WATER);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.0;
	}

	Pm_Accelerate(dir, speed, PM_ACCEL_WATER);

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_AirMove(void) {
	vec3_t vel, dir;
	float max_speed, speed, accel;

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();

	Pm_Gravity();

	vel = Vec3_Zero();
	vel = Vec3_Add(vel, Vec3_Scale(pml.forward_xy, pm->cmd.forward));
	vel = Vec3_Add(vel, Vec3_Scale(pml.right_xy, pm->cmd.right));

	vel.z = 0.0;

	dir = Vec3_NormalizeLength(vel, &speed);

	max_speed = PM_SPEED_AIR;

	// accounting for walk modulus
	if (pm->cmd.buttons & BUTTON_WALK) {
		max_speed *= PM_SPEED_MOD_WALK;
	}

	speed = Clampf(speed, 0.0, max_speed);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.0;
	}

	accel = PM_ACCEL_AIR;

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
	float speed, max_speed, accel;
	vec3_t forward, right, vel, dir;

	if (Pm_CheckJump()) { // jumped away
		if (pm->water_level > WATER_FEET) {
			Pm_WaterMove();
		} else {
			Pm_AirMove();
		}
		return;
	}

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction();

	Pm_Currents();

	// project the desired movement into the X/Y plane

	forward = Pm_ClipVelocity(pml.forward_xy, pml.ground_plane.normal, PM_CLIP_BOUNCE);
	right = Pm_ClipVelocity(pml.right_xy, pml.ground_plane.normal, PM_CLIP_BOUNCE);

	forward = Vec3_Normalize(forward);
	right = Vec3_Normalize(right);

	vel = Vec3_Zero();
	vel = Vec3_Add(vel, Vec3_Scale(forward, pm->cmd.forward));
	vel = Vec3_Add(vel, Vec3_Scale(right, pm->cmd.right));

	dir = Vec3_NormalizeLength(vel, &speed);

	// clamp to max speed
	if (pm->water_level > WATER_FEET) {
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
	speed = Clampf(speed, 0.0, max_speed);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.0;
	}

	// accelerate based on slickness of ground surface
	accel = (pml.ground_surface->flags & SURF_SLICK) ? PM_ACCEL_GROUND_SLICK : PM_ACCEL_GROUND;

	Pm_Accelerate(dir, speed, accel);

	// determine the speed after acceleration
	speed = Vec3_Length(pm->s.velocity);

	// clip to the ground
	pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, pml.ground_plane.normal, PM_CLIP_BOUNCE);

	// and now scale by the speed to avoid slowing down on slopes
	pm->s.velocity = Vec3_Normalize(pm->s.velocity);
	pm->s.velocity = Vec3_Scale(pm->s.velocity, speed);

	// and finally, step if moving in X/Y
	if (pm->s.velocity.x || pm->s.velocity.y) {
		Pm_StepSlideMove();
	}
}

/**
 * @brief
 */
static void Pm_SpectatorMove(void) {
	vec3_t vel;
	float speed;

	Pm_Friction();

	// user intentions on X/Y/Z
	vel = Vec3_Zero();
	vel = Vec3_Add(vel, Vec3_Scale(pml.forward, pm->cmd.forward));
	vel = Vec3_Add(vel, Vec3_Scale(pml.right, pm->cmd.right));

	// add explicit Z
	vel.z += pm->cmd.up;

	vel = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0.0, PM_SPEED_SPECTATOR);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.0;
	}

	// accelerate
	Pm_Accelerate(vel, speed, PM_ACCEL_SPECTATOR);

	// do the move
	pm->s.origin = Vec3_Add(pm->s.origin, Vec3_Scale(pm->s.velocity, pml.time));
}

/**
 * @brief
 */
static void Pm_FreezeMove(void) {

	Pm_Debug("%s\n", vtos(pm->s.origin));
}

/**
 * @brief
 */
static void Pm_Init(void) {

	// set the default bounding box
	if (pm->s.type == PM_DEAD) {

		if (pm->s.flags & PMF_GIBLET) {
			pm->mins = PM_GIBLET_MINS;
			pm->maxs = PM_GIBLET_MAXS;
		} else {
			pm->mins = Vec3_Scale(PM_DEAD_MINS, PM_SCALE);
			pm->maxs = Vec3_Scale(PM_DEAD_MAXS, PM_SCALE);
		}
	} else {
		pm->mins = Vec3_Scale(PM_MINS, PM_SCALE);
		pm->maxs = Vec3_Scale(PM_MAXS, PM_SCALE);
	}

	pm->angles = Vec3_Zero();

	pm->num_touch_ents = 0;
	pm->water_level = WATER_NONE;
	pm->water_type = 0;

	pm->step = 0.0;

	// reset flags that we test each move
	pm->s.flags &= ~(PMF_ON_GROUND | PMF_ON_STAIRS | PMF_ON_LADDER);
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
 * @brief
 */
static void Pm_ClampAngles(void) {

	// copy the command angles into the outgoing state
	pm->s.view_angles = pm->cmd.angles;

	// add the delta angles
	pm->angles = Vec3_Add(pm->cmd.angles, pm->s.delta_angles);

	// clamp pitch to prevent the player from looking up or down more than 90º
	if (pm->angles.x > 90.0 && pm->angles.x < 270.0) {
		pm->angles.x = 90.0;
	} else if (pm->angles.x <= 360.0 && pm->angles.x >= 270.0) {
		pm->angles.x -= 360.0;
	}
}

/**
 * @brief
 */
static void Pm_InitLocal(void) {

	memset(&pml, 0, sizeof(pml));

	// save previous values in case move fails, and to detect landings
	pml.previous_origin = pm->s.origin;
	pml.previous_velocity = pm->s.velocity;

	// convert from milliseconds to seconds
	pml.time = pm->cmd.msec * 0.001;

	// calculate the directional vectors for this move
	Vec3_Vectors(pm->angles, &pml.forward, &pml.right, &pml.up);

	// and calculate the directional vectors in the XY plane
	Vec3_Vectors(Vec3(0.f, pm->angles.y, 0.f), &pml.forward_xy, &pml.right_xy, NULL);
}

/**
 * @brief Called by the game and the client game to update the player's
 * authoritative or predicted movement state, respectively.
 */
void Pm_Move(pm_move_t *pm_move) {
	pm = pm_move;

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
	} else if (pm->water_level > WATER_FEET) {
		Pm_WaterMove();
	} else {
		Pm_AirMove();
	}

	// ensure we're in a valid spot, or revert
	Pm_CorrectPosition();

	// check for ground at new spot
	Pm_CheckGround();

	// snap us to adjacent walls
	Pm_SnapToWalls();

	// check for water level, water type at new spot
	Pm_CheckWater();
}

