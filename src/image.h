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

#include "color.h"
#include "files.h"
#include "filesystem.h"

#include <SDL_image.h>

/**
 * @brief Loads an image by the specified Quake path to the given surface.
 */
SDL_Surface *Img_LoadSurface(const char *name);

typedef enum {
	IMG_AXIS_VERTICAL,
	IMG_AXIS_HORIZONTAL
} img_axis_t;

/**
 * @brief Flips the given RGBA surface's pixel data.
 */
void Img_FlipSurface(SDL_Surface *surf, img_axis_t axis);

/**
* @brief Write pixel data to a PNG file.
*/
_Bool Img_WritePNG(const char *path, byte *data, uint32_t width, uint32_t height);

/**
* @brief Write pixel data to a TGA file.
*/
_Bool Img_WriteTGA(const char *path, byte *data, uint32_t width, uint32_t height);

/**
* @brief Write pixel data to a PBM file.
*/
_Bool Img_WritePBM(const char *path, byte *data, uint32_t width, uint32_t height, uint32_t bpp);
