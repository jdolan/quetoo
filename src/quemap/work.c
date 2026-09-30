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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <SDL3/SDL_timer.h>

#include "quemap.h"

static struct {
  SDL_Mutex *lock; // mutex on all running work
  const char *name; // the work name
  WorkFunc func; // the work function
  int32_t index; // current work cycle
  int32_t count; // total work cycles
  int32_t percent; // last fraction of work completed
} module;

/**
 * @brief Return an iteration of work, updating progress when appropriate.
 */
static int32_t GetWork(void) {

  int32_t w = -1;
  SDL_LockMutex(module.lock);

  if (Com_WasInit(QUEMAP)) {
    if (module.index < module.count) {

      // update work percent and output progress
      const int32_t p = ceilf(100.0 * module.index / module.count);
      if (p != module.percent) {
        if (module.name) {
          Com_Print("\r%-24s [%3d%%]", module.name, p);
        }
        module.percent = p;
      }

      // assign the next work iteration
      w = module.index++;
    }
  }

  SDL_UnlockMutex(module.lock);
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
    module.func(w);
  }
}

/**
 * @brief Entry point for all thread work requests.
 */
void Work(const char *name, WorkFunc func, int32_t count) {

  memset(&module, 0, sizeof(module));

  module.lock = SDL_CreateMutex();
  module.name = name;
  module.count = count;
  module.func = func;
  module.index = 0;
  module.percent = -1;

  const uint32_t start = (uint32_t) SDL_GetTicks();

  const int32_t threadCount = Thread_Count();

  if (threadCount == 0) {
    RunWorkFunc(0);
  } else {
    WorkerThread *threads[threadCount];

    for (int32_t i = 0; i < threadCount; i++) {
      threads[i] = Thread_Create(RunWorkFunc, NULL, 0);
    }

    for (int32_t i = 0; i < threadCount; i++) {
      Thread_Wait(threads[i]);
    }
  }

  SDL_DestroyMutex(module.lock);
  module.lock = NULL;

  const uint32_t end = (uint32_t) SDL_GetTicks();

  if (module.name) {
    Com_Print(" %d ms\n", end - start);
  }
}

/**
 * @brief Outputs progress to the console.

 */
void Progress(const char *progress, int32_t percent) {
  static char *string = "-\\|/-|";
  static int32_t index = 0;
  static int32_t lastPercent;

  if (percent == -1) {
    Com_Print("\r%-24s [%c]", progress, string[index]);
    index = (index + 1) % q_strlen(string);
  } else {
    if (percent != lastPercent) {
      Com_Print("\r%-24s [%3d%%]", progress, percent);
      lastPercent = percent;
    }
  }
}
