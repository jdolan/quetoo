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

// Attribute indices - these should be assigned to
// every program that uses them, and are also used for buffer storage.
typedef enum {
	R_ARRAY_VERTEX,
	R_ARRAY_COLOR,
	R_ARRAY_NORMAL,
	R_ARRAY_TANGENT,
	R_ARRAY_TEX_DIFFUSE,
	R_ARRAY_TEX_LIGHTMAP,

	// These three are only used for shader-based lerp.
	// They are only enabled if the ones that match up
	// to it are enabled as well.
	R_ARRAY_NEXT_VERTEX,
	R_ARRAY_NEXT_NORMAL,
	R_ARRAY_NEXT_TANGENT,

	R_ARRAY_MAX_ATTRIBS,
} r_attribute_id_t;

// These are the masks used to tell which data
// should be actually bound. They should match
// up with the ones above to make things simple.
#define R_ARRAY_MASK_VERTEX			(1 << R_ARRAY_VERTEX)
#define R_ARRAY_MASK_COLOR			(1 << R_ARRAY_COLOR)
#define R_ARRAY_MASK_NORMAL			(1 << R_ARRAY_NORMAL)
#define R_ARRAY_MASK_TANGENT		(1 << R_ARRAY_TANGENT)
#define R_ARRAY_MASK_TEX_DIFFUSE	(1 << R_ARRAY_TEX_DIFFUSE)
#define R_ARRAY_MASK_TEX_LIGHTMAP	(1 << R_ARRAY_TEX_LIGHTMAP)

#define R_ARRAY_MASK_NEXT_VERTEX	(1 << R_ARRAY_NEXT_VERTEX)
#define R_ARRAY_MASK_NEXT_NORMAL	(1 << R_ARRAY_NEXT_NORMAL)
#define R_ARRAY_MASK_NEXT_TANGENT	(1 << R_ARRAY_NEXT_TANGENT)

#define R_ARRAY_MASK_ALL			(1 << R_ARRAY_MAX_ATTRIBS) - 1

#ifdef __R_LOCAL_H__
void R_SetArrayState(const r_model_t *mod);
void R_ResetArrayState(void);
void R_DrawArrays(GLenum type, GLint start, GLsizei count);
int32_t R_ArraysMask(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_ARRAY_H__ */
