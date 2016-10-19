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
// every program, and are also used for buffer storage.
typedef enum {
	R_ARRAY_VERTEX,
	R_ARRAY_COLOR,
	R_ARRAY_NORMAL,
	R_ARRAY_TANGENT,
	R_ARRAY_TEX_DIFFUSE,
	R_ARRAY_TEX_LIGHTMAP,

	R_ARRAY_MAX_ATTRIBS,
} r_attribute_id_t;

// These are the masks used to tell which data
// should be actually bound. They don't have
// to match up to the above, but it's nice if they do.
#define R_ARRAY_MASK_VERTEX			0x01
#define R_ARRAY_MASK_COLOR			0x02
#define R_ARRAY_MASK_NORMAL			0x04
#define R_ARRAY_MASK_TANGENT		0x08
#define R_ARRAY_MASK_TEX_DIFFUSE	0x10
#define R_ARRAY_MASK_TEX_LIGHTMAP	0x20
#define R_ARRAY_MASK_ALL			0xFF

#ifdef __R_LOCAL_H__
void R_SetArrayState(const r_model_t *mod);
void R_ResetArrayState(void);
void R_DrawArrays(GLenum type, GLint start, GLsizei count);
int32_t R_ArraysMask(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_ARRAY_H__ */
