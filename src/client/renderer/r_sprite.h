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

r_sprite_t *R_AddSprite(r_view_t *view, const r_sprite_t *p);
r_beam_t *R_AddBeam(r_view_t *view, const r_beam_t *p);

#ifdef __R_LOCAL_H__
void R_UpdateSprites(r_view_t *view);
void R_DrawSprites(const r_view_t *view, int32_t blend_depth);
void R_ShutdownSprites(void);
void R_InitSprites(void);
#endif
