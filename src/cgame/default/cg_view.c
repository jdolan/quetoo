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

/**
 * @brief Update the field of view, which affects the view port as well as the culling
 * frustum.
 */
static void Cg_UpdateFov(void) {

	if (!cg_fov->modified && !cgi.view->update)
		return;

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
			time = cgi.client->systime;
		}

		const vec_t frac = (cgi.client->systime - time) / (cg_fov_interpolate->value * 100.0);
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
}

/**
 * @brief Update the third person offset, if any. This is used as a client-side
 * option, or as the default chase camera view.
 */
static void Cg_UpdateThirdPerson(const player_state_t *ps __attribute__((unused))) {
	vec3_t angles, forward, dest;
	vec3_t mins, maxs;
	vec_t dist;
	cm_trace_t tr;

	cgi.client->third_person = cg_third_person->integer;

	if (!cg_third_person->value)
		return;

	VectorCopy(cgi.view->angles, angles);
	angles[YAW] += cg_third_person_yaw->value;

	AngleVectors(angles, forward, NULL, NULL);

	dist = cg_third_person->value;

	if (!dist)
		dist = 1.0;

	dist = fabs(150.0 * dist);

	// project the view origin back and up for 3rd person
	VectorMA(cgi.view->origin, -dist, forward, dest);
	dest[2] += 20.0;

	// clip it to the world
	VectorSet(mins, -5.0, -5.0, -5.0);
	VectorSet(maxs, 5.0, 5.0, 5.0);
	tr = cgi.Trace(cgi.view->origin, dest, mins, maxs, 0, MASK_CLIP_PLAYER);
	VectorCopy(tr.end, cgi.view->origin);

	// adjust view angles to compensate for height offset
	VectorMA(cgi.view->origin, 2048.0, forward, dest);
	VectorSubtract(dest, cgi.view->origin, dest);

	// copy angles back to view
	VectorAngles(dest, cgi.view->angles);
	AngleVectors(cgi.view->angles, cgi.view->forward, cgi.view->right, cgi.view->up);
}

/**
 * @brief Calculate the view bob. This is done using a running time counter and a
 * simple sin function. The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cg_UpdateBob(const player_state_t *ps) {
	static uint32_t time, vtime;
	vec3_t velocity;

	if (!cg_bob->value)
		return;

	if (cg_third_person->value)
		return;

	if (ps->pm_state.type != PM_NORMAL) {

		// if we're frozen and not chasing, don't bob
		if (!ps->stats[STAT_CHASE])
			return;
	}

	const _Bool ducked = ps->pm_state.flags & PMF_DUCKED;
	const vec_t max_speed = ducked ? PM_SPEED_DUCKED : PM_SPEED_AIR;

	VectorCopy(ps->pm_state.velocity, velocity);
	velocity[2] = 0.0;

	vec_t speed = VectorLength(velocity) / (max_speed * 2.0);
	speed = Clamp(speed, 0.0, 1.0);

	vec_t ftime = Clamp(cgi.view->time - vtime, 1, 1000);
	ftime *= (1.0 + speed * 1.0 + speed);

	if (!(ps->pm_state.flags & PMF_ON_GROUND))
		ftime *= 0.25;

	time += ftime;
	vtime = cgi.view->time;

	cgi.view->bob = sin(0.0045 * time) * (0.5 + speed) * (0.5 + speed);
	cgi.view->bob *= cg_bob->value; // scale via cvar too

	VectorMA(cgi.view->origin, -cgi.view->bob, cgi.view->forward, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->right, cgi.view->origin);
	VectorMA(cgi.view->origin, cgi.view->bob, cgi.view->up, cgi.view->origin);
}

/**
 * @brief Updates the view for the renderer. The camera origin, bob effect, and field
 * of view are each augmented here. Other modifications can be made at your own
 * risk. This is called once per frame by the engine to finalize the view so
 * that rendering may begin.
 */
void Cg_UpdateView(const cl_frame_t *frame) {

	Cg_UpdateFov();

	Cg_UpdateThirdPerson(&frame->ps);

	Cg_UpdateBob(&frame->ps);
}

/**
 * @brief Processes all entities, particles, emits, etc.. adding them to the view.
 * This is called once per frame by the engine, after Cg_UpdateView, and is run
 * in a separate thread while the renderer begins drawing the world.
 */
void Cg_PopulateView(const cl_frame_t *frame) {

	// add entities
	Cg_AddEntities(frame);

	// and client side emits
	Cg_AddEmits();

	// and client side effects
	Cg_AddEffects();

	// and finally particles
	Cg_AddParticles();
}
