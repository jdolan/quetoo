/**
 * @file mem.c
 * @brief Memory handling with sentinel checking and pools with tags for grouped free'ing
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "common.h"
#include "mem.h"
#include "scripts.h"
#include <SDL_thread.h>

#define MEM_HEAD_SENTINEL_TOP	0xFEBDFAED
#define MEM_HEAD_SENTINEL_BOT	0xD0BAF0FF
#define MEM_FOOT_SENTINEL		0xF00DF00D

static SDL_mutex *z_lock;

typedef struct memBlockFoot_s {
	uint32_t sentinel;				/**< For memory integrity checking */
} memBlockFoot_t;

typedef struct memBlock_s {
	struct memBlock_s *next;

	uint32_t topSentinel;			/**< For memory integrity checking */

	struct memPool_s *pool;			/**< Owner pool */
	int tagNum;						/**< For group free */
	size_t size;					/**< Size of allocation including this header */

	const char *allocFile;			/**< File the memory was allocated in */
	int allocLine;					/**< Line the memory was allocated at */

	void *memPointer;				/**< pointer to allocated memory */
	size_t memSize;					/**< Size minus the header */

	memBlockFoot_t *footer;			/**< Allocated in the space AFTER the block to check for overflow */

	uint32_t botSentinel;			/**< For memory integrity checking */
} memBlock_t;

#define MEM_MAX_POOLCOUNT	32
#define MEM_MAX_POOLNAME	64
#define MEM_HASH			11

typedef struct memPool_s {
	char name[MEM_MAX_POOLNAME];	/**< Name of pool */
	qboolean inUse;					/**< Slot in use? */

	memBlock_t *blocks[MEM_HASH];	/**< Allocated blocks */

	uint32_t blockCount;			/**< Total allocated blocks */
	uint32_t byteCount;				/**< Total allocated bytes */

	const char *createFile;			/**< File this pool was created on */
	int createLine;					/**< Line this pool was created on */
} memPool_t;

static memPool_t m_poolList[MEM_MAX_POOLCOUNT];
static uint32_t m_numPools;

/*==============================================================================
POOL MANAGEMENT
==============================================================================*/

static memPool_t *Mem_FindPool (const char *name)
{
	memPool_t *pool;
	uint32_t i;

	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;
		if (strcmp(name, pool->name))
			continue;

		return pool;
	}

	return NULL;
}

/**
 * @sa _Mem_DeletePool
 */
memPool_t *_Mem_CreatePool (const char *name, const char *fileName, const int fileLine)
{
	memPool_t *pool;
	uint32_t i;

	/* Check name */
	if (!name || !name[0])
		Sys_Error("Mem_CreatePool: NULL name %s:#%i", fileName, fileLine);
	if (strlen(name) + 1 >= MEM_MAX_POOLNAME)
		Com_Printf("Mem_CreatePoole: name '%s' too long, truncating!\n", name);

	/* See if it already exists */
	pool = Mem_FindPool(name);
	if (pool)
		return pool;

	/* Nope, create a slot */
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			break;
	}
	if (i == m_numPools) {
		if (m_numPools + 1 >= MEM_MAX_POOLCOUNT)
			Sys_Error("Mem_CreatePool: MEM_MAX_POOLCOUNT");
		pool = &m_poolList[m_numPools++];
	}

	/* Store values */
	for (i = 0; i < MEM_HASH; i++)
		pool->blocks[i] = NULL;
	pool->blockCount = 0;
	pool->byteCount = 0;
	pool->createFile = fileName;
	pool->createLine = fileLine;
	pool->inUse = qtrue;
	Q_strncpyz(pool->name, name, sizeof(pool->name));

	return pool;
}

/**
 * @brief
 * @sa _Mem_CreatePool
 * @sa _Mem_FreePool
 */
uint32_t _Mem_DeletePool (struct memPool_s *pool, const char *fileName, const int fileLine)
{
	uint32_t size;

	if (!pool)
		return 0;

	/* Release all allocated memory */
	size = _Mem_FreePool(pool, fileName, fileLine);

	/* Simple, yes? */
	pool->inUse = qfalse;
	pool->name[0] = '\0';

	return size;
}


/*==============================================================================
POOL AND TAG MEMORY ALLOCATION
==============================================================================*/

/**
 * @sa _Mem_FreePool
 */
uint32_t _Mem_Free (void *ptr, const char *fileName, const int fileLine)
{
	memBlock_t *mem;
	memBlock_t *search;
	memBlock_t **prev;
	uint32_t size;

	if (!ptr)
		return 0;

	/* Check sentinels */
	mem = (memBlock_t *)((byte *)ptr - sizeof(memBlock_t));
	if (mem->topSentinel != MEM_HEAD_SENTINEL_TOP) {
		Sys_Error("Mem_Free: bad memory header top sentinel [buffer underflow]\n"
			"free: %s:#%i", fileName, fileLine);
	} else if (mem->botSentinel != MEM_HEAD_SENTINEL_BOT) {
		Sys_Error("Mem_Free: bad memory header bottom sentinel [buffer underflow]\n"
			"free: %s:#%i", fileName, fileLine);
	} else if (!mem->footer) {
		Sys_Error("Mem_Free: bad memory footer [buffer overflow]\n"
			"pool: %s\n"
			"alloc: %s:#%i\n"
			"free: %s:#%i",
			mem->pool ? mem->pool->name : "UNKNOWN", mem->allocFile, mem->allocLine, fileName, fileLine);
	} else if (mem->footer->sentinel != MEM_FOOT_SENTINEL) {
		Sys_Error("Mem_Free: bad memory footer sentinel [buffer overflow]\n"
			"pool: %s\n"
			"alloc: %s:#%i\n"
			"free: %s:#%i",
			mem->pool ? mem->pool->name : "UNKNOWN", mem->allocFile, mem->allocLine, fileName, fileLine);
	}

	SDL_mutexP(z_lock);

	/* Decrement counters */
	mem->pool->blockCount--;
	mem->pool->byteCount -= mem->size;
	size = mem->size;

	/* De-link it */
	prev = &mem->pool->blocks[(uintptr_t)mem % MEM_HASH];
	for (;;) {
		search = *prev;
		if (!search)
			break;

		if (search == mem) {
			*prev = search->next;
			break;
		}
		prev = &search->next;
	}

	SDL_mutexV(z_lock);

	/* Free it */
	free(mem);
	return size;
}

/**
 * @brief Free memory blocks assigned to a specified tag within a pool
 */
uint32_t _Mem_FreeTag (struct memPool_s *pool, const int tagNum, const char *fileName, const int fileLine)
{
	memBlock_t *mem, *next;
	uint32_t size;
	int j = 0;

	if (!pool)
		return 0;

	size = 0;
	for (j = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; mem = next) {
			next = mem->next;
			if (mem->tagNum == tagNum)
				size += _Mem_Free(mem->memPointer, fileName, fileLine);
		}
	}

	return size;
}


/**
 * @brief Free all items within a pool
 * @sa _Mem_CreatePool
 * @sa _Mem_DeletePool
 */
uint32_t _Mem_FreePool (struct memPool_s *pool, const char *fileName, const int fileLine)
{
	memBlock_t *mem, *next;
	uint32_t size;
	int j = 0;

	if (!pool)
		return 0;

	size = 0;
	for (j = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; mem = next) {
			next = mem->next;
			size += _Mem_Free(mem->memPointer, fileName, fileLine);
		}
	}

	assert(pool->blockCount == 0);
	assert(pool->byteCount == 0);
	return size;
}


/**
 * @brief Optionally returns 0 filled memory allocated in a pool with a tag
 */
void *_Mem_Alloc (size_t size, qboolean zeroFill, memPool_t *pool, const int tagNum, const char *fileName, const int fileLine)
{
	memBlock_t *mem;

	/* Check pool */
	if (!pool) {
		Com_Printf("Mem_Alloc: Error - no pool given\n" "alloc: %s:#%i\n", fileName, fileLine);
		return NULL;
	}

	/* Add header and round to cacheline */
	size = (size + sizeof(memBlock_t) + sizeof(memBlockFoot_t) + 31) & ~31;
	mem = malloc(size);

	/* Zero fill */
	if (zeroFill)
		memset(mem, 0, size);

	/* Fill in the header */
	mem->topSentinel = MEM_HEAD_SENTINEL_TOP;
	mem->tagNum = tagNum;
	mem->size = size;
	mem->memPointer = (void *)(mem + 1);
	mem->memSize = size - sizeof(memBlock_t) - sizeof(memBlockFoot_t);
	mem->pool = pool;
	mem->allocFile = fileName;
	mem->allocLine = fileLine;
	mem->footer = (memBlockFoot_t *)((byte *)mem->memPointer + mem->memSize);
	mem->botSentinel = MEM_HEAD_SENTINEL_BOT;

	/* Fill in the footer */
	mem->footer->sentinel = MEM_FOOT_SENTINEL;

	SDL_mutexP(z_lock);

	/* For integrity checking and stats */
	pool->blockCount++;
	pool->byteCount += size;

	/* Link it in to the appropriate pool */
	mem->next = pool->blocks[(uintptr_t)mem % MEM_HASH];
	pool->blocks[(uintptr_t)mem % MEM_HASH] = mem;

	SDL_mutexV(z_lock);

	return mem->memPointer;
}

void* _Mem_ReAlloc (void *ptr, size_t size, const char *fileName, const int fileLine)
{
	memBlock_t *mem;
	memPool_t *pool;
	memBlock_t *search;
	memBlock_t **prev;

	if (!size)
		Sys_Error("Use Mem_Free instead");

	if (!ptr)
		Sys_Error("Use Mem_Alloc instead");

	mem = (memBlock_t *)((byte *)ptr - sizeof(memBlock_t));
	mem = realloc(mem, size);

	pool = mem->pool;

	SDL_mutexP(z_lock);

	/* De-link it */
	prev = &pool->blocks[(uintptr_t)mem % MEM_HASH];
	for (;;) {
		search = *prev;
		if (!search)
			break;

		if (search == mem) {
			*prev = search->next;
			break;
		}
		prev = &search->next;
	}

	/* Link it in to the appropriate pool */
	mem->next = pool->blocks[(uintptr_t)mem % MEM_HASH];
	pool->blocks[(uintptr_t)mem % MEM_HASH] = mem;

	/* Add header and round to cacheline */
	size = (size + sizeof(memBlock_t) + sizeof(memBlockFoot_t) + 31) & ~31;

	/* counters */
	pool->byteCount -= mem->size;
	pool->byteCount += size;

	SDL_mutexV(z_lock);

	mem->size = size;
	mem->memPointer = (void *)(mem + 1);
	mem->memSize = size - sizeof(memBlock_t) - sizeof(memBlockFoot_t);
	mem->allocFile = fileName;
	mem->allocLine = fileLine;
	mem->footer = (memBlockFoot_t *)((byte *)mem->memPointer + mem->memSize);

	/* Fill in the footer */
	mem->footer->sentinel = MEM_FOOT_SENTINEL;

	return mem->memPointer;
}

/*==============================================================================
MISC FUNCTIONS
==============================================================================*/

/**
 * @param[in] ptr The ptr to get the allocated size from
 */
size_t Mem_Size (const void *ptr)
{
	const memBlock_t *mem = (const memBlock_t *)((const byte *)ptr - sizeof(memBlock_t));
	return mem->size;
}

/**
 * @brief Saves a string to client hunk
 * @param[in] in String to store in the given pool
 * @param[out] out The location where you want the pool pointer to be stored
 * @param[in] pool The pool to allocate the memory in
 * @param[in] tagNum
 * @param[in] fileName The filename where this function was called from
 * @param[in] fileLine The line where this function was called from
 */
char *_Mem_PoolStrDupTo (const char *in, char **out, struct memPool_s *pool, const int tagNum, const char *fileName, const int fileLine)
{
	if (!out)
		return NULL;
	*out = _Mem_PoolStrDup(in, pool, tagNum, fileName, fileLine);
	return *out;
}

/**
 * @brief No need to null terminate the extra spot because Mem_Alloc returns zero-filled memory
 * @param[in] in String to store in the given pool
 * @param[in] pool The pool to allocate the memory in
 * @param[in] tagNum
 * @param[in] fileName The filename where this function was called from
 * @param[in] fileLine The line where this function was called from
 */
char *_Mem_PoolStrDup (const char *in, struct memPool_s *pool, const int tagNum, const char *fileName, const int fileLine)
{
	char *out;

	out = _Mem_Alloc((size_t)(strlen (in) + 1), qtrue, pool, tagNum, fileName, fileLine);
	strcpy(out, in);

	return out;
}

/**
 * @param[in] pool The pool to get the size from
 */
uint32_t _Mem_PoolSize (struct memPool_s *pool)
{
	if (!pool)
		return 0;

	return pool->byteCount;
}


uint32_t _Mem_TagSize (struct memPool_s *pool, const int tagNum)
{
	memBlock_t *mem;
	uint32_t size;
	int j = 0;

	if (!pool)
		return 0;

	size = 0;
	for (j = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; mem = mem->next) {
			if (mem->tagNum == tagNum)
				size += mem->size;
		}
	}

	return size;
}


uint32_t _Mem_ChangeTag (struct memPool_s *pool, const int tagFrom, const int tagTo)
{
	memBlock_t *mem;
	uint32_t numChanged;
	int j = 0;

	if (!pool)
		return 0;

	numChanged = 0;

	for (j = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; mem = mem->next) {
			if (mem->tagNum == tagFrom) {
				mem->tagNum = tagTo;
				numChanged++;
			}
		}
	}

	return numChanged;
}


void _Mem_CheckPoolIntegrity (struct memPool_s *pool, const char *fileName, const int fileLine)
{
	memBlock_t *mem;
	uint32_t blocks;
	uint32_t size;
	int j = 0;

	assert(pool);
	if (!pool)
		return;

	/* Check sentinels */
	for (j = 0, blocks = 0, size = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; blocks++, mem = mem->next) {
			size += mem->size;
			if (mem->topSentinel != MEM_HEAD_SENTINEL_TOP) {
				Sys_Error("Mem_CheckPoolIntegrity: bad memory head top sentinel [buffer underflow]\n"
					"check: %s:#%i", fileName, fileLine);
			} else if (mem->botSentinel != MEM_HEAD_SENTINEL_BOT) {
				Sys_Error("Mem_CheckPoolIntegrity: bad memory head bottom sentinel [buffer underflow]\n"
					"check: %s:#%i", fileName, fileLine);
			} else if (!mem->footer) {
				Sys_Error("Mem_CheckPoolIntegrity: bad memory footer [buffer overflow]\n"
					"pool: %s\n"
					"alloc: %s:#%i\n"
					"check: %s:#%i",
					mem->pool ? mem->pool->name : "UNKNOWN", mem->allocFile, mem->allocLine, fileName, fileLine);
			} else if (mem->footer->sentinel != MEM_FOOT_SENTINEL) {
				Sys_Error("Mem_CheckPoolIntegrity: bad memory foot sentinel [buffer overflow]\n"
					"pool: %s\n"
					"alloc: %s:#%i\n"
					"check: %s:#%i",
					mem->pool ? mem->pool->name : "UNKNOWN", mem->allocFile, mem->allocLine, fileName, fileLine);
			}
		}
	}

	/* Check block/byte counts */
	if (pool->blockCount != blocks)
		Sys_Error("Mem_CheckPoolIntegrity: bad block count\n" "check: %s:#%i", fileName, fileLine);
	if (pool->byteCount != size)
		Sys_Error("Mem_CheckPoolIntegrity: bad pool size\n" "check: %s:#%i", fileName, fileLine);
}


void _Mem_CheckGlobalIntegrity (const char *fileName, const int fileLine)
{
	memPool_t *pool;
	uint32_t i;

	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (pool->inUse)
			_Mem_CheckPoolIntegrity(pool, fileName, fileLine);
	}
}


void _Mem_TouchPool (struct memPool_s *pool, const char *fileName, const int fileLine)
{
	memBlock_t *mem;
	uint32_t i;
	int sum;
	int j = 0;

	if (!pool)
		return;

	sum = 0;

	/* Cycle through the blocks */
	for (j = 0; j < MEM_HASH; j++) {
		for (mem = pool->blocks[j]; mem; mem = mem->next) {
			/* Touch each page */
			for (i = 0; i < mem->memSize; i += 128) {
				sum += ((byte *)mem->memPointer)[i];
			}
		}
	}
}

/**
 * Searches a given pointer in all memory pool blocks
 * @param pool The pool to search the pointer in
 * @param pointer The pointer to search in the pool
 */
void* _Mem_AllocatedInPool (struct memPool_s *pool, const void *pointer)
{
	memBlock_t *mem;

	if (!pool)
		return NULL;

	/* if it's in the pool, it must be in THIS block */
	mem = pool->blocks[(uintptr_t)pointer % MEM_HASH];
	if (!mem)		/* the block might be in initialized state */
		return NULL;
	/* Cycle through the blocks */
	for ( ; mem; mem = mem->next) {
		if (mem->memPointer == pointer)
			return mem->memPointer;
	}

	return NULL;
}

void _Mem_TouchGlobal (const char *fileName, const int fileLine)
{
	memPool_t *pool;
	uint32_t i, num;

	/* Touch every pool */
	num = 0;
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (pool->inUse) {
			_Mem_TouchPool(pool, fileName, fileLine);
			num++;
		}
	}
}

/*==============================================================================
CONSOLE COMMANDS
==============================================================================*/

static void Mem_Check_f (void)
{
	Mem_CheckGlobalIntegrity();
}

static void Mem_Stats_f (void)
{
	uint32_t totalBlocks, totalBytes;
	memPool_t *pool;
	uint32_t poolNum, i;
	int j = 0;

	if (Cmd_Argc() > 1) {
		memPool_t *best;
		memBlock_t *mem;

		best = NULL;
		for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
			if (!pool->inUse)
				continue;
			if (strstr(pool->name, Cmd_Args())) {
				if (best) {
					Com_Printf("Too many matches for '%s'...\n", Cmd_Args());
					return;
				}
				best = pool;
			}
		}
		if (!best) {
			Com_Printf("No matches for '%s'...\n", Cmd_Args());
			return;
		}

		Com_Printf("Pool stats for '%s':\n", best->name);
		Com_Printf("block line  file                 size       \n");
		Com_Printf("----- ----- -------------------- ---------- \n");

		totalBytes = 0;
		for (j = 0; j < MEM_HASH; j++) {
			for (i = 0, mem = best->blocks[j]; mem; mem = mem->next, i++) {
				Com_Printf("%5i %5i %20s "Q2W_SIZE_T"B\n", i + 1, mem->allocLine, mem->allocFile, mem->size);

				totalBytes += mem->size;
			}
		}

		Com_Printf("----------------------------------------\n");
		Com_Printf("Total: %i blocks, %i bytes (%6.3fMB)\n", i, totalBytes, totalBytes/1048576.0f);
		return;
	}

	Com_Printf("Memory stats:\n");
	Com_Printf("    blocks size                  name\n");
	Com_Printf("--- ------ ---------- ---------- --------\n");

	totalBlocks = 0;
	totalBytes = 0;
	poolNum = 0;
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;

		poolNum++;

		Com_Printf("#%2i %6i %9iB (%6.3fMB) %s\n", poolNum, pool->blockCount, pool->byteCount, pool->byteCount/1048576.0f, pool->name);

		totalBlocks += pool->blockCount;
		totalBytes += pool->byteCount;
	}

	Com_Printf("----------------------------------------\n");
	Com_Printf("Total: %i pools, %i blocks, %i bytes (%6.3fMB)\n", i, totalBlocks, totalBytes, totalBytes/1048576.0f);
}

/**
 * @sa Qcommon_Init
 * @sa Mem_Shutdown
 */
void Mem_Init (void)
{
	Cmd_AddCommand("mem_stats", Mem_Stats_f, "Prints out current internal memory statistics");
	Cmd_AddCommand("mem_check", Mem_Check_f, "Checks global memory integrity");

	z_lock = SDL_CreateMutex();
}

/**
 * @sa Mem_Init
 * @sa Mem_CreatePool
 * @sa Qcommon_Shutdown
 */
uint32_t Mem_Shutdown (void)
{
	memPool_t *pool;
	uint32_t size = 0;
	int i;

	/* don't use cvars, debug print or anything else inside of this loop
	 * the mem you are trying to use here might already be wiped away */
	for (i = 0, pool = &m_poolList[0]; i < m_numPools; pool++, i++) {
		if (!pool->inUse)
			continue;
		size += Mem_DeletePool(pool);
	}

	SDL_DestroyMutex(z_lock);

	return size;
}
