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
#include "shared/qstring.h"

#define MEM_MAGIC 0x69696969
typedef uint32_t MemMagic;

typedef struct MemBlock {
  MemMagic magic;
  MemTag tag; // for group free
  struct MemBlock *parent;
  struct MemBlock *firstChild;   // head of intrusive children list
  struct MemBlock *nextSibling;  // next in parent's children list
  struct MemBlock *prevBlock;    // prev in global block list
  struct MemBlock *nextBlock;    // next in global block list
  size_t size;
} MemBlock;

typedef struct {
  MemMagic magic;
} MemFooter;

typedef struct {
  MemBlock *head;   // head of global doubly-linked block list
  size_t size;
  SDL_SpinLock lock;
} MemState;

static MemState memState;

/**
 * @brief Returns a properly aligned pointer to the footer for a data block.
 */
static inline MemFooter *Mem_Footer(const void *data, size_t size) {
  const uintptr_t addr = (uintptr_t) ((const byte *) data + size);
  const uintptr_t aligned = (addr + _Alignof(MemFooter) - 1) & ~((uintptr_t) _Alignof(MemFooter) - 1);
  return (MemFooter *) aligned;
}

/**
 * @brief Throws a fatal error if the specified memory block is non-`NULL` but
 * not owned by the memory subsystem.
 */
static MemBlock *Mem_CheckMagic(void *p) {
  MemBlock *b = NULL;

  if (p) {
    b = ((MemBlock *) p) - 1;

    if (b->magic != MEM_MAGIC) {
      fprintf(stderr, "Invalid magic (%d) for %p\n", b->magic, p);
      raise(SIGABRT);
    }

    MemFooter *footer = Mem_Footer(p, b->size);

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
static void Mem_Free_(MemBlock *b) {

  // Validate this block before freeing its children
  if (b->magic != MEM_MAGIC) {
    fprintf(stderr, "Mem_Free_: Corrupted block %p (magic: %d, expected: %d)\n", 
            (void *)b, b->magic, MEM_MAGIC);
    raise(SIGABRT);
  }

  // Validate all children before freeing them
  if (b->firstChild) {
    MemBlock *child = b->firstChild;
    int childNum = 0;
    bool hasCorruption = false;
    while (child) {
      if (child->magic != MEM_MAGIC) {
        hasCorruption = true;
        fprintf(stderr, "Mem_Free_: Parent %p (tag: %d, size: %zu) has corrupted child #%d: %p (magic: %d)\n",
                (void *)b, b->tag, b->size, childNum, (void *)child, child->magic);
        fprintf(stderr, "  First 8 words of bad child: %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n",
                ((unsigned long long *)child)[0], ((unsigned long long *)child)[1],
                ((unsigned long long *)child)[2], ((unsigned long long *)child)[3],
                ((unsigned long long *)child)[4], ((unsigned long long *)child)[5],
                ((unsigned long long *)child)[6], ((unsigned long long *)child)[7]);
      }
      child = child->nextSibling;
      childNum++;
    }

    if (hasCorruption) {
      fprintf(stderr, "Mem_Free_: Parent details - address: %p, magic: %d, tag: %d, size: %zu, num_children: %d\n",
              (void *)b, b->magic, b->tag, b->size, childNum);
      raise(SIGABRT);
    }

    // Recursively free children
    child = b->firstChild;
    while (child) {
      MemBlock *next = child->nextSibling;
      Mem_Free_(child);
      child = next;
    }
  }

  // decrement the pool size and free the memory
  memState.size -= b->size;

  free(b);
}

/**
 * @brief Free an allocation of managed memory. Any child objects are
 * automatically freed as well.
 */
void Mem_Free(void *p) {
  if (p) {
    MemBlock *b = Mem_CheckMagic(p);

    SDL_LockSpinlock(&memState.lock);

    if (b->parent) {
      // Unlink from parent's intrusive children list
      MemBlock **pp = &b->parent->firstChild;
      while (*pp && *pp != b) {
        pp = &(*pp)->nextSibling;
      }
      if (*pp) { *pp = b->nextSibling; }
    } else {
      // Unlink from global doubly-linked list
      if (b->prevBlock) { b->prevBlock->nextBlock = b->nextBlock; }
      else { memState.head = b->nextBlock; }
      if (b->nextBlock) { b->nextBlock->prevBlock = b->prevBlock; }
    }

    Mem_Free_(b);

    SDL_UnlockSpinlock(&memState.lock);
  }
}

/**
 * @brief Free all managed items allocated with the specified tag.
 */
void Mem_FreeTag(MemTag tag) {

  SDL_LockSpinlock(&memState.lock);

  MemBlock *b = memState.head;
  while (b) {
    MemBlock *next = b->nextBlock;
    if (tag == MEM_TAG_ALL || b->tag == tag) {
      // Unlink from global list
      if (b->prevBlock) { b->prevBlock->nextBlock = b->nextBlock; }
      else { memState.head = b->nextBlock; }
      if (b->nextBlock) { b->nextBlock->prevBlock = b->prevBlock; }
      Mem_Free_(b);
    }
    b = next;
  }

  SDL_UnlockSpinlock(&memState.lock);
}

/**
 * @brief Returns the total size of a memory block.
 */
static size_t Mem_BlockSize(const size_t size) {
  const size_t aligned = (size + _Alignof(MemFooter) - 1) & ~(_Alignof(MemFooter) - 1);
  return sizeof(MemBlock) + aligned + sizeof(MemFooter);
}

/**
 * @brief Performs the grunt work of allocating a `MemBlock` and inserting it
 * into the managed memory structures. Note that parent should be a pointer to
 * a previously allocated structure, and not to a `MemBlock`.
 *
 * @param size The number of bytes to allocate.
 * @param tag The tag to allocate with (e.g. `MEM_TAG_DEFAULT`).
 * @param parent The parent to link this allocation to.
 *
 * @return A block of managed memory initialized to 0x0.
 */
static void *Mem_Malloc_(size_t size, MemTag tag, void *parent) {
  MemBlock *b, *p = Mem_CheckMagic(parent);

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

  MemFooter *footer = Mem_Footer(data, size);
  footer->magic = (MemMagic) (MEM_MAGIC + b->size);

  // insert it into the managed memory structures
  SDL_LockSpinlock(&memState.lock);

  if (b->parent) {
    // Prepend to parent's intrusive children list
    b->nextSibling = b->parent->firstChild;
    b->parent->firstChild = b;
  } else {
    // Prepend to global doubly-linked list
    b->nextBlock = memState.head;
    b->prevBlock = NULL;
    if (memState.head) { memState.head->prevBlock = b; }
    memState.head = b;
  }

  memState.size += size;

  SDL_UnlockSpinlock(&memState.lock);

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
void *Mem_TagMalloc(size_t size, MemTag tag) {
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

  MemBlock *b = Mem_CheckMagic(p), *newB;

  // no change to size
  if (b->size == size) {
    return (void *) (b + 1);
  }

  // allocate the block plus the desired size
  const size_t oldSize = b->size;
  const size_t s = Mem_BlockSize(size);

  // Once unlinked, a childless block is unreachable by other threads, so the
  // reallocation can happen without holding the lock. A block with children is
  // still reachable through their parent pointers, so it keeps the lock until
  // those have been re-seated.
  const bool hasChildren = b->firstChild != NULL;

  SDL_LockSpinlock(&memState.lock);

  // remove the old block while b is still a valid pointer
  if (b->parent) {
    MemBlock **pp = &b->parent->firstChild;
    while (*pp && *pp != b) { pp = &(*pp)->nextSibling; }
    if (*pp) { *pp = b->nextSibling; }
  } else {
    if (b->prevBlock) { b->prevBlock->nextBlock = b->nextBlock; }
    else { memState.head = b->nextBlock; }
    if (b->nextBlock) { b->nextBlock->prevBlock = b->prevBlock; }
  }

  if (!hasChildren) {
    SDL_UnlockSpinlock(&memState.lock);
  }

  b->size = size;

  if (!(newB = realloc(b, s))) {
    fprintf(stderr, "Failed to re-allocate %u bytes\n", (uint32_t) s);
    raise(SIGABRT);
    return NULL;
  }

  void *data = (void *) (newB + 1);

  MemFooter *footer = Mem_Footer(data, size);
  footer->magic = (MemMagic) (MEM_MAGIC + newB->size);

  if (!hasChildren) {
    SDL_LockSpinlock(&memState.lock);
  }

  // re-seat us in our parent or in global list
  if (newB->parent) {
    newB->nextSibling = newB->parent->firstChild;
    newB->parent->firstChild = newB;
  } else {
    newB->nextBlock = memState.head;
    newB->prevBlock = NULL;
    if (memState.head) { memState.head->prevBlock = newB; }
    memState.head = newB;
  }

  // change our children's parent pointers
  if (newB->firstChild) {
    for (MemBlock *child = newB->firstChild; child; child = child->nextSibling) {
      child->parent = newB;
    }
  }

  memState.size -= oldSize;
  memState.size += size;

  SDL_UnlockSpinlock(&memState.lock);

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
  MemBlock *c = Mem_CheckMagic(child);
  MemBlock *p = Mem_CheckMagic(parent);

  SDL_LockSpinlock(&memState.lock);

  if (c->parent) {
    MemBlock **pp = &c->parent->firstChild;
    while (*pp && *pp != c) { pp = &(*pp)->nextSibling; }
    if (*pp) { *pp = c->nextSibling; }
  } else {
    if (c->prevBlock) { c->prevBlock->nextBlock = c->nextBlock; }
    else { memState.head = c->nextBlock; }
    if (c->nextBlock) { c->nextBlock->prevBlock = c->prevBlock; }
  }

  c->parent = p;
  c->nextSibling = p->firstChild;
  p->firstChild = c;

  SDL_UnlockSpinlock(&memState.lock);

  return child;
}

/**
 * @return The current size (user bytes) of the zone allocation pool.
 */
size_t Mem_Size(void) {
  return memState.size;
}

/**
 * @brief Allocates and returns a copy of the specified string.
 */
char *Mem_TagCopyString(const char *in, MemTag tag) {
  char *out;

  out = Mem_TagMalloc(q_strlen(in) + 1, tag);
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
 * @brief Comparison function for sorting `MemStat` entries by descending allocated size.
 */
static Order Mem_Stats_Sort(const ident a, const ident b) {
  const int64_t diff = (int64_t) ((const MemStat *) b)->size - (int64_t) ((const MemStat *) a)->size;
  return diff < 0 ? OrderAscending : diff > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Recursively calculates the total allocated size of a block, including all child blocks.
 */
static size_t Mem_CalculateBlockSize(const MemBlock *b) {

  size_t size = b->size;

  for (MemBlock *child = b->firstChild; child; child = child->nextSibling) {
    size += Mem_CalculateBlockSize(child);
  }

  return size;
}

/**
 * @brief Fetches stats about allocated memory to the console.
 */
Vector *Mem_Stats(void) {

  SDL_LockSpinlock(&memState.lock);

  Vector *statArray = $(alloc(Vector), initWithSize, sizeof(MemStat));

  MemStat total = { .tag = -1, .size = memState.size, .count = 0 };
  $(statArray, add, &total);

  for (const MemBlock *b = memState.head; b; b = b->nextBlock) {
    MemStat *stats = NULL;

    for (size_t i = 0; i < statArray->count; i++) {
      MemStat *statI = VectorElement(statArray, MemStat, i);
      if (statI->tag == b->tag) {
        stats = statI;
        break;
      }
    }

    if (stats == NULL) {
      MemStat entry = {
        .tag = b->tag,
        .size = Mem_CalculateBlockSize(b),
        .count = 1
      };
      $(statArray, add, &entry);
    } else {
      stats->size += Mem_CalculateBlockSize(b);
      stats->count++;
    }
  }

  SDL_UnlockSpinlock(&memState.lock);

  $(statArray, sort, Mem_Stats_Sort);

  return statArray;
}

/**
 * @brief Initializes the managed memory subsystem. This should be one of the first
 * subsystems initialized by Quetoo.
 */
void Mem_Init(void) {

  memset(&memState, 0, sizeof(memState));
}

/**
 * @brief Shuts down the managed memory subsystem. This should be one of the last
 * subsystems brought down by Quetoo.
 */
void Mem_Shutdown(void) {

  Mem_FreeTag(MEM_TAG_ALL);
}
