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

#include "quemap.h"
#include "thread.h"

semaphores_t semaphores;
thread_work_t thread_work;

/**
 * @brief Initializes the shared semaphores that threads will touch.
 */
void Sem_Init(void) {

	memset(&semaphores, 0, sizeof(semaphores));

	semaphores.active_portals = SDL_CreateSemaphore(0);
	semaphores.active_nodes = SDL_CreateSemaphore(0);
	semaphores.vis_nodes = SDL_CreateSemaphore(0);
	semaphores.nonvis_nodes = SDL_CreateSemaphore(0);
	semaphores.active_brushes = SDL_CreateSemaphore(0);
	semaphores.active_windings = SDL_CreateSemaphore(0);
	semaphores.removed_points = SDL_CreateSemaphore(0);
}

/**
 * @brief Shuts down shared semaphores, releasing any resources they hold.
 */
void Sem_Shutdown(void) {

	SDL_DestroySemaphore(semaphores.active_portals);
	SDL_DestroySemaphore(semaphores.active_nodes);
	SDL_DestroySemaphore(semaphores.vis_nodes);
	SDL_DestroySemaphore(semaphores.nonvis_nodes);
	SDL_DestroySemaphore(semaphores.active_brushes);
	SDL_DestroySemaphore(semaphores.active_windings);
	SDL_DestroySemaphore(semaphores.removed_points);
}

/**
 * @brief Return an iteration of work, updating progress when appropriate.
 */
static int32_t GetThreadWork(void) {

	ThreadLock();

	if (thread_work.index == thread_work.count || !Com_WasInit(QUETOO_MAPTOOL)) { // done or killed
		ThreadUnlock();
		return -1;
	}

	// update work fraction and output progress if desired
	const int32_t f = 50 * thread_work.index / thread_work.count;
	if (f != thread_work.fraction) {
		if (thread_work.progress && !(verbose || debug)) {
			for (int32_t i = thread_work.fraction; i < f; i++) {
				if (i % 5 == 0) {
					Com_Print("%i", i / 5);
				} else {
					Com_Print(".");
				}
			}
		}
		thread_work.fraction = f;
	}

	// assign the next work iteration
	const int32_t r = thread_work.index;
	thread_work.index++;

	ThreadUnlock();

	return r;
}

// generic function pointer to actual work to be done
static ThreadWorkFunc WorkFunction;

/**
 * @brief Shared work entry point by all threads. Retrieve and perform
 * chunks of work iteratively until work is finished.
 */
static void ThreadWork(void *p) {

	while (true) {
		const int32_t work = GetThreadWork();
		if (work == -1) {
			break;
		}
		WorkFunction(work);
	}
}

static SDL_mutex *lock = NULL;

/**
 * @brief
 */
void ThreadLock(void) {

	if (!lock) {
		return;
	}

	SDL_mutexP(lock);
}

/**
 * @brief
 */
void ThreadUnlock(void) {

	if (!lock) {
		return;
	}

	SDL_mutexV(lock);
}

/**
 * @brief
 */
static void RunThreads(void) {

	const uint16_t thread_count = Thread_Count();

	if (thread_count == 0) {
		ThreadWork(0);
		return;
	}

	assert(!lock);
	lock = SDL_CreateMutex();

	thread_t *threads[thread_count];

	for (uint16_t i = 0; i < thread_count; i++) {
		threads[i] = Thread_Create(ThreadWork, NULL);
	}

	for (uint16_t i = 0; i < thread_count; i++) {
		Thread_Wait(threads[i]);
	}

	SDL_DestroyMutex(lock);
	lock = NULL;
}

/**
 * @brief Entry point for all thread work requests.
 */
void RunThreadsOn(int32_t work_count, _Bool progress, ThreadWorkFunc func) {

	thread_work.index = 0;
	thread_work.count = work_count;
	thread_work.fraction = 0;
	thread_work.progress = progress;

	WorkFunction = func;

	const time_t start = time(NULL);

	RunThreads();

	const time_t end = time(NULL);

	if (thread_work.progress) {
		Com_Print(" (%i seconds)\n", (int32_t) (end - start));
	}
}
