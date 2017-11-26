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

	if (cgi.client->frame.ps.pm_state.type == PM_FREEZE) {
		return false;
	}

	return true;
}

/**
 * @brief Trace wrapper for Pm_Move.
 */
static cm_trace_t Cg_PredictMovement_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
        const vec3_t maxs) {
	return cgi.Trace(start, end, mins, maxs, 0, MASK_CLIP_PLAYER);
}

/**
 * @brief Run recent movement commands through the player movement code locally, storing the
 * resulting state so that it may be interpolated to and reconciled later.
 */
void Cg_PredictMovement(const GList *cmds) {
	static pm_move_t pm;

	cl_predicted_state_t *pr = &cgi.client->predicted_state;

	// copy current state to into the move
	memset(&pm, 0, sizeof(pm));
	pm.s = cgi.client->frame.ps.pm_state;

	pm.ground_entity = pr->ground_entity;
	pm.hook_pull_speed = cg_state.hook_pull_speed;

	pm.PointContents = cgi.PointContents;
	pm.Trace = Cg_PredictMovement_Trace;

	pm.Debug = cgi.PmDebug;

	const GList *e = cmds;

	// run the commands
	while (e) {
		const cl_cmd_t *cmd = (cl_cmd_t *) e->data;

		if (cmd->cmd.msec) { // if the command has time, simulate the movement

			pm.cmd = cmd->cmd;
			Pm_Move(&pm);

			// for each movement, check for stair interaction and interpolate
			if ((pm.s.flags & PMF_ON_STAIRS) && (cmd->time > pr->step.time)) {

				Cg_TraverseStep(&pr->step, cmd->timestamp, pm.step);

				// ensure we only count each step once
				pr->step.time = cmd->time;
			}
		}

		// save for error detection
		const uint32_t frame = (uint32_t) (uintptr_t) (cmd - cgi.client->cmds);
		VectorCopy(pm.s.origin, pr->origins[frame]);

		e = e->next;
	}

	// copy results out for rendering
	VectorCopy(pm.s.origin, pr->view.origin);
	UnpackVector(pm.s.view_offset, pr->view.offset);
	UnpackAngles(pm.cmd.angles, pr->view.angles);
	pr->ground_entity = pm.ground_entity;
}
