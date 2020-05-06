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

#define STAGE_TEXTURE      (1 << 0)
#define STAGE_BLEND        (1 << 2)
#define STAGE_COLOR        (1 << 3)
#define STAGE_PULSE        (1 << 4)
#define STAGE_STRETCH      (1 << 5)
#define STAGE_ROTATE       (1 << 6)
#define STAGE_SCROLL_S     (1 << 7)
#define STAGE_SCROLL_T     (1 << 8)
#define STAGE_SCALE_S      (1 << 9)
#define STAGE_SCALE_T      (1 << 10)
#define STAGE_TERRAIN      (1 << 11)
#define STAGE_ANIM         (1 << 12)
#define STAGE_LIGHTMAP     (1 << 13)
#define STAGE_DIRTMAP      (1 << 14)
#define STAGE_ENVMAP       (1 << 15)
#define STAGE_WARP         (1 << 16)
#define STAGE_FLARE        (1 << 17)
#define STAGE_FOG          (1 << 18)

#define STAGE_DRAW         (1 << 28)
#define STAGE_MATERIAL     (1 << 29)

const float DIRTMAP[10] = float[](0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0);
