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

#include <unistd.h>
#include <SDL/SDL_thread.h>

#include "client.h"

renderer_thread_t r_threadpool[2];


/*
 * R_ThreadSleep
 */
static void R_ThreadSleep(renderer_thread_t *t){

	while(t->state != THREAD_RUN){
		usleep(0);
	}
}


#define THREAD_IDLE_INTERVAL 100

/*
 * R_RunBspThread
 */
static int R_RunBspThread(void *p){

	while(r_bsp_thread.state > THREAD_DEAD){

		if(!r_view.ready){
			usleep(THREAD_IDLE_INTERVAL);
			continue;
		}

		//printf("R_RunBspThread: %u\n", SDL_ThreadID());

		R_ThreadSleep(&r_bsp_thread);

		R_UpdateFrustum();

		R_MarkLeafs();

		R_MarkSurfaces();

		r_bsp_thread.state = THREAD_WAIT;
	}

	return 0;
}


/*
 * R_RunCaptureThread
 */
static int R_RunCaptureThread(void *p){

	while(r_capture_thread.state > THREAD_DEAD){

		//printf("R_RunCaptureThread: %u\n", SDL_ThreadID());

		R_ThreadSleep(&r_capture_thread);

		R_FlushCapture();

		r_capture_thread.state = THREAD_WAIT;
	}

	return 0;
}


/*
 * R_WaitForThread
 */
void R_WaitForThread(renderer_thread_t *t){

	while(t->state == THREAD_RUN){
		t->wait_count++;
		usleep(0);
	}

	if(t->wait_count){
		//printf("Waited %d for %s\n", t->wait_count, t->name);
		t->wait_count = 0;
	}
}


/*
 * R_ShutdownThread
 */
static void R_ShutdownThread(renderer_thread_t *t){

	if(t->state == THREAD_DEAD)
		return;

	Com_Debug("Waiting for thread %s (%u)\n", t->name, SDL_GetThreadID(t->thread));

	R_WaitForThread(t);

	Com_Debug("Killing thread %s (%u)\n", t->name, SDL_GetThreadID(t->thread));

	SDL_KillThread(t->thread);

	t->state = THREAD_DEAD;
}


/*
 * R_InitThread
 *
 * Creates (and starts) a new thread.
 */
static void R_InitThread(renderer_thread_t *t, const char *name, int (func(void *p))){

	strncpy(t->name, name, sizeof(t->name) - 1);

	t->state = THREAD_IDLE;

	t->thread = SDL_CreateThread(func, NULL);

	Com_Debug("Spawned thread %s (%u)\n", t->name, SDL_GetThreadID(t->thread));
}


/*
 * Stops or starts the appropriate threads depending on the specified bit mask.
 */
void R_UpdateThreads(int mask){

	if(mask & 1)
		R_InitThread(&r_bsp_thread, "BSP", R_RunBspThread);
	else
		R_ShutdownThread(&r_bsp_thread);

	if(mask & 2)
		R_InitThread(&r_capture_thread, "Capture", R_RunCaptureThread);
	else
		R_ShutdownThread(&r_capture_thread);
}


/*
 * R_ShutdownThreads
 */
void R_ShutdownThreads(void){

	if(r_bsp_thread.state > THREAD_DEAD)
		R_ShutdownThread(&r_bsp_thread);

	if(r_capture_thread.state > THREAD_DEAD)
		R_ShutdownThread(&r_capture_thread);
}


/*
 * R_InitThreads
 *
 * Initializes the thread pool.  No threads are started here; we let the
 * modification of the console variable trigger that for us.
 */
void R_InitThreads(void){

	memset(&r_threadpool, 0, sizeof(r_threadpool));
}
