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
#include "filesystem.h"

int BSP_Main(void);
int VIS_Main(void);
int LIGHT_Main(void);
int MAT_Main(void);
int PAK_Main(void);

extern char map_name[MAX_OSPATH];
extern char bsp_name[MAX_OSPATH];
extern char outbase[MAX_OSPATH];

extern boolean_t verbose;
extern boolean_t debug;
extern boolean_t legacy;

// threads.c
typedef struct thread_work_s {
	int index;  // current work cycle
	int count;  // total work cycles
	int fraction;  // last fraction of work completed (tenths)
	boolean_t progress;  // are we reporting progress
} thread_work_t;

extern thread_work_t thread_work;

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(int workcount, boolean_t progress, void(*func)(int));


#endif /*__Q2WMAP_H__*/
