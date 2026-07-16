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
 * @brief Common uniform slot bindings.
 */
enum {
	SLOT_UNIFORMS_GLOBALS = 0, // per-frame globals
	SLOT_UNIFORMS_LOCALS  = 1, // per-draw (model matrix / light cull, etc.)
};

/**
 * @brief Maps a `cm_blend_t` blend factor to its SDL_gpu equivalent.
 * @remarks `static inline` so every pipeline's material-stage blend cache
 * (R_StagePipeline and friends) can use it without a shared translation unit.
 */
static inline SDL_GPUBlendFactor R_BlendFactor(cm_blend_t blend) {
  switch (blend) {
    case BLEND_ZERO:                return SDL_GPU_BLENDFACTOR_ZERO;
    case BLEND_ONE:                 return SDL_GPU_BLENDFACTOR_ONE;
    case BLEND_SRC_COLOR:           return SDL_GPU_BLENDFACTOR_SRC_COLOR;
    case BLEND_ONE_MINUS_SRC_COLOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case BLEND_SRC_ALPHA:           return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case BLEND_ONE_MINUS_SRC_ALPHA: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BLEND_DST_COLOR:           return SDL_GPU_BLENDFACTOR_DST_COLOR;
    default:                        return SDL_GPU_BLENDFACTOR_ONE;
  }
}
