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

#ifndef __PAK_H__
#define __PAK_H__

#include "hash.h"

#define ERR_OK   0
#define ERR_DIR -1
#define ERR_PAK -2
#define ERR_ARG -3

#define PAK_HEADER  (('K' << 24) + ('C' << 16) + ('A' << 8) + 'P')

#define MAX_PAK_ENTRIES 4096

typedef struct {
	char name[56];
	int file_ofs, file_len;
} pak_entry_t;

typedef struct {
	int ident;  // == IDPAKHEADER
	int dir_ofs;
	int dir_len;
} pak_header_t;

typedef struct pak_s {
	char file_name[MAX_OSPATH];
	FILE *handle;
	int num_entries;
	pak_entry_t *entries;
	hash_table_t hash_table;
} pak_t;

/*
Pak_ReadPakfile

Return a populated Pakfile from the specified path, with entries
hashed by name for fast finds.
*/
pak_t *Pak_ReadPakfile(const char *pakfile);


/*
Pak_FreePakfile

Frees and closes any resources allocated to read the specified Pakfile.
*/
void Pak_FreePakfile(pak_t *pak);

/*
Pak_ExtractPakfile

A convenience function for deserializing a Pakfile to the filesystem.
*/
void Pak_ExtractPakfile(const char *pakfile, char *dir, boolean_t test);

/*
Pak_CreatePakstream

Allocate a new Pakfile for creating a new archive from arbitrary resources.
*/
pak_t *Pak_CreatePakstream(char *pakfile);


/*
Pak_ClosePakstream

Finalizes and frees a newly created Pakfile archive.
*/
void Pak_ClosePakstream(pak_t *pak);

/*
Pak_AddEntry

Add an entry to the specified Pakfile stream.
*/
void Pak_AddEntry(pak_t *pak, char *name, int len, void *p);

/*
Pak_CreatePakfile

A convenience function for creating Pakfile archives from the filesystem tree.
*/
void Pak_CreatePakfile(char *pakfile, int numdirs, char **dirs);

extern int err;

#endif /*__PAK_H__*/
