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

typedef struct {

	/**
	 * @brief The lock governing global thread pool access.
	 */
	SDL_SpinLock lock;

	/**
	 * @brief The thread dispatch identifier.
	 */
	SDL_atomic_t id;

	/**
	 * @brief The number of threads in the pool.
	 */
	size_t num_threads;

	/**
	 * @brief The threads.
	 */
	thread_t *threads;
} thread_pool_t;

static thread_pool_t thread_pool;

/**
 * @brief A sentinel thread function to indicate thread termination.
 */
static ThreadRunFunc ThreadTerminate = (ThreadRunFunc) &ThreadTerminate;

/**
 * @brief The main thread ID.
 */
SDL_threadID thread_main;

/**
 * @brief The number of threads to spawn, or 0 to disable.
 */
cvar_t *threads;

/**
 * @brief Wrap the user's function in our own for introspection.
 */
static int32_t Thread_Run(void *data) {
	thread_t *t = (thread_t *) data;

	while (t->Run != ThreadTerminate) {

		SDL_LockMutex(t->mutex);

		if (t->status == THREAD_RUNNING) {
			t->Run(t->data);
			if (t->options & THREAD_NO_WAIT) {
				t->status = THREAD_IDLE;
			} else {
				t->status = THREAD_WAITING;
			}
		} else {
			SDL_CondWait(t->cond, t->mutex);
		}

		SDL_UnlockMutex(t->mutex);
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

		for (size_t i = 0; i < thread_pool.num_threads; i++, t++) {
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

		for (size_t i = 0; i < thread_pool.num_threads; i++, t++) {
			Thread_Wait(t);
			t->Run = ThreadTerminate;
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
thread_t *Thread_Create_(const char *name, ThreadRunFunc run, void *data, thread_options_t options) {

	// if threads are available, find an idle one and dispatch it
	if (thread_pool.num_threads) {
		SDL_AtomicLock(&thread_pool.lock);

		thread_t *t = thread_pool.threads;
		for (size_t i = 0; i < thread_pool.num_threads; i++, t++) {

			// if the thread appears idle, lock it and check again
			if (t->status == THREAD_IDLE) {

				SDL_LockMutex(t->mutex);

				// if the thread is idle, dispatch it
				if (t->status == THREAD_IDLE) {
					t->status = THREAD_RUNNING;
					t->options = options;

					g_strlcpy(t->name, name, sizeof(t->name));

					t->Run = run;
					t->data = data;

					SDL_UnlockMutex(t->mutex);
					SDL_CondSignal(t->cond);

					SDL_AtomicUnlock(&thread_pool.lock);
					return t;
				}

				SDL_UnlockMutex(t->mutex);
			}
		}

		SDL_AtomicUnlock(&thread_pool.lock);
	}

	// if we failed to allocate a thread, run the function in this thread

	run(data);
	return NULL;
}

/**
 * @brief Wait for the specified thread to complete.
 */
void Thread_Wait(thread_t *t) {

	if (!t || t->status == THREAD_IDLE) {
		return;
	}

	while (t->status != THREAD_WAITING) {
		SDL_Delay(0); // FIXME: Reuse the conditional here?
	}

	t->status = THREAD_IDLE;
}

/**
 * @brief Returns the number of threads in the pool.
 */
int32_t Thread_Count(void) {
	return (int32_t) thread_pool.num_threads;
}

/**
 * @brief Initializes the thread pool.
 */
void Thread_Init(ssize_t num_threads) {

	memset(&thread_pool, 0, sizeof(thread_pool));

	Thread_Init_(num_threads);

	thread_main = SDL_ThreadID();
}

/**
 * @brief Shuts down the thread pool.
 */
void Thread_Shutdown(void) {

	Thread_Shutdown_();

	memset(&thread_pool, 0, sizeof(thread_pool));
}
