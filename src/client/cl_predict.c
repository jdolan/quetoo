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
#include "pmove.h"

/*
 * @brief Returns true if client side prediction should be used.
 */bool Cl_UsePrediction(void) {

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
 * @brief Checks for client side prediction errors. Problems here mean that
 * Pmove or the protocol are not functioning correctly.
 */
void Cl_CheckPredictionError(void) {
	int16_t d[3];
	vec3_t delta;

	if (!cl_predict->value || (cl.frame.ps.pm_state.pm_flags & PMF_NO_PREDICTION))
		return;

	// calculate the last user_cmd_t we sent that the server has processed
	const uint32_t frame = (cls.netchan.incoming_acknowledged & CMD_MASK);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract(cl.frame.ps.pm_state.origin, cl.predicted_origins[frame], d);
	UnpackPosition(d, delta); // convert back to floating point

	const float error = VectorLength(delta);

	if (error > 256.0) { // assume a teleport or something
		VectorClear(cl.prediction_error);
	} else { // save the prediction error for interpolation
		if (error > 4.0) {
			Com_Debug("%s\n", vtos(delta));
		}

		VectorCopy(cl.frame.ps.pm_state.origin, cl.predicted_origins[frame]);
		VectorCopy(delta, cl.prediction_error);
	}
}

/*
 * @brief Clips the intended movement to all solid entities the client knows of.
 */
static void Cl_PredictMovement_Trace_Clip(const vec3_t start, const vec3_t mins, const vec3_t maxs,
		const vec3_t end, c_trace_t *tr) {
	int32_t i;

	for (i = 0; i < cl.frame.num_entities; i++) {
		const int32_t num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[num];

		int32_t head_node;
		const float *angles;

		c_trace_t trace;

		if (!ent->solid)
			continue;

		if (ent->number == cl.player_num + 1)
			continue;

		if (ent->solid == 31) { // special value for bsp model
			const c_model_t *model = cl.model_clip[ent->model1];
			if (!model)
				continue;
			head_node = model->head_node;
			angles = ent->angles;
		} else { // encoded bbox
			const int32_t x = 8 * (ent->solid & 31);
			const int32_t zd = 8 * ((ent->solid >> 5) & 31);
			const int32_t zu = 8 * ((ent->solid >> 10) & 63) - 32;
			vec3_t bmins, bmaxs;

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
 * @brief
 */
static c_trace_t Cl_PredictMovement_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs,
		const vec3_t end) {
	c_trace_t t;

	// check against world
	t = Cm_BoxTrace(start, end, mins, maxs, 0, MASK_PLAYER_SOLID);
	if (t.fraction < 1.0)
		t.ent = (struct g_edict_s *) 1;

	// check all other solid models
	Cl_PredictMovement_Trace_Clip(start, mins, maxs, end, &t);

	return t;
}

/*
 * @brief
 */
static int32_t Cl_PredictMovement_PointContents(const vec3_t point) {
	int32_t i;
	c_model_t *model;
	int32_t contents;

	contents = Cm_PointContents(point, 0);

	for (i = 0; i < cl.frame.num_entities; i++) {
		const int32_t num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
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

/*
 * @brief Run the latest movement command through the player movement code locally,
 * using the resulting origin and angles to reduce perceived latency.
 */
void Cl_PredictMovement(void) {
	pm_move_t pm;

	if (!Cl_UsePrediction())
		return;

	const uint32_t current = cls.netchan.outgoing_sequence;
	uint32_t ack = cls.netchan.incoming_acknowledged;

	// if we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP) {
		Com_Debug("Exceeded CMD_BACKUP\n");
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
		const uint32_t frame = ack & CMD_MASK;
		const user_cmd_t *cmd = &cl.cmds[frame];

		if (!cmd->msec)
			continue;

		pm.cmd = *cmd;
		Pmove(&pm);

		// for each movement, check for stair interaction and interpolate
		const float step = pm.s.origin[2] * 0.125 - cl.predicted_origin[2];

		if ((pm.s.pm_flags & PMF_ON_STAIRS) && fabs(step) >= 8.0) {
			cl.predicted_step_time = cls.real_time;
			cl.predicted_step = step;
		}

		// save for debug checking
		VectorCopy(pm.s.origin, cl.predicted_origins[frame]);
	}

	// copy results out for rendering
	UnpackPosition(pm.s.origin, cl.predicted_origin);
	UnpackPosition(pm.s.view_offset, cl.predicted_offset);

	UnpackAngles(pm.cmd.angles, cl.predicted_angles);

	cl.predicted_ground_entity = pm.ground_entity;
}

/*
 * @brief Ensures client-side prediction has the current collision model at its
 * disposal.
 */
void Cl_UpdatePrediction(void) {
	int32_t i;

	// ensure the world model is loaded
	if (!Com_WasInit(Q2W_SERVER) || !Cm_NumModels()) {
		int32_t bs;

		const char *bsp_name = cl.config_strings[CS_MODELS];
		const int bsp_size = atoi(cl.config_strings[CS_BSP_SIZE]);

		Cm_LoadBsp(bsp_name, &bs);

		if (bs != bsp_size) {
			Com_Error(ERR_DROP, "Local map version differs from server: %i != %i\n", bs, bsp_size);
		}
	}

	// load the inline models for prediction as well
	for (i = 1; i < MAX_MODELS && cl.config_strings[CS_MODELS + i][0]; i++) {

		const char *s = cl.config_strings[CS_MODELS + i];
		if (*s == '*')
			cl.model_clip[i] = Cm_Model(cl.config_strings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;
	}
}
