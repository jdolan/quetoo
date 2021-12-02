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

#include <SDL_timer.h>

#include "quemap.h"

typedef struct {
	SDL_mutex *lock; // mutex on all running work
	const char *name; // the work name
	WorkFunc func; // the work function
	int32_t index; // current work cycle
	int32_t count; // total work cycles
	int32_t percent; // last fraction of work completed
} work_t;

static work_t work;

/**
 * @brief Return an iteration of work, updating progress when appropriate.
 */
static int32_t GetWork(void) {

	int32_t w = -1;
	SDL_LockMutex(work.lock);

	if (Com_WasInit(QUEMAP)) {
		if (work.index < work.count) {

			// update work percent and output progress
			const int32_t p = ceilf(100.0 * work.index / work.count);
			if (p != work.percent) {
				if (work.name) {
					Com_Print("\r%-24s [%3d%%]", work.name, p);
				}
				work.percent = p;
			}

			// assign the next work iteration
			w = work.index++;
		}
	}

	SDL_UnlockMutex(work.lock);
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
 * @brief Entry point for all thread work requests.
 */
void Work(const char *name, WorkFunc func, int32_t count) {

	memset(&work, 0, sizeof(work));

	work.lock = SDL_CreateMutex();
	work.name = name;
	work.count = count;
	work.func = func;
	work.index = 0;
	work.percent = -1;

	const uint32_t start = SDL_GetTicks();

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
	work.lock = NULL;

	const uint32_t end = SDL_GetTicks();

	if (work.name) {
		Com_Print(" %d ms\n", end - start);
	}
}

/**
 * @brief Outputs progress to the console or XML monitor.
 * @details In the case of the XML monitor, progress output is throttled because of
 * poor GtkRadiant console performance.
 */
void Progress(const char *progress, int32_t percent) {
	static char *string = "-\\|/-|";
	static int32_t index = 0;
	static int32_t last_percent;

	if (Mon_IsConnected() && percent != 0 && percent != 100) {
		static uint32_t last_ticks;
		if (SDL_GetTicks() - last_ticks < 200) {
			return;
		}
		last_ticks = SDL_GetTicks();
	}

	if (percent == -1) {
		Com_Print("\r%-24s [%c]", progress, string[index]);
		index = (index + 1) % strlen(string);
	} else {
		if (percent != last_percent) {
			Com_Print("\r%-24s [%3d%%]", progress, percent);
			last_percent = percent;
		}
	}
}
