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

/**
 * @brief Interpolation type between points
 */
typedef enum {
	LERP_LINEAR,
	LERP_COSINE,
	LERP_CUBIC,
	LERP_HERMITE
} cg_transition_interpolation_t;

/**
 * @brief A structure that defines a spline transition with no extrapolation.
 */
typedef struct {
	/**
	 * @brief The interpolation point
	 */
	vec2_t							point;

	/**
	 * @brief The interpolation method; note that this doesn't apply to the final point
	 */
	cg_transition_interpolation_t	lerp;

	/**
	 * @brief Parameters for hermite
	 */
	struct {
		float	tension;
		float	bias;
	} hermite;
} cg_transition_point_t;

extern cg_transition_point_t cg_linear_transition[];

float Cg_ResolveTransition(const cg_transition_point_t *transition, const size_t point_count, float time);