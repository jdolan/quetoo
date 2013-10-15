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
 * @brief Returns true if client side prediction should be used. The actual
 * movement is handled by the client game.
 */
_Bool Cl_UsePrediction(void) {

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
 * @brief
 */
int32_t Cl_PointContents(const vec3_t point) {

	int32_t i, c = Cm_PointContents(point, 0);

	for (i = 0; i < cl.frame.num_entities; i++) {
		const int32_t num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[num];

		if (ent->solid == 31) { // clip to bsp submodels
			const c_model_t *mod = cl.model_clip[ent->model1];
			if (!mod) {
				Com_Warn("Invalid clip model %d", ent->model1);
				continue;
			}

			c |= Cm_TransformedPointContents(point, mod->head_node, ent->origin, ent->angles);
		}
	}

	return c;
}

/*
 * @brief A structure facilitating clipping to SOLID_BOX entities.
 */
typedef struct {
	const vec_t *mins, *maxs;
	const vec_t *start, *end;
	c_trace_t trace;
	uint16_t skip;
	int32_t contents;
} cl_trace_t;

/*
 * @brief Clips the specified trace to other solid entities in the frame.
 */
static void Cl_ClipTraceToEntities(cl_trace_t *trace) {
	int32_t i;

	// check all other solid models
	for (i = 0; i < cl.frame.num_entities; i++) {
		const int32_t num = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[num];

		if (ent->number == trace->skip)
			continue;

		if (ent->number == cl.player_num + 1)
			continue;

		if (ent->solid == SOLID_NOT)
			continue;

		int32_t head_node;
		const vec_t *angles;
		if (ent->solid == SOLID_BSP) {

			const c_model_t *mod = cl.model_clip[ent->model1];
			if (!mod) {
				Com_Warn("Invalid clip model: %d\n", ent->model1);
				continue;
			}

			if (!mod->head_node) {
				Com_Debug("No head_node for %d\n", ent->model1);
				continue;
			}

			head_node = mod->head_node;
			angles = ent->angles;
		} else { // something with an encoded box

			vec3_t emins, emaxs;
			UnpackBounds(ent->solid, emins, emaxs);

			head_node = Cm_HeadnodeForBox(emins, emaxs);
			angles = vec3_origin;
		}

		c_trace_t tr = Cm_TransformedBoxTrace(trace->start, trace->end, trace->mins, trace->maxs,
				head_node, trace->contents, ent->origin, angles);

		if (tr.all_solid || tr.start_solid || tr.fraction < trace->trace.fraction) {
			trace->trace = tr;
			trace->trace.ent = (struct g_edict_s *) (intptr_t) ent->number;
		}
	}
}

/*
 * @brief Client-side collision model tracing. This is the reciprocal of
 * Sv_Trace.
 *
 * @param skip An optional entity number for which all tests are skipped. Pass
 * 0 for none, because entity 0 is the world, which we always test.
 */
c_trace_t Cl_Trace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		const uint16_t skip, const int32_t contents) {

	cl_trace_t trace;

	memset(&trace, 0, sizeof(trace));

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// clip to world
	trace.trace = Cm_BoxTrace(start, end, mins, maxs, 0, contents);
	if (trace.trace.fraction < 1.0) {
		trace.trace.ent = (struct g_edict_s *) (intptr_t) -1;

		if (trace.trace.start_solid) { // blocked entirely
			return trace.trace;
		}
	}

	trace.start = start;
	trace.end = end;
	trace.mins = mins;
	trace.maxs = maxs;
	trace.skip = skip;
	trace.contents = contents;

	Cl_ClipTraceToEntities(&trace);

	return trace.trace;
}

/*
 * @brief Entry point for client-side prediction. For each server frame, run
 * the player movement code with the user commands we've sent to the server
 * but have not yet received acknowledgment for. Store the resulting move so
 * that it may be interpolated into by Cl_UpdateView.
 *
 * Most of the work is passed off to the client game, which is responsible for
 * the implementation Pm_Move.
 */
void Cl_PredictMovement(void) {

	if (Cl_UsePrediction()) {
		const uint32_t current = cls.net_chan.outgoing_sequence;
		uint32_t ack = cls.net_chan.incoming_acknowledged;

		// if we are too far out of date, just freeze in place
		if (current - ack >= CMD_BACKUP) {
			Com_Debug("Exceeded CMD_BACKUP\n");
			return;
		}

		GList *cmds = NULL;

		while (++ack <= current) {
			cmds = g_list_append(cmds, &cl.cmds[ack & CMD_MASK]);
		}

		cls.cgame->PredictMovement(cmds);

		g_list_free(cmds);
	}
}

/*
 * @brief Checks for client side prediction errors. Problems here mean that
 * Pm_Move or the protocol are not functioning correctly.
 */
void Cl_CheckPredictionError(void) {
	int16_t d[3];
	vec3_t delta;

	if (!Cl_UsePrediction())
		return;

	// calculate the last user_cmd_t we sent that the server has processed
	const uint32_t frame = (cls.net_chan.incoming_acknowledged & CMD_MASK);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract(cl.frame.ps.pm_state.origin, cl.predicted_origins[frame], d);
	UnpackPosition(d, delta); // convert back to floating point

	const vec_t error = VectorLength(delta);

	if (error > 256.0) { // assume a teleport or something
		VectorClear(cl.prediction_error);
	} else { // save the prediction error for interpolation
		VectorCopy(delta, cl.prediction_error);

		if (error > 1.0) {
			Com_Debug("%s\n", vtos(delta));
		}
	}
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
		const int32_t bsp_size = atoi(cl.config_strings[CS_BSP_SIZE]);

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
