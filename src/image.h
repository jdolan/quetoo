/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "files.h"
#include "filesystem.h"

#ifdef BUILD_CLIENT
#include <SDL/SDL_image.h>

// 8 bit palette for .wal images and particles
#define IMG_PALETTE_SIZE 256

typedef uint32_t img_palette_t[IMG_PALETTE_SIZE];
extern img_palette_t img_palette;

_Bool Img_LoadImage(const char *name, SDL_Surface **surf);
_Bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf);
void Img_InitPalette(void);
void Img_ColorFromPalette(uint8_t c, vec_t *res);
_Bool Img_WriteJPEG(const char *path, byte *data, uint32_t width, uint32_t height, int32_t quality);

#endif /* BUILD_CLIENT */
#endif /*__IMAGE_H__*/
