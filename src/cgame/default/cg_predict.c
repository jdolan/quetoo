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

#include "cg_local.h"
#include "game/default/bg_pmove.h"

/*
 * @brief Trace wrapper for Pm_Move.
 */
static c_trace_t Cg_PredictMovement_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
		const vec3_t maxs) {
	return cgi.Trace(start, end, mins, maxs, 0, MASK_PLAYER_SOLID);
}

/*
 * @brief Debug messages for Pm_Move.
 */
static void Cg_PredictMovement_Debug(const char *msg) {
	cgi.Debug("!Client: %u %s", cgi.client->time, msg);
}

/*
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

	const GList *e = cmds;

	// run frames
	while (e) {
		const cl_cmd_t *cmd = (cl_cmd_t *) e->data;

		if (cmd->cmd.msec) {

			pm.cmd = cmd->cmd;
			Pm_Move(&pm);

			// for each movement, check for stair interaction and interpolate
			if (pm.s.pm_flags & PMF_ON_STAIRS) {
				cgi.client->predicted_state.step.time = cmd->time;
				cgi.client->predicted_state.step.interval = 120 * (fabs(pm.step) / 16.0);
				cgi.client->predicted_state.step.step = pm.step;
			}

			// save for debug checking
			const uint32_t frame = (intptr_t) (cmd - cgi.client->cmds);
			VectorCopy(pm.s.origin, cgi.client->predicted_state.origins[frame]);
		}

		e = e->next;
	}

	// copy results out for rendering
	UnpackPosition(pm.s.origin, cgi.client->predicted_state.origin);

	UnpackPosition(pm.s.view_offset, cgi.client->predicted_state.view_offset);
	UnpackAngles(pm.cmd.angles, cgi.client->predicted_state.view_angles);

	cgi.client->predicted_state.ground_entity = pm.ground_entity;
}

