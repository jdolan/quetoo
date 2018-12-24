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

#include "quemap.h"

extern _Bool no_prune;
extern _Bool no_detail;
extern _Bool all_structural;
extern _Bool only_ents;
extern _Bool no_merge;
extern _Bool no_water;
extern _Bool no_csg;
extern _Bool no_weld;
extern _Bool no_share;
extern _Bool no_tjunc;
extern _Bool leak_test;

extern int32_t block_xl, block_xh, block_yl, block_yh;

extern vec_t micro_volume;

extern int32_t entity_num;

int32_t BSP_Main(void);
