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

#include "cg_types.h"

typedef struct {
	SDL_Color Main; // dark gray
	SDL_Color MainHighlight;

	SDL_Color Dark; // almost black
	SDL_Color DarkHighlight;

	SDL_Color Theme; // cyan
	SDL_Color ThemeHighlight;

	SDL_Color Dialog; // beige
	SDL_Color DialogHighlight;

	SDL_Color Contrast; // white
	SDL_Color Watermark; // light gray; translucent text isn't supported :(

	SDL_Color BorderLight; // lighter border edge
	SDL_Color BorderDark; // darker border edge
} _QColors;

extern const _QColors QColors;
