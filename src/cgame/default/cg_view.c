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

#include "cg_local.h"
#include "game/default/bg_pmove.h"

#define NEAR_Z 4.0
#define FAR_Z  (MAX_WORLD_COORD * 4.0)

/**
 * @brief Update the field of view, which affects the view port as well as the culling
 * frustum.
 */
static void Cg_UpdateFov(void) {

	if (!cg_fov->modified && !cgi.view->update) {
		return;
	}

	cg_fov->value = Clamp(cg_fov->value, 10.0, 160.0);
	cg_fov_interpolate->value = Clamp(cg_fov_interpolate->value, 0.0, 10.0);

	vec_t fov = cg_fov->value;

	if (cg_fov_interpolate->value && cgi.view->fov[0] && cgi.view->fov[1]) {
		static vec_t prev, next;
		static uint32_t time;

		if (next && next != cg_fov->value) {
			time = 0;
		}

		if (time == 0) {
			prev = cgi.view->fov[0] * 2.0, next = cg_fov->value;
			time = cgi.client->ticks;
		}

		const vec_t frac = (cgi.client->ticks - time) / (cg_fov_interpolate->value * 100.0);
		if (frac >= 1.0) {
			time = 0;
			fov = next;
			cg_fov->modified = false;
		} else {
			fov = prev + frac * (next - prev);
		}
	} else {
		cg_fov->modified = false;
	}

	cgi.view->fov[0] = fov / 2.0;

	const vec_t x = cgi.context->width / tan(Radians(fov));

	const vec_t y = atan2(cgi.context->height, x);

	const vec_t a = cgi.context->height / (vec_t ) cgi.context->width;

	cgi.view->fov[1] = Degrees(y) * a / 2.0;

	// set up projection matrix
	const vec_t aspect = (vec_t) cgi.view->viewport_3d.w / (vec_t) cgi.view->viewport_3d.h;

	const vec_t ymax = NEAR_Z * tan(Radians(cgi.view->fov[1]));
	const vec_t ymin = -ymax;

	const vec_t xmin = ymin * aspect;
	const vec_t xmax = ymax * aspect;

	Matrix4x4_FromFrustum(&cgi.view->matrix_base_3d, xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);
}

/**
 * @brief Update the third person offset, if any. This is used as a client-side
 * option, or as the default chase camera view.
 */
static void Cg_UpdateThirdPerson(const player_state_t *ps) {
	vec3_t angles, forward, dest;

	const vec3_t mins = { -8.0, -8.0, -8.0 };
	const vec3_t maxs = { 8.0, 8.0, 8.0 };

	if (!cg_third_person->value) {
		return;
	}

	VectorCopy(cgi.view->angles, angles);
	angles[YAW] += cg_third_person_yaw->value;

	AngleVectors(angles, forward, NULL, NULL);

	const vec_t dist = fabs(150.0 * cg_third_person->value);

	// project the view origin back and up for 3rd person
	VectorMA(cgi.view->origin, -dist, forward, dest);
	dest[2] += 20.0;

	// clip it to the world
	const cm_trace_t tr = cgi.Trace(cgi.view->origin, dest, mins, maxs, 0, MASK_CLIP_PLAYER);
	VectorCopy(tr.end, cgi.view->origin);

	// adjust view angles to compensate for height offset
	VectorMA(cgi.view->origin, 1024.0, forward, dest);
	VectorSubtract(dest, cgi.view->origin, dest);

	// copy angles back to view
	VectorAngles(dest, cgi.view->angles);
	AngleVectors(cgi.view->angles, cgi.view->forward, cgi.view->right, cgi.view->up);
}

/**
 * @brief Augments the view angles based on game-dependent events.
 */
static void Cg_UpdateAngles(const player_state_t *ps) {

	if (ps->pm_state.type == PM_DEAD) { // look only on x axis
		cgi.view->angles[0] = 0.0;
		cgi.view->angles[2] = 45.0;
	}
}

/**
 * @brief Periodically calculates the player's horizontal speed, and interpolates it
 * over a small interval to smooth out rapid changes in velocity.
 */
static vec_t Cg_BobSpeedModulus(const player_state_t *ps) {
	static vec_t old_speed, new_speed;
	static uint32_t ticks;

	if (cgi.client->ticks < ticks) {
		ticks = 0;
		old_speed = new_speed = 0.0;
	}

	vec_t speed;

	const uint32_t delta = cgi.client->ticks - ticks;
	if (delta < 200) {
		const vec_t lerp = delta / (vec_t) 200;
		speed = old_speed + lerp * (new_speed - old_speed);
	} else {
		const _Bool ducked = ps->pm_state.flags & PMF_DUCKED;
		const vec_t max_speed = ducked ? PM_SPEED_DUCKED : PM_SPEED_AIR;

		vec3_t velocity;
		VectorCopy(ps->pm_state.velocity, velocity);
		velocity[2] = 0.0;

		old_speed = new_speed;
		new_speed = VectorLength(velocity) / max_speed;
		new_speed = Clamp(new_speed, 0.0, 1.0);
		speed = old_speed;

		ticks = cgi.client->ticks;
	}

	return 0.66 + speed;
}

/**
 * @brief Calculate the view bob. This is done using a running time counter and a
 * simple sin function. The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cg_UpdateBob(const player_state_t *ps) {
	static uint32_t bob, ticks;

	if (!cg_bob->value) {
		return;
	}

	if (cg_third_person->value) {
		return;
	}

	if (ps->pm_state.type > PM_HOOK_SWING) {

		// if we're frozen and not chasing, don't bob
		if (!ps->stats[STAT_CHASE]) {
			return;
		}
	}

	if (cgi.client->ticks < ticks) {
		bob = ticks = 0;
	}

	const vec_t mod = Cg_BobSpeedModulus(ps);

	// then calculate how much bob to add this frame
	vec_t frame_bob = Clamp(cgi.client->ticks - ticks, 1u, 1000u) * mod;

	if (!(ps->pm_state.flags & PMF_ON_GROUND)) {
		frame_bob *= 0.25;
	}

	bob += frame_bob;
	ticks = cgi.client->ticks;

	cgi.view->bob = sin(0.0045 * bob) * mod * mod;
	cgi.view->bob *= cg_bob->value; // scale via cvar too

	VectorMA(cgi.view->origin, -cgi.view->bob, cgi.view->forward, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->right, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->up, cgi.view->origin);
}

/**
 * @brief Updates the view definition. The camera origin, bob effect, and field of view are each
 * augmented here. Other modifications can be made at your own risk. This is called potentially
 * several times per client frame by the engine to prepare the view for frame interpolation.
 */
void Cg_UpdateView(const cl_frame_t *frame) {

	Cg_UpdateFov();

	Cg_UpdateThirdPerson(&frame->ps);

	Cg_UpdateAngles(&frame->ps);

	Cg_UpdateBob(&frame->ps);

	Cg_AddEntities(frame);

	Cg_AddEmits();

	Cg_AddEffects();

	Cg_AddParticles();
}
