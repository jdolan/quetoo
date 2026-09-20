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

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_timer.h>

#include "shared/qstring.h"
#include "thread.h"

typedef struct {

  /**
   * @brief The lock governing global thread pool access.
   */
  SDL_SpinLock lock;

  /**
   * @brief The thread dispatch identifier.
   */
  SDL_AtomicInt id;

  /**
   * @brief The number of threads in the pool.
   */
  size_t numThreads;

  /**
   * @brief The threads.
   */
  WorkerThread *threads;
} WorkerThreadPool;

static WorkerThreadPool threadPool;

/**
 * @brief A sentinel thread function to indicate thread termination.
 */
static ThreadRunFunc ThreadTerminate = (ThreadRunFunc) &ThreadTerminate;

/**
 * @brief The main thread ID.
 */
SDL_ThreadID threadMain;

/**
 * @brief The current thread ID.
 */
_Thread_local SDL_ThreadID threadId;

/**
 * @brief Wrap the user's function in our own for introspection.
 */
static int32_t Thread_Run(void *data) {
  WorkerThread *t = (WorkerThread *) data;

  threadId = SDL_GetCurrentThreadID();

  while (t->Run != ThreadTerminate) {

    SDL_LockMutex(t->mutex);

    if (t->status == THREAD_RUNNING) {
      t->Run(t->data);
      if (t->options & THREAD_NO_WAIT) {
        t->status = THREAD_IDLE;
      } else {
        t->status = THREAD_WAITING;
      }
      SDL_SignalCondition(t->cond);
    } else {
      SDL_WaitCondition(t->cond, t->mutex);
    }

    SDL_UnlockMutex(t->mutex);
  }

  return 0;
}

/**
 * @brief Initializes the threads backing the thread pool.
 */
static void Thread_Init_(ssize_t numThreads) {

  if (numThreads == 0) {
    numThreads = SDL_GetNumLogicalCPUCores();
  } else if (numThreads == -1) {
    numThreads = 0;
  } else if (numThreads > MAX_THREADS) {
    numThreads = MAX_THREADS;
  }

  threadPool.numThreads = numThreads;

  if (threadPool.numThreads) {
    threadPool.threads = Mem_Malloc(sizeof(WorkerThread) * threadPool.numThreads);

    WorkerThread *t = threadPool.threads;

    for (size_t i = 0; i < threadPool.numThreads; i++, t++) {
      t->cond = SDL_CreateCondition();
      t->mutex = SDL_CreateMutex();
      t->thread = SDL_CreateThread(Thread_Run, __func__, t);
    }
  }
}

/**
 * `Thread_Shutdown_`
 */
static void Thread_Shutdown_(void) {

  if (threadPool.numThreads) {
    WorkerThread *t = threadPool.threads;

    for (size_t i = 0; i < threadPool.numThreads; i++, t++) {
      Thread_Wait(t);
      t->Run = ThreadTerminate;
      SDL_SignalCondition(t->cond);
      SDL_WaitThread(t->thread, NULL);
      SDL_DestroyCondition(t->cond);
      SDL_DestroyMutex(t->mutex);
    }

    Mem_Free(threadPool.threads);
  }
}

/**
 * @brief Creates a new thread to run the specified function. Callers must use
 * `Thread_Wait` on the returned handle to release the thread when finished.
 */
WorkerThread *Thread_Create_(const char *name, ThreadRunFunc run, void *data, WorkerThreadOptions options) {

  // if threads are available, find an idle one and dispatch it
  if (threadPool.numThreads) {
    SDL_LockSpinlock(&threadPool.lock);

    WorkerThread *t = threadPool.threads;
    for (size_t i = 0; i < threadPool.numThreads; i++, t++) {

      // if the thread appears idle, lock it and check again
      if (t->status == THREAD_IDLE) {

        SDL_LockMutex(t->mutex);

        // if the thread is idle, dispatch it
        if (t->status == THREAD_IDLE) {
          t->status = THREAD_RUNNING;
          t->options = options;

          q_strlcpy(t->name, name, sizeof(t->name));

          t->Run = run;
          t->data = data;

          SDL_UnlockMutex(t->mutex);
          SDL_SignalCondition(t->cond);

          SDL_UnlockSpinlock(&threadPool.lock);
          return t;
        }

        SDL_UnlockMutex(t->mutex);
      }
    }

    SDL_UnlockSpinlock(&threadPool.lock);
  }

  // if we failed to allocate a thread, run the function in this thread

  run(data);
  return NULL;
}

/**
 * @brief Wait for the specified thread to complete.
 */
void Thread_Wait(WorkerThread *t) {

  if (!t) {
    return;
  }

  SDL_LockMutex(t->mutex);

  if (t->status == THREAD_RUNNING) {
    SDL_WaitCondition(t->cond, t->mutex);
    assert(t->status == THREAD_WAITING);
  }

  SDL_UnlockMutex(t->mutex);

  t->status = THREAD_IDLE;
}

/**
 * @brief Returns the number of threads in the pool.
 */
int32_t Thread_Count(void) {
  return (int32_t) threadPool.numThreads;
}

/**
 * @brief Initializes the thread pool.
 */
void Thread_Init(ssize_t numThreads) {

  memset(&threadPool, 0, sizeof(threadPool));

  Thread_Init_(numThreads);

  threadMain = SDL_GetCurrentThreadID();
}

/**
 * @brief Shuts down the thread pool.
 */
void Thread_Shutdown(void) {

  Thread_Shutdown_();

  memset(&threadPool, 0, sizeof(threadPool));
}
