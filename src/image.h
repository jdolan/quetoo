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

#include "files.h"
#include "filesystem.h"

#include <SDL_image.h>

#define IMG_PALETTE_SIZE 256
typedef uint32_t img_palette_t[IMG_PALETTE_SIZE];

/**
 * @brief The 8-bit lookup palette, mapping 0-255 to RGB colors.
 */
extern img_palette_t img_palette;

/**
 * @brief Loads an image by the specified Quake path to the given surface.
 */
_Bool Img_LoadImage(const char *name, SDL_Surface **surf);

/**
 * @brief Initializes the 8-bit lookup palette.
 */
void Img_InitPalette(void);

/**
 * @brief Resolves an RGB color value for the given value.
 */
void Img_ColorFromPalette(uint8_t c, vec_t *res);

/**
* @brief Write pixel data to a PNG file.
*/
_Bool Img_WritePNG(const char *path, byte *data, uint32_t width, uint32_t height);

/**
* @brief Write pixel data to a TGA file.
*/
_Bool Img_WriteTGA(const char *path, byte *data, uint32_t width, uint32_t height);
