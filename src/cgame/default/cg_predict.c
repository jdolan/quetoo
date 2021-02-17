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
 * @brief Returns true if client side prediction should be used.
 */
_Bool Cg_UsePrediction(void) {

	if (!cg_predict->value) {
		return false;
	}

	if (cgi.client->demo_server) {
		return false;
	}

	if (cgi.client->third_person) {
		return false;
	}

	if (cgi.client->delta_frame == NULL) {
		return false;
	}

	if (cgi.client->frame.ps.pm_state.type == PM_FREEZE) {
		return false;
	}

	return true;
}

/**
 * @brief Trace wrapper for Pm_Move.
 */
static cm_trace_t Cg_PredictMovement_Trace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs) {
	return cgi.Trace(start, end, mins, maxs, 0, CONTENTS_MASK_CLIP_PLAYER);
}

/**
 * @brief Run recent movement commands through the player movement code locally, storing the
 * resulting state so that it may be interpolated to and reconciled later.
 */
void Cg_PredictMovement(const GPtrArray *cmds) {

	assert(cmds);
	assert(cmds->len);

	cl_predicted_state_t *pr = &cgi.client->predicted_state;

	// copy current state to into the move
	pm_move_t pm = {};
	pm.s = cgi.client->frame.ps.pm_state;

	pm.ground_entity = pr->ground_entity;
	pm.hook_pull_speed = Cg_GetHookPullSpeed();

	pm.PointContents = cgi.PointContents;
	pm.Trace = Cg_PredictMovement_Trace;

	pm.Debug = cgi.Debug_;
	pm.DebugMask = cgi.DebugMask;
	pm.debug_mask = DEBUG_PMOVE_CLIENT;

	// run the commands
	for (guint i = 0; i < cmds->len; i++) {
		cl_cmd_t *cmd = g_ptr_array_index(cmds, i);

		if (cmd->cmd.msec) { // if the command has time, run it

			// timestamp it so the client knows we have valid results
			cmd->prediction.time = cgi.client->time;

			// simulate the movement
			pm.cmd = cmd->cmd;
			Pm_Move(&pm);
		}

		// save for error detection
		cmd->prediction.origin = pm.s.origin;
	}

	// save for rendering
	pr->view.origin = pm.s.origin;
	pr->view.offset = pm.s.view_offset;
	pr->view.step_offset = pm.s.step_offset;
	pr->view.angles = pm.cmd.angles;
	pr->ground_entity = pm.ground_entity;
}
