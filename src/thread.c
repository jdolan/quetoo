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

#include <SDL_cpuinfo.h>
#include <SDL_timer.h>

#include "thread.h"

typedef struct thread_pool_s {
	SDL_mutex *mutex;

	thread_t *threads;
	size_t num_threads;
} thread_pool_t;

static thread_pool_t thread_pool;

cvar_t *threads;

/**
 * @brief Wrap the user's function in our own for introspection.
 */
static int32_t Thread_Run(void *data) {
	thread_t *t = (thread_t *) data;

	while (thread_pool.mutex) {

		SDL_mutexP(t->mutex);

		if (t->status == THREAD_RUNNING) {
			t->Run(t->data);

			t->Run = NULL;
			t->data = NULL;

			t->status = THREAD_WAIT;
		} else {
			SDL_CondWait(t->cond, t->mutex);
		}

		SDL_mutexV(t->mutex);
	}

	return 0;
}

/**
 * @brief Initializes the threads backing the thread pool.
 */
static void Thread_Init_(ssize_t num_threads) {

	if (num_threads == 0) {
		num_threads = SDL_GetCPUCount();
	} else if (num_threads == -1) {
		num_threads = 0;
	} else if (num_threads > MAX_THREADS) {
		num_threads = MAX_THREADS;
	}

	thread_pool.num_threads = num_threads;

	if (thread_pool.num_threads) {
		thread_pool.threads = Mem_Malloc(sizeof(thread_t) * thread_pool.num_threads);

		thread_t *t = thread_pool.threads;
		uint16_t i = 0;

		for (i = 0; i < thread_pool.num_threads; i++, t++) {
			t->cond = SDL_CreateCond();
			t->mutex = SDL_CreateMutex();
			t->thread = SDL_CreateThread(Thread_Run, __func__, t);
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
			Thread_Wait(t);
			SDL_CondSignal(t->cond);
			SDL_WaitThread(t->thread, NULL);
			SDL_DestroyCond(t->cond);
			SDL_DestroyMutex(t->mutex);
		}

		Mem_Free(thread_pool.threads);
	}
}

/**
 * @brief Creates a new thread to run the specified function. Callers must use
 * Thread_Wait on the returned handle to release the thread when finished.
 */
thread_t *Thread_Create_(const char *name, ThreadRunFunc run, void *data) {

	thread_t *t = thread_pool.threads;
	uint16_t i = 0;

	// if threads are available, find an idle one and dispatch it
	if (thread_pool.num_threads) {
		SDL_mutexP(thread_pool.mutex);

		for (i = 0; i < thread_pool.num_threads; i++, t++) {

			// if the thread appears idle, lock it and check again
			if (t->status == THREAD_IDLE) {

				SDL_mutexP(t->mutex);

				// if the thread is idle, dispatch it
				if (t->status == THREAD_IDLE) {
					g_strlcpy(t->name, name, sizeof(t->name));

					t->Run = run;
					t->data = data;

					t->status = THREAD_RUNNING;

					SDL_mutexV(t->mutex);
					SDL_CondSignal(t->cond);

					break;
				}

				SDL_mutexV(t->mutex);
			}
		}

		SDL_mutexV(thread_pool.mutex);
	}

	// if we failed to allocate a thread, run the function in this thread
	if (i == thread_pool.num_threads) {
#if 0
		if (thread_pool.num_threads) {
			printf("No threads available for %s\n", name);
		}
#endif
		t = NULL;
		run(data);
	}

	return t;
}

/**
 * @brief Wait for the specified thread to complete.
 */
void Thread_Wait(thread_t *t) {

	if (!t || t->status == THREAD_IDLE) {
		return;
	}

	while (t->status != THREAD_WAIT) {
		SDL_Delay(0);
	}

	t->status = THREAD_IDLE;
}

/**
 * @brief Returns the number of threads in the pool.
 */
uint16_t Thread_Count(void) {
	return thread_pool.num_threads;
}

/**
 * @brief Initializes the thread pool.
 */
void Thread_Init(ssize_t num_threads) {

	memset(&thread_pool, 0, sizeof(thread_pool));

	thread_pool.mutex = SDL_CreateMutex();

	Thread_Init_(num_threads);
}

/**
 * @brief Shuts down the thread pool.
 */
void Thread_Shutdown(void) {

	if (thread_pool.mutex) {
		SDL_DestroyMutex(thread_pool.mutex);
		thread_pool.mutex = NULL;
	}

	Thread_Shutdown_();

	memset(&thread_pool, 0, sizeof(thread_pool));
}
