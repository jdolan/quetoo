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

/**
 * @brief Updates the sound stage from the interpolated frame.
 */
void Cg_PrepareStage(const cl_frame_t *frame) {

	cgi.stage->ticks = cgi.view->ticks;
	cgi.stage->origin = cgi.view->origin;
	cgi.stage->angles = cgi.view->angles;
	cgi.stage->forward = cgi.view->forward;
	cgi.stage->right = cgi.view->right;
	cgi.stage->up = cgi.view->up;
	cgi.stage->velocity = frame->ps.pm_state.velocity;
	cgi.stage->contents = cgi.view->contents;
}

/**
 * @brief S_PlaySampleThink implementation.
 */
static void Cg_PlaySampleThink(const s_stage_t *stage, s_play_sample_t *play) {
	
	if (play->entity) {
		const cl_entity_t *ent = &cgi.client->entities[play->entity];
		if (ent == Cg_Self()) {
			play->flags |= S_PLAY_RELATIVE;
		} else if (ent->current.solid == SOLID_BSP) {
			play->origin = Box3_ClampPoint(ent->abs_bounds, stage->origin);
			play->velocity = Vec3_Subtract(ent->prev.origin, ent->current.origin);
		} else {
			play->origin = ent->origin;
			play->velocity = Vec3_Subtract(ent->prev.origin, ent->current.origin);
		}
	}

	if (play->flags & S_PLAY_RELATIVE) {
		play->origin = play->velocity = Vec3_Zero();
	} else {
		if ((cgi.PointContents(play->origin) & CONTENTS_MASK_LIQUID) != (cgi.PointContents(stage->origin) & CONTENTS_MASK_LIQUID)) {
			play->flags |= S_PLAY_UNDERWATER;
		} else {
			play->flags &= ~S_PLAY_UNDERWATER;
		}

		const cm_trace_t tr = cgi.Trace(stage->origin, play->origin, Box3_Zero(), play->entity, CONTENTS_MASK_CLIP_PROJECTILE);
		if (tr.fraction < 1.f) {
			play->flags |= S_PLAY_OCCLUDED;
		} else {
			play->flags &= ~S_PLAY_OCCLUDED;
		}
	}
}

/**
 * @brief Wraps cgi.AddSample, installing the default PlaySampleThink function.
 */
void Cg_AddSample(s_stage_t *stage, const s_play_sample_t *play) {

	s_play_sample_t s = *play;

	if (s.Think == NULL) {
		s.Think = Cg_PlaySampleThink;
	}

	cgi.AddSample(stage, &s);
}

