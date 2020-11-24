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

cg_view_t cg_view;

/**
 * @brief Update the field of view, which affects the view port as well as the culling
 * frustum.
 */
static void Cg_UpdateFov(void) {

	if (!cg_fov->modified && !cgi.view->update) {
		return;
	}

	cg_fov->value = Clampf(cg_fov->value, 10.0, 160.0);
	cg_fov_interpolate->value = Clampf(cg_fov_interpolate->value, 0.0, 10.0);

	float fov = cg_fov->value;

	if (cg_fov_interpolate->value && cgi.view->fov.x && cgi.view->fov.y) {
		static float prev, next;
		static uint32_t time;

		if (next && next != cg_fov->value) {
			time = 0;
		}

		if (time == 0) {
			prev = cgi.view->fov.x * 2.0;
			next = cg_fov->value;
			time = cgi.client->unclamped_time;
		}

		const float frac = (cgi.client->unclamped_time - time) / (cg_fov_interpolate->value * 100.0);
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

	cgi.view->fov.x = fov / 2.0;

	const float x = cgi.context->width / tanf(Radians(fov));
	const float y = atan2f(cgi.context->height, x);
	const float a = cgi.context->height / (float ) cgi.context->width;

	cgi.view->fov.y = Degrees(y) * a / 2.0;
}

/**
 * @brief Update the third person offset, if any. This is used as a client-side
 * option, or as the default chase camera view.
 */
static void Cg_UpdateThirdPerson(const player_state_t *ps) {
	vec3_t forward, right, up, origin, point;

	const vec3_t mins = Vec3(-16.0, -16.0, -16.0);
	const vec3_t maxs = Vec3( 16.0,  16.0,  16.0);

	if (cg_third_person->value || (cg_third_person_chasecam->value && ps->stats[STAT_CHASE])) {
		cgi.client->third_person = true;
	} else {
		cgi.client->third_person = false;
		return;
	}

	const vec3_t offset = Vec3(
		cg_third_person_x->value,
		cg_third_person_y->value,
		cg_third_person_z->value
	);

	const vec3_t angles = Vec3_ClampEuler(Vec3(
		cgi.view->angles.x + cg_third_person_pitch->value,
		cgi.view->angles.y + cg_third_person_yaw->value,
		cgi.view->angles.z
	));

	const float yaw = angles.y;

	Vec3_Vectors(angles, &forward, &right, &up);

	point = Vec3_Add(cgi.view->origin, Vec3_Scale(forward, 512.0));

	origin = cgi.view->origin;

	origin = Vec3_Add(origin, Vec3_Scale(up, offset.z));
	origin = Vec3_Add(origin, Vec3_Scale(right, offset.y));
	origin = Vec3_Add(origin, Vec3_Scale(forward, offset.x));

	const cm_trace_t tr = cgi.Trace(cgi.view->origin, origin, mins, maxs, 0, CONTENTS_MASK_CLIP_PLAYER);
	cgi.view->origin = tr.end;

	point = Vec3_Subtract(point, cgi.view->origin);
	cgi.view->angles = Vec3_Euler(Vec3_Normalize(point));
	cgi.view->angles.y = yaw;

	Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Periodically calculates the player's horizontal speed, and interpolates it
 * over a small interval to smooth out rapid changes in velocity.
 */
static float Cg_BobSpeedModulus(const player_state_t *ps) {
	static float old_speed, new_speed;
	static uint32_t time;

	if (cgi.client->unclamped_time < time) {
		time = 0;
		old_speed = new_speed = 0.0;
	}

	float speed;

	const uint32_t delta = cgi.client->unclamped_time - time;
	if (delta < 200) {
		const float lerp = delta / (float) 200;
		speed = old_speed + lerp * (new_speed - old_speed);
	} else {
		const _Bool ducked = ps->pm_state.flags & PMF_DUCKED;
		const float max_speed = ducked ? PM_SPEED_DUCKED : PM_SPEED_AIR;

		vec3_t velocity = ps->pm_state.velocity;
		velocity.z = 0.0;

		old_speed = new_speed;
		new_speed = Vec3_Length(velocity) / max_speed;
		new_speed = Clampf(new_speed, 0.0, 1.0);
		speed = old_speed;

		time = cgi.client->unclamped_time;
	}

	return 0.66 + speed;
}

/**
 * @brief Calculate the view bob. This is done using a running time counter and a
 * simple sin function. The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cg_UpdateBob(const player_state_t *ps) {
	static uint32_t bob, time;

	if (!cg_bob->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->pm_state.type > PM_HOOK_SWING) {

		// if we're frozen and not chasing, don't bob
		if (!ps->stats[STAT_CHASE]) {
			return;
		}
	}

	if (cg_bob->modified) {
		cgi.SetCvarValue(cg_bob->name, Clampf(cg_bob->value, 0.0, 2.0));
		cg_bob->modified = false;
	}

	if (cgi.client->unclamped_time < time) {
		bob = time = 0;
	}

	const float mod = Cg_BobSpeedModulus(ps);

	// then calculate how much bob to add this frame
	float frame_bob = Clampf(cgi.client->unclamped_time - time, 1u, 1000u) * mod;

	if (!(ps->pm_state.flags & PMF_ON_GROUND)) {
		frame_bob *= 0.25;
	}

	bob += frame_bob;
	time = cgi.client->unclamped_time;

	cg_view.bob = sinf(0.0066 * bob) * mod * mod;
	cg_view.bob *= cg_bob->value; // scale via cvar too

	cgi.view->origin = Vec3_Add(cgi.view->origin, Vec3_Scale(cgi.view->forward, -cg_view.bob));
	cgi.view->origin = Vec3_Add(cgi.view->origin, Vec3_Scale(cgi.view->right,  cg_view.bob));
	cgi.view->origin = Vec3_Add(cgi.view->origin, Vec3_Scale(cgi.view->up,  cg_view.bob));
}

/**
 * @brief Resolves the view origin for the pending frame.
 * @param ps0 The player state to interpolate from.
 * @param ps1 The player state to interpolate to.
 */
static void Cg_UpdateOrigin(const player_state_t *ps0, const player_state_t *ps1) {

	if (Cg_UsePrediction()) {
		const cl_predicted_state_t *pr = &cgi.client->predicted_state;

		cgi.view->origin = Vec3_Add(pr->view.origin, pr->view.offset);
		cgi.view->origin = Vec3_Add(cgi.view->origin, Vec3_Scale(pr->error, -(1.0 - cgi.client->lerp)));

		Cg_InterpolateStep(&cgi.client->predicted_state.step);
		cgi.view->origin.z -= cgi.client->predicted_state.step.delta_height;

	} else {
		cgi.view->origin = Vec3_Mix(ps0->pm_state.origin, ps1->pm_state.origin, cgi.client->lerp);
		const vec3_t offset = Vec3_Mix(ps0->pm_state.view_offset, ps1->pm_state.view_offset, cgi.client->lerp);

		cgi.view->origin = Vec3_Add(cgi.view->origin, offset);

		const cl_entity_t *ent = Cg_Self();
		if (ent) {
			if (ent->step.delta_height) {
				cgi.view->origin.z = ps1->pm_state.origin.z - ent->step.delta_height + offset.z;
			}
		}
	}
}

/**
 * @brief Resolves the view angles for the pending frame.
 * @param ps0 The player state to interpolate from.
 * @param ps1 The player state to interpolate to.
 */
static void Cg_UpdateAngles(const player_state_t *ps0, const player_state_t *ps1) {
	vec3_t angles, angles0, angles1;

	if (Cg_UsePrediction()) {
		const cl_predicted_state_t *pr = &cgi.client->predicted_state;
		cgi.view->angles = pr->view.angles;
	} else {

		angles0 = ps0->pm_state.view_angles;
		angles1 = ps1->pm_state.view_angles;

		cgi.view->angles = Vec3_MixEuler(angles0, angles1, cgi.client->lerp);
	}

	angles0 = ps0->pm_state.delta_angles;
	angles1 = ps1->pm_state.delta_angles;

	angles = angles1;

	// for small delta angles, such as riding a rotator, interpolate them
	if (!Vec3_Equal(angles0, angles1)) {
		int32_t i;

		for (i = 0; i < 3; i++) {
			const float delta = fabs(angles1.xyz[i] - angles0.xyz[i]);
			if (delta > 5.0 && delta < 355.0) {
				break;
			}
		}

		if (i == 3) {
			angles = Vec3_MixEuler(angles0, angles1, cgi.client->lerp);
		}
	}

	cgi.view->angles = Vec3_Add(cgi.view->angles, angles);

	if (ps1->pm_state.type == PM_DEAD) {
		cgi.view->angles.x = 0.0;
	} else if (ps1->pm_state.type == PM_FREEZE) {
		cgi.client->angles = cgi.view->angles;
	}
}

/**
 * @brief Updates the view definition. The camera origin, view angles, and field of view are each
 * calculated here. Other modifications can be made at your own risk. This is called potentially
 * several times per client frame by the engine to prepare the view for frame interpolation.
 */
void Cg_UpdateView(const cl_frame_t *frame) {

	const player_state_t *ps0;

	if (cgi.client->previous_frame) {
		ps0 = &cgi.client->previous_frame->ps;
	} else {
		ps0 = &frame->ps;
	}

	const player_state_t *ps1 = &frame->ps;

	Cg_UpdateOrigin(ps0, ps1);

	Cg_UpdateAngles(ps0, ps1);

	Cg_UpdateThirdPerson(ps1);

	Cg_UpdateFov();

	Cg_UpdateBob(ps1);

	Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);

	cgi.view->contents = cgi.PointContents(cgi.view->origin);

	Cg_AddEntities(frame);

	Cg_AddEffects();

	Cg_AddSprites();

	Cg_AddLights();
}
