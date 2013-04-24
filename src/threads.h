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

#ifndef __THREADS_H__
#define __THREADS_H__

#include <unistd.h>
#include <SDL/SDL_thread.h>

#include "mem.h"

typedef enum thread_status_e {
	THREAD_IDLE,
	THREAD_RUNNING,
	THREAD_WAIT
} thread_status_t;

typedef void (*thread_function_t)(void *data);

typedef struct thread_s {
	SDL_Thread *thread;
	SDL_cond *cond;
	SDL_mutex *mutex;
	char name[64];
	thread_status_t status;
	thread_function_t run;
	void *data;
} thread_t;

extern cvar_t *threads;

thread_t *Thread_Create_(const char *name, thread_function_t run, void *data);
#define Thread_Create(f, d) Thread_Create_(#f, f, d)
void Thread_Wait(thread_t **t);
void Thread_Init(void);
void Thread_Shutdown(void);

#endif /*__THREADS_H__ */
