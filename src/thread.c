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

#include "thread.h"

thread_pool_t thread_pool;

cvar_t *threads;


/*
 * Thread_Run
 *
 * We wrap the user's function with our own. The function pointer and data to
 * manipulate are set on the thread_t itself, allowing us to simply take the
 * thread as a parameter here.
 */
static int Thread_Run(void *p){
	thread_t *t = (thread_t *)p;

	while(!thread_pool.shutdown){

		if(t->state == THREAD_RUN){  // invoke the user function
			t->function(t->data);
			t->state = THREAD_DONE;
		}

		usleep(0);
	}

	return 0;
}


/*
 * Thread_Create_
 *
 * Creates a new thread to run the specified function. Callers must use
 * Thread_Wait on the returned handle to release the thread when finished.
 */
thread_t *Thread_Create_(const char *name, void (function)(void *data), void *data){

	if(thread_pool.num_threads){
		thread_t *t;
		int i;

		SDL_mutexP(thread_pool.mutex);

		for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++){
			if(t->state == THREAD_IDLE){

				strncpy(t->name, name, sizeof(t->name));

				t->function = function;
				t->data = data;

				t->state = THREAD_RUN;
				break;
			}
		}

		SDL_mutexV(thread_pool.mutex);

		if(i < thread_pool.num_threads)
			return t;
	}

	Com_Debug("Thread_Create: No threads available\n");

	function(data);  // call the function in this thread

	return NULL;
}


/*
 * Thread_Wait
 *
 * Wait for the specified thread to complete.
 */
void Thread_Wait(thread_t *t){

	if(!t)
		return;

	while(t->state == THREAD_RUN){
		printf("waiting for %s\n", t->name);
		usleep(0);
	}

	t->state = THREAD_IDLE;
}


/*
 * Thread_Shutdown
 *
 * Terminates any running threads, resetting the thread pool.
 */
void Thread_Shutdown(void){
	thread_t *t;
	int i;

	if(!thread_pool.num_threads)
		return;

	thread_pool.shutdown = true;  // inform threads to quit

	for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++){
		SDL_WaitThread(t->thread, NULL);
	}

	Z_Free(thread_pool.threads);

	SDL_DestroyMutex(thread_pool.mutex);
}


/*
 * Thread_Init
 *
 * Initializes the thread pool.
 */
void Thread_Init(void){
	thread_t *t;
	int i;

	memset(&thread_pool, 0, sizeof(thread_pool));

	threads = Cvar_Get("threads", "2", CVAR_ARCHIVE, "The number of threads (cores) to utilize");

	if(threads->integer > MAX_THREADS)
		Cvar_SetValue("threads", MAX_THREADS);

	else if(threads->integer < 0)
		Cvar_SetValue("threads", 0);

	thread_pool.num_threads = threads->integer;
	threads->modified = false;

	if(thread_pool.num_threads){

		thread_pool.threads = Z_Malloc(sizeof(thread_t) * thread_pool.num_threads);

		for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++){
			t->thread = SDL_CreateThread(Thread_Run, t);
		}

		thread_pool.mutex = SDL_CreateMutex();

		Com_Print("Running %d threads\n", thread_pool.num_threads);
	}
}
