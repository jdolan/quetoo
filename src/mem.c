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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <signal.h>
#include <SDL_thread.h>

#include "mem.h"

#if defined(SUPER_MEMORY_CHECKS)
  #define MAX_MEMORY_STACK 32

  #if defined(WIN32)
    #include <DbgHelp.h>
  #endif
#endif

#define MEM_MAGIC 0x69696969
typedef uint32_t mem_magic_t;

typedef struct mem_block_s {
	mem_magic_t magic;
	mem_tag_t tag; // for group free
	struct mem_block_s *parent;
	GSList *children;
	size_t size;
#if defined(SUPER_MEMORY_CHECKS)
	void *stack[MAX_MEMORY_STACK];
#endif
} mem_block_t;

typedef struct {
	mem_magic_t magic;
} mem_footer_t;

typedef struct {
	GHashTable *blocks;
	size_t size;
	SDL_SpinLock lock;
} mem_state_t;

static mem_state_t mem_state;

#if defined(SUPER_MEMORY_CHECKS)
/**
 * @brief
 */
static void Mem_SetStack(mem_block_t *block) {
#if defined(WIN32)
	CaptureStackBackTrace(0, MAX_MEMORY_STACK, block->stack, NULL);
#elif HAVE_EXECINFO
	backtrace(block->stack, MAX_MEMORY_STACK);
#endif
}

/**
 * @brief
 */
static void Mem_Print(const mem_block_t *block, const char *why) {
#if defined(WIN32) && defined(SUPER_MEMORY_PRINTS)
	static char str[MAX_STRING_CHARS];
	
	g_snprintf(str, sizeof(str), "%s block 0x%" PRIXPTR " tag %i\n", why, (ULONG64) (block + 1), block->tag);
	OutputDebugString(str);

	for (int32_t i = 3; i < 5; i++) {
		
		if (!block->stack[i]) {
			break;
		}

		g_snprintf(str, sizeof(str), "  [0x%" PRIXPTR "]\n",  (ULONG64) (block->stack[i]));
		OutputDebugString(str);
	}
#else
#endif
}
#endif

/**
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

		mem_footer_t *footer = (mem_footer_t *) (((byte *) p) + b->size);

		if (footer->magic != (MEM_MAGIC + b->size)) {
			fprintf(stderr, "Invalid footer magic (%d) for %p\n", b->magic, p);
			raise(SIGABRT);
		}
	}

	return b;
}

/**
 * @brief
 */
void Mem_Check(void *p) {
	Mem_CheckMagic(p);
}

/**
 * @brief Recursively frees linked managed memory.
 */
static void Mem_Free_(mem_block_t *b) {

#if defined(SUPER_MEMORY_CHECKS)
	Mem_Print(b, "Freeing");
#endif

	// recurse down the tree, freeing children
	if (b->children) {
		g_slist_free_full(b->children, (GDestroyNotify) Mem_Free_);
	}

	// decrement the pool size and free the memory
	mem_state.size -= b->size;

	free(b);
}

/**
 * @brief Free an allocation of managed memory. Any child objects are
 * automatically freed as well.
 */
void Mem_Free(void *p) {
	if (p) {
		mem_block_t *b = Mem_CheckMagic(p);

		SDL_AtomicLock(&mem_state.lock);

		if (b->parent) {
			b->parent->children = g_slist_remove(b->parent->children, b);
		} else {
			g_hash_table_remove(mem_state.blocks, (gconstpointer) b);
		}

		Mem_Free_(b);

		SDL_AtomicUnlock(&mem_state.lock);
	}
}

/**
 * @brief Free all managed items allocated with the specified tag.
 */
void Mem_FreeTag(mem_tag_t tag) {
	GHashTableIter it;
	gpointer key, value;

	SDL_AtomicLock(&mem_state.lock);

	g_hash_table_iter_init(&it, mem_state.blocks);

	while (g_hash_table_iter_next(&it, &key, &value)) {
		mem_block_t *b = (mem_block_t *) key;

		if (tag == MEM_TAG_ALL || b->tag == tag) {
			g_hash_table_iter_remove(&it);
			Mem_Free_(b);
		}
	}

	SDL_AtomicUnlock(&mem_state.lock);
}

/**
 * @brief Returns the total size of a memory block.
 */
static size_t Mem_BlockSize(const size_t size) {
	return size + sizeof(mem_block_t) + sizeof(mem_footer_t);
}

/**
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
	const size_t s = Mem_BlockSize(size);

	if (!(b = calloc(1, s))) {
		fprintf(stderr, "Failed to allocate %u bytes\n", (uint32_t) s);
		raise(SIGABRT);
		return NULL;
	}

	b->magic = MEM_MAGIC;
	b->tag = tag;
	b->parent = p;
	b->size = size;

	void *data = (void *) (b + 1);

	mem_footer_t *footer = (mem_footer_t *) (((byte *) data) + size);
	footer->magic = (mem_magic_t) (MEM_MAGIC + b->size);

	// insert it into the managed memory structures
	SDL_AtomicLock(&mem_state.lock);

	if (b->parent) {
		b->parent->children = g_slist_prepend(b->parent->children, b);
	} else {
		g_hash_table_add(mem_state.blocks, b);
	}

	mem_state.size += size;
	
#if defined(SUPER_MEMORY_CHECKS)
	Mem_SetStack(b);
#endif

	SDL_AtomicUnlock(&mem_state.lock);

	// return the address in front of the block
	return data;
}

/**
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

/**
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

/**
 * @brief Allocates a block of managed memory. All managed memory is freed when
 * the game exits, but may be explicitly freed with Mem_Free.
 *
 * @return A block of memory initialized to 0x0.
 */
void *Mem_Malloc(size_t size) {
	return Mem_Malloc_(size, MEM_TAG_DEFAULT, NULL);
}

/**
 * @brief Reallocates a block of memory with a new size.
 *
 * @return The new pointer to the resized memory. Do not try to write to p after
 * calling this function, the results are undefined!
 */
void *Mem_Realloc(void *p, size_t size) {

	if (!p) {
		return Mem_Malloc(size);
	}

	mem_block_t *b = Mem_CheckMagic(p), *new_b;

	// no change to size
	if (b->size == size) {
		return (void *) (b + 1);
	}

	// allocate the block plus the desired size
	const size_t old_size = b->size;
	const size_t s = Mem_BlockSize(size);

	b->size = size;

#if defined(SUPER_MEMORY_PRINTS)
	Mem_Print(b, "Reallocating");
#endif

	if (!(new_b = realloc(b, s))) {
		fprintf(stderr, "Failed to re-allocate %u bytes\n", (uint32_t) s);
		raise(SIGABRT);
		return NULL;
	}

	void *data = (void *) (new_b + 1);

	mem_footer_t *footer = (mem_footer_t *) (((byte *) data) + size);
	footer->magic = (mem_magic_t) (MEM_MAGIC + new_b->size);

	SDL_AtomicLock(&mem_state.lock);

	// re-seat us in our parent or in global hash list
	if (new_b->parent) {
		new_b->parent->children = g_slist_remove(new_b->parent->children, b);
		new_b->parent->children = g_slist_prepend(new_b->parent->children, new_b);
	} else {
		g_hash_table_remove(mem_state.blocks, b);
		g_hash_table_add(mem_state.blocks, new_b);
	}

	// change our childrens' parent pointers
	if (new_b->children) {
		for (GSList *children = new_b->children; children; children = children->next) {
			mem_block_t *child = (mem_block_t *) children->data;
			child->parent = new_b;
		}
	}

	mem_state.size -= old_size;
	mem_state.size += size;
	
#if defined(SUPER_MEMORY_CHECKS)
	Mem_SetStack(new_b);

#if defined(SUPER_MEMORY_PRINTS)
	Mem_Print(new_b, "Reallocated");
#endif
#endif

	SDL_AtomicUnlock(&mem_state.lock);

	return data;
}

/**
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

	SDL_AtomicLock(&mem_state.lock);

	if (c->parent) {
		c->parent->children = g_slist_remove(c->parent->children, c);
	} else {
		g_hash_table_remove(mem_state.blocks, c);
	}

	c->parent = p;
	p->children = g_slist_prepend(p->children, c);

	SDL_AtomicUnlock(&mem_state.lock);

	return child;
}

/**
 * @return The current size (user bytes) of the zone allocation pool.
 */
size_t Mem_Size(void) {
	return mem_state.size;
}

/**
 * @brief Allocates and returns a copy of the specified string.
 */
char *Mem_TagCopyString(const char *in, mem_tag_t tag) {
	char *out;

	out = Mem_TagMalloc(strlen(in) + 1, tag);
	strcpy(out, in);

	return out;
}

/**
 * @brief Allocates and returns a copy of the specified string.
 */
char *Mem_CopyString(const char *in) {

	return Mem_TagCopyString(in, MEM_TAG_DEFAULT);
}

/**
 * @brief
 */
static gint Mem_Stats_Sort(gconstpointer a, gconstpointer b) {
	return (gint) (((const mem_stat_t *) b)->size - ((const mem_stat_t *) a)->size);
}

/**
 * @brief
 */
static size_t Mem_CalculateBlockSize(const mem_block_t *b) {

	size_t size = b->size;

	for (GSList *child = b->children; child; child = child->next) {
		size += Mem_CalculateBlockSize((const mem_block_t *) child->data);
	}

	return size;
}

/**
 * @brief Fetches stats about allocated memory to the console.
 */
GArray *Mem_Stats(void) {

	GHashTableIter it;
	gpointer key, value;

	SDL_AtomicLock(&mem_state.lock);

	GArray *stat_array = g_array_new(false, true, sizeof(mem_stat_t));

	stat_array = g_array_append_vals(stat_array, &(const mem_stat_t) {
		.tag = -1,
		 .size = mem_state.size,
		  .count = 0
	}, 1);

	g_hash_table_iter_init(&it, mem_state.blocks);

	while (g_hash_table_iter_next(&it, &key, &value)) {
		const mem_block_t *b = (const mem_block_t *) key;
		mem_stat_t *stats = NULL;

		for (size_t i = 0; i < stat_array->len; i++) {

			mem_stat_t *stat_i = &g_array_index(stat_array, mem_stat_t, i);

			if (stat_i->tag == b->tag) {
				stats = stat_i;
				break;
			}
		}

		if (stats == NULL) {
			stat_array = g_array_append_vals(stat_array, &(const mem_stat_t) {
				.tag = b->tag,
				 .size = Mem_CalculateBlockSize(b),
				  .count = 1
			}, 1);
		} else {
			stats->size += Mem_CalculateBlockSize(b);
			stats->count++;
		}
	}

	SDL_AtomicUnlock(&mem_state.lock);

	g_array_sort(stat_array, Mem_Stats_Sort);

	return stat_array;
}

/**
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quetoo.
 */
void Mem_Init(void) {

	memset(&mem_state, 0, sizeof(mem_state));

	mem_state.blocks = g_hash_table_new(g_direct_hash, g_direct_equal);
}

/**
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quetoo.
 */
void Mem_Shutdown(void) {

	Mem_FreeTag(MEM_TAG_ALL);

	g_hash_table_destroy(mem_state.blocks);
}
