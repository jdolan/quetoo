/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "common.h"
#include "hash.h"


/*
 * Com_HashInit
 * 
 * Initializes the specified hashtable.
 */
void Com_HashInit(hashtable_t *hashtable){

	if(!hashtable)
		return;

	memset(hashtable, 0, sizeof(*hashtable));
}


/*
 * Com_HashCode
 * 
 * Generate a bin number (not a unique code) for the specified key.
 */
unsigned Com_HashCode(const char *key){
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
 * Com_HashInsert
 * 
 * Insert the specified key-value pair to hashtable.
 */
unsigned Com_HashInsert(hashtable_t *hashtable, const char *key, void *value){
	hashentry_t *e;
	unsigned code;

	if(!key || !*key || !value)
		return -1;

	e = Z_Malloc(sizeof(*e));
	memset(e, 0, sizeof(*e));

	e->key = key;
	e->value = value;

	code = Com_HashCode(key);

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
 * Com_HashEntry
 * 
 * Returns the first entry associated to the specified key.
 */
hashentry_t *Com_HashEntry(hashtable_t *hashtable, const char *key){
	hashentry_t *e;
	unsigned code;

	code = Com_HashCode(key);

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
 * Com_HashValue
 * 
 * Return the first value hashed at key from hashtable.
 */
void *Com_HashValue(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	if((e = Com_HashEntry(hashtable, key)))
		return e->value;

	return NULL;
}


/*
 * Com_HashRemoveEntry
 * 
 * Removes the specified entry from the hash and frees it, returning its
 * value so that it may also be freed if desired.
 */
void *Com_HashRemoveEntry(hashtable_t *hashtable, hashentry_t *entry){
	unsigned code;
	void *ret;

	if(!hashtable || !entry)
		return NULL;

	code = Com_HashCode(entry->key);

	if(hashtable->bins[code] == entry)  // fix the bin if we were the head
		hashtable->bins[code] = entry->next;

	if(entry->prev)
		entry->prev->next = entry->next;

	ret = entry->value;

	Z_Free(entry);

	return ret;
}


/*
 * Com_HashRemove
 * 
 * Removes the first entry associated to key from the specified hash.
 */
void *Com_HashRemove(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	if((e = Com_HashEntry(hashtable, key)))
		return Com_HashRemoveEntry(hashtable, e);

	return NULL;
}

/*
 * Com_HashRemoveAll
 * 
 * Removes all entries associated to key from the specified hash.
 */
void Com_HashRemoveAll(hashtable_t *hashtable, const char *key){
	hashentry_t *e;

	while((e = Com_HashEntry(hashtable, key)))
		Com_HashRemoveEntry(hashtable, e);
}


/*
 * Com_HashFree
 * 
 * Free all hashentries associated with hashtable.  Does not free any of the
 * values referenced by the entries.
 */
void Com_HashFree(hashtable_t *hashtable){
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

