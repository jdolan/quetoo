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

#include "cl_local.h"

/**
 * @brief
 */
void Cl_S_Restart_f(void) {

	if (cls.state == CL_LOADING) {
		return;
	}

	S_Shutdown();

	S_Init();

	const cl_state_t state = cls.state;

	Cl_LoadMedia();

	cls.state = state;
}

static void Cl_PlaySampleThink(const s_stage_t *stage, s_play_sample_t *play) {

	if (Cl_PointContents(play->origin) & CONTENTS_MASK_LIQUID) {
		play->flags |= S_PLAY_UNDERWATER;
	} else {
		play->flags &= ~S_PLAY_UNDERWATER;
	}

	const cm_trace_t tr = Cl_Trace(stage->origin, play->origin, Vec3_Zero(), Vec3_Zero(), play->entity, CONTENTS_MASK_CLIP_PROJECTILE);
	if (tr.fraction < 1.f) {
		play->flags |= S_PLAY_OCCLUDED;
	} else {
		play->flags &= ~S_PLAY_OCCLUDED;
	}
}

/**
 * @brief Wraps S_AddSample, handling collision interactions for convenience.
 */
void Cl_AddSample(s_stage_t *stage, const s_play_sample_t *play) {

	s_play_sample_t s = *play;

	if (s.Think == NULL) {
		s.Think = Cl_PlaySampleThink;
	}

	S_AddSample(stage, &s);
}
