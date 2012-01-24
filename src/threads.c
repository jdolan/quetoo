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

#include "mem.h"
#include "threads.h"

cvar_t *threads;

/*
 * Thread_Run
 *
 * Wrap the user's function in our own for introspection.
 */
static int Thread_Run(void *data) {
	thread_t *t = (thread_t *) data;

	t->function(t->data);

	return 0;
}

/*
 * Thread_Create_
 *
 * Creates a new thread to run the specified function. Callers must use
 * Thread_Wait on the returned handle to release the thread when finished.
 */
thread_t *Thread_Create_(const char *name, void( function)(void *data),
		void *data) {

	if (!threads->integer) {
		function(data);
		return NULL;
	}

	thread_t *t = Z_Malloc(sizeof(thread_t));

	strncpy(t->name, name, sizeof(t->name));

	t->function = function;
	t->data = data;

	t->thread = SDL_CreateThread(Thread_Run, t);

	return t;
}

/*
 * Thread_Wait
 *
 * Wait for the specified thread to complete.
 */
void Thread_Wait(thread_t **t) {

	if (!*t)
		return;

	//struct timeval start, end;
	//gettimeofday(&start, NULL);

	SDL_WaitThread((*t)->thread, NULL);

	//gettimeofday(&end, NULL);
	//printf("%s: %ld\n", end->tv_usec - start->tv_usec, t->name);

	Z_Free(*t);
	*t = NULL;
}

/*
 * Thread_Init
 */
void Thread_Init(void) {
	threads = Cvar_Get("threads", "1", CVAR_ARCHIVE,
			"Enable or disable multicore processing.");
}

/*
 * Thread_Shutdown
 */
void Thread_Shutdown(void) {

}
