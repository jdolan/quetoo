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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/*
 * TODO(#864): 3D debug primitives (lines, boxes) are not yet ported to SDL_gpu.
 * These are stubs preserving the API used by the client and cgame.
 */

/**
 * @brief
 */
void R_Draw3DLines(SDL_GPUPrimitiveType mode, const vec3_t *points, size_t count, const color_t color, bool depth_test) {
}

/**
 * @brief
 */
void R_Draw3DBox(const box3_t bounds, const color_t color, bool depth_test) {
}

/**
 * @brief
 */
void R_Draw3D(void) {
}

/**
 * @brief
 */
void R_InitDraw3D(void) {
}

/**
 * @brief
 */
void R_ShutdownDraw3D(void) {
}
