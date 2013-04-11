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

#define Z_MAGIC 0x69
typedef byte z_magic_t;

typedef struct z_block_s {
	z_magic_t magic;
	z_tag_t tag; // for group free
	struct z_block_s *parent;
	GList *children;
	size_t size;
} z_block_t;

typedef struct {
	GList *blocks;
	size_t size;
	SDL_mutex *lock;
} z_state_t;

static z_state_t z_state;

/*
 * @brief Performs the actual grunt work of freeing managed memory.
 */
static void Z_Free_(gpointer data) {
	z_block_t *z = (z_block_t *) data;

	if (z->children) {
		GList *next, *c = z->children;
		while (c) {
			next = c->next;
			Z_Free_(c->data);
			c = next;
		}
		g_list_free(z->children);
	}

	if (z->parent) {
		z->parent->children = g_list_remove(z->parent->children, data);
	} else {
		z_state.blocks = g_list_remove(z_state.blocks, data);
	}

	z_state.size -= z->size;

	free(z);
}

/*
 * @brief Free an allocation of managed memory.
 */
void Z_Free(void *p) {
	z_block_t *z = ((z_block_t *) p) - 1;

	if (z->magic != Z_MAGIC) {
		Com_Error(ERR_FATAL, "Z_Free: Bad magic for %p.\n", p);
	}

	SDL_mutexP(z_state.lock);

	Z_Free_(z);

	SDL_mutexV(z_state.lock);
}

/*
 * @brief Free all managed items allocated with the specified tag.
 */
void Z_FreeTag(z_tag_t tag) {

	SDL_mutexP(z_state.lock);

	GList *e = z_state.blocks;

	while (e) {
		GList *next = e->next;

		z_block_t *z = (z_block_t *) e->data;
		if (tag == Z_TAG_ALL || z->tag == tag) {
			Z_Free_(z);
		}

		e = next;
	}

	SDL_mutexV(z_state.lock);
}

/*
 * @brief Performs the grunt work of allocating a z_block_t and inserting it
 * into the managed memory structures. Note that parent should be a pointer to
 * a previously allocated structure, and not to a z_block_t.
 *
 * @param size The number of bytes to allocate.
 * @param tag The tag to allocate with (e.g. Z_TAG_DEFAULT).
 * @param parent The parent to link this allocation to.
 *
 * @return A block of managed memory initialized to 0x0.
 */
static void *Z_Malloc_(size_t size, z_tag_t tag, void *parent) {
	z_block_t *z, *p = NULL;

	// ensure the specified parent was valid
	if (parent) {
		p = ((z_block_t *) parent) - 1;

		if (p->magic != Z_MAGIC) {
			Com_Error(ERR_FATAL, "Z_Malloc_: Invalid parent.\n");
		}
	}

	// allocate the block plus the desired size
	const size_t s = size + sizeof(z_block_t);

	if (!(z = malloc(s))) {
		Com_Error(ERR_FATAL, "Z_Malloc_: Failed to allocate %zu bytes.\n", s);
	}

	// clear it to 0x0
	memset(z, 0, s);

	z->magic = Z_MAGIC;
	z->tag = tag;
	z->parent = p;
	z->size = size;

	// insert it into the managed memory structures
	SDL_mutexP(z_state.lock);

	if (z->parent) {
		z->parent->children = g_list_prepend(z->parent->children, z);
	} else {
		z_state.blocks = g_list_prepend(z_state.blocks, z);
	}

	z_state.size += size;

	SDL_mutexV(z_state.lock);

	// return the address in front of the block
	return (void *) (z + 1);
}

/*
 * @brief Allocates a block of managed memory with the specified tag.
 *
 * @param size The number of bytes to allocate.
 * @param tag Tags allow related objects to be freed in bulk e.g. when a
 * subsystem quits.
 *
 * @return A block of managed memory initialized to 0x0.
 */
void *Z_TagMalloc(size_t size, z_tag_t tag) {
	return Z_Malloc_(size, tag, NULL);
}

/*
 * @brief Allocates a block of managed memory with the specified parent.
 *
 * @param size The number of bytes to allocate.
 * @param parent The parent block previously allocated through Z_Malloc /
 * Z_TagMalloc. The returned block will automatically be released when the
 * parent is freed through Z_Free.
 *
 * @return A block of managed memory initialized to 0x0.
 */
void *Z_LinkMalloc(size_t size, void *parent) {
	return Z_Malloc_(size, Z_TAG_DEFAULT, parent);
}

/*
 * @brief Allocates a block of managed memory. All managed memory is freed when
 * the game exits, but may be explicitly freed with Z_Free.
 *
 * @return A block of memory initialized to 0x0.
 */
void *Z_Malloc(size_t size) {
	return Z_Malloc_(size, Z_TAG_DEFAULT, NULL);
}

/*
 * @brief Returns the current size (user bytes) of the zone allocation pool.
 */
size_t Z_Size(void) {
	return  z_state.size;
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

/*
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quake2World.
 */
void Z_Init(void) {

	memset(&z_state, 0, sizeof(z_state));

	z_state.lock = SDL_CreateMutex();
}

/*
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quake2World.
 */
void Z_Shutdown(void) {

	Z_FreeTag(Z_TAG_ALL);

	SDL_DestroyMutex(z_state.lock);
}
