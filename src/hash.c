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
 * Initializes the specified hash_table.
 */
void Hash_Init(hash_table_t *table) {

	if (!table)
		return;

	memset(table, 0, sizeof(*table));
}

/*
 * Hash_Hashcode
 *
 * Generate a bin number (not a unique code) for the specified key.
 */
int Hash_Hashcode(const char *key) {
	int code;
	const char *c;

	if (!key || !*key)
		return -1;

	code = 0;
	c = key;
	while (*c) {
		code += *c;
		c++;
	}

	return code & (HASH_BINS - 1);
}

/*
 * Hash_Put
 *
 * Insert the specified key-value pair to hash_table.
 */
int Hash_Put(hash_table_t *table, const char *key, void *value) {
	hash_entry_t *e;
	int code;

	if (!key || !*key || !value)
		return -1;

	e = Z_Malloc(sizeof(*e));
	memset(e, 0, sizeof(*e));

	e->key = key;
	e->value = value;

	code = Hash_Hashcode(key);

	if (!table->bins[code]) {
		table->bins[code] = e;
		return code;
	}

	table->bins[code]->prev = e;
	e->next = table->bins[code];
	table->bins[code] = e;
	return code;
}

/*
 * Hash_GetEntry
 *
 * Returns the first entry associated to the specified key.
 */
hash_entry_t *Hash_GetEntry(hash_table_t *table, const char *key) {
	hash_entry_t *e;
	int code;

	code = Hash_Hashcode(key);

	if (code == -1)
		return NULL;

	if (!table->bins[code])
		return NULL;

	e = table->bins[code];
	while (e) {
		if (!strcmp(e->key, key))
			return e;
		e = e->next;
	}

	return NULL;
}

/*
 * Hash_Get
 *
 * Return the first value hashed at key from hash_table.
 */
void *Hash_Get(hash_table_t *table, const char *key) {
	hash_entry_t *e;

	if ((e = Hash_GetEntry(table, key)))
		return e->value;

	return NULL;
}

/*
 * Hash_RemoveEntry
 *
 * Removes the specified entry from the hash and frees it, returning its
 * value so that it may also be freed if desired.
 */
void *Hash_RemoveEntry(hash_table_t *table, hash_entry_t *entry) {
	int code;
	void *ret;

	if (!table || !entry)
		return NULL;

	code = Hash_Hashcode(entry->key);

	if (table->bins[code] == entry) // fix the bin if we were the head
		table->bins[code] = entry->next;

	if (entry->prev)
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
void *Hash_Remove(hash_table_t *table, const char *key) {
	hash_entry_t *e;

	if ((e = Hash_GetEntry(table, key)))
		return Hash_RemoveEntry(table, e);

	return NULL;
}

/*
 * Hash_Clear
 *
 * Removes all entries associated to key from the specified hash.
 */
void Hash_Clear(hash_table_t *table, const char *key) {
	hash_entry_t *e;

	while ((e = Hash_GetEntry(table, key)))
		Hash_RemoveEntry(table, e);
}

/*
 * Hash_Free
 *
 * Free all entries associated with specified hash.  Does not free any of the
 * values referenced by the entries.
 */
void Hash_Free(hash_table_t *table) {
	hash_entry_t *e, *f;
	int i;

	for (i = 0; i < HASH_BINS; i++) {

		e = table->bins[i];
		while (e) {
			f = e->next;
			Z_Free(e);
			e = f;
		}
		table->bins[i] = NULL;
	}
}

