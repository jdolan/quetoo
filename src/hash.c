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

#include "hash.h"


/*
 * Hash_Init
 *
 * Initializes the specified hashtable.
 */
void Hash_Init(hashtable_t *hashtable){

	if(!hashtable)
		return;

	memset(hashtable, 0, sizeof(*hashtable));
}


/*
 * Hash_Hashcode
 *
 * Generate a bin number (not a unique code) for the specified key.
 */
unsigned Hash_Hashcode(const char *key){
	unsigned code;
	const char *c;

	if(!key || !*key)
		return -1;

	code = 0;
	c = key;
	while(*c){
		code += *c;
		c++;
	}

	return code & (HASHBINS - 1);
}


/*
 * Hash_Put
 *
 * Insert the specified key-value pair to hashtable.
 */
unsigned Hash_Put(hashtable_t *hashtable, const char *key, void *value){
	hashentry_t *e;
	unsigned code;

	if(!key || !*key || !value)
		return -1;

	e = Z_Malloc(sizeof(*e));
	memset(e, 0, sizeof(*e));

	e->key = key;
	e->value = value;

	code = Hash_Hashcode(key);

	if(!hashtable->bins[code]){
		hashtable->bins[code] = e;
		return code;
	}

	hashtable->bins[code]->prev = e;
	e->next = hashtable->bins[code];
	hashtable->bins[code] = e;
	return code;
}


/*
 * Hash_GetEntry
 *
 * Returns the first entry associated to the specified key.
 */
hashentry_t *Hash_GetEntry(hashtable_t *hashtable, const char *key){
	hashentry_t *e;
	unsigned code;

	code = Hash_Hashcode(key);

	if(!hashtable->bins[code])
		return NULL;

	e = hashtable->bins[code];
	while(e){
		if(!strcmp(e->key, key))
			return e;
		e = e->next;
	}

	return NULL;
}


/*
 * Hash_Get
 *
 * Return the first value hashed at key from hashtable.
 */
void *Hash_Get(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	if((e = Hash_GetEntry(hashtable, key)))
		return e->value;

	return NULL;
}


/*
 * Hash_RemoveEntry
 *
 * Removes the specified entry from the hash and frees it, returning its
 * value so that it may also be freed if desired.
 */
void *Hash_RemoveEntry(hashtable_t *hashtable, hashentry_t *entry){
	unsigned code;
	void *ret;

	if(!hashtable || !entry)
		return NULL;

	code = Hash_Hashcode(entry->key);

	if(hashtable->bins[code] == entry)  // fix the bin if we were the head
		hashtable->bins[code] = entry->next;

	if(entry->prev)
		entry->prev->next = entry->next;

	ret = entry->value;

	Z_Free(entry);

	return ret;
}


/*
 * Hash_Remove
 *
 * Removes the first entry associated to key from the specified hash.
 */
void *Hash_Remove(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	if((e = Hash_GetEntry(hashtable, key)))
		return Hash_RemoveEntry(hashtable, e);

	return NULL;
}

/*
 * Hash_Clear
 *
 * Removes all entries associated to key from the specified hash.
 */
void Hash_Clear(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	while((e = Hash_GetEntry(hashtable, key)))
		Hash_RemoveEntry(hashtable, e);
}


/*
 * Hash_Free
 *
 * Free all hashentries associated with hashtable.  Does not free any of the
 * values referenced by the entries.
 */
void Hash_Free(hashtable_t *hashtable){
	hashentry_t *e, *f;
	int i;

	for(i = 0; i < HASHBINS; i++){

		e = hashtable->bins[i];
		while(e){
			f = e->next;
			Z_Free(e);
			e = f;
		}
		hashtable->bins[i] = NULL;
	}
}

