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

#ifndef __R_ARRAY_H__
#define __R_ARRAY_H__

#include "r_types.h"

// interleave constants
typedef struct {
	vec3_t		vertex;
	u8vec4_t	color;
	int32_t		normal;
	int32_t		tangent;
	vec2_t		diffuse;
	vec2_t		lightmap;
} r_interleave_vertex_t;

extern r_buffer_layout_t r_default_buffer_layout[];

#ifdef __R_LOCAL_H__
	void R_SetArrayState(const r_model_t *mod);
	void R_ResetArrayState(void);
	void R_DrawArrays(GLenum type, GLint start, GLsizei count);
	int32_t R_ArraysMask(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_ARRAY_H__ */
