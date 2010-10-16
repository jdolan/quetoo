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

// hashtable implementation
#define HASHBINS 512

typedef struct hashentry_s {
	const char *key;
	void *value;
	struct hashentry_s *prev, *next;
} hashentry_t;

typedef struct hashtable_s {
	hashentry_t *bins[HASHBINS];
} hashtable_t;

/*
Hash_Init

Initializes the specified hashtable.
*/
void Hash_Init(hashtable_t *hashtable);

/*
Hash_Hashcode

Generate a bin number (not a unique code) for the specified key.
*/
unsigned Hash_Hashcode(const char *key);

/*
Hash_Put

Insert the specified key-value pair to hashtable.
*/
unsigned Hash_Put(hashtable_t *hashtable, const char *key, void *value);

/*
Hash_GetEntry

Returns the first entry associated to the specified key.
*/
hashentry_t *Hash_GetEntry(hashtable_t *hashtable, const char *key);

/*
Hash_Get

Return the first value hashed at key from hashtable.
*/
void *Hash_Get(hashtable_t *hashtable, const char *key);

/*
Hash_RemoveEntry

Removes the specified entry from the hash and frees it, returning its
value so that it may also be freed if desired.
*/
void *Hash_RemoveEntry(hashtable_t *hashtable, hashentry_t *entry);

/*
Hash_Remove

Removes the first entry associated to key from the specified hash and
frees it, returning its value so that it may also be freed if desired.
*/
void *Hash_Remove(hashtable_t *hashtable, const char *key);

/*
Hash_Clear

Removes all entries associated to key from the specified hash.
*/
void Hash_Clear(hashtable_t *hashtable, const char *key);

/*
Hash_Free

Free all hashentries associated with hashtable.  Does not free any of the
values referenced by the entries.
*/
void Hash_Free(hashtable_t *hashtable);

#endif /*__HASH_H__*/
