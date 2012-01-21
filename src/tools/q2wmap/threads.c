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

#include "q2wmap.h"

thread_work_t thread_work;


/*
 * GetThreadWork
 *
 * Return an iteration of work, updating progress when appropriate.
 */
static int GetThreadWork(void){
	int	r;
	int	f;

	ThreadLock();

	if(thread_work.index == thread_work.count){  // done
		ThreadUnlock();
		return -1;
	}

	// update work fraction and output progress if desired
	f = 10 * thread_work.index / thread_work.count;
	if(f != thread_work.fraction){
		thread_work.fraction = f;
		if(thread_work.progress && !(verbose || debug)){
			Com_Print("%i...", f);
			fflush(stdout);
		}
	}

	// assign the next work iteration
	r = thread_work.index;
	thread_work.index++;

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
static void ThreadWork(void *p){
	int work;

	while(true){
		work = GetThreadWork();
		if(work == -1)
			break;
		WorkFunction(work);
	}
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
	thread_t *t[64];
	int i;

	if(!threads->integer){
		ThreadWork(0);
		return;
	}

	lock = SDL_CreateMutex();

	for(i = 0; i < threads->integer; i++)
		t[i] = Thread_Create(ThreadWork, NULL);

	for(i = 0; i < threads->integer; i++)
		Thread_Wait(&t[i]);

	SDL_DestroyMutex(lock);
	lock = NULL;
}


/*
 * RunThreadsOn
 *
 * Entry point for all thread work requests.
 */
void RunThreadsOn(int workcount, boolean_t progress, void(*func)(int)){
	time_t start, end;

	thread_work.index = 0;
	thread_work.count = workcount;
	thread_work.fraction = -1;
	thread_work.progress = progress;

	WorkFunction = func;

	start = time(NULL);

	RunThreads();

	end = time(NULL);

	if(thread_work.progress)
		Com_Print(" (%i seconds)\n", (int)(end - start));
}

