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

#include "cl_local.h"

/*
 * Cl_ClearView
 */
static void Cl_ClearView(void) {

	// reset entity, light, particle and corona counts
	r_view.num_entities = r_view.num_lights = 0;
	r_view.num_particles = r_view.num_coronas = 0;

	// reset geometry counters
	r_view.bsp_polys = r_view.mesh_polys = 0;
}

/*
 * Cl_UpdateViewSize
 */
static void Cl_UpdateViewSize(void) {
	int size;

	if (!cl_view_size->modified && !r_view.update)
		return;

	if (cl_view_size->value < 40.0)
		Cvar_Set("cl_view_size", "40.0");
	if (cl_view_size->value > 100.0)
		Cvar_Set("cl_view_size", "100.0");

	size = cl_view_size->value;

	r_view.width = r_context.width * size / 100.0;
	r_view.height = r_context.height * size / 100.0;

	r_view.x = (r_context.width - r_view.width) / 2.0;
	r_view.y = (r_context.height - r_view.height) / 2.0;

	cl_view_size->modified = false;
}

/*
 * Cl_UpdateLerp
 */
static void Cl_UpdateLerp(cl_frame_t *from) {

	if (time_demo->value) {
		cl.time = cl.frame.server_time;
		cl.lerp = 1.0;
		return;
	}

	if (cl.time > cl.frame.server_time) {
		Com_Debug("Cl_UpdateViewValues: High clamp.\n");
		cl.time = cl.frame.server_time;
		cl.lerp = 1.0;
	} else if (cl.time < from->server_time) {
		Com_Debug("Cl_UpdateViewValues: Low clamp.\n");
		cl.time = from->server_time;
		cl.lerp = 0.0;
	} else {
		cl.lerp = (float) (cl.time - from->server_time) / (float) (cl.frame.server_time
				- from->server_time);
	}
}

/**
 * Cl_UpdateOrigin
 *
 * The origin is typically calculated using client sided prediction, provided
 * the client is not viewing a demo, playing in 3rd person mode, or chasing
 * another player.
 */
static void Cl_UpdateOrigin(player_state_t *ps, player_state_t *ops) {
	int i, ms;

	if (!cl.demo_server && !cl.third_person && cl_predict->value && !(cl.frame.ps.pmove.pm_flags
			& PMF_NO_PREDICTION)) {

		// use client sided prediction
		for (i = 0; i < 3; i++) {
			r_view.origin[i] = cl.predicted_origin[i] + cl.predicted_offset[i];
			r_view.origin[i] -= (1.0 - cl.lerp) * cl.prediction_error[i];
		}

		// lerp stairs over 50ms
		ms = cls.real_time - cl.predicted_step_time;

		if (ms < 50) // small step
			r_view.origin[2] -= cl.predicted_step * (50 - ms) * 0.02;
	} else { // just use interpolated values from frame
		for (i = 0; i < 3; i++) {
			r_view.origin[i] = ops->pmove.origin[i] + cl.lerp * (ps->pmove.origin[i]
					- ops->pmove.origin[i]);
			r_view.origin[i] += ops->pmove.view_offset[i] + cl.lerp * (ps->pmove.view_offset[i]
					- ops->pmove.view_offset[i]);
		}

		// scaled back to world coordinates
		VectorScale(r_view.origin, 0.125, r_view.origin);
	}

	// update the contents mask for e.g. under-water effects
	r_view.contents = R_PointContents(r_view.origin);
}

/**
 * Cl_UpdateAngles
 *
 * The angles are typically fetched directly from input, unless the client is
 * watching a demo or chasing someone.
 */
static void Cl_UpdateAngles(player_state_t *ps, player_state_t *ops) {

	// if not running a demo or chasing, use the predicted (input + delta) angles
	if (cl.frame.ps.pmove.pm_type <= PM_DEAD) {

		// interpolate small changes in delta angles
		short *od = ops->pmove.delta_angles;
		short *cd = ps->pmove.delta_angles;

		vec3_t old_delta, current_delta, delta;

		VectorSet(old_delta, SHORT2ANGLE(od[0]), SHORT2ANGLE(od[1]), SHORT2ANGLE(od[2]));
		VectorSet(current_delta, SHORT2ANGLE(cd[0]), SHORT2ANGLE(cd[1]), SHORT2ANGLE(cd[2]));

		VectorSubtract(current_delta, old_delta, delta);
		float f = VectorLength(delta);

		if (f > 0.0 && f < 15.0) {
			VectorLerp(old_delta, current_delta, cl.lerp, delta);
		} else {
			VectorCopy(current_delta, delta);
		}

		// the predicted angles include the raw frame delta, so back them out
		VectorSubtract(cl.predicted_angles, current_delta, r_view.angles);

		// and add the interpolated delta back in
		VectorAdd(r_view.angles, delta, r_view.angles);

		if (cl.frame.ps.pmove.pm_type == PM_DEAD) { // look only on x axis
			r_view.angles[0] = r_view.angles[2] = 0.0;
			r_view.angles[2] = 30.0;
		}
	} else { // for demos and chasing, simply interpolate between server states
		AngleLerp(ops->angles, ps->angles, cl.lerp, r_view.angles);
	}

	AngleVectors(r_view.angles, r_view.forward, r_view.right, r_view.up);
}

/**
 * Cl_UpdateView
 *
 * Updates the r_view_t for the renderer. Origin, angles, etc are calculated.
 * Scene population is then delegated to the client game.
 */
void Cl_UpdateView(void) {
	cl_frame_t *prev;
	player_state_t *ps, *ops;
	vec3_t delta;

	if (!cl.frame.valid && !r_view.update)
		return; // not a valid frame, and no forced update

	// find the previous frame to interpolate from
	prev = &cl.frames[(cl.frame.server_frame - 1) & UPDATE_MASK];

	if (prev->server_frame != cl.frame.server_frame - 1 || !prev->valid)
		prev = &cl.frame; // previous frame was dropped or invalid

	Cl_UpdateLerp(prev);

	ps = &cl.frame.ps;
	ops = &prev->ps;

	if (ps != ops) { // see if we've teleported
		VectorSubtract(ps->pmove.origin, ops->pmove.origin, delta);
		if (VectorLength(delta) > 256.0 * 8.0)
			ops = ps; // don't interpolate
	}

	Cl_ClearView();

	Cl_UpdateOrigin(ps, ops);

	Cl_UpdateAngles(ps, ops);

	Cl_UpdateViewSize();

	cls.cgame->UpdateView(&cl.frame);

	// set time in seconds
	r_view.time = cl.time * 0.001;

	// set area bits to mark visible leafs
	r_view.area_bits = cl.frame.area_bits;

	// create the thread which populates the view
	r_view.thread = Thread_Create(cls.cgame->PopulateView, &cl.frame);
}

/*
 * Cl_ViewSizeUp_f
 */
static void Cl_ViewSizeUp_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer + 10);
}

/*
 * Cl_ViewSizeDown_f
 */
static void Cl_ViewSizeDown_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer - 10);
}

/*
 * Cl_InitView
 */
void Cl_InitView(void) {
	Cmd_AddCommand("view_size_up", Cl_ViewSizeUp_f, 0, NULL);
	Cmd_AddCommand("view_size_down", Cl_ViewSizeDown_f, 0, NULL);
}
