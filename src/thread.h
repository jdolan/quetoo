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

#ifndef __THREAD_H__
#define __THREAD_H__

#include <unistd.h>
#include <SDL/SDL_thread.h>

#include "cvar.h"

#define MAX_THREADS 16

typedef enum thread_state_s {
	THREAD_IDLE,
	THREAD_RUN,
	THREAD_DONE
} thread_state_t;

typedef struct thread_s {
	thread_state_t state;
	SDL_Thread *thread;
	char name[64];
	void (*function)(void *data);
	void *data;
} thread_t;

typedef struct thread_pool_s {
	thread_t *threads;
	int num_threads;
	SDL_mutex *mutex;
	qboolean shutdown;
} thread_pool_t;

extern thread_pool_t thread_pool;

extern cvar_t *threads;

thread_t *Thread_Create_(const char *name, void (function)(void *data), void *data);
#define Thread_Create(f, d) Thread_Create(#f, f, d)
void Thread_Wait(thread_t *t);
void Thread_Shutdown(void);
void Thread_Init(void);

#endif /*__THREAD_H__ */
