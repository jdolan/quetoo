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

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pak.h"
#include "filesystem.h"

/*
 * Pak_ReadPakfile
 *
 * Return a populated Pakfile from the specified path, with entries
 * hashed by name for fast finds.
 */
pak_t *Pak_ReadPakfile(const char *pakfile) {
	pak_header_t header;
	int i;
	pak_t *pak;

	pak = (pak_t *) Z_Malloc(sizeof(*pak));

	pak->handle = fopen(pakfile, "rb");
	if (!pak->handle) {
		Com_Warn("Pak_ReadPakfile: Couldn't open %s.\n", pakfile);
		Z_Free(pak);
		return NULL;
	}

	strcpy(pak->file_name, pakfile);

	Fs_Read(&header, 1, sizeof(pak_header_t), pak->handle);
	if (LittleLong(header.ident) != PAK_HEADER) {
		Com_Warn("Pak_ReadPakfile: %s is not a pak file.\n", pakfile);
		Fs_CloseFile(pak->handle);
		Z_Free(pak);
		return NULL;
	}

	header.dir_ofs = LittleLong(header.dir_ofs);
	header.dir_len = LittleLong(header.dir_len);

	pak->num_entries = header.dir_len / sizeof(pak_entry_t);
	if (pak->num_entries > MAX_PAK_ENTRIES) {
		Com_Warn("Pak_ReadPakfile: %s has %i files.\n", pakfile,
				pak->num_entries);
		Fs_CloseFile(pak->handle);
		Z_Free(pak);
		return NULL;
	}

	pak->entries = (pak_entry_t *) Z_Malloc(
			pak->num_entries * sizeof(pak_entry_t));

	fseek(pak->handle, header.dir_ofs, SEEK_SET);
	Fs_Read(pak->entries, 1, header.dir_len, pak->handle);

	Hash_Init(&pak->hash_table);

	// parse the directory
	for (i = 0; i < pak->num_entries; ++i) {
		pak->entries[i].file_ofs = LittleLong(pak->entries[i].file_ofs);
		pak->entries[i].file_len = LittleLong(pak->entries[i].file_len);

		Hash_Put(&pak->hash_table, pak->entries[i].name, &pak->entries[i]);
	}

	return pak;
}

/*
 * Pak_FreePakfile
 *
 * Frees and closes any resources allocated to read the specified Pakfile.
 */
void Pak_FreePakfile(pak_t *pak) {

	if (!pak)
		return;

	if (pak->handle)
		Fs_CloseFile(pak->handle);

	if (pak->entries)
		Z_Free(pak->entries);

	Hash_Free(&pak->hash_table);

	Z_Free(pak);
}

/*
 * The game only needs Pak_ReadPakfile and Pak_FreePakfile.  Below, we have
 * the remainder of the functionality for the pak command line utility and
 * map compiler.
 */

int err;

/*
 * Pak_MakePath
 *
 * This is basically a combination of Fs_CreatePath and Sys_Mkdir,
 * but we encapsulate it here to make linking the `pak` program simple.
 */
static void Pak_MakePath(char *path) {
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++) {
		if (*ofs == '/') { // create the directory
			*ofs = 0;
#ifdef _WIN32
			mkdir(path);
#else
			mkdir(path, 0777);
#endif
			*ofs = '/';
		}
	}
}

/*
 * Pak_ExtractPakfile
 *
 * A convenience function for deserializing a Pakfile to the filesystem.
 */
void Pak_ExtractPakfile(const char *pakfile, char *dir, boolean_t test) {
	pak_t *pak;
	FILE *f;
	void *p;
	int i;

	if (dir && chdir(dir) == -1) {
		fprintf(stderr, "Couldn't unpak to %s.\n", dir);
		err = ERR_DIR;
		return;
	}

	if (!(pak = Pak_ReadPakfile(pakfile))) {
		err = ERR_PAK;
		return;
	}

	for (i = 0; i < pak->num_entries; i++) {

		if (test) { // print contents and continue
			printf("Contents %s (%d bytes).\n", pak->entries[i].name,
					pak->entries[i].file_len);
			continue;
		}

		Pak_MakePath(pak->entries[i].name);

		if (!(f = fopen(pak->entries[i].name, "wb"))) {
			fprintf(stderr, "Couldn't write %s.\n", pak->entries[i].name);
			err = ERR_PAK;
			continue;
		}

		fseek(pak->handle, pak->entries[i].file_ofs, SEEK_SET);

		p = (void *) Z_Malloc(pak->entries[i].file_len);

		Fs_Read(p, 1, pak->entries[i].file_len, pak->handle);

		Fs_Write(p, 1, pak->entries[i].file_len, f);

		Fs_CloseFile(f);
		Z_Free(p);

		printf("Extracted %s (%d bytes).\n", pak->entries[i].name,
				pak->entries[i].file_len);
	}

	Pak_FreePakfile(pak);
}

/*
 * Pak_CreatePakstream
 *
 * Allocate a new Pakfile for creating a new archive from arbitrary resources.
 */
pak_t *Pak_CreatePakstream(char *pakfile) {
	FILE *f;
	pak_t *pak;
	pak_header_t header;

	if (!(f = fopen(pakfile, "wb"))) {
		fprintf(stderr, "Couldn't open %s.\n", pakfile);
		err = ERR_DIR;
		return NULL;
	}

	// allocate a new pak
	pak = (pak_t *) Z_Malloc(sizeof(*pak));
	pak->entries = (pak_entry_t *) Z_Malloc(
			sizeof(pak_entry_t) * MAX_PAK_ENTRIES);

	pak->num_entries = 0;

	pak->handle = f;
	strcpy(pak->file_name, pakfile);

	// stuff a bogus header for now
	memset(&header, 0, sizeof(header));
	Fs_Write(&header, sizeof(header), 1, pak->handle);

	return pak;
}

/*
 * Pak_ClosePakstream
 *
 * Finalizes and frees a newly created Pakfile archive.
 */
void Pak_ClosePakstream(pak_t *pak) {
	pak_header_t header;
	int i;

	header.ident = PAK_HEADER;
	header.dir_len = pak->num_entries * sizeof(pak_entry_t);
	header.dir_ofs = ftell(pak->handle);

	// write the directory (table of contents) of entries
	for (i = 0; i < pak->num_entries; i++)
		Fs_Write(&pak->entries[i], sizeof(pak_entry_t), 1, pak->handle);

	// go back to the beginning to finalize header
	fseek(pak->handle, 0, SEEK_SET);
	Fs_Write(&header, sizeof(pak_header_t), 1, pak->handle);

	Pak_FreePakfile(pak);
}

/*
 * Pak_AddEntry
 *
 * Add an entry to the specified Pakfile stream.
 */
void Pak_AddEntry(pak_t *pak, char *name, int len, void *p) {
	char *c;

	if (pak->num_entries == MAX_PAK_ENTRIES) {
		fprintf(stderr, "Maximum pak entries (4096) reached.\n");
		return;
	}

	c = name; // strip './' from names
	if (!strncmp(c, "./", 2))
		c += 2;

	memset(pak->entries[pak->num_entries].name, 0, 56);
	strncpy(pak->entries[pak->num_entries].name, c, 55);

	pak->entries[pak->num_entries].file_len = len;
	pak->entries[pak->num_entries].file_ofs = ftell(pak->handle);

	Fs_Write(p, len, 1, pak->handle);

	printf("Added %s (%d bytes).\n", c, len);

	pak->num_entries++;
}

/*
 * Pak_RecursiveAdd
 */
static void Pak_RecursiveAdd(pak_t *pak, const char *dir) {
	char s[MAX_OSPATH];
	struct dirent *e;
	struct stat st;
	void *p;
	FILE *f;
	DIR *d;

	if (!(d = opendir(dir))) {
		fprintf(stderr, "Couldn't open %s.\n", dir);
		err = ERR_DIR;
		return;
	}

	while ((e = readdir(d))) {

		if (e->d_name[0] == '.')
			continue;

		if (strstr(e->d_name, ".pak"))
			continue;

		sprintf(s, "%s/%s", dir, e->d_name);
		stat(s, &st);

		if (S_ISDIR(st.st_mode)) {
			Pak_RecursiveAdd(pak, s);
			continue;
		}

		if (!S_ISREG(st.st_mode))
			continue;

		if (!(f = fopen(s, "rb"))) {
			fprintf(stderr, "Couldn't open %s.\n", s);
			err = ERR_PAK;
			continue;
		}

		p = (void *) Z_Malloc(st.st_size);

		Fs_Read(p, 1, st.st_size, f);

		Pak_AddEntry(pak, s, st.st_size, p);

		Fs_CloseFile(f);

		Z_Free(p);
	}

	closedir(d);
}

/*
 * Pak_CreatePakfile
 *
 * A convenience function for creating Pakfile archives from the filesystem tree.
 */
void Pak_CreatePakfile(char *pakfile, int numdirs, char **dirs) {
	pak_t *pak;
	int i;

	pak = Pak_CreatePakstream(pakfile);

	if (!pak)
		return;

	for (i = 0; i < numdirs; i++) {
		Pak_RecursiveAdd(pak, dirs[i]);

		if (err) { // error assembling pak
			Fs_CloseFile(pak->handle);
			return;
		}
	}

	Pak_ClosePakstream(pak);
}
