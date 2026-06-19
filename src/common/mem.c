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
#include <SDL3/SDL_thread.h>

#include <Objectively/Vector.h>

#include "mem.h"

#define MEM_MAGIC 0x69696969
typedef uint32_t mem_magic_t;

typedef struct mem_block_s {
  mem_magic_t magic;
  mem_tag_t tag; // for group free
  struct mem_block_s *parent;
  struct mem_block_s *first_child;   // head of intrusive children list
  struct mem_block_s *next_sibling;  // next in parent's children list
  struct mem_block_s *prev_block;    // prev in global block list
  struct mem_block_s *next_block;    // next in global block list
  size_t size;
} mem_block_t;

typedef struct {
  mem_magic_t magic;
} mem_footer_t;

typedef struct {
  mem_block_t *head;   // head of global doubly-linked block list
  size_t size;
  SDL_SpinLock lock;
} mem_state_t;

static mem_state_t mem_state;

/**
 * @brief Returns a properly aligned pointer to the footer for a data block.
 */
static inline mem_footer_t *Mem_Footer(const void *data, size_t size) {
  const uintptr_t addr = (uintptr_t) ((const byte *) data + size);
  const uintptr_t aligned = (addr + _Alignof(mem_footer_t) - 1) & ~((uintptr_t) _Alignof(mem_footer_t) - 1);
  return (mem_footer_t *) aligned;
}

/**
 * @brief Throws a fatal error if the specified memory block is non-`NULL` but
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

    mem_footer_t *footer = Mem_Footer(p, b->size);

    if (footer->magic != (MEM_MAGIC + b->size)) {
      fprintf(stderr, "Invalid footer magic (%d) for %p\n", b->magic, p);
      raise(SIGABRT);
    }
  }

  return b;
}

/**
 * @brief Validates the magic number of the specified managed memory block, aborting on corruption.
 */
void Mem_Check(void *p) {
  Mem_CheckMagic(p);
}

/**
 * @brief Recursively frees linked managed memory.
 */
static void Mem_Free_(mem_block_t *b) {

  // Validate this block before freeing its children
  if (b->magic != MEM_MAGIC) {
    fprintf(stderr, "Mem_Free_: Corrupted block %p (magic: %d, expected: %d)\n", 
            (void *)b, b->magic, MEM_MAGIC);
    raise(SIGABRT);
  }

  // Validate all children before freeing them
  if (b->first_child) {
    mem_block_t *child = b->first_child;
    int child_num = 0;
    bool has_corruption = false;
    while (child) {
      if (child->magic != MEM_MAGIC) {
        has_corruption = true;
        fprintf(stderr, "Mem_Free_: Parent %p (tag: %d, size: %zu) has corrupted child #%d: %p (magic: %d)\n",
                (void *)b, b->tag, b->size, child_num, (void *)child, child->magic);
        fprintf(stderr, "  First 8 words of bad child: %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n",
                ((unsigned long long *)child)[0], ((unsigned long long *)child)[1],
                ((unsigned long long *)child)[2], ((unsigned long long *)child)[3],
                ((unsigned long long *)child)[4], ((unsigned long long *)child)[5],
                ((unsigned long long *)child)[6], ((unsigned long long *)child)[7]);
      }
      child = child->next_sibling;
      child_num++;
    }

    if (has_corruption) {
      fprintf(stderr, "Mem_Free_: Parent details - address: %p, magic: %d, tag: %d, size: %zu, num_children: %d\n",
              (void *)b, b->magic, b->tag, b->size, child_num);
      raise(SIGABRT);
    }

    // Recursively free children
    child = b->first_child;
    while (child) {
      mem_block_t *next = child->next_sibling;
      Mem_Free_(child);
      child = next;
    }
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

    SDL_LockSpinlock(&mem_state.lock);

    if (b->parent) {
      // Unlink from parent's intrusive children list
      mem_block_t **pp = &b->parent->first_child;
      while (*pp && *pp != b) {
        pp = &(*pp)->next_sibling;
      }
      if (*pp) { *pp = b->next_sibling; }
    } else {
      // Unlink from global doubly-linked list
      if (b->prev_block) { b->prev_block->next_block = b->next_block; }
      else { mem_state.head = b->next_block; }
      if (b->next_block) { b->next_block->prev_block = b->prev_block; }
    }

    Mem_Free_(b);

    SDL_UnlockSpinlock(&mem_state.lock);
  }
}

/**
 * @brief Free all managed items allocated with the specified tag.
 */
void Mem_FreeTag(mem_tag_t tag) {

  SDL_LockSpinlock(&mem_state.lock);

  mem_block_t *b = mem_state.head;
  while (b) {
    mem_block_t *next = b->next_block;
    if (tag == MEM_TAG_ALL || b->tag == tag) {
      // Unlink from global list
      if (b->prev_block) { b->prev_block->next_block = b->next_block; }
      else { mem_state.head = b->next_block; }
      if (b->next_block) { b->next_block->prev_block = b->prev_block; }
      Mem_Free_(b);
    }
    b = next;
  }

  SDL_UnlockSpinlock(&mem_state.lock);
}

/**
 * @brief Returns the total size of a memory block.
 */
static size_t Mem_BlockSize(const size_t size) {
  const size_t aligned = (size + _Alignof(mem_footer_t) - 1) & ~(_Alignof(mem_footer_t) - 1);
  return sizeof(mem_block_t) + aligned + sizeof(mem_footer_t);
}

/**
 * @brief Performs the grunt work of allocating a `mem_block_t` and inserting it
 * into the managed memory structures. Note that parent should be a pointer to
 * a previously allocated structure, and not to a `mem_block_t`.
 *
 * @param size The number of bytes to allocate.
 * @param tag The tag to allocate with (e.g. `MEM_TAG_DEFAULT`).
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

  mem_footer_t *footer = Mem_Footer(data, size);
  footer->magic = (mem_magic_t) (MEM_MAGIC + b->size);

  // insert it into the managed memory structures
  SDL_LockSpinlock(&mem_state.lock);

  if (b->parent) {
    // Prepend to parent's intrusive children list
    b->next_sibling = b->parent->first_child;
    b->parent->first_child = b;
  } else {
    // Prepend to global doubly-linked list
    b->next_block = mem_state.head;
    b->prev_block = NULL;
    if (mem_state.head) { mem_state.head->prev_block = b; }
    mem_state.head = b;
  }

  mem_state.size += size;

  SDL_UnlockSpinlock(&mem_state.lock);

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
 * @param parent The parent block previously allocated through `Mem_Malloc` /
 * `Mem_TagMalloc`. The returned block will automatically be released when the
 * parent is freed through `Mem_Free`.
 *
 * @return A block of managed memory initialized to 0x0.
 */
void *Mem_LinkMalloc(size_t size, void *parent) {
  return Mem_Malloc_(size, MEM_TAG_DEFAULT, parent);
}

/**
 * @brief Allocates a block of managed memory. All managed memory is freed when
 * the game exits, but may be explicitly freed with `Mem_Free`.
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

  SDL_LockSpinlock(&mem_state.lock);

  // remove the old block while b is still a valid pointer
  if (b->parent) {
    mem_block_t **pp = &b->parent->first_child;
    while (*pp && *pp != b) { pp = &(*pp)->next_sibling; }
    if (*pp) { *pp = b->next_sibling; }
  } else {
    if (b->prev_block) { b->prev_block->next_block = b->next_block; }
    else { mem_state.head = b->next_block; }
    if (b->next_block) { b->next_block->prev_block = b->prev_block; }
  }

  b->size = size;

  if (!(new_b = realloc(b, s))) {
    fprintf(stderr, "Failed to re-allocate %u bytes\n", (uint32_t) s);
    raise(SIGABRT);
    return NULL;
  }

  void *data = (void *) (new_b + 1);

  mem_footer_t *footer = Mem_Footer(data, size);
  footer->magic = (mem_magic_t) (MEM_MAGIC + new_b->size);

  // re-seat us in our parent or in global list
  if (new_b->parent) {
    new_b->next_sibling = new_b->parent->first_child;
    new_b->parent->first_child = new_b;
  } else {
    new_b->next_block = mem_state.head;
    new_b->prev_block = NULL;
    if (mem_state.head) { mem_state.head->prev_block = new_b; }
    mem_state.head = new_b;
  }

  // change our children's parent pointers
  if (new_b->first_child) {
    for (mem_block_t *child = new_b->first_child; child; child = child->next_sibling) {
      child->parent = new_b;
    }
  }

  mem_state.size -= old_size;
  mem_state.size += size;

  SDL_UnlockSpinlock(&mem_state.lock);

  return data;
}

/**
 * @brief Links the specified child to the given parent. The child will
 * subsequently be freed with the parent.
 *
 * @param child The child object, previously allocated with `Mem_Malloc`.
 * @param parent The parent object, previously allocated with `Mem_Malloc`.
 *
 * @return The child, for convenience.
 */
void *Mem_Link(void *child, void *parent) {
  mem_block_t *c = Mem_CheckMagic(child);
  mem_block_t *p = Mem_CheckMagic(parent);

  SDL_LockSpinlock(&mem_state.lock);

  if (c->parent) {
    mem_block_t **pp = &c->parent->first_child;
    while (*pp && *pp != c) { pp = &(*pp)->next_sibling; }
    if (*pp) { *pp = c->next_sibling; }
  } else {
    if (c->prev_block) { c->prev_block->next_block = c->next_block; }
    else { mem_state.head = c->next_block; }
    if (c->next_block) { c->next_block->prev_block = c->prev_block; }
  }

  c->parent = p;
  c->next_sibling = p->first_child;
  p->first_child = c;

  SDL_UnlockSpinlock(&mem_state.lock);

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
 * @brief Comparison function for sorting `mem_stat_t` entries by descending allocated size.
 */
static int32_t Mem_Stats_Sort(const void * a, const void * b) {
  return (int32_t) (((const mem_stat_t *) b)->size - ((const mem_stat_t *) a)->size);
}

/**
 * @brief Recursively calculates the total allocated size of a block, including all child blocks.
 */
static size_t Mem_CalculateBlockSize(const mem_block_t *b) {

  size_t size = b->size;

  for (mem_block_t *child = b->first_child; child; child = child->next_sibling) {
    size += Mem_CalculateBlockSize(child);
  }

  return size;
}

/**
 * @brief Fetches stats about allocated memory to the console.
 */
Vector *Mem_Stats(void) {

  SDL_LockSpinlock(&mem_state.lock);

  Vector *stat_array = $(alloc(Vector), initWithSize, sizeof(mem_stat_t));

  const mem_stat_t total = { .tag = -1, .size = mem_state.size, .count = 0 };
  $(stat_array, addElement, &total);

  for (const mem_block_t *b = mem_state.head; b; b = b->next_block) {
    mem_stat_t *stats = NULL;

    for (size_t i = 0; i < stat_array->count; i++) {
      mem_stat_t *stat_i = VectorElement(stat_array, mem_stat_t, i);
      if (stat_i->tag == b->tag) {
        stats = stat_i;
        break;
      }
    }

    if (stats == NULL) {
      const mem_stat_t entry = {
        .tag = b->tag,
        .size = Mem_CalculateBlockSize(b),
        .count = 1
      };
      $(stat_array, addElement, &entry);
    } else {
      stats->size += Mem_CalculateBlockSize(b);
      stats->count++;
    }
  }

  SDL_UnlockSpinlock(&mem_state.lock);

  $(stat_array, sort, Mem_Stats_Sort);

  return stat_array;
}

/**
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quetoo.
 */
void Mem_Init(void) {

  memset(&mem_state, 0, sizeof(mem_state));
}

/**
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quetoo.
 */
void Mem_Shutdown(void) {

  Mem_FreeTag(MEM_TAG_ALL);
}
