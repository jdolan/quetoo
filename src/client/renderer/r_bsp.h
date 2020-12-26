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

const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p);

#ifdef __R_LOCAL_H__

typedef enum {
	BLEND_DEPTH_NONE   = 0x0,
	BLEND_DEPTH_ENTITY = 0x1,
	BLEND_DEPTH_SPRITE = 0x2
} r_blend_depth_type_t;

int32_t R_BlendDepthForPoint(const vec3_t p, const r_blend_depth_type_t);
void R_UpdateBspInlineEntities(void);
#endif /* __R_LOCAL_H__ */
