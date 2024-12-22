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

#include "collision/collision.h"
#include "common/common.h"
#include "monitor.h"
#include "work.h"

/**
 * @brief Quemap .prt file magic.
 */
#define	PORTALFILE	"PRT1"

extern char map_base[MAX_QPATH];

extern char map_name[MAX_OS_PATH];
extern char bsp_name[MAX_OS_PATH];

extern bool verbose;
extern bool debug;
extern bool do_mat;
extern bool do_bsp;
extern bool do_light;
extern bool do_zip;

enum {
	MEM_TAG_QBSP = 1000,
	MEM_TAG_EPAIR,
	MEM_TAG_BRUSH,
	MEM_TAG_NODE,
	MEM_TAG_TREE,
	MEM_TAG_PORTAL,
	MEM_TAG_FACE,
	MEM_TAG_QLIGHT,
	MEM_TAG_LIGHT,
	MEM_TAG_LIGHTMAP,
	MEM_TAG_LIGHTGRID,
	MEM_TAG_QMAT,
	MEM_TAG_QZIP
};
