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
 * Cl_UpdateFov
 */
static void Cl_UpdateFov(void) {

	if (!cl_fov->modified && !r_view.update)
		return;

	if (cl_fov->value < 10.0 || cl_fov->value > 179.0)
		Cvar_Set("cl_fov", "100.0");

	const float x = r_context.width / tan(cl_fov->value / 360.0 * M_PI);

	const float a = atan(r_context.height / x);

	r_view.fov[0] = cl_fov->value;

	r_view.fov[1] = a * 360.0 / M_PI;

	cl_fov->modified = false;
}

/*
 * Cl_UpdateViewsize
 */
static void Cl_UpdateViewsize(void) {
	int size;

	if (!cl_view_size->modified && !r_view.update)
		return;

	if (cl_view_size->value < 40.0)
		Cvar_Set("cl_viewsize", "40.0");
	if (cl_view_size->value > 100.0)
		Cvar_Set("cl_viewsize", "100.0");

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
		cl.lerp = (float) (cl.time - from->server_time)
				/ (float) (cl.frame.server_time - from->server_time);
	}
}

/*
 * Cl_UpdateDucking
 */
static void Cl_UpdateDucking(void) {
	static unsigned int ducktime, standtime;
	vec3_t mins, maxs;
	float height, view_height;

	VectorScale(PM_MINS, PM_SCALE, mins);
	VectorScale(PM_MAXS, PM_SCALE, maxs);

	height = maxs[2] - mins[2];
	view_height = mins[2] + (height * 0.75);

	if (cl.frame.ps.pmove.pm_flags & PMF_DUCKED) {

		if (standtime > ducktime) // go back down
			ducktime = cls.real_time + (cls.real_time - standtime);
		else if (!ducktime) // or just start to duck
			ducktime = cls.real_time + 200;

		if (ducktime > cls.real_time)
			r_view.origin[2] += (ducktime - cls.real_time) * view_height / 200;

		return;
	}

	if (ducktime > standtime) // currently ducked, but able to begin standing
		standtime = cls.real_time;

	if (cls.real_time - standtime <= 200) { // rise
		r_view.origin[2] += (cls.real_time - standtime) * view_height / 200;
	} else { // cancel ducking, add normal height
		ducktime = standtime = 0;
		r_view.origin[2] += view_height;
	}
}

/*
 * Cl_UpdateOrigin
 *
 * The origin is typically calculated using client sided prediction, provided
 * the client is not viewing a demo, playing in 3rd person mode, or chasing
 * another player.
 */
static void Cl_UpdateOrigin(player_state_t *ps, player_state_t *ops) {
	int i, ms;

	if (!cl.demo_server && !cl_third_person->value && cl_predict->value
			&& !(cl.frame.ps.pmove.pm_flags & PMF_NO_PREDICTION)) {

		// use client sided prediction
		for (i = 0; i < 3; i++)
			r_view.origin[i] = cl.predicted_origin[i] - (1.0 - cl.lerp)
					* cl.prediction_error[i];

		// lerp stairs over 50ms
		ms = cls.real_time - cl.predicted_step_time;

		if (ms < 50) // small step
			r_view.origin[2] -= cl.predicted_step * (50 - ms) * 0.02;
	} else { // just use interpolated values from frame
		for (i = 0; i < 3; i++)
			r_view.origin[i] = ops->pmove.origin[i] + cl.lerp
					* (ps->pmove.origin[i] - ops->pmove.origin[i]);

		// scaled back to world coordinates
		VectorScale(r_view.origin, 0.125, r_view.origin);
	}

	// add any ducking
	Cl_UpdateDucking();
}

/*
 * Cl_UpdateAngles
 *
 * The angles are typically fetched directly from input, unless the client is
 * watching a demo or chasing someone.
 */
static void Cl_UpdateAngles(player_state_t *ps, player_state_t *ops) {

	// if not running a demo or chasing, add the local angle movement
	if (cl.frame.ps.pmove.pm_type <= PM_DEAD) { // use predicted (input) values
		VectorCopy(cl.predicted_angles, r_view.angles);

		if (cl.frame.ps.pmove.pm_type == PM_DEAD) // look only on x axis
			r_view.angles[0] = r_view.angles[2] = 0.0;
	} else { // for demos and chasing, lerp between states without prediction
		AngleLerp(ops->angles, ps->angles, cl.lerp, r_view.angles);
	}

	AngleVectors(r_view.angles, r_view.forward, r_view.right, r_view.up);
}

/*
 * Cl_UpdateVelocity
 */
static void Cl_UpdateVelocity(player_state_t *ps, player_state_t *ops) {
	vec3_t old_vel, new_vel;

	VectorCopy(ops->pmove.velocity, old_vel);
	VectorCopy(ps->pmove.velocity, new_vel);

	// lerp it
	VectorMix(old_vel, new_vel, cl.lerp, r_view.velocity);

	// convert back to float
	VectorScale(r_view.velocity, 0.125, r_view.velocity);
}

/*
 * Cl_UpdateThirdperson
 */
static void Cl_UpdateThirdperson(player_state_t *ps) {
	vec3_t angles, forward, dest;
	float dist;

	if (!ps->stats[STAT_CHASE]) { // chasing uses client side 3rd person

		// if we're spectating, don't translate the origin because we have
		// no visible player model to begin with
		if (ps->pmove.pm_type == PM_SPECTATOR && !ps->stats[STAT_HEALTH])
			return;

		if (!cl_third_person->value)
			return;
	}

	// we're either chasing, or intentionally using 3rd person
	VectorCopy(r_view.angles, angles);

	if (cl_third_person->value < 0.0) // in front of the player
		angles[1] += 180.0;

	AngleVectors(angles, forward, NULL, NULL);

	dist = cl_third_person->value;

	if (!dist)
		dist = 1.0;

	dist = fabs(100.0 * dist);

	// project the view origin back and up for 3rd person
	VectorMA(r_view.origin, -dist, forward, dest);
	dest[2] += 20.0;

	// clip it to the world
	R_Trace(r_view.origin, dest, 5.0, MASK_SHOT);
	VectorCopy(r_view.trace.end, r_view.origin);

	// adjust view angles to compensate for height offset
	VectorMA(r_view.origin, 2048.0, forward, dest);
	VectorSubtract(dest, r_view.origin, dest);

	// copy angles back to view
	VectorAngles(dest, r_view.angles);
	AngleVectors(r_view.angles, r_view.forward, r_view.right, r_view.up);
}

/*
 * Cl_UpdateBob
 *
 * Calculate the view bob.  This is done using a running time counter and a
 * simple sin function.  The player's speed, as well as whether or not they
 * are on the ground, determine the bob frequency and amplitude.
 */
static void Cl_UpdateBob(void) {
	static float time, vtime;
	float ftime, speed;
	vec3_t velocity;

	if (!cl_bob->value)
		return;

	if (cl_third_person->value)
		return;

	if (cl.frame.ps.pmove.pm_type != PM_NORMAL)
		return;

	VectorCopy(r_view.velocity, velocity);
	velocity[2] = 0.0;

	speed = VectorLength(velocity) / 450.0;

	if (speed > 1.0)
		speed = 1.0;

	ftime = r_view.time - vtime;

	if (ftime < 0.0) // clamp for level changes
		ftime = 0.0;

	ftime *= (1.0 + speed * 1.0 + speed);

	if (!r_view.ground)
		ftime *= 0.25;

	time += ftime;
	vtime = r_view.time;

	r_view.bob = sin(3.5 * time) * (0.5 + speed) * (0.5 + speed);

	r_view.bob *= cl_bob->value; // scale via cvar too

	VectorMA(r_view.origin, -r_view.bob, r_view.forward, r_view.origin);
	VectorMA(r_view.origin, r_view.bob, r_view.right, r_view.origin);
	VectorMA(r_view.origin, r_view.bob, r_view.up, r_view.origin);
}

/*
 * Cl_PopulateView
 *
 * Processes all entities, particles, emits, etc.. adding them to the view.
 * This is all done in a separate thread while the main thread begins drawing
 * the world.
 */
static void Cl_PopulateView(void *data) {

	// clear state from the previous frame
	Cl_ClearView();

	// add entities
	Cl_AddEntities(&cl.frame);

	// and particles
	Cl_AddParticles();

	// and client sided emits
	Cl_AddEmits();
}

/*
 * Cl_UpdateView
 *
 * Updates the view_t for the renderer.  Origin, angles, etc are calculated.
 * Entities, particles, etc are then lerped and added and pulled through to
 * the renderer.
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
			ops = ps; // don't lerp
	}

	Cl_UpdateOrigin(ps, ops);

	Cl_UpdateAngles(ps, ops);

	Cl_UpdateVelocity(ps, ops);

	Cl_UpdateThirdperson(ps);

	Cl_UpdateBob();

	Cl_UpdateFov();

	Cl_UpdateViewsize();

	// set time in seconds
	r_view.time = cl.time * 0.001;

	// inform the renderer if the client is on the ground
	r_view.ground = ps->pmove.pm_flags & PMF_ON_GROUND;

	// set area bits to mark visible leafs
	r_view.area_bits = cl.frame.area_bits;

	// create the thread which populates the view
	r_view.thread = Thread_Create(Cl_PopulateView, NULL);
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
	Cmd_AddCommand("view_size_up", Cl_ViewSizeUp_f, NULL);
	Cmd_AddCommand("view_size_down", Cl_ViewSizeDown_f, NULL);
}
