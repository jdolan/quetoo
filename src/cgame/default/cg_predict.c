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
 * @brief Trace wrapper for Pm_Move.
 */
static cm_trace_t Cg_PredictMovement_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
        const vec3_t maxs) {
	return cgi.Trace(start, end, mins, maxs, 0, MASK_CLIP_PLAYER);
}

/**
 * @brief Debug messages for Pm_Move.
 */
static void Cg_PredictMovement_Debug(const char *msg) {
	cgi.Debug("!Client: %u %s", cgi.client->time, msg);
}

/**
 * @brief Run recent movement commands through the player movement code
 * locally, storing the resulting origin and angles so that they may be
 * interpolated to by Cl_UpdateView.
 */
void Cg_PredictMovement(const GList *cmds) {
	pm_move_t pm;

	// copy current state to into the move
	memset(&pm, 0, sizeof(pm));
	pm.s = cgi.client->frame.ps.pm_state;

	pm.ground_entity = cgi.client->predicted_state.ground_entity;

	pm.PointContents = cgi.PointContents;
	pm.Trace = Cg_PredictMovement_Trace;

	pm.Debug = Cg_PredictMovement_Debug;

	cl_predicted_state_t *pr = &cgi.client->predicted_state;

	const GList *e = cmds;

	// run the commands
	while (e) {
		const cl_cmd_t *cmd = (cl_cmd_t *) e->data;

		if (cmd->cmd.msec) {

			pm.cmd = cmd->cmd;
			Pm_Move(&pm);

			// for each movement, check for stair interaction and interpolate
			if ((pm.s.flags & PMF_ON_STAIRS) && (cmd->time > pr->step_time)) {

				// ensure we only count each step once
				pr->step_time = cmd->time;

				// determine if we're still interpolating the previous step
				const uint32_t step_delta = cgi.client->systime - pr->step_timestamp;

				if (step_delta < pr->step_interval) {
					const vec_t lerp = (pr->step_interval - step_delta) / (vec_t) pr->step_interval;
					pr->step = pr->step * (1.0 - lerp) + pm.step;
				} else {
					pr->step = pm.step;
					pr->step_timestamp = cmd->timestamp;
				}

				pr->step_interval = 128.0 * (fabs(pr->step) / PM_STEP_HEIGHT);
				pr->step_interval *= cgi.CvarValue("time_scale");
			}

			// save for debug checking
			const uint32_t frame = (uint32_t) (uintptr_t) (cmd - cgi.client->cmds);
			VectorCopy(pm.s.origin, pr->origins[frame]);
		}

		e = e->next;
	}

	// copy results out for rendering
	VectorCopy(pm.s.origin, pr->origin);

	UnpackVector(pm.s.view_offset, pr->view_offset);
	UnpackAngles(pm.cmd.angles, pr->view_angles);

	pr->ground_entity = pm.ground_entity;
}
