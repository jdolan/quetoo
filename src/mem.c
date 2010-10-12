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

#include "mem.h"

#ifdef HAVE_SDL
#include <SDL/SDL_thread.h>
static SDL_mutex *z_lock;
#endif

static zhead_t z_chain;
static size_t z_count, z_bytes;


/*
 * Z_Init
 */
void Z_Init(void){

	z_chain.next = z_chain.prev = &z_chain;

#ifdef HAVE_SDL
	z_lock = SDL_CreateMutex();
#endif
}

/*
 * Z_Shutdown
 */
void Z_Shutdown(void){

	Z_FreeTags(-1);

#ifdef HAVE_SDL
	SDL_DestroyMutex(z_lock);
#endif
}


/*
 * Z_Free
 */
void Z_Free(void *ptr){
	zhead_t *z;

	z = ((zhead_t *)ptr) - 1;

	if(z->magic != Z_MAGIC){
		Com_Error(ERR_FATAL, "Z_Free: Bad magic for %p.", ptr);
	}

#ifdef HAVE_SDL
	SDL_mutexP(z_lock);
#endif

	z->prev->next = z->next;
	z->next->prev = z->prev;

#ifdef HAVE_SDL
	SDL_mutexV(z_lock);
#endif

	z_count--;
	z_bytes -= z->size;
	free(z);
}


/*
 * Z_FreeTags
 */
void Z_FreeTags(int tag){
	zhead_t *z, *next;

	for(z = z_chain.next; z != &z_chain; z = next){
		next = z->next;
		if(-1 == tag || z->tag == tag)
			Z_Free((void *)(z + 1));
	}
}


/*
 * Z_TagMalloc
 */
void *Z_TagMalloc(size_t size, int tag){
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = malloc(size);
	if(!z){
		Com_Error(ERR_FATAL, "Z_TagMalloc: Failed to allocate "Q2W_SIZE_T" bytes.", size);
	}

	z_count++;
	z_bytes += size;

	memset(z, 0, size);
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

#ifdef HAVE_SDL
	SDL_mutexP(z_lock);
#endif

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

#ifdef HAVE_SDL
	SDL_mutexV(z_lock);
#endif

	return (void *)(z + 1);
}


/*
 * Z_Malloc
 */
void *Z_Malloc(size_t size){
	return Z_TagMalloc(size, 0);
}

/*
 * Z_CopyString
 */
char *Z_CopyString(const char *in){
	char *out;

	out = Z_Malloc(strlen(in) + 1);
	strcpy(out, in);
	return out;
}
