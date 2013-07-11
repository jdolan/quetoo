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

#include "common.h"

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
	GHashTable *blocks;
	size_t size;
	SDL_mutex *lock;
} z_state_t;

static z_state_t z_state;

/*
 * @brief Throws a fatal error if the specified memory block is non-NULL but
 * not owned by the memory subsystem.
 */
static z_block_t *Z_CheckMagic(void *p) {
	z_block_t *z = NULL;

	if (p) {
		z = ((z_block_t *) p) - 1;

		if (z->magic != Z_MAGIC) {
			Com_Error(ERR_FATAL, "Invalid magic (%d) for %p\n", z->magic, p);
		}
	}

	return z;
}

/*
 * @brief Recursively frees linked managed memory.
 */
static void Z_Free_(z_block_t *z) {

	// recurse down the tree, freeing children
	if (z->children) {
		g_list_free_full(z->children, (GDestroyNotify) Z_Free_);
	}

	// decrement the pool size and free the memory
	z_state.size -= z->size;

	free(z);
}

/*
 * @brief Free an allocation of managed memory. Any child objects are
 * automatically freed as well.
 */
void Z_Free(void *p) {
	z_block_t *z = Z_CheckMagic(p);

	SDL_mutexP(z_state.lock);

	if (z->parent) {
		z->parent->children = g_list_remove(z->parent->children, z);
	} else {
		g_hash_table_remove(z_state.blocks, (gconstpointer) z);
	}

	Z_Free_(z);

	SDL_mutexV(z_state.lock);
}

/*
 * @brief Free all managed items allocated with the specified tag.
 */
void Z_FreeTag(z_tag_t tag) {
	GHashTableIter it;
	gpointer key, value;

	SDL_mutexP(z_state.lock);

	g_hash_table_iter_init(&it, z_state.blocks);

	while (g_hash_table_iter_next(&it, &key, &value)) {
		z_block_t *z = (z_block_t *) key;

		if (tag == Z_TAG_ALL || z->tag == tag) {
			g_hash_table_iter_remove(&it);
			Z_Free_(z);
		}
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
	z_block_t *z, *p = Z_CheckMagic(parent);

	// allocate the block plus the desired size
	const size_t s = size + sizeof(z_block_t);

	if (!(z = calloc(s, 1))) {
		Com_Error(ERR_FATAL, "Failed to allocate %u bytes\n", (uint32_t) s);
	}

	z->magic = Z_MAGIC;
	z->tag = tag;
	z->parent = p;
	z->size = size;

	// insert it into the managed memory structures
	SDL_mutexP(z_state.lock);

	if (z->parent) {
		z->parent->children = g_list_prepend(z->parent->children, z);
	} else {
		g_hash_table_insert(z_state.blocks, z, z);
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
 * @brief Links the specified child to the given parent. The child will
 * subsequently be freed with the parent.
 *
 * @param child The child object, previously allocated with Z_Malloc.
 * @param parent The parent object, previously allocated with Z_Malloc.
 *
 * @return The child, for convenience.
 */
void *Z_Link(void *child, void *parent) {
	z_block_t *c = Z_CheckMagic(child);
	z_block_t *p = Z_CheckMagic(parent);

	SDL_mutexP(z_state.lock);

	if (c->parent) {
		c->parent->children = g_list_remove(c->parent->children, c);
	} else {
		g_hash_table_remove(z_state.blocks, c);
	}

	c->parent = p;
	p->children = g_list_prepend(p->children, c);

	SDL_mutexV(z_state.lock);

	return child;
}

/*
 * @return The current size (user bytes) of the zone allocation pool.
 */
size_t Z_Size(void) {
	return z_state.size;
}

/*
 * @brief Prints the current size (in MB) of the zone allocation pool.
 */
void Z_Size_f(void) {
	Com_Print("%.2fMB\n", (vec_t) (Z_Size() / (1024.0 * 1024.0)));
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

	z_state.blocks = g_hash_table_new(g_direct_hash, g_direct_equal);

	z_state.lock = SDL_CreateMutex();
}

/*
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quake2World.
 */
void Z_Shutdown(void) {

	Z_FreeTag(Z_TAG_ALL);

	g_hash_table_destroy(z_state.blocks);

	SDL_DestroyMutex(z_state.lock);
}
