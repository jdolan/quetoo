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

typedef struct thread_pool_s {
	SDL_mutex *mutex;

	thread_t *threads;
	uint16_t num_threads;
} thread_pool_t;

static thread_pool_t thread_pool;

cvar_t *threads;

/**
 * Thread_Run
 *
 * Wrap the user's function in our own for introspection.
 */
static int32_t Thread_Run(void *data) {
	thread_t *t = (thread_t *) data;

	while (thread_pool.mutex) {
		SDL_mutexP(t->mutex);
		if (t->status == THREAD_RUNNING) {
			t->function(t->data);

			t->function = NULL;
			t->data = NULL;

			t->status = THREAD_IDLE;
		} else {
			SDL_CondWait(t->cond, t->mutex);
		}
		SDL_mutexV(t->mutex);
	}

	return 0;
}

/*
 * Thread_Init_
 *
 * Initializes the threads backing the thread pool.
 */
static void Thread_Init_(void) {

	thread_pool.num_threads = threads->integer;
	 if (thread_pool.num_threads < 0)
		 thread_pool.num_threads = 0;

	if (thread_pool.num_threads) {
		thread_pool.threads = Z_Malloc(sizeof(thread_t) * thread_pool.num_threads);

		thread_t *t = thread_pool.threads;
		uint16_t i = 0;

		for (i = 0; i < thread_pool.num_threads; i++, t++) {
			t->cond = SDL_CreateCond();
			t->mutex = SDL_CreateMutex();
			t->thread = SDL_CreateThread(Thread_Run, t);
		}
	}
}

/**
 * Thread_Shutdown_
 */
static void Thread_Shutdown_(void) {

	if (thread_pool.num_threads) {
		thread_t *t = thread_pool.threads;
		uint16_t i = 0;

		for (i = 0; i < thread_pool.num_threads; i++, t++) {
			Thread_Wait(&t);
			SDL_CondSignal(t->cond);
			SDL_WaitThread(t->thread, NULL);
			SDL_DestroyCond(t->cond);
			SDL_DestroyMutex(t->mutex);
		}

		Z_Free(thread_pool.threads);
	}
}

/**
 * Thread_Create_
 *
 * Creates a new thread to run the specified function. Callers must use
 * Thread_Wait on the returned handle to release the thread when finished.
 */
thread_t *Thread_Create_(const char *name, void( function)(void *data), void *data) {

	// update the thread pool if needed
	if (threads->modified) {
		threads->modified = false;

		Thread_Shutdown();
		Thread_Init();
	}

	thread_t *t = thread_pool.threads;
	uint16_t i = 0;

	// if threads are available, find an idle one and dispatch it
	if (thread_pool.num_threads) {
		SDL_mutexP(thread_pool.mutex);

		for (i = 0; i < thread_pool.num_threads; i++, t++) {
			if (t->status == THREAD_IDLE) {
				SDL_mutexP(t->mutex);
				strncpy(t->name, name, sizeof(t->name));

				t->function = function;
				t->data = data;

				t->status = THREAD_RUNNING;
				SDL_mutexV(t->mutex);
				SDL_CondSignal(t->cond);
				break;
			}
		}

		SDL_mutexV(thread_pool.mutex);
	}

	// if we failed to allocate a thread, run the function in this thread
	if (i == thread_pool.num_threads) {
		if (thread_pool.num_threads) {
			Com_Debug("Thread_Create_: No threads available for %s\n", name);
		}
		function(data);
	}

	return t;
}

/**
 * Thread_Wait
 *
 * Wait for the specified thread to complete.
 */
void Thread_Wait(thread_t **t) {

	if (!*t || (*t)->status != THREAD_RUNNING)
		return;

	while ((*t)->status == THREAD_RUNNING) {
		usleep(0);
	}

	(*t)->status = THREAD_IDLE;
}

/*
 * Thread_Init
 */
void Thread_Init(void) {

	threads = Cvar_Get("threads", "4", CVAR_ARCHIVE, "Enable or disable multicore processing.");
	threads->modified = false;

	memset(&thread_pool, 0, sizeof(thread_pool));

	thread_pool.mutex = SDL_CreateMutex();

	Thread_Init_();
}

/*
 * Thread_Shutdown
 */
void Thread_Shutdown(void) {

	SDL_DestroyMutex(thread_pool.mutex);
	thread_pool.mutex = NULL;

	Thread_Shutdown_();

	memset(&thread_pool, 0, sizeof(thread_pool));
}
