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

#include "thread.h"

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

typedef void (*WorkFunc)(int32_t);

void WorkLock(void);
void WorkUnlock(void);
void Work(WorkFunc func, int32_t count);
