/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

renderer_threadstate_t r_threadstate;

#define THREAD_SLEEP_INTERVAL 100


/*
 * R_RunThread
 */
int R_RunThread(void *p){

	Com_Dprintf("R_RunThread: %d\n", SDL_ThreadID());

	while(r_threads->value){

		if(!r_view.ready){
			usleep(THREAD_SLEEP_INTERVAL);
			continue;
		}

		// the renderer is up, so busy-wait for it
		while(r_threadstate.state != THREAD_BSP)
			usleep(0);

		R_UpdateFrustum();

		R_MarkLeafs();

		R_MarkSurfaces();

		r_threadstate.state = THREAD_RENDERER;
	}

	return 0;
}


/*
 * R_ShutdownThreads
 */
void R_ShutdownThreads(void){

	if(r_threadstate.thread)
		SDL_KillThread(r_threadstate.thread);

	r_threadstate.thread = NULL;
}


/*
 * R_InitThreads
 */
void R_InitThreads(void){

	r_threadstate.thread = SDL_CreateThread(R_RunThread, NULL);
}
