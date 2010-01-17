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

#include <SDL/SDL_thread.h>

#include "q2wmap.h"

#define	MAX_THREADS	8

threadstate_t threadstate;


/*
 * GetThreadWork
 *
 * Return an iteration of work, updating progress when appropriate.
 */
static int GetThreadWork(void){
	int	r;
	int	f;

	ThreadLock();

	if(threadstate.workindex == threadstate.workcount){  // done
		ThreadUnlock();
		return -1;
	}

	// update work fraction and output progress if desired
	f = 10 * threadstate.workindex / threadstate.workcount;
	if(f != threadstate.workfrac){
		threadstate.workfrac = f;
		if(threadstate.progress && !(verbose || debug)){
			Print("%i...", f);
			fflush(stdout);
		}
	}

	// assign the next work iteration
	r = threadstate.workindex;
	threadstate.workindex++;

	ThreadUnlock();

	return r;
}


// generic function pointer to actual work to be done
static void(*WorkFunction)(int);


/*
 * ThreadWork
 *
 * Shared work entry point by all threads.  Retrieve and perform
 * chunks of work iteratively until work is finished.
 */
static int ThreadWork(void *p){
	int work;

	while(true){
		work = GetThreadWork();
		if(work == -1)
			break;
		WorkFunction(work);
	}

	return 0;
}


SDL_mutex *lock = NULL;

/*
 * ThreadLock
 */
void ThreadLock(void){

	if(!lock)
		return;

	SDL_mutexP(lock);
}


/*
 * ThreadUnlock
 */
void ThreadUnlock(void){

	if(!lock)
		return;

	SDL_mutexV(lock);
}


/*
 * RunThreads
 */
static void RunThreads(void){
	SDL_Thread *threads[MAX_THREADS];
	int i;

	if(threadstate.numthreads == 1){
		ThreadWork(0);
		return;
	}

	lock = SDL_CreateMutex();

	for(i = 0; i < threadstate.numthreads; i++)
		threads[i] = SDL_CreateThread(ThreadWork, NULL);

	for(i = 0; i < threadstate.numthreads; i++)
		SDL_WaitThread(threads[i], NULL);

	SDL_DestroyMutex(lock);
	lock = NULL;
}


/*
 * RunThreadsOn
 *
 * Entry point for all thread work requests.
 */
void RunThreadsOn(int workcount, qboolean progress, void(*func)(int)){
	time_t start, end;

	if(threadstate.numthreads < 1)  // ensure safe thread counts
		threadstate.numthreads = 1;

	if(threadstate.numthreads > MAX_THREADS)
		threadstate.numthreads = MAX_THREADS;

	threadstate.workindex = 0;
	threadstate.workcount = workcount;
	threadstate.workfrac = -1;
	threadstate.progress = progress;

	WorkFunction = func;

	start = time(NULL);

	RunThreads();

	end = time(NULL);

	if(threadstate.progress)
		Print(" (%i seconds)\n", (int)(end - start));
}

