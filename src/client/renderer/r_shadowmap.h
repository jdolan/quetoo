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

#pragma once

#include "r_types.h"

typedef struct {
	/**
	 * @brief A cubemap array teture with `MAX_LIGHTS` layers.
	 */
	GLuint cubemap_array;

	/**
	 * @brief The per-light depth framebuffers.
	 */
	GLuint framebuffers[MAX_LIGHTS];
} r_shadowmaps_t;

extern r_shadowmaps_t r_shadowmaps;

void R_DrawShadowmaps(const r_view_t *view);
void R_InitShadowmaps(void);
void R_ShutdownShadowmaps(void);