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
 * @brief
 */
static inline float Lerp_Linear(float y1, float y2, float time) {
   return (y1 * (1.f - time)) + (y2 * time);
}

/**
 * @brief
 */
static inline float Lerp_Cosine(float y1, float y2, float time) {
   return Lerp_Linear(y1, y2, (1.f - cosf(time * M_PI)) * .5f);
}

/**
 * @brief
 */
static inline float Lerp_Cubic(float y0, float y1, float y2, float y3, float time) {
   const float time_squared = time * time;
   const float a0 = y3 - y2 - y0 + y1;
   const float a1 = y0 - y1 - a0;
   const float a2 = y2 - y0;
   const float a3 = y1;

   return (a0 * time * time_squared) + (a1 * time_squared) + (a2 * time) + a3;
}

/**
 * @brief
 */
static inline float Lerp_Hermite(float y0, float y1, float y2, float y3, float time, float tension, float bias) {
	const float time_squared = time * time;
	const float time_3 = time_squared * time;
	const float m0 = ((y1 - y0) * (1 + bias) * (1 - tension) * .5f) + ((y2 - y1) * (1 - bias) * (1 - tension) * .5f);
	const float m1 = ((y2 - y1) * (1 + bias) * (1 - tension) * .5f) + ((y3 - y2) * (1 - bias) * (1 - tension) * .5f);
	const float a0 = (2 * time_3) - (3 * time_squared) + 1;
	const float a1 = time_3 - (2 * time_squared) + time;
	const float a2 = time_3 - time_squared;
	const float a3 = (-2 * time_3) + (3 * time_squared);

   return (a0 * y1) + (a1 * m0) + (a2 * m1) + (a3 * y2);
}

cg_transition_point_t cg_linear_transition[] = {
	{ .point = { .x = 0, .y = 0 } },
	{ .point = { .x = 1, .y = 1 } }
};

/**
 * @brief Resolve a transition.
 * @param transition The beginning of the transition list
 * @param point_count The number of transition points
 * @param time The current time factor
 * @return The factor value
 */
float Cg_ResolveTransition(const cg_transition_point_t *transition, int32_t point_count, float time) {
	int32_t i;

	// If we're beyond the edges, just clamp to the edge.
	if (time <= transition[0].point.x) {
		return transition[0].point.y;
	} else if (time >= transition[point_count - 1].point.x) {
		return transition[point_count - 1].point.y;
	}

	// Find the "previous" point.
	const cg_transition_point_t *point;

	for (i = point_count - 1, point = transition + point_count - 1; i >= 0; i--, point--) {
		if (point->point.x <= time) {
			break;
		}
	}

	// Invalid spline?
	assert(i != point_count);

	// Where are we between now and the next point?
	const vec2_t *next_point = &transition[MIN(point_count - 1, i + 1)].point;
	const float inner_time = (time - point->point.x) / (next_point->x - point->point.x);

	switch (point->lerp) {
		default:
		case LERP_LINEAR:
			return Lerp_Linear(point->point.y, next_point->y, inner_time);
		case LERP_COSINE:
			return Lerp_Cosine(point->point.y, next_point->y, inner_time);
		case LERP_CUBIC: {
			const vec2_t *prev_point = &transition[MAX(0, i - 1)].point;
			const vec2_t *next_next_point = &transition[MIN(point_count - 1, i + 2)].point;
			return Lerp_Cubic(prev_point->y, point->point.y, next_point->y, next_next_point->y, inner_time);
		}
		case LERP_HERMITE: {
			const vec2_t *prev_point = &transition[MAX(0, i - 1)].point;
			const vec2_t *next_next_point = &transition[MIN(point_count - 1, i + 2)].point;
			return Lerp_Hermite(prev_point->y, point->point.y, next_point->y, next_next_point->y, inner_time, transition->hermite.tension, transition->hermite.bias);
		}
	}
}
