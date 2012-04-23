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
#include "game/game.h"
#include "pmove.h"

/**
 * Cl_UsePrediction
 *
 * Returns true if client side prediction should be used.
 */
bool Cl_UsePrediction(void) {

	if (!cl_predict->value)
		return false;

	if (cls.state != CL_ACTIVE)
		return false;

	if (cl.demo_server || cl.third_person)
		return false;

	if (cl.frame.ps.pm_state.pm_flags & PMF_NO_PREDICTION)
		return false;

	if (cl.frame.ps.pm_state.pm_type == PM_FREEZE)
		return false;

	return true;
}

/*
 * Cl_CheckPredictionError
 */
void Cl_CheckPredictionError(void) {
	int frame;
	short delta[3];
	vec3_t fdelta;

	if (!cl_predict->value || (cl.frame.ps.pm_state.pm_flags & PMF_NO_PREDICTION))
		return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract(cl.frame.ps.pm_state.origin, cl.predicted_origins[frame], delta);

	VectorScale(delta, 0.125, fdelta); // denormalize back to floats

	if (VectorLength(fdelta) > 256.0) { // assume a teleport or something
		VectorClear(cl.prediction_error);
	} else { // save the prediction error for interpolation
		if (delta[0] || delta[1] || delta[2]) {
			Com_Debug("Cl_Predict: Miss on %i: %3.2f %3.2f %3.2f\n", cl.frame.server_frame,
					fdelta[0], fdelta[1], fdelta[2]);
		}

		VectorCopy(cl.frame.ps.pm_state.origin, cl.predicted_origins[frame]);
		VectorCopy(fdelta, cl.prediction_error);
	}
}

/*
 * Cl_ClipMoveToEntities
 */
static void Cl_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs,
		const vec3_t end, c_trace_t *tr) {
	int i;
	c_trace_t trace;
	int head_node;
	const float *angles;
	vec3_t bmins, bmaxs;

	for (i = 0; i < cl.frame.num_entities; i++) {
		const int num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[num];

		if (!ent->solid)
			continue;

		if (ent->number == cl.player_num + 1)
			continue;

		if (ent->solid == 31) { // special value for bmodel
			const c_model_t *model = cl.model_clip[ent->model1];
			if (!model)
				continue;
			head_node = model->head_node;
			angles = ent->angles;
		} else { // encoded bbox
			const int x = 8 * (ent->solid & 31);
			const int zd = 8 * ((ent->solid >> 5) & 31);
			const int zu = 8 * ((ent->solid >> 10) & 63) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			head_node = Cm_HeadnodeForBox(bmins, bmaxs);
			angles = vec3_origin; // boxes don't rotate
		}

		trace = Cm_TransformedBoxTrace(start, end, mins, maxs, head_node, MASK_PLAYER_SOLID,
				ent->origin, angles);

		if (trace.start_solid || trace.fraction < tr->fraction) {
			trace.ent = (struct g_edict_s *) ent;
			*tr = trace;
		}
	}
}

/*
 * Cl_PredictMovement_Trace
 */
static c_trace_t Cl_PredictMovement_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs,
		const vec3_t end) {
	c_trace_t t;

	// check against world
	t = Cm_BoxTrace(start, end, mins, maxs, 0, MASK_PLAYER_SOLID);
	if (t.fraction < 1.0)
		t.ent = (struct g_edict_s *) 1;

	// check all other solid models
	Cl_ClipMoveToEntities(start, mins, maxs, end, &t);

	return t;
}

/*
 * Cl_PredictMovement_PointContents
 */
static int Cl_PredictMovement_PointContents(const vec3_t point) {
	int i;
	c_model_t *model;
	int contents;

	contents = Cm_PointContents(point, r_world_model->first_node);

	for (i = 0; i < cl.frame.num_entities; i++) {
		const int num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[num];

		if (ent->solid != 31) // special value for bsp models
			continue;

		model = cl.model_clip[ent->model1];
		if (!model)
			continue;

		contents |= Cm_TransformedPointContents(point, model->head_node, ent->origin, ent->angles);
	}

	return contents;
}

/**
 * Cl_PredictMovement
 *
 * Run the latest movement command through the player movement code locally,
 * using the resulting origin and angles to reduce perceived latency.
 */
void Cl_PredictMovement(void) {
	pm_move_t pm;

	if (!Cl_UsePrediction())
		return;

	const unsigned int current = cls.netchan.outgoing_sequence;
	unsigned int ack = cls.netchan.incoming_acknowledged;

	// if we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP) {
		Com_Warn("Cl_PredictMovement: Exceeded CMD_BACKUP.\n");
		return;
	}

	// copy current state to pmove
	memset(&pm, 0, sizeof(pm));
	pm.s = cl.frame.ps.pm_state;

	pm.ground_entity = cl.predicted_ground_entity;

	pm.Trace = Cl_PredictMovement_Trace;
	pm.PointContents = Cl_PredictMovement_PointContents;

	// run frames
	while (++ack <= current) {
		const unsigned int frame = ack & CMD_MASK;
		const user_cmd_t *cmd = &cl.cmds[frame];

		if (!cmd->msec)
			continue;

		pm.cmd = *cmd;
		Pmove(&pm);

		// save for debug checking
		VectorCopy(pm.s.origin, cl.predicted_origins[frame]);
	}

	const float step = pm.s.origin[2] * 0.125 - cl.predicted_origin[2];

	if ((pm.s.pm_flags & PMF_ON_STAIRS) && (step >= 4.0 || step <= -4.0)) { // save for stair interpolation
		cl.predicted_step_time = cls.real_time;
		cl.predicted_step = step;
	}

	// copy results out for rendering
	UnpackPosition(pm.s.origin, cl.predicted_origin);
	UnpackPosition(pm.s.view_offset, cl.predicted_offset);

	UnpackAngles(pm.cmd.angles, cl.predicted_angles);

	cl.predicted_ground_entity = pm.ground_entity;
}
