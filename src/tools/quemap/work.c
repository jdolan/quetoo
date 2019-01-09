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

typedef struct {
	SDL_mutex *lock; // mutex on all running work
	WorkFunc func; // the work function
	int32_t index; // current work cycle
	int32_t count; // total work cycles
	int32_t percent; // last fraction of work completed
} work_t;

work_t work;

/**
 * @brief Return an iteration of work, updating progress when appropriate.
 */
static int32_t GetWork(void) {

	WorkLock();

	if (work.index == work.count || !Com_WasInit(QUEMAP)) { // done or killed
		if (work.percent != 100) {
			Com_Print("\r100%%");
		}
		WorkUnlock();
		return -1;
	}

	// update work percent and output progress
	const int32_t p = 100.0 * work.index / work.count;
	if (p != work.percent) {
		Com_Print("\r%2d%%", p);
		work.percent = p;
	}

	// assign the next work iteration
	const int32_t w = work.index;
	work.index++;

	WorkUnlock();

	return w;
}

/**
 * @brief Shared work entry point by all threads. Retrieve and perform
 * chunks of work iteratively until work is finished.
 */
static void RunWorkFunc(void *p) {

	while (true) {
		const int32_t w = GetWork();
		if (w == -1) {
			break;
		}
		work.func(w);
	}
}

/**
 * @brief
 */
void WorkLock(void) {
	SDL_LockMutex(work.lock);
}

/**
 * @brief
 */
void WorkUnlock(void) {
	SDL_UnlockMutex(work.lock);
}

/**
 * @brief Entry point for all thread work requests.
 */
void Work(WorkFunc func, int32_t count) {

	memset(&work, 0, sizeof(work));

	work.lock = SDL_CreateMutex();
	work.count = count;
	work.func = func;

	const time_t start = time(NULL);

	const int32_t thread_count = Thread_Count();

	if (thread_count == 0) {
		RunWorkFunc(0);
	} else {
		thread_t *threads[thread_count];

		for (int32_t i = 0; i < thread_count; i++) {
			threads[i] = Thread_Create(RunWorkFunc, NULL, 0);
		}

		for (int32_t i = 0; i < thread_count; i++) {
			Thread_Wait(threads[i]);
		}
	}

	SDL_DestroyMutex(work.lock);

	const time_t end = time(NULL);

	Com_Print(" (%i seconds)\n", (int32_t) (end - start));
}
