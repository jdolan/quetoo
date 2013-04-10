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

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
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

static GHashTable *fs_hash_table; // known files are pushed into a hash

/*
 * @brief
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
 * @brief
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
 * @brief
 */
void Fs_CloseFile(FILE *f) {
	fclose(f);
}

/*
 * @brief
 */
static int32_t Fs_FileLength(FILE *f) {
	int32_t pos;
	int32_t end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/*
 * @brief Creates any directories needed to store the given path.
 */
void Fs_CreatePath(const char *path) {
	char p[MAX_OSPATH];
	char *ofs;

	g_strlcpy(p, path, sizeof(p));

	for (ofs = p + 1; *ofs; ofs++) {
		if (*ofs == '/') { // create the directory
			*ofs = 0;
			Sys_Mkdir(p);
			*ofs = '/';
		}
	}
}

char *fs_last_pak = NULL; // the server uses this to determine CS_PAK

/*
 * @brief Attempts to open the specified file on the search path. Returns filesize
 * and an open FILE pointer. This generalizes opening files from paks vs
 * opening filesystem resources directly.
 */
int32_t Fs_OpenFile(const char *file_name, FILE **file, file_mode_t mode) {
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

		g_snprintf(path, sizeof(path), "%s/%s", Fs_Gamedir(), file_name);
		Fs_CreatePath(path);

		*file = fopen(path, (mode == FILE_WRITE ? "wb" : "ab"));
		return *file ? 0 : -1;
	}

	// try the search paths
	for (search = fs_search_paths; search; search = search->next) {

		g_snprintf(path, sizeof(path), "%s/%s", search->path, file_name);

		if (stat(path, &sbuf) == -1 || !S_ISREG(sbuf.st_mode))
			continue;

		if ((*file = fopen(path, "rb")) == NULL)
			continue;

		return Fs_FileLength(*file);
	}

	// fall back on the pak files
	if ((pak = (pak_t *) g_hash_table_lookup(fs_hash_table, file_name))) {

		// find the entry within the pak file
		if ((e = (pak_entry_t *) g_hash_table_lookup(pak->hash_table, file_name))) {

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
		Lowercase(file_name, lower);

		return Fs_OpenFile(lower, file, mode);
	}

	//Com_Debug("Fs_OpenFile: can't find %s.\n", file_name);

	*file = NULL;
	return -1;
}

/*
 * @brief Properly handles partial reads.
 */
void Fs_ReadFile(void *buffer, int32_t len, FILE *f) {
	int32_t read;

	read = Fs_Read(buffer, 1, len, f);

	if (read != len) { // read failed, exit
		Com_Error(ERR_DROP, "Fs_ReadFile: %d bytes read.\n", read);
	}
}

/*
 * @brief Filename are relative to the quake search path. A NULL buffer will just
 * return the file length without loading.
 */
int32_t Fs_LoadFile(const char *path, void **buffer) {
	FILE *h;
	byte *buf;
	int32_t len;

	buf = NULL; // quiet compiler warning

	// look for it in the filesystem or pak files
	len = Fs_OpenFile(path, &h, FILE_READ);
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
 * @brief
 */
void Fs_FreeFile(void *buffer) {
	Z_Free(buffer);
}

/*
 * @brief
 */
void Fs_AddPakfile(const char *pakfile) {
	pak_t *pak;
	int32_t i;

	if (!(pak = Pak_ReadPakfile(pakfile)))
		return;

	for (i = 0; i < pak->num_entries; i++) // hash the entries to the pak
		g_hash_table_insert(fs_hash_table, pak->entries[i].name, pak);

	Com_Print("Added %s: %i files.\n", pakfile, pak->num_entries);
}

/*
 * @brief Adds the directory to the head of the path, and loads all paks within it.
 */
static void Fs_AddSearchPath(const char *dir) {
	search_path_t *search;
	const char *p;

	// don't add the same search path twice
	search = fs_search_paths;
	while (search) {
		if (!strcmp(search->path, dir))
			return;
		search = search->next;
	}

	Com_Print("Adding path %s..\n", dir);

	// add the base directory to the search path
	search = Z_Malloc(sizeof(search_path_t));
	g_strlcpy(search->path, dir, sizeof(search->path));

	search->next = fs_search_paths;
	fs_search_paths = search;

	p = Sys_FindFirst(va("%s/*.pak", dir));
	while (p) {
		Fs_AddPakfile(p);
		p = Sys_FindNext();
	}

	Sys_FindClose();
}

/*
 * @brief
 */
static const char *Fs_Homedir(void) {
	static char homedir[MAX_OSPATH];
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
	g_snprintf(homedir, sizeof(homedir), "%s/.quake2world", getenv("HOME"));
#endif
	return homedir;
}

/*
 * @brief Adds the user-specific search path, setting fs_gamedir in the process. This
 * is where all files produced by the game are written to.
 */
static void Fs_AddUserSearchPath(const char *dir) {
	char gdir[MAX_OSPATH];

	g_snprintf(gdir, sizeof(gdir), "%s/%s", Fs_Homedir(), dir);

	Com_Print("Using %s for writing.\n", gdir);

	Fs_CreatePath(va("%s/", gdir));

	g_strlcpy(fs_gamedir, gdir, sizeof(fs_gamedir));

	Fs_AddSearchPath(gdir);
}

/*
 * @brief Called to find where to write a file (demos, screenshots, etc)
 */
const char *Fs_Gamedir(void) {
	return fs_gamedir;
}

/*
 * @brief
 */
const char *Fs_FindFirst(const char *path, bool absolute) {
	const char *n;
	char name[MAX_OSPATH];
	const search_path_t *s;

	// search through all the paths for path
	for (s = fs_search_paths; s != NULL; s = s->next) {

		g_snprintf(name, sizeof(name), "%s/%s", s->path, path);
		if ((n = Sys_FindFirst(name))) {
			Sys_FindClose();
			return absolute ? n : n + strlen(s->path) + 1;
		}

		Sys_FindClose();
	}

	return NULL;
}

/*
 * @brief Execs the local autoexec.cfg for the current gamedir. This is
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

		g_snprintf(name, sizeof(name), "%s/autoexec.cfg", s->path);

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
 * @brief
 */
static gboolean FS_SetGame_(gpointer key __attribute__((unused)), gpointer value, gpointer data) {

	if (strstr((char *) key, (char *) data))
		return false;

	Pak_FreePakfile((pak_t *) value);
	return true;
}

/*
 * @brief Sets the game path to a relative directory.
 */
void Fs_SetGame(const char *dir) {
	search_path_t *s;

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
		g_hash_table_foreach_remove(fs_hash_table, FS_SetGame_, fs_search_paths->path);

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
 * @brief Inflates the specified file in place, removing the .gz suffix from the path.
 * The original archive file is removed upon successful decompression.
 */
void Fs_GunzipFile(const char *path) {
	gzFile gz;
	FILE *f;
	char *c;
	byte *buffer;
	int32_t r, w;
	char p[MAX_OSPATH];

	if (!path || *path == '\0')
		return;

	g_strlcpy(p, path, sizeof(p));

	if (!(c = strstr(p, ".gz"))) {
		Com_Warn("Fs_GunzipFile: %s lacks .gz suffix.\n", p);
		return;
	}

	if (!(gz = gzopen(p, "rb"))) {
		Com_Warn("Fs_GunzipFile: Failed to open %s.\n", p);
		return;
	}

	*c = '\0'; // mute .gz extension

	if (!(f = fopen(p, "wb"))) {
		Com_Warn("Fs_GunzipFile: Failed to open %s.\n", p);
		gzclose(gz);
		return;
	}

	*c = '\0';
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
 * @brief
 */
void Fs_Init(void) {
	char dir[MAX_OSPATH];

	fs_search_paths = NULL;

	fs_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

	// allow the game to run from outside the data tree
	fs_base = Cvar_Get("fs_base", "", CVAR_NO_SET, NULL);

	if (strlen(fs_base->string)) { // something was specified
		g_snprintf(dir, sizeof(dir), "%s/%s", fs_base->string, DEFAULT_GAME);
		Fs_AddSearchPath(dir);
	}

#ifdef __APPLE__
	// add Contents/MacOS and Contents/Resources to the search path
	char path[MAX_OSPATH], *c;
	uint32_t i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {

		if ((c = strstr(path, "Quake2World.app"))) {
			strcpy(c + strlen("Quake2World.app/Contents/"), "MacOS/"DEFAULT_GAME);
			Fs_AddSearchPath(path);

			strcpy(c + strlen("Quake2World.app/Contents/"), "Resources/"DEFAULT_GAME);
			Fs_AddSearchPath(path);
		}
	} else {
		Com_Warn("Fs_Init: Failed to resolve executable path\n");
	}
#elif __LINUX__
	// add ./lib/default and ./share/default to the search path
	char path[MAX_OSPATH], *c;

	memset(path, 0, sizeof(path));

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {

		Com_Debug("Fs_Init: Resolved executable path %s\n", path);

		if ((c = strstr(path, "quake2world/bin"))) {
			strcpy(c + strlen("quake2world/"), "lib/"DEFAULT_GAME);
			Fs_AddSearchPath(path);

			strcpy(c + strlen("quake2world/"), "share/"DEFAULT_GAME);
			Fs_AddSearchPath(path);
		}
	}
	else {
		Com_Warn("Fs_Init: Failed to read /proc/%d/exe\n", getpid());
	}
#endif

	// add default to search path
	Fs_AddSearchPath(PKGLIBDIR"/"DEFAULT_GAME);
	Fs_AddSearchPath(PKGDATADIR"/"DEFAULT_GAME);

	// then add a '.quake2world/default' directory in home directory
	Fs_AddUserSearchPath(DEFAULT_GAME);

	// any set game paths will be freed up to here
	fs_base_search_paths = fs_search_paths;

	// check for game override
	fs_game = Cvar_Get("game", DEFAULT_GAME, CVAR_LATCH | CVAR_SERVER_INFO, NULL);

	if (strcmp(fs_game->string, DEFAULT_GAME))
		Fs_SetGame(fs_game->string);
}

typedef struct fs_complete_file_s {
	const char *name;
	const char *dir;
	GList *list;
} fs_complete_file_t;

/*
 * @brief GHFunc for Fs_CompleteFile.
 */
static void Fs_CompleteFile_(gpointer key, __attribute__((unused)) gpointer value, gpointer data) {
	char *path = (char *) key;
	fs_complete_file_t *matches = (fs_complete_file_t *) data;

	if (GlobMatch(matches->name, (char *) key)) {
		matches->list = g_list_prepend(matches->list, path + strlen(matches->dir));
	}
}

/*
 * @brief Console completion for file names.
 */
int32_t Fs_CompleteFile(const char *dir, const char *prefix, const char *suffix,
		const char *matches[]) {
	static char file_list[MAX_QPATH * 256];
	char *fl = file_list;
	const char *n;
	char name[MAX_OSPATH];
	const search_path_t *s;
	fs_complete_file_t files;

	files.name = name;
	files.dir = dir;
	files.list = NULL;

	// search through paths for matches
	for (s = fs_search_paths; s != NULL; s = s->next) {
		g_snprintf(name, sizeof(name), "%s/%s%s*%s", s->path, dir, prefix, suffix);

		if ((n = Sys_FindFirst(name)) == NULL) {
			Sys_FindClose();
			continue;
		}
		do {
			// copy file_name to local buffer
			strcpy(fl, n + strlen(s->path) + 1 + strlen(dir));
			files.list = g_list_prepend(files.list, fl);
			fl = fl + strlen(fl) + 1;
		} while ((n = Sys_FindNext()) != NULL);

		Sys_FindClose();
	}

	// search through paks for matches
	g_snprintf(name, sizeof(name), "%s%s*%s", dir, prefix, suffix);
	g_hash_table_foreach(fs_hash_table, Fs_CompleteFile_, &files);

	// sort in alphabetical order
	files.list = g_list_sort(files.list, (GCompareFunc) g_strcmp0);

	GList *head = files.list;
	int32_t i, j = g_list_length(head);

	// print32_t out list of matches (minus duplicates)
	for (i = 0; i < j; i++) {
		matches[i] = (char *) files.list->data;
		files.list = files.list->next;
		if (i == 0 || strcmp(matches[i], matches[i - 1])) {
			strncpy(name, matches[i], MAX_OSPATH);
			if (strstr(name, suffix) != NULL)
				*strstr(name, suffix) = 0; // lop off file extension
			Com_Print("%s\n", name);
		}
	}
	g_list_free(head);

	return j;
}
