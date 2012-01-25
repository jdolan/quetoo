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

/*
Hash_Init

Initializes the specified hash_table.
*/
void Hash_Init(hash_table_t *table);

/*
Hash_Hashcode

Generate a bin number (not a unique code) for the specified key.
*/
int Hash_Hashcode(const char *key);

/*
Hash_Put

Insert the specified key-value pair to hash_table.
*/
int Hash_Put(hash_table_t *table, const char *key, void *value);

/*
Hash_GetEntry

Returns the first entry associated to the specified key.
*/
hash_entry_t *Hash_GetEntry(hash_table_t *table, const char *key);

/*
Hash_Get

Return the first value hashed at key from hash_table.
*/
void *Hash_Get(hash_table_t *table, const char *key);

/*
Hash_RemoveEntry

Removes the specified entry from the hash and frees it, returning its
value so that it may also be freed if desired.
*/
void *Hash_RemoveEntry(hash_table_t *table, hash_entry_t *entry);

/*
Hash_Remove

Removes the first entry associated to key from the specified hash and
frees it, returning its value so that it may also be freed if desired.
*/
void *Hash_Remove(hash_table_t *table, const char *key);

/*
Hash_Clear

Removes all entries associated to key from the specified hash.
*/
void Hash_Clear(hash_table_t *table, const char *key);

/*
Hash_Free

Free all entries associated with hash table.  Does not free any of the
values referenced by the entries.
*/
void Hash_Free(hash_table_t *table);

#endif /*__HASH_H__*/
