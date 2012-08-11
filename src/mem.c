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

#include <SDL/SDL_thread.h>

static z_head_t z_chain;
static size_t z_count, z_bytes;
static SDL_mutex *z_lock;

/*
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quake2World.
 */
void Z_Init(void) {

	z_chain.next = z_chain.prev = &z_chain;

	z_lock = SDL_CreateMutex();
}

/*
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quake2World.
 */
void Z_Shutdown(void) {

	Z_FreeTag(-1);

	SDL_DestroyMutex(z_lock);
}

/*
 * @brief Performs the actual grunt work of freeing managed memory.
 */
static void Z_Free_(z_head_t *z) {

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;

	free(z);
}

/*
 * @brief Free an allocation of managed memory.
 */
void Z_Free(void *ptr) {
	z_head_t *z;

	z = ((z_head_t *) ptr) - 1;

	if (z->magic != Z_MAGIC) {
		Com_Error(ERR_FATAL, "Z_Free: Bad magic for %p.\n", ptr);
	}

	SDL_mutexP(z_lock);

	Z_Free_(z);

	SDL_mutexV(z_lock);
}

/*
 * @brief Free all managed items allocated with the specified tag.
 */
void Z_FreeTag(int16_t tag) {
	z_head_t *z, *next;

	SDL_mutexP(z_lock);

	for (z = z_chain.next; z != &z_chain; z = next) {
		next = z->next;
		if (-1 == tag || z->tag == tag)
			Z_Free_(z);
	}

	SDL_mutexV(z_lock);
}

/*
 * @brief Allocates and initializes a block of managed memory for the specified tag.
 * Tags allow related objects to be freed in bulk e.g. when a subsystem quits.
 */
void *Z_TagMalloc(size_t size, int16_t tag) {
	z_head_t *z;

	size = size + sizeof(z_head_t);
	z = malloc(size);
	if (!z) {
		Com_Error(ERR_FATAL, "Z_TagMalloc: Failed to allocate "Q2W_SIZE_T" bytes.\n", size);
	}

	memset(z, 0, size);
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	SDL_mutexP(z_lock);

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	z_count++;
	z_bytes += size;

	SDL_mutexV(z_lock);

	return (void *) (z + 1);
}

/*
 * @brief Allocates and initializes a block of managed memory. Free the memory via
 * Z_Free when done. As a fallback, all managed memory is freed by the engine
 * on exit.
 */
void *Z_Malloc(size_t size) {
	return Z_TagMalloc(size, 0);
}

/*
 * @brief Allocates and returns a copy of the specified string.
 */
char *Z_CopyString(const char *in) {
	char *out;

	out = Z_Malloc(strlen(in) + 1);
	strcpy(out, in);
	return out;
}
