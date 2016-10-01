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

#include "cl_local.h"

/**
 * @brief Clears all volatile view members so that a new scene may be populated.
 */
static void Cl_ClearView(void) {

	// reset entity, light, particle and corona counts
	r_view.num_entities = r_view.num_lights = 0;
	r_view.num_particles = r_view.num_coronas = 0;

	// reset counters
	r_view.num_bind_texture = r_view.num_bind_lightmap = r_view.num_bind_deluxemap = 0;
	r_view.num_bind_normalmap = r_view.num_bind_specularmap = 0;

	r_view.num_bsp_surfaces = 0;

	r_view.num_mesh_models = r_view.num_mesh_tris = 0;
}

/**
 * @brief
 */
static void Cl_UpdateViewSize(void) {

	if (!cl_view_size->modified && !r_view.update)
		return;

	Cvar_SetValue(cl_view_size->name, Clamp(cl_view_size->value, 40.0, 100.0));

	r_view.viewport.w = r_context.width * cl_view_size->value / 100.0;
	r_view.viewport.h = r_context.height * cl_view_size->value / 100.0;

	r_view.viewport.x = (r_context.width - r_view.viewport.w) / 2.0;
	r_view.viewport.y = (r_context.height - r_view.viewport.h) / 2.0;

	cl_view_size->modified = false;
}

/**
 * @brief The origin is typically calculated using client sided prediction, provided
 * the client is not viewing a demo, playing in 3rd person mode, or chasing
 * another player.
 */
static void Cl_UpdateOrigin(const player_state_t *from, const player_state_t *to) {

	if (Cl_UsePrediction()) {

		// use client sided prediction
		for (int32_t i = 0; i < 3; i++) {
			r_view.origin[i] = cl.predicted_state.origin[i] + cl.predicted_state.view_offset[i];
			r_view.origin[i] -= (1.0 - cl.lerp) * cl.predicted_state.error[i];
		}

		const uint32_t delta = cl.time - cl.predicted_state.step_time;
		const uint32_t interval = cl.predicted_state.step_interval;

		if (delta < interval) { // interpolate stair traversal
			const vec_t lerp = (interval - delta) / (vec_t) interval;
			r_view.origin[2] -= cl.predicted_state.step * lerp;
		}

	} else { // just use interpolated values from frame
		vec3_t origin;
		vec3_t from_offset, to_offset, offset;

		VectorLerp(from->pm_state.origin, to->pm_state.origin, cl.lerp, origin);

		UnpackVector(from->pm_state.view_offset, from_offset);
		UnpackVector(to->pm_state.view_offset, to_offset);

		VectorLerp(from_offset, to_offset, cl.lerp, offset);

		VectorAdd(origin, offset, r_view.origin);
	}

	// update the contents mask for e.g. under-water effects
	r_view.contents = Cl_PointContents(r_view.origin);
}

/**
 * @brief The angles are typically fetched from input, after factoring in client-side
 * prediction, unless the client is watching a demo or chase camera.
 */
static void Cl_UpdateAngles(const player_state_t *from, const player_state_t *to) {
	vec3_t old_angles, new_angles, angles;

	// start with the predicted angles, or interpolate the server states
	if (Cl_UsePrediction()) {
		VectorCopy(cl.predicted_state.view_angles, r_view.angles);
	} else {
		UnpackAngles(from->pm_state.view_angles, old_angles);
		UnpackAngles(to->pm_state.view_angles, new_angles);

		AngleLerp(old_angles, new_angles, cl.lerp, r_view.angles);
	}

	// add in the kick angles
	UnpackAngles(from->pm_state.kick_angles, old_angles);
	UnpackAngles(to->pm_state.kick_angles, new_angles);

	AngleLerp(old_angles, new_angles, cl.lerp, angles);
	VectorAdd(r_view.angles, angles, r_view.angles);

	// and lastly the delta angles
	UnpackAngles(from->pm_state.delta_angles, old_angles);
	UnpackAngles(to->pm_state.delta_angles, new_angles);

	VectorCopy(new_angles, angles);

	// check for small delta angles, and interpolate them
	if (!VectorCompare(old_angles, new_angles)) {
		int32_t i;

		for (i = 0; i < 3; i++) {
			const vec_t delta = fabs(new_angles[i] - old_angles[i]);
			if (delta > 5.0 && delta < 355.0) {
				break;
			}
		}

		if (i == 3) {
			AngleLerp(old_angles, new_angles, cl.lerp, angles);
		}
	}

	VectorAdd(r_view.angles, angles, r_view.angles);

	if (cl.frame.ps.pm_state.type == PM_DEAD) { // look only on x axis
		r_view.angles[0] = 0.0;
		r_view.angles[2] = 45.0;
	}

	// and finally set the view directional vectors
	AngleVectors(r_view.angles, r_view.forward, r_view.right, r_view.up);
}

/**
 * @brief Updates the r_view_t for the renderer. Origin, angles, etc are calculated.
 * Scene population is then delegated (asynchronously) to the client game.
 */
void Cl_UpdateView(void) {

	if (!cl.frame.valid && !r_view.update)
		return; // not a valid frame, and no forced update

	Cl_ClearView();

	const cl_frame_t *frame = &cl.frames[(cl.frame.frame_num - 1) & PACKET_MASK];
	const player_state_t *from = frame->frame_num == cl.frame.frame_num - 1 ? &frame->ps : &cl.frame.ps;

	Cl_UpdateOrigin(from, &cl.frame.ps);

	Cl_UpdateAngles(from, &cl.frame.ps);

	Cl_UpdateViewSize();

	cls.cgame->UpdateView(&cl.frame);

	// set time
	r_view.time = cl.systime;

	// set area bits to mark visible leafs
	r_view.area_bits = cl.frame.area_bits;

	// create the thread which populates the view
	r_view.thread = Thread_Create((ThreadRunFunc) cls.cgame->PopulateView, &cl.frame);
}

/**
 * @brief
 */
static void Cl_ViewSizeUp_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer + 10);
}

/**
 * @brief
 */
static void Cl_ViewSizeDown_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer - 10);
}

/**
 * @brief
 */
void Cl_InitView(void) {
	Cmd_Add("cl_view_size_up", Cl_ViewSizeUp_f, CMD_CLIENT, NULL);
	Cmd_Add("cl_view_size_down", Cl_ViewSizeDown_f, CMD_CLIENT, NULL);
}
