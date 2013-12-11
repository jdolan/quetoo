/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include <signal.h>
#include <SDL/SDL_thread.h>

#include "mem.h"

#define MEM_MAGIC 0x69
typedef byte mem_magic_t;

typedef struct mem_block_s {
	mem_magic_t magic;
	mem_tag_t tag; // for group free
	struct mem_block_s *parent;
	GList *children;
	size_t size;
} mem_block_t;

typedef struct {
	GHashTable *blocks;
	size_t size;
	SDL_mutex *lock;
} mem_state_t;

static mem_state_t mem_state;

/*
 * @brief Throws a fatal error if the specified memory block is non-NULL but
 * not owned by the memory subsystem.
 */
static mem_block_t *Mem_CheckMagic(void *p) {
	mem_block_t *b = NULL;

	if (p) {
		b = ((mem_block_t *) p) - 1;

		if (b->magic != MEM_MAGIC) {
			fprintf(stderr, "Invalid magic (%d) for %p\n", b->magic, p);
			raise(SIGABRT);
		}
	}

	return b;
}

/*
 * @brief Recursively frees linked managed memory.
 */
static void Mem_Free_(mem_block_t *b) {

	// recurse down the tree, freeing children
	if (b->children) {
		g_list_free_full(b->children, (GDestroyNotify) Mem_Free_);
	}

	// decrement the pool size and free the memory
	mem_state.size -= b->size;

	free(b);
}

/*
 * @brief Free an allocation of managed memory. Any child objects are
 * automatically freed as well.
 */
void Mem_Free(void *p) {
	mem_block_t *b = Mem_CheckMagic(p);

	SDL_mutexP(mem_state.lock);

	if (b->parent) {
		b->parent->children = g_list_remove(b->parent->children, b);
	} else {
		g_hash_table_remove(mem_state.blocks, (gconstpointer) b);
	}

	Mem_Free_(b);

	SDL_mutexV(mem_state.lock);
}

/*
 * @brief Free all managed items allocated with the specified tag.
 */
void Mem_FreeTag(mem_tag_t tag) {
	GHashTableIter it;
	gpointer key, value;

	SDL_mutexP(mem_state.lock);

	g_hash_table_iter_init(&it, mem_state.blocks);

	while (g_hash_table_iter_next(&it, &key, &value)) {
		mem_block_t *b = (mem_block_t *) key;

		if (tag == MEM_TAG_ALL || b->tag == tag) {
			g_hash_table_iter_remove(&it);
			Mem_Free_(b);
		}
	}

	SDL_mutexV(mem_state.lock);
}

/*
 * @brief Performs the grunt work of allocating a mem_block_t and inserting it
 * into the managed memory structures. Note that parent should be a pointer to
 * a previously allocated structure, and not to a mem_block_t.
 *
 * @param size The number of bytes to allocate.
 * @param tag The tag to allocate with (e.g. MEM_TAG_DEFAULT).
 * @param parent The parent to link this allocation to.
 *
 * @return A block of managed memory initialized to 0x0.
 */
static void *Mem_Malloc_(size_t size, mem_tag_t tag, void *parent) {
	mem_block_t *b, *p = Mem_CheckMagic(parent);

	// allocate the block plus the desired size
	const size_t s = size + sizeof(mem_block_t);

	if (!(b = calloc(s, 1))) {
		fprintf(stderr, "Failed to allocate %u bytes\n", (uint32_t) s);
		raise(SIGABRT);
	}

	b->magic = MEM_MAGIC;
	b->tag = tag;
	b->parent = p;
	b->size = size;

	// insert it into the managed memory structures
	SDL_mutexP(mem_state.lock);

	if (b->parent) {
		b->parent->children = g_list_prepend(b->parent->children, b);
	} else {
		g_hash_table_insert(mem_state.blocks, b, b);
	}

	mem_state.size += size;

	SDL_mutexV(mem_state.lock);

	// return the address in front of the block
	return (void *) (b + 1);
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
void *Mem_TagMalloc(size_t size, mem_tag_t tag) {
	return Mem_Malloc_(size, tag, NULL);
}

/*
 * @brief Allocates a block of managed memory with the specified parent.
 *
 * @param size The number of bytes to allocate.
 * @param parent The parent block previously allocated through Mem_Malloc /
 * Mem_TagMalloc. The returned block will automatically be released when the
 * parent is freed through Mem_Free.
 *
 * @return A block of managed memory initialized to 0x0.
 */
void *Mem_LinkMalloc(size_t size, void *parent) {
	return Mem_Malloc_(size, MEM_TAG_DEFAULT, parent);
}

/*
 * @brief Allocates a block of managed memory. All managed memory is freed when
 * the game exits, but may be explicitly freed with Mem_Free.
 *
 * @return A block of memory initialized to 0x0.
 */
void *Mem_Malloc(size_t size) {
	return Mem_Malloc_(size, MEM_TAG_DEFAULT, NULL);
}

/*
 * @brief Links the specified child to the given parent. The child will
 * subsequently be freed with the parent.
 *
 * @param child The child object, previously allocated with Mem_Malloc.
 * @param parent The parent object, previously allocated with Mem_Malloc.
 *
 * @return The child, for convenience.
 */
void *Mem_Link(void *child, void *parent) {
	mem_block_t *c = Mem_CheckMagic(child);
	mem_block_t *p = Mem_CheckMagic(parent);

	SDL_mutexP(mem_state.lock);

	if (c->parent) {
		c->parent->children = g_list_remove(c->parent->children, c);
	} else {
		g_hash_table_remove(mem_state.blocks, c);
	}

	c->parent = p;
	p->children = g_list_prepend(p->children, c);

	SDL_mutexV(mem_state.lock);

	return child;
}

/*
 * @return The current size (user bytes) of the zone allocation pool.
 */
size_t Mem_Size(void) {
	return mem_state.size;
}

/*
 * @brief Allocates and returns a copy of the specified string.
 */
char *Mem_CopyString(const char *in) {
	char *out;

	out = Mem_Malloc(strlen(in) + 1);
	strcpy(out, in);

	return out;
}

/*
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quake2World.
 */
void Mem_Init(void) {

	memset(&mem_state, 0, sizeof(mem_state));

	mem_state.blocks = g_hash_table_new(g_direct_hash, g_direct_equal);

	mem_state.lock = SDL_CreateMutex();
}

/*
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quake2World.
 */
void Mem_Shutdown(void) {

	Mem_FreeTag(MEM_TAG_ALL);

	g_hash_table_destroy(mem_state.blocks);

	SDL_DestroyMutex(mem_state.lock);
}
