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

#define __R_LOCAL_H__

#include "renderer.h"
#include <Objectively/Vector.h>

/**
 * @brief The only two C-side descriptor slots genuinely shared by every
 * pipeline: the per-frame globals block, and the per-draw locals block
 * (model matrix in the vertex stage; light-cull bitmask in the fragment
 * stage). Every other resource's binding is local to the pipeline that uses
 * it -- see e.g. the enum at the top of r_bsp_draw.c -- since pipelines don't
 * actually share a descriptor layout beyond this.
 */
enum {
	SLOT_UNIFORMS_GLOBALS = 0, // per-frame globals
	SLOT_UNIFORMS_LOCALS  = 1, // per-draw (model matrix / light cull)
};

/**
 * @brief The scene framebuffer color format: an HDR (unsigned float) target so
 * lighting can exceed 1.0 and feed bloom. All 3D pipelines render into it; the
 * present framebuffer stays the swapchain (LDR) format, and R_DrawPost composites
 * the scene into it (adding bloom, clamping to LDR).
 */
#define R_SCENE_COLOR_FORMAT SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT
