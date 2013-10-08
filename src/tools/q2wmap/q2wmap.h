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

#ifndef __Q2WMAP_H__
#define __Q2WMAP_H__

#include "files.h"
#include "monitor.h"
#include "thread.h"

int32_t BSP_Main(void);
int32_t VIS_Main(void);
int32_t LIGHT_Main(void);
int32_t AAS_Main(void);
int32_t MAT_Main(void);
int32_t ZIP_Main(void);

extern char map_name[MAX_OSPATH];
extern char bsp_name[MAX_OSPATH];
extern char outbase[MAX_OSPATH];

extern _Bool verbose;
extern _Bool debug;
extern _Bool legacy;

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
	int32_t fraction; // last fraction of work completed (tenths)
	_Bool progress; // are we reporting progress
} thread_work_t;

extern thread_work_t thread_work;

typedef void (*ThreadWorkFunc)(int32_t);

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(int32_t workcount, _Bool progress, ThreadWorkFunc func);

#endif /*__Q2WMAP_H__*/
