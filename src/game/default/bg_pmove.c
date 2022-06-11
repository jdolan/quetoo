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
 * @brief PM_BOUNDS is the default bounding box, scaled by PM_SCALE
 * in Pm_Init. They are referenced in a few other places e.g. to create effects
 * at a certain body position on the player model.
 */
const box3_t PM_BOUNDS = {
	.mins = { { -16.f, -16.f, -24.f } },
	.maxs = { {  16.f,  16.f,  36.f } }
};

const box3_t PM_CROUCHED_BOUNDS = {
	.mins = { { -16.f, -16.f, -24.f } },
	.maxs = { {  16.f,  16.f,  6.f } }
};

static const box3_t PM_DEAD_BOUNDS = {
	.mins = { { -16.f, -16.f, -24.f } },
	.maxs = { {  16.f,  16.f,  -4.f } }
};

static const box3_t PM_GIBLET_BOUNDS = {
	.mins = { { -8.f, -8.f, -8.f } },
	.maxs = { {  8.f,  8.f,  8.f } }
};

static pm_move_t *pm;

#define MAX_CLIP_PLANES	6

/**
 * @brief A structure containing full floating point precision copies of all
 * movement variables. This is initialized with the player's last movement
 * at each call to Pm_Move (this is obviously not thread-safe).
 */
static struct {

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
	 * @brief The impacted brush sides per slide-move.
	 */
	const cm_bsp_brush_side_t *brush_sides[MAX_CLIP_PLANES];

	/**
	 * @brief The number of impacted brush_sides.
	 */
	int32_t num_brush_sides;

} pm_locals;

#define Pm_Debug(...) ({ if (pm->DebugMask() & pm->debug_mask) { pm->Debug(pm->debug_mask, __func__, __VA_ARGS__); } })

/**
 * @brief Mark the specified entity as touched. This enables the game module to
 * detect player -> entity interactions.
 */
static void Pm_TouchEntity(const cm_trace_t *trace) {

	if (trace->ent == NULL) {
		return;
	}

	if (pm->num_touched == PM_MAX_TOUCHS) {
		Pm_Debug("MAX_TOUCH_ENTS\n");
		return;
	}

	for (int32_t i = 0; i < pm->num_touched; i++) {
		if (pm->touched[i].ent == trace->ent) {
			return;
		}
	}

	pm->touched[pm->num_touched++] = *trace;
}

/**
 * Adapted from Quake III, this function adjusts a trace so that if it starts inside of a wall,
 * it is adjusted so that the trace begins outside of the solid it impacts.
 * @return The actual trace.
 */
static cm_trace_t Pm_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {

	const float offsets[] = { 0.f, 1.f, -1.f };

	// jitter around
	for (uint32_t i = 0; i < lengthof(offsets); i++) {
		for (uint32_t j = 0; j < lengthof(offsets); j++) {
			for (uint32_t k = 0; k < lengthof(offsets); k++) {
				const vec3_t point = Vec3_Add(start, Vec3(offsets[i], offsets[j], offsets[k]));
				const cm_trace_t trace = pm->Trace(point, end, bounds);
				
				if (!trace.all_solid) {

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
 * @brief Slide off of the impacted plane.
 */
static vec3_t Pm_ClipVelocity(const vec3_t in, const vec3_t normal, float bounce) {

	float backoff = Vec3_Dot(in, normal);

	if (backoff < 0.f) {
		backoff *= bounce;
	} else {
		backoff /= bounce;
	}

	return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

/**
 * @brief Collide with the results of the trace, clipping our velocity along the normal.
 */
static void Pm_ImpactBrush(const cm_trace_t *trace) {

	if (trace->ent == NULL) {
		return;
	}

	if (pm_locals.num_brush_sides == MAX_CLIP_PLANES) {
		Pm_Debug("MAX_CLIP_PLANES\n");
		return;
	}

	for (int32_t i = 0; i < pm_locals.num_brush_sides; i++) {
		if (trace->brush_side == pm_locals.brush_sides[i]) {
			return;
		}
	}

	pm_locals.brush_sides[pm_locals.num_brush_sides++] = trace->brush_side;

	pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, trace->plane.normal, PM_CLIP_BOUNCE);

	pm->s.origin = Vec3_Fmaf(pm->s.origin, TRACE_EPSILON, trace->plane.normal);
}

/**
 * @brief Slide through the world, clipping to impacted planes.
 */
static float Pm_SlideMove(void) {

	const vec3_t org0 = pm->s.origin;

	memset(pm_locals.brush_sides, 0, sizeof(pm_locals.brush_sides));
	pm_locals.num_brush_sides = 0;

	float time_remaining = pm_locals.time;
	while (time_remaining > FLT_EPSILON) {

		// project desired destination
		const vec3_t pos = Vec3_Fmaf(pm->s.origin, pm_locals.time, pm->s.velocity);

		// trace to it
		const cm_trace_t trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

		// move to the end position
		pm->s.origin = trace.end;

		// store a reference to the entity for firing game events
		Pm_TouchEntity(&trace);

		// clip along the plane
		Pm_ImpactBrush(&trace);

		// if the player is trapped in a solid, we're stuck, but don't build up falling velocity
		if (trace.all_solid) {
			pm->s.velocity.z = 0.f;
			break;
		}

		// update the movement time remaining
		time_remaining -= time_remaining * Maxf(trace.fraction, TRACE_EPSILON);
	}

	const vec3_t org1 = pm->s.origin;

	return fabsf(Vec3_Distance(org0, org1));
}

/**
 * @return True if the downward trace yielded a step, false otherwise.
 */
static _Bool Pm_CheckStep(const cm_trace_t *trace) {

	if (!trace->all_solid) {
		if (trace->ent && trace->plane.normal.z >= PM_STEP_NORMAL) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
static void Pm_StepDown(const cm_trace_t *trace) {

	pm->s.origin = trace->end;
	
	const float step_height = pm->s.origin.z - pm_locals.previous_origin.z;

	if (fabsf(step_height) >= PM_STEP_HEIGHT_MIN) {
		pm->step = step_height;
	}
}

/**
 * @brief
 */
static void Pm_StepSlideMove(void) {

	// store pre-move parameters
	const vec3_t org0 = pm->s.origin;
	const vec3_t vel0 = pm->s.velocity;

	// attempt to move
	float dist0 = Pm_SlideMove();

	// attempt to step down to remain on ground
	if ((pm->s.flags & PMF_ON_GROUND) && pm->cmd.up <= 0) {

		const vec3_t down = Vec3_Fmaf(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, Vec3_Down());
		const cm_trace_t step_down = Pm_Trace(pm->s.origin, down, pm->bounds);

		if (Pm_CheckStep(&step_down)) {
			Pm_StepDown(&step_down);
		}
	}

	// now that we're on the ground, try to step over any obstacles
	const vec3_t org1 = pm->s.origin;
	const vec3_t vel1 = pm->s.velocity;

	const vec3_t up = Vec3_Fmaf(org0, PM_STEP_HEIGHT, Vec3_Up());
	const cm_trace_t step_up = Pm_Trace(org0, up, pm->bounds);

	if (!step_up.all_solid) {

		// step from the higher position, with the original velocity
		pm->s.origin = step_up.end;
		pm->s.velocity = vel0;

		const float dist1 = Pm_SlideMove();
		if (dist1 > dist0 + FLT_EPSILON) {

			// settle to the new ground, keeping the step if and only if it was successful
			const vec3_t down = Vec3_Fmaf(pm->s.origin, PM_STEP_HEIGHT + PM_GROUND_DIST, Vec3_Down());
			const cm_trace_t step_down = Pm_Trace(pm->s.origin, down, pm->bounds);

			if (Pm_CheckStep(&step_down)) {
				// Quake2 trick jump secret sauce
				if ((pm->s.flags & PMF_ON_GROUND) || vel0.z < PM_SPEED_UP) {
					Pm_StepDown(&step_down);
				} else {
					pm->step = pm->s.origin.z - pm_locals.previous_origin.z;
				}

				return;
			}
		}
	}

	// stepping up was not helpful, so take the lower movement
	pm->s.origin = org1;
	pm->s.velocity = vel1;
}

/**
 * @brief Handles friction against user intentions, and based on contents.
 * @param flying Whether we should clear Z velocity as well if we are going to stop
 */
static void Pm_Friction(const bool flying) {
	vec3_t vel = pm->s.velocity;

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

	const float control = Maxf(PM_SPEED_STOP, speed);

	float friction = 0.f;

	if (pm->s.type == PM_SPECTATOR) { // spectator friction
		friction = PM_FRICT_SPECTATOR;
	} else if (pm->s.flags & PMF_ON_LADDER) { // ladder friction
		friction = PM_FRICT_LADDER;
	} else if (pm->water_level > WATER_FEET) { // water friction
		friction = PM_FRICT_WATER;
	} else if (pm->s.flags & PMF_ON_GROUND) { // ground friction
		if (pm_locals.ground.ent && (pm_locals.ground.surface & SURF_SLICK)) {
			friction = PM_FRICT_GROUND_SLICK;
		} else {
			friction = PM_FRICT_GROUND;
		}
	} else { // everything else friction
		friction = PM_FRICT_AIR;
	}

	// scale the velocity, taking care to not reverse direction
	const float scale = Maxf(0.f, speed - (friction * control * pm_locals.time)) / speed;

	pm->s.velocity = Vec3_Scale(pm->s.velocity, scale);
}

/**
 * @brief Handles user intended acceleration.
 */
static void Pm_Accelerate(const vec3_t dir, float speed, float accel) {
	const float current_speed = Vec3_Dot(pm->s.velocity, dir);
	const float add_speed = speed - current_speed;

	if (add_speed <= 0.f) {
		return;
	}

	float accel_speed = accel * pm_locals.time * speed;

	if (accel_speed > add_speed) {
		accel_speed = add_speed;
	}

	pm->s.velocity = Vec3_Fmaf(pm->s.velocity, accel_speed, dir);
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

	pm->s.velocity.z -= gravity * pm_locals.time;
}

/**
 * @brief
 */
static void Pm_Currents(void) {
	vec3_t current = Vec3_Zero();

	// add water currents
	if (pm->water_level) {
		if (pm->water_type & CONTENTS_CURRENT_0) {
			current.x += 1.f;
		}
		if (pm->water_type & CONTENTS_CURRENT_90) {
			current.y += 1.f;
		}
		if (pm->water_type & CONTENTS_CURRENT_180) {
			current.x -= 1.f;
		}
		if (pm->water_type & CONTENTS_CURRENT_270) {
			current.y -= 1.f;
		}
		if (pm->water_type & CONTENTS_CURRENT_UP) {
			current.z += 1.f;
		}
		if (pm->water_type & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.f;
		}
	}

	// add conveyer belt velocities
	if (pm->ground.ent) {
		if (pm_locals.ground.contents & CONTENTS_CURRENT_0) {
			current.x += 1.f;
		}
		if (pm_locals.ground.contents & CONTENTS_CURRENT_90) {
			current.y += 1.f;
		}
		if (pm_locals.ground.contents & CONTENTS_CURRENT_180) {
			current.x -= 1.f;
		}
		if (pm_locals.ground.contents & CONTENTS_CURRENT_270) {
			current.y -= 1.f;
		}
		if (pm_locals.ground.contents & CONTENTS_CURRENT_UP) {
			current.z += 1.f;
		}
		if (pm_locals.ground.contents & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.f;
		}
	}

	if (!Vec3_Equal(current, Vec3_Zero())) {
		current = Vec3_Normalize(current);
	}

	pm->s.velocity = Vec3_Fmaf(pm->s.velocity, PM_SPEED_CURRENT, current);
}

/**
 * @return True if the player will be eligible for trick jumping should they
 * impact the ground on this frame, false otherwise.
 */
static _Bool Pm_CheckTrickJump(void) {

	if (pm->ground.ent) {
		return false;
	}

	if (pm_locals.previous_velocity.z < PM_SPEED_UP) {
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
 * @return True if the player is attempting to leave the ground via grappling hook.
 */
static bool Pm_CheckHookJump(void) {

	if ((pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) && (pm->s.velocity.z > 1.f)) {

		pm->s.flags &= ~PMF_ON_GROUND;
		memset(&pm->ground, 0, sizeof(pm->ground));

		return true;
	}

	return false;
}

/**
 * @brief
 */
static void Pm_CheckHook(void) {

	// hookers only
	if (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO) {
		pm->s.flags &= ~PMF_HOOK_RELEASED;
		return;
	}

	// if we let go of hook, just go back to normal
	if ((pm->s.type == PM_HOOK_PULL || pm->s.type == PM_HOOK_SWING_AUTO) && !(pm->cmd.buttons & BUTTON_HOOK)) {
		pm->s.type = PM_NORMAL;
		return;
	}

	// get chain length
	if (pm->s.type == PM_HOOK_PULL) {

		pm->cmd.forward = pm->cmd.right = 0;

		// pull physics
		const float dist = Vec3_DistanceDir(pm->s.hook_position, pm->s.origin, &pm->s.velocity);
		if (dist > PM_HOOK_MIN_DIST && !Pm_CheckHookJump()) {
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

		const float hook_rate = (pm->hook_pull_speed / 1.5f) * pm_locals.time;

		// chain physics
		// grow/shrink chain based on input
		if ((pm->cmd.up > 0 || !(pm->s.flags & PMF_HOOK_RELEASED)) && (pm->s.hook_length > PM_HOOK_MIN_DIST)) {
			pm->s.hook_length = Maxf(pm->s.hook_length - hook_rate, PM_HOOK_MIN_DIST);
		} else if ((pm->cmd.up < 0) && (pm->s.hook_length < PM_HOOK_MAX_DIST)) {
			pm->s.hook_length = Minf(pm->s.hook_length + hook_rate, PM_HOOK_MAX_DIST);
		}

		vec3_t chain_vec = Vec3_Subtract(pm->s.hook_position, pm->s.origin);
		float chain_len = Vec3_Length(chain_vec);

		// if player's location is already within the chain's reach
		if (chain_len <= pm->s.hook_length) {
			return;
		}

		// reel us in!
		vec3_t vel_part;

		// determine player's velocity component of chain vector
		vel_part = Vec3_Scale(chain_vec, Vec3_Dot(pm->s.velocity, chain_vec) / Vec3_Dot(chain_vec, chain_vec));

		// restrainment default force
		float force = (chain_len - pm->s.hook_length) * 5.f;

		// if player's velocity heading is away from the hook
		if (Vec3_Dot(pm->s.velocity, chain_vec) < 0.f) {

			// if chain has streched for PM_HOOK_MIN_DIST units
			if (chain_len > pm->s.hook_length + PM_HOOK_MIN_DIST) {

				// remove player's velocity component moving away from hook
				pm->s.velocity = Vec3_Subtract(pm->s.velocity, vel_part);
			}
		} else { // if player's velocity heading is towards the hook

			if (Vec3_Length(vel_part) < force) {
				force -= Vec3_Length(vel_part);
			} else {
				force = 0.f;
			}
		}

		if (force) {
			// applies chain restrainment
			chain_vec = Vec3_Normalize(chain_vec);
			pm->s.velocity = Vec3_Fmaf(pm->s.velocity, force, chain_vec);
		}
	}
}

/**
 * @brief Checks for ground interaction, enabling trick jumping and dealing with landings.
 */
static void Pm_CheckGround(void) {

	if (Pm_CheckHookJump()) {
		return;
	}

	// if we jumped, or been pushed, do not attempt to seek ground
	if (pm->s.flags & (PMF_JUMPED | PMF_TIME_PUSHED | PMF_ON_LADDER)) {
		return;
	}

	// seek ground eagerly if the player wishes to trick jump
	const _Bool trick_jump = Pm_CheckTrickJump();
	vec3_t pos;

	if (trick_jump) {
		pos = Vec3_Fmaf(pm->s.origin, pm_locals.time, pm->s.velocity);
		pos.z -= PM_GROUND_DIST_TRICK;
	} else {
		pos = pm->s.origin;
		pos.z -= PM_GROUND_DIST;
	}

	// seek the ground
	cm_trace_t trace = pm_locals.ground = Pm_Trace(pm->s.origin, pos, pm->bounds);

	// if we hit an upward facing plane, make it our ground
	if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {

		// if we had no ground, then handle landing events
		if (!pm->ground.ent) {

			// any landing terminates the water jump
			if (pm->s.flags & PMF_TIME_WATER_JUMP) {
				pm->s.flags &= ~PMF_TIME_WATER_JUMP;
				pm->s.time = 0;
			}

			// hard landings disable jumping briefly
			if (pm_locals.previous_velocity.z <= PM_SPEED_LAND) {
				pm->s.flags |= PMF_TIME_LAND;
				pm->s.time = 1;

				if (pm_locals.previous_velocity.z <= PM_SPEED_FALL) {
					pm->s.time = 16;

					if (pm_locals.previous_velocity.z <= PM_SPEED_FALL_FAR) {
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
		pm->ground = trace;

		// and sink down to it if not trick jumping
		if (!(pm->s.flags & PMF_TIME_TRICK_JUMP)) {
			pm->s.origin = trace.end;

			pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, trace.plane.normal, PM_CLIP_BOUNCE);
		}

		// clear jump buffer
		pm->s.jump_buffer = 0;
	} else {

		// leaving ground; if we had ground, give us jump buffer
		if (pm->ground.ent) {
			pm->s.jump_buffer = PM_JUMP_BUFFER_TIME;
		}

		pm->s.flags &= ~PMF_ON_GROUND;
		memset(&pm->ground, 0, sizeof(pm->ground));
	}

	// always touch the entity, even if we couldn't stand on it
	Pm_TouchEntity(&trace);
}

/**
 * @brief Checks for water interaction, accounting for player ducking, etc.
 */
static void Pm_CheckWater(void) {

	pm->water_level = WATER_NONE;
	pm->water_type = 0;

	vec3_t pos = pm->s.origin;
	pos.z = pm->s.origin.z + pm->bounds.mins.z + PM_GROUND_DIST;

	int32_t contents = pm->PointContents(pos);
	if (contents & CONTENTS_MASK_LIQUID) {

		pm->water_type = contents;
		pm->water_level = WATER_FEET;

		pos.z = pm->s.origin.z;

		contents = pm->PointContents(pos);

		if (contents & CONTENTS_MASK_LIQUID) {

			pm->water_type |= contents;
			pm->water_level = WATER_WAIST;

			pos.z = pm->s.origin.z + pm->s.view_offset.z + 1.f;

			contents = pm->PointContents(pos);

			if (contents & CONTENTS_MASK_LIQUID) {
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
			pm->s.view_offset.z = 0.f;
		} else {
			pm->s.view_offset.z = -16.f;
		}
	} else {

		const _Bool is_ducking = pm->s.flags & PMF_DUCKED;
		const _Bool wants_ducking = (pm->cmd.up < 0) && !(pm->s.flags & PMF_ON_LADDER);

		if (!is_ducking && wants_ducking) {
			pm->s.flags |= PMF_DUCKED;
		} else if (is_ducking && !wants_ducking) {
			const cm_trace_t trace = Pm_Trace(pm->s.origin, pm->s.origin, pm->bounds);

			if (!trace.all_solid && !trace.start_solid) {
				pm->s.flags &= ~PMF_DUCKED;
			}
		}

		const float height = Box3_Size(pm->bounds).z;

		if (pm->s.flags & PMF_DUCKED) { // ducked, reduce height
			const float target = pm->bounds.mins.z + height * 0.5f;

			if (pm->s.view_offset.z > target) { // go down
				pm->s.view_offset.z -= pm_locals.time * PM_SPEED_DUCK_STAND;
			}

			if (pm->s.view_offset.z < target) {
				pm->s.view_offset.z = target;
			}

			// change the bounding box to reflect ducking
			pm->bounds = PM_CROUCHED_BOUNDS;
		} else {
			const float target = pm->bounds.mins.z + height * 0.9f;

			if (pm->s.view_offset.z < target) { // go up
				pm->s.view_offset.z += pm_locals.time * PM_SPEED_DUCK_STAND;
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

	if (pm->s.velocity.z < 0.f) {
		pm->s.velocity.z = jump;
	} else {
		pm->s.velocity.z += jump;
	}

	// indicate that jump is currently held
	pm->s.flags |= (PMF_JUMPED | PMF_JUMP_HELD);

	// clear the ground indicators
	pm->s.flags &= ~PMF_ON_GROUND;
	memset(&pm->ground, 0, sizeof(pm->ground));

	// we can trick jump soon
	pm->s.flags |= PMF_TIME_TRICK_START;
	pm->s.time = 100;

	return true;
}

/**
 * @brief Check for ladder interaction.
 *
 * @return True if the player is on a ladder, false otherwise.
 */
static void Pm_CheckLadder(void) {

	if (pm->s.flags & PMF_TIME_MASK) {
		return;
	}

	if (pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) {
		return;
	}

	const vec3_t pos = Vec3_Fmaf(pm->s.origin, 4.f, pm_locals.forward_xy);
	const cm_trace_t trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

	if (trace.contents & CONTENTS_LADDER) {
		pm->s.flags |= PMF_ON_LADDER;

		memset(&pm->ground, 0, sizeof(pm->ground));
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

	if (pm->s.type >= PM_HOOK_PULL && pm->s.type <= PM_HOOK_SWING_AUTO) {
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

	vec3_t pos = Vec3_Fmaf(pm->s.origin, 16.f, pm_locals.forward);
	cm_trace_t trace = Pm_Trace(pm->s.origin, pos, pm->bounds);

	if (trace.contents & CONTENTS_MASK_SOLID) {

		pos.z += PM_STEP_HEIGHT + Box3_Size(pm->bounds).z;

		trace = Pm_Trace(pos, pos, pm->bounds);

		if (trace.start_solid) {
			Pm_Debug("Can't exit water: blocked\n");
			return false;
		}

		vec3_t pos2 = Vec3(pos.x, pos.y, pm->s.origin.z);

		trace = Pm_Trace(pos, pos2, pm->bounds);

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

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction(false);

	Pm_Currents();

	// user intentions in X/Y
	vec3_t vel = Vec3_Zero();
	vel = Vec3_Fmaf(vel, pm->cmd.forward, pm_locals.forward_xy);
	vel = Vec3_Fmaf(vel, pm->cmd.right, pm_locals.right_xy);

	const float s = PM_SPEED_LADDER * 0.125f;

	// limit horizontal speed when on a ladder
	vel.x = Clampf(vel.x, -s, s);
	vel.y = Clampf(vel.y, -s, s);
	vel.z = 0.f;

	// handle Z intentions differently
	if (fabsf(pm->s.velocity.z) < PM_SPEED_LADDER) {

		if ((pm->angles.x <= -15.f) && (pm->cmd.forward > 0)) {
			vel.z = PM_SPEED_LADDER;
		} else if ((pm->angles.x >= 15.f) && (pm->cmd.forward > 0)) {
			vel.z = -PM_SPEED_LADDER;
		} else if (pm->cmd.up > 0) {
			vel.z = PM_SPEED_LADDER;
		} else if (pm->cmd.up < 0) {
			vel.z = -PM_SPEED_LADDER;
		} else {
			vel.z = 0.f;
		}
	}

	if (pm->cmd.up > 0) { // avoid jumps when exiting ladders
		pm->s.flags |= PMF_JUMP_HELD;
	}

	float speed;
	const vec3_t dir = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0.f, PM_SPEED_LADDER);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.f;
	}

	Pm_Accelerate(dir, speed, PM_ACCEL_LADDER);

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_WaterJumpMove(void) {

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction(false);

	Pm_Gravity();

	// check for a usable spot directly in front of us
	const vec3_t pos = Vec3_Fmaf(pm->s.origin, 30.f, pm_locals.forward_xy);

	// if we've reached a usable spot, clamp the jump to avoid launching
	if (Pm_Trace(pm->s.origin, pos, pm->bounds).fraction == 1.f) {
		pm->s.velocity.z = Clampf(pm->s.velocity.z, 0.f, PM_SPEED_JUMP);
	}

	// if we're falling back down, clear the timer to regain control
	if (pm->s.velocity.z <= 0.f) {
		pm->s.flags &= ~PMF_TIME_MASK;
		pm->s.time = 0;
	}

	Pm_StepSlideMove();
}

/**
 * @brief
 */
static void Pm_WaterMove(void) {

	if (Pm_CheckWaterJump()) {
		Pm_WaterJumpMove();
		return;
	}

	Pm_Debug("%s\n", vtos(pm->s.origin));

	// apply friction, slowing rapidly when first entering the water
	float speed = Vec3_Length(pm->s.velocity);

	for (int32_t i = speed / PM_SPEED_WATER; i >= 0; i--) {
		Pm_Friction(true);
	}

	// and sink
	if (!pm->cmd.forward && !pm->cmd.right && !pm->cmd.up && (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO)) {
		if (pm->s.velocity.z > PM_SPEED_WATER_SINK) {
			Pm_Gravity();
		}
	}

	Pm_Currents();

	// user intentions on X/Y/Z
	vec3_t vel = Vec3_Zero();
	vel = Vec3_Fmaf(vel, pm->cmd.forward, pm_locals.forward);
	vel = Vec3_Fmaf(vel, pm->cmd.right, pm_locals.right);

	// add explicit Z
	vel.z += pm->cmd.up;

	// disable water skiing
	if (pm->s.type < PM_HOOK_PULL || pm->s.type > PM_HOOK_SWING_AUTO) {
		if (pm->water_level == WATER_WAIST) {
			vec3_t view = Vec3_Add(pm->s.origin, pm->s.view_offset);
			view.z -= 4.f;

			if (!(pm->PointContents(view) & CONTENTS_MASK_LIQUID)) {
				pm->s.velocity.z = Minf(pm->s.velocity.z, 0.f);
				vel.z = Minf(vel.z, 0.f);
			}
		}
	}

	const vec3_t dir = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0, PM_SPEED_WATER);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.f;
	}

	Pm_Accelerate(dir, speed, PM_ACCEL_WATER);

	if (pm->cmd.up > 0) {
		Pm_SlideMove();
	} else {
		Pm_StepSlideMove();
	}
}

/**
 * @brief
 */
static void Pm_AirMove(void) {

	Pm_Debug("%s\n", vtos(pm->s.origin));

	if (pm->s.jump_buffer) {
		Pm_CheckJump();
	}

	Pm_Friction(false);

	Pm_Gravity();

	vec3_t vel = Vec3_Zero();
	vel = Vec3_Fmaf(vel, pm->cmd.forward, pm_locals.forward_xy);
	vel = Vec3_Fmaf(vel, pm->cmd.right, pm_locals.right_xy);
	vel.z = 0.f;

	float max_speed = PM_SPEED_AIR;

	// accounting for walk modulus
	if (pm->cmd.buttons & BUTTON_WALK) {
		max_speed *= PM_SPEED_MOD_WALK;
	}

	float speed;
	const vec3_t dir = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0.f, max_speed);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.f;
	}

	float accel = PM_ACCEL_AIR;

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

	// check for beginning of a jump
	if (Pm_CheckJump()) {
		Pm_AirMove();
		return;
	}

	Pm_Debug("%s\n", vtos(pm->s.origin));

	Pm_Friction(false);

	Pm_Currents();

	// if the player is walking on the sea floor and wishes to swim, let them

	if (pm->water_level == WATER_UNDER && pm_locals.forward.z > 0.f) {

		pm->s.flags &= ~PMF_ON_GROUND;
		memset(&pm->ground, 0, sizeof(pm->ground));

		Pm_WaterMove();
		return;
	}

	// project the desired movement into the X/Y plane

	const vec3_t forward = Vec3_Normalize(Pm_ClipVelocity(pm_locals.forward_xy, pm_locals.ground.plane.normal, PM_CLIP_BOUNCE));
	const vec3_t right = Vec3_Normalize(Pm_ClipVelocity(pm_locals.right_xy, pm_locals.ground.plane.normal, PM_CLIP_BOUNCE));

	vec3_t vel = Vec3_Zero();
	vel = Vec3_Fmaf(vel, pm->cmd.forward, forward);
	vel = Vec3_Fmaf(vel, pm->cmd.right, right);

	float max_speed;

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
	float speed;
	const vec3_t dir = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0.f, max_speed);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.f;
	}

	// accelerate based on slickness of ground surface
	const float accel = (pm_locals.ground.surface & SURF_SLICK) ? PM_ACCEL_GROUND_SLICK : PM_ACCEL_GROUND;

	Pm_Accelerate(dir, speed, accel);

	// determine the speed after acceleration
	speed = Vec3_Length(pm->s.velocity);

	// clip to the ground
	pm->s.velocity = Pm_ClipVelocity(pm->s.velocity, pm_locals.ground.plane.normal, PM_CLIP_BOUNCE);

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

	Pm_Friction(true);

	// user intentions on X/Y/Z
	vec3_t vel = Vec3_Zero();
	vel = Vec3_Fmaf(vel, pm->cmd.forward, pm_locals.forward);
	vel = Vec3_Fmaf(vel, pm->cmd.right, pm_locals.right);

	// add explicit Z
	vel.z += pm->cmd.up;

	float speed;
	vel = Vec3_NormalizeLength(vel, &speed);
	speed = Clampf(speed, 0.f, PM_SPEED_SPECTATOR);

	if (speed < PM_STOP_EPSILON) {
		speed = 0.f;
	}

	// accelerate
	Pm_Accelerate(vel, speed, PM_ACCEL_SPECTATOR);

	// do the move
	pm->s.origin = Vec3_Fmaf(pm->s.origin, pm_locals.time, pm->s.velocity);
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
			pm->bounds = PM_GIBLET_BOUNDS;
		} else {
			pm->bounds = Box3_Scale(PM_DEAD_BOUNDS, PM_SCALE);
		}
	} else {
		pm->bounds = Box3_Scale(PM_BOUNDS, PM_SCALE);
	}

	pm->angles = Vec3_Zero();

	pm->num_touched = 0;
	pm->water_level = WATER_NONE;
	pm->water_type = 0;

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

	if (pm->s.jump_buffer) {
		if (pm->cmd.msec > pm->s.jump_buffer) {
			pm->s.jump_buffer = 0;
		} else {
			pm->s.jump_buffer -= pm->cmd.msec;
		}
		Pm_Debug("jump buffer: %u\n", pm->s.jump_buffer);
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
	if (pm->angles.x > 90.f && pm->angles.x < 270.f) {
		pm->angles.x = 90.f;
	} else if (pm->angles.x <= 360.f && pm->angles.x >= 270.f) {
		pm->angles.x -= 360.f;
	}
}

/**
 * @brief
 */
static void Pm_InitLocal(void) {

	memset(&pm_locals, 0, sizeof(pm_locals));

	// save previous values in case move fails, and to detect landings
	pm_locals.previous_origin = pm->s.origin;
	pm_locals.previous_velocity = pm->s.velocity;

	// convert from milliseconds to seconds
	pm_locals.time = pm->cmd.msec * .001f;

	// calculate the directional vectors for this move
	Vec3_Vectors(pm->angles, &pm_locals.forward, &pm_locals.right, &pm_locals.up);

	// and calculate the directional vectors in the XY plane
	Vec3_Vectors(Vec3(0.f, pm->angles.y, 0.f), &pm_locals.forward_xy, &pm_locals.right_xy, NULL);
}

/**
 * @brief
 */
static void Pm_CheckViewStep(void) {

	// add the step offset we've made on this frame
	if (pm->step) {
		pm->s.step_offset += pm->step;
	}

	// calculate change to the step offset
	if (pm->s.step_offset) {

		const float step_speed = pm_locals.time * (PM_SPEED_STEP * (Maxf(1.f, fabsf(pm->s.step_offset) / PM_STEP_HEIGHT)));

		if (pm->s.step_offset > 0) {
			pm->s.step_offset = Maxf(0.f, pm->s.step_offset - step_speed);
		} else {
			pm->s.step_offset = Minf(0.f, pm->s.step_offset + step_speed);
		}
	}
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

	// check for ground at new spot
	Pm_CheckGround();

	// check for water level, water type at new spot
	Pm_CheckWater();

	// check for offset changes for our view
	Pm_CheckViewStep();
}

