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

#include "cmd.h"
#include "cvar.h"
#include "files.h"

int32_t BSP_Main(void);
int32_t VIS_Main(void);
int32_t LIGHT_Main(void);
int32_t MAT_Main(void);
int32_t ZIP_Main(void);

extern char map_name[MAX_OSPATH];
extern char bsp_name[MAX_OSPATH];
extern char outbase[MAX_OSPATH];

extern bool verbose;
extern bool debug;
extern bool legacy;

// threads.c
typedef struct thread_work_s {
	int32_t index;  // current work cycle
	int32_t count;  // total work cycles
	int32_t fraction;  // last fraction of work completed (tenths)
	bool progress;  // are we reporting progress
} thread_work_t;

extern thread_work_t thread_work;

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(int32_t workcount, bool progress, void(*func)(int));


#endif /*__Q2WMAP_H__*/
