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

#ifdef _WIN32
#include <windows.h>

#define CSIDL_APPDATA  0x001a
#define CSIDL_PERSONAL 0x0005

#endif /* ifdef _WIN32 */

#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "filesystem.h"

static char fs_gamedir[MAX_OSPATH];

static cvar_t *fs_base;
cvar_t *fs_game;

typedef struct search_path_s {
	char path[MAX_OSPATH];
	struct search_path_s *next;
} search_path_t;

static search_path_t *fs_search_paths;
static search_path_t *fs_base_search_paths; // without gamedirs

static hash_table_t fs_hash_table; // pakfiles are pushed into a hash


/*
 * Fs_Write
 */
size_t Fs_Write(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t len;

	if (size * nmemb == 0)
		return 0;

	len = fwrite(ptr, size, nmemb, stream);

	if (len <= 0)
		Com_Warn("Fs_Write: Failed to write\n");

	return len;
}

/*
 * Fs_Read
 */
size_t Fs_Read(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t len;

	if (size * nmemb == 0)
		return 0;

	len = fread(ptr, size, nmemb, stream);

	if (len <= 0) // read failed, exit
		Com_Warn("Fs_Read: Failed to read file.");

	return len;
}

/*
 * Fs_CloseFile
 */
void Fs_CloseFile(FILE *f) {
	fclose(f);
}

/*
 * Fs_FileLength
 */
static int Fs_FileLength(FILE *f) {
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/*
 * Fs_CreatePath
 *
 * Creates any directories needed to store the given path.
 */
void Fs_CreatePath(const char *path) {
	char pathCopy[MAX_OSPATH];
	char *ofs;

	strncpy(pathCopy, path, sizeof(pathCopy) - 1);

	for (ofs = pathCopy + 1; *ofs; ofs++) {
		if (*ofs == '/') { // create the directory
			*ofs = 0;
			Sys_Mkdir(pathCopy);
			*ofs = '/';
		}
	}
}

char *fs_last_pak = NULL; // the server uses this to determine CS_PAK

/*
 * Fs_OpenFile
 *
 * Attempts to open the specified file on the search path.  Returns filesize
 * and an open FILE pointer.  This generalizes opening files from paks vs
 * opening filesystem resources directly.
 */
int Fs_OpenFile(const char *file_name, FILE **file, file_mode_t mode) {
	search_path_t *search;
	char path[MAX_OSPATH];
	struct stat sbuf;
	pak_t *pak;
	pak_entry_t *e;

	fs_last_pak = NULL;

	if (!file_name || *file_name == '\0') {
		Com_Warn("Fs_OpenFile: Missing file name\n");
		return -1;
	}

	// open for write or append in game dir and return
	if (mode == FILE_WRITE || mode == FILE_APPEND) {

		snprintf(path, sizeof(path), "%s/%s", Fs_Gamedir(), file_name);
		Fs_CreatePath(path);

		*file = fopen(path, (mode == FILE_WRITE ? "wb" : "ab"));
		return *file ? 0 : -1;
	}

	// try the search paths
	for (search = fs_search_paths; search; search = search->next) {

		snprintf(path, sizeof(path), "%s/%s", search->path, file_name);

		if (stat(path, &sbuf) == -1 || !S_ISREG(sbuf.st_mode))
			continue;

		if ((*file = fopen(path, "rb")) == NULL)
			continue;

		return Fs_FileLength(*file);
	}

	// fall back on the pak files
	if ((pak = (pak_t *) Hash_Get(&fs_hash_table, file_name))) {

		// find the entry within the pak file
		if ((e = (pak_entry_t *) Hash_Get(&pak->hash_table, file_name))) {

			*file = fopen(pak->file_name, "rb");

			if (!*file) {
				Com_Warn("Fs_OpenFile: couldn't reopen %s.\n", pak->file_name);
				return -1;
			}

			fseek(*file, e->file_ofs, SEEK_SET);

			fs_last_pak = strrchr(pak->file_name, '/') + 1;
			return e->file_len;
		}
	}

	if (MixedCase(file_name)) { // try lowercase version
		char lower[MAX_QPATH];
		strncpy(lower, file_name, sizeof(lower) - 1);
		return Fs_OpenFile(Lowercase(lower), file, mode);
	}

	//Com_Debug("Fs_OpenFile: can't find %s.\n", file_name);

	*file = NULL;
	return -1;
}

/*
 * Fs_ReadFile
 *
 * Properly handles partial reads
 */
void Fs_ReadFile(void *buffer, int len, FILE *f) {
	int read;

	read = Fs_Read(buffer, 1, len, f);

	if (read != len) { // read failed, exit
		Com_Error(ERR_DROP, "Fs_ReadFile: %d bytes read.\n", read);
	}
}

/*
 * Fs_LoadFile
 *
 * Filename are relative to the quake search path
 * A NULL buffer will just return the file length without loading.
 */
int Fs_LoadFile(const char *file_name, void **buffer) {
	FILE *h;
	byte *buf;
	int len;

	buf = NULL; // quiet compiler warning

	// look for it in the filesystem or pak files
	len = Fs_OpenFile(file_name, &h, FILE_READ);
	if (!h) {
		if (buffer)
			*buffer = NULL;
		return -1;
	}

	if (!buffer) {
		Fs_CloseFile(h);
		return len;
	}

	// allocate, and string-terminate it in case it's text
	buf = Z_Malloc(len + 1);
	buf[len] = 0;

	*buffer = buf;

	Fs_ReadFile(buf, len, h);

	Fs_CloseFile(h);

	return len;
}

/*
 * Fs_FreeFile
 */
void Fs_FreeFile(void *buffer) {
	Z_Free(buffer);
}

/*
 * Fs_AddPakfile
 */
void Fs_AddPakfile(const char *pakfile) {
	pak_t *pak;
	int i;

	if (!(pak = Pak_ReadPakfile(pakfile)))
		return;

	for (i = 0; i < pak->num_entries; i++) // hash the entries to the pak
		Hash_Put(&fs_hash_table, pak->entries[i].name, pak);

	Com_Print("Added %s: %i files.\n", pakfile, pak->num_entries);
}

/*
 * Fs_AddSearchPath
 *
 * Adds the directory to the head of the path, and loads all paks within it.
 */
static void Fs_AddSearchPath(const char *dir) {
	search_path_t *search;
	const char *p;

	// don't add the same search_path twice
	search = fs_search_paths;
	while (search) {
		if (!strcmp(search->path, dir))
			return;
		search = search->next;
	}

	// add the base directory to the search path
	search = Z_Malloc(sizeof(search_path_t));
	strncpy(search->path, dir, sizeof(search->path) - 1);
	search->path[sizeof(search->path) - 1] = 0;

	search->next = fs_search_paths;
	fs_search_paths = search;

	p = Sys_FindFirst(va("%s/*.pak", dir));
	while (p) {
		Fs_AddPakfile(p);
		p = Sys_FindNext();
	}

	Sys_FindClose();
}

static char homedir[MAX_OSPATH];

/*
 * Fs_Homedir
 */
static char *Fs_Homedir(void) {
#ifdef _WIN32
	void *handle;
	FARPROC GetFolderPath;

	memset(homedir, 0, sizeof(homedir));

	if((handle = dlopen("shfolder.dll", 0))) {
		if((GetFolderPath = dlsym(handle, "SHGetFolderPathA")))
		GetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, homedir);
		dlclose(handle);
	}

	if(*homedir != '\0') // append our directory name
	strcat(homedir, "/My Games/Quake2World");
	else // or simply use ./
	strcat(homedir, PKGDATADIR);
#else
	memset(homedir, 0, sizeof(homedir));

	strncpy(homedir, getenv("HOME"), sizeof(homedir));
	strcat(homedir, "/.quake2world");
#endif
	return homedir;
}

/*
 * Fs_AddUserSearchPath
 *
 * Adds the user-specific search path, setting fs_gamedir in the process. This
 * is where all files produced by the game are written to.
 */
static void Fs_AddUserSearchPath(const char *dir) {
	char gdir[MAX_OSPATH];

	memset(homedir, 0, sizeof(homedir));
	snprintf(gdir, sizeof(gdir), "%s/%s", Fs_Homedir(), dir);

	Com_Print("Using %s for writing.\n", gdir);

	Fs_CreatePath(va("%s/", gdir));

	strncpy(fs_gamedir, gdir, sizeof(fs_gamedir) - 1);
	fs_gamedir[sizeof(fs_gamedir) - 1] = 0;

	Fs_AddSearchPath(gdir);
}

/*
 * Fs_Gamedir
 *
 * Called to find where to write a file (demos, screenshots, etc)
 */
const char *Fs_Gamedir(void) {
	return fs_gamedir;
}

/*
 * Fs_FindFirst
 */
const char *Fs_FindFirst(const char *path, boolean_t absolute) {
	const char *n;
	char name[MAX_OSPATH];
	const search_path_t *s;

	// search through all the paths for path
	for (s = fs_search_paths; s != NULL; s = s->next) {

		snprintf(name, sizeof(name), "%s/%s", s->path, path);
		if ((n = Sys_FindFirst(name))) {
			Sys_FindClose();
			return absolute ? n : n + strlen(s->path) + 1;
		}

		Sys_FindClose();
	}

	return NULL;
}

/*
 * Fs_ExecAutoexec
 *
 * Execs the local autoexec.cfg for the current gamedir.  This is
 * a function call rather than simply stuffing "exec autoexec.cfg"
 * because we do not wish to use default/autoexec.cfg for all mods.
 */
void Fs_ExecAutoexec(void) {
	char name[MAX_QPATH];
	search_path_t *s, *end;

	// don't look in default if gamedir is set
	if (fs_search_paths == fs_base_search_paths)
		end = NULL;
	else
		end = fs_base_search_paths;

	// search through all the paths for an autoexec.cfg file
	for (s = fs_search_paths; s != end; s = s->next) {

		snprintf(name, sizeof(name), "%s/autoexec.cfg", s->path);

		if (Sys_FindFirst(name)) {
			Cbuf_AddText("exec autoexec.cfg\n");
			Sys_FindClose();
			break;
		}

		Sys_FindClose();
	}

	Cbuf_Execute(); // execute it
}

/*
 * Fs_SetGame
 *
 * Sets the game path to a relative directory.
 */
void Fs_SetGame(const char *dir) {
	search_path_t *s;
	hash_entry_t *e;
	pak_t *pak;
	int i;

	if (!dir || !*dir) {
		dir = DEFAULT_GAME;
	}

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":")) {
		Com_Warn("Fs_SetGame: Game should be a directory name, not a path.\n");
		return;
	}

	// free up any current game dir info
	while (fs_search_paths != fs_base_search_paths) {

		// free paks not living in base search paths
		for (i = 0; i < HASH_BINS; i++) {
			e = fs_hash_table.bins[i];
			while (e) {
				pak = (pak_t*) e->value;

				if (!strstr(pak->file_name, fs_search_paths->path))
					continue;

				Hash_RemoveEntry(&fs_hash_table, e);
				Pak_FreePakfile(pak);
			}
		}
		s = fs_search_paths->next;
		Z_Free(fs_search_paths);
		fs_search_paths = s;
	}

	// now add new entries for
	if (!strcmp(dir, DEFAULT_GAME)) {
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVER_INFO);
	} else {
		Fs_AddSearchPath(va(PKGLIBDIR"/%s", dir));
		Fs_AddSearchPath(va(PKGDATADIR"/%s", dir));
		Fs_AddUserSearchPath(dir);
	}
}

#define GZIP_BUFFER (64 * 1024)

/*
 * Fs_GunzipFile
 *
 * Inflates the specified file in place, removing the .gz suffix from the path.
 * The original archive file is removed upon successful decompression.
 */
void Fs_GunzipFile(const char *path) {
	gzFile gz;
	FILE *f;
	char *c;
	byte *buffer;
	int r, w;
	char p[MAX_OSPATH];

	if (!path || *path == '\0')
		return;

	strncpy(p, path, sizeof(p));

	if (!(c = strstr(p, ".gz"))) {
		Com_Warn("Fs_GunzipFile: %s lacks .gz suffix.\n", p);
		return;
	}

	if (!(gz = gzopen(p, "rb"))) {
		Com_Warn("Fs_GunzipFile: Failed to open %s.\n", p);
		return;
	}

	*c = 0; // mute .gz extension

	if (!(f = fopen(p, "wb"))) {
		Com_Warn("Fs_GunzipFile: Failed to open %s.\n", p);
		gzclose(gz);
		return;
	}

	*c = 0;
	buffer = (byte *) Z_Malloc(GZIP_BUFFER);

	while (true) {
		r = gzread(gz, buffer, GZIP_BUFFER);

		if (r == 0) // end of file
			break;

		if (r == -1) { // error in gzip stream
			Com_Warn("Fs_GunzipFile: Error in gzip stream %s.\n", path);
			break;
		}

		w = Fs_Write(buffer, 1, r, f);

		if (r != w) { // bad write
			Com_Warn("Fs_GunzipFile: Incomplete write %s.\n", path);
			break;
		}
	}

	gzclose(gz);
	Fs_CloseFile(f);

	Z_Free(buffer);

	unlink(path);
}

/*
 * Fs_Init
 */
void Fs_Init(void) {
	char bd[MAX_OSPATH];

	fs_search_paths = NULL;

	Hash_Init(&fs_hash_table);

	// allow the game to run from outside the data tree
	fs_base = Cvar_Get("fs_base", "", CVAR_NO_SET, NULL);

	if (fs_base && strlen(fs_base->string)) { // something was specified
		snprintf(bd, sizeof(bd), "%s/%s", fs_base->string, DEFAULT_GAME);
		Fs_AddSearchPath(bd);
	}

	// add default to search path
	Fs_AddSearchPath(PKGLIBDIR"/"DEFAULT_GAME);
	Fs_AddSearchPath(PKGDATADIR"/"DEFAULT_GAME);

	// then add a '.quake2world/default' directory in home directory by default
	Fs_AddUserSearchPath(DEFAULT_GAME);

	// any set game paths will be freed up to here
	fs_base_search_paths = fs_search_paths;

	// check for game override
	fs_game = Cvar_Get("game", DEFAULT_GAME, CVAR_LATCH | CVAR_SERVER_INFO, NULL);

	if (strcmp(fs_game->string, DEFAULT_GAME))
		Fs_SetGame(fs_game->string);
}

/*
 * strcmp_
 *
 * Wraps strcmp so it can be used to sort strings in qsort.
 */
static int strcmp_(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

/*
 * Fs_CompleteFile
 */
int Fs_CompleteFile(const char *dir, const char *prefix, const char *suffix,
		const char *matches[]) {
	static char file_list[MAX_QPATH * 256];
	char *fl = file_list;
	const char *n;
	char name[MAX_OSPATH];
	const search_path_t *s;
	const hash_entry_t *h;
	int i = 0;
	int m = 0;

	// search through paths for matches
	for (s = fs_search_paths; s != NULL; s = s->next) {
		snprintf(name, sizeof(name), "%s/%s%s*%s", s->path, dir, prefix, suffix);

		if ((n = Sys_FindFirst(name)) == NULL) {
			Sys_FindClose();
			continue;
		}
		do {
			// copy file_name to local buffer
			strcpy(fl, n + strlen(s->path) + 1 + strlen(dir));
			matches[m] = fl;
			m++;
			fl = fl + strlen(fl) + 1;
		} while ((n = Sys_FindNext()) != NULL);

		Sys_FindClose();
	}

	// search through paks for matches
	snprintf(name, sizeof(name), "%s%s*%s", dir, prefix, suffix);
	for (i = 0; i < HASH_BINS; i++) {
		h = fs_hash_table.bins[i];
		while (h != NULL) {
			if (GlobMatch(name, h->key)
					&& !(dir == NULL && strchr(h->key, '/'))) {
				// omit configs not in the root gamedir
				matches[m] = h->key + strlen(dir);
				m++;
			}
			h = h->next;
		}
	}

	// sort in alphabetical order
	qsort(matches, m, sizeof(matches[0]), strcmp_);

	// print out list of matches (minus duplicates)
	for (i = 0; i < m; i++) {
		if (i == 0 || strcmp(matches[i], matches[i - 1])) {
			strcpy(name, matches[i]);
			if (strstr(name, suffix) != NULL)
				*strstr(name, suffix) = 0; // lop off file extension
			Com_Print("%s\n", name);
		}
	}

	return m;
}
