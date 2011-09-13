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
 * Thread_Alloc
 *
 * Allocates a free thread_t, returning NULL if none are available.
 */
static thread_t *Thread_Alloc(void){
	thread_t *t;
	int i;

	SDL_mutexP(thread_pool.mutex);

	for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++){
		if(t->state == THREAD_IDLE){
			t->state = THREAD_RUN;
			break;
		}
	}

	SDL_mutexV(thread_pool.mutex);

	if(i == thread_pool.num_threads){
		Com_Debug("Thread_Run: No threads available\n");
		return NULL;
	}

	return t;
}


/*
 * Thread_Release
 */
static void Thread_Release(thread_t *t){
	memset(t, 0, sizeof(t));
}


/*
 * Thread_Run
 *
 * We wrap the user's function with our own. The function pointer and data to
 * manipulate are set on the thread_t itself, allowing us to simply take the
 * thread as a parameter here.
 */
static int Thread_Run(void *p){
	thread_t *t = (thread_t *)p;

	t->function(t->data);  // invoke the user function

	Thread_Release(t);  // mark ourselves as available

	return 0;
}


/*
 * Thread_Create
 *
 * Creates a new thread to run the specified function. Callers can use
 * Thread_Wait on the returned handle if they care when the thread finishes.
 */
thread_t *Thread_Create(void (function)(void *data), void *data){
	static thread_t null_thread;
	thread_t *t;

	if(!(t = Thread_Alloc())){
		function(data);
		return &null_thread;
	}

	t->function = function;
	t->data = data;

	t->thread = SDL_CreateThread(Thread_Run, t);

	return t;
}


/*
 * Thread_Wait
 */
void Thread_Wait(thread_t *t){

	if(t->state == THREAD_IDLE)
		return;

	SDL_WaitThread(t->thread, NULL);
}


/*
 * Thread_Kill
 */
static void Thread_Kill(thread_t *t){

	if(t->state == THREAD_IDLE)
		return;

	SDL_KillThread(t->thread);
}


/*
 * Thread_Shutdown
 *
 * Terminates any running threads, resetting the thread pool.
 */
void Thread_Shutdown(void){
	thread_t *t;
	int i;

	for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++){

		if(t->state > THREAD_IDLE)
			Thread_Kill(t);
	}

	Z_Free(thread_pool.threads);

	SDL_DestroyMutex(thread_pool.mutex);
}


/*
 * Thread_Init
 *
 * Initializes the thread pool.  No threads are started here.
 */
void Thread_Init(void){

	memset(&thread_pool, 0, sizeof(thread_pool));

	threads = Cvar_Get("threads", "4", CVAR_ARCHIVE | CVAR_LATCH, "The number of threads to initialize");

	thread_pool.num_threads = threads->integer;
	thread_pool.threads = Z_Malloc(sizeof(thread_t) * thread_pool.num_threads);
	thread_pool.mutex = SDL_CreateMutex();
}
