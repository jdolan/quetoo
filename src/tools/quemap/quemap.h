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
#include "monitor.h"
#include "thread.h"
#include "collision/cm_material.h"

int32_t BSP_Main(void);
int32_t VIS_Main(void);
int32_t LIGHT_Main(void);
int32_t AAS_Main(void);
int32_t MAT_Main(void);
int32_t ZIP_Main(void);

extern char map_base[MAX_QPATH];

extern char map_name[MAX_OS_PATH];
extern char bsp_name[MAX_OS_PATH];

extern _Bool verbose;
extern _Bool debug;
extern _Bool legacy;

// BSP
extern int32_t entity_num;

extern vec3_t map_mins, map_maxs;

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
extern _Bool leaktest;

extern int32_t block_xl, block_xh, block_yl, block_yh;

extern vec_t micro_volume;

// VIS
extern _Bool fast_vis;
extern _Bool no_sort;

// LIGHT
extern _Bool antialias;
extern _Bool indirect;
extern int32_t indirect_bounces;
extern int32_t indirect_bounce;

extern vec3_t ambient;

extern vec_t brightness;
extern vec_t saturation;
extern vec_t contrast;

extern vec_t lightmap_scale;

extern vec_t patch_size;

// threads.c
typedef struct semaphores_s {
	SDL_sem *active_portals;
	SDL_sem *active_nodes;
	SDL_sem *vis_nodes;
	SDL_sem *nonvis_nodes;
	SDL_sem *active_brushes;
	SDL_sem *active_windings;
	SDL_sem *removed_points;
} semaphores_t;

extern semaphores_t semaphores;

void Sem_Init(void);
void Sem_Shutdown(void);

typedef struct thread_work_s {
	int32_t index; // current work cycle
	int32_t count; // total work cycles
	int32_t fraction; // last fraction of work completed
	_Bool progress; // are we reporting progress
} thread_work_t;

extern thread_work_t thread_work;

typedef void (*ThreadWorkFunc)(int32_t);

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(int32_t workcount, _Bool progress, ThreadWorkFunc func);

enum {
	MEM_TAG_QUEMAP	= 1000,
	MEM_TAG_TREE,
	MEM_TAG_NODE,
	MEM_TAG_BRUSH,
	MEM_TAG_EPAIR,
	MEM_TAG_FACE,
	MEM_TAG_VIS,
	MEM_TAG_LIGHT,
	MEM_TAG_LIGHTMAP,
	MEM_TAG_PATCH,
	MEM_TAG_WINDING,
	MEM_TAG_PORTAL,
	MEM_TAG_ASSET
};
