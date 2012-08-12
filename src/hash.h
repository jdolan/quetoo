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

#ifndef __HASH_H__
#define __HASH_H__

#include "mem.h"

// hash table implementation
#define HASH_BINS 512

typedef struct hash_entry_s {
	const char *key;
	void *value;
	struct hash_entry_s *prev, *next;
} hash_entry_t;

typedef struct hash_table_s {
	hash_entry_t *bins[HASH_BINS];
} hash_table_t;

void Hash_Init(hash_table_t *table);
int32_t Hash_Hashcode(const char *key);
int32_t Hash_Put(hash_table_t *table, const char *key, void *value);
hash_entry_t *Hash_GetEntry(hash_table_t *table, const char *key);
void *Hash_Get(hash_table_t *table, const char *key);
void *Hash_RemoveEntry(hash_table_t *table, hash_entry_t *entry);
void *Hash_Remove(hash_table_t *table, const char *key);
void Hash_Clear(hash_table_t *table, const char *key);
void Hash_Free(hash_table_t *table);

#endif /*__HASH_H__*/
