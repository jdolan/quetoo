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

#include "filesystem.h"

#define FS_FILE_BUFFER (1024 * 1024 * 2)

//#define FS_LOAD_DEBUG // track Fs_Load / Fs_Free

typedef struct fs_state_s {
	char **base_search_paths;

#ifdef FS_LOAD_DEBUG
	GHashTable *loaded_files;
#endif
} fs_state_t;

static fs_state_t fs_state;

/*
 * @brief Closes the file.
 *
 * @return True on successful flush and close, false otherwise.
 */
bool Fs_Close(file_t *file) {
	return PHYSFS_close(file) ? true : false;
}

/*
 * @return True if the end of the file has been reached, false otherwise.
 */
bool Fs_Eof(file_t *file) {
	return PHYSFS_eof(file) ? true : false;
}

/*
 * @return True if the specified filename exists on the search path.
 */
bool Fs_Exists(const char *filename) {
	return PHYSFS_exists(filename) ? true : false;
}

/*
 * @return True if the file flushed successfully, false otherwise.
 */
bool Fs_Flush(file_t *file) {
	return PHYSFS_flush(file) ? true : false;
}

/*
 * @return The last error message resulting from filesystem operations.
 */
const char *Fs_LastError(void) {
	return PHYSFS_getLastError();
}

/*
 * @brief Creates the specified directory (and any ancestors) in Fs_WriteDir.
 */
bool Fs_Mkdir(const char *dir) {
	return PHYSFS_mkdir(dir) ? true : false;
}

/*
 * @brief Opens the specified file for appending.
 */
file_t *Fs_OpenAppend(const char *filename) {
	char dir[MAX_QPATH];
	file_t *file;

	Dirname(filename, dir);
	Fs_Mkdir(dir);

	if ((file = PHYSFS_openAppend(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("Fs_OpenRead: %s: %s\n", filename, Fs_LastError());
		}
	}

	return file;
}

/*
 * @brief Opens the specified file for reading.
 */
file_t *Fs_OpenRead(const char *filename) {
	file_t *file;

	if ((file = PHYSFS_openRead(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("Fs_OpenRead: %s: %s\n", filename, Fs_LastError());
		}
	}

	return file;
}

/*
 * @brief Opens the specified file for writing.
 */
file_t *Fs_OpenWrite(const char *filename) {
	char dir[MAX_QPATH];
	file_t *file;

	Dirname(filename, dir);
	Fs_Mkdir(dir);

	if ((file = PHYSFS_openWrite(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("Fs_OpenWrite: %s: %s\n", filename, Fs_LastError());
		}
	}

	return file;
}

/*
 * @brief Prints the specified formatted string to the given file.
 *
 * @return The number of characters written, or -1 on failure.
 */
int64_t Fs_Print(file_t *file, const char *fmt, ...) {
	va_list args;
	static char string[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(string, sizeof(string), fmt, args);
	va_end(args);

	return Fs_Write(file, string, 1, strlen(string));
}

/*
 * @brief Reads from the specified file.
 *
 * @return The number of objects read, or -1 on failure.
 */
int64_t Fs_Read(file_t *file, void *buffer, size_t size, size_t count) {
	return PHYSFS_read(file, buffer, size, count);
}

/*
 * @brief Reads a line from the specified file.
 *
 * @return True on success, false on failures.
 */
bool Fs_ReadLine(file_t *file, char *buffer, size_t len) {
	size_t i;
	char *c;

	for (i = 0, c = buffer; i < len - 1; i++, c++) {

		if (Fs_Read(file, c, 1, 1) != 1) {
			break;
		}

		if (*c == '\n') {
			i++;
			c++;
			break;
		}
	}

	*c = '\0';
	return i ? true : false;
}

/*
 * @brief Seeks to the specified offset.
 */
bool Fs_Seek(file_t *file, size_t offset) {
	return PHYSFS_seek(file, offset) ? true : false;
}

/*
 * @return The current file offset.
 */
int64_t Fs_Tell(file_t *file) {
	return PHYSFS_tell(file);
}

/*
 * @brief Writes to the specified file.
 *
 * @return The number of objects read, or -1 on failure.
 */
int64_t Fs_Write(file_t *file, void *buffer, size_t size, size_t count) {
	return PHYSFS_write(file, buffer, size, count);
}

/*
 * @brief Loads the specified file into the given buffer, which is automatically
 * allocated if non-NULL. Returns the file length, or -1 if it is unable to be
 * read. Be sure to free the buffer when finished with Fs_Free.
 *
 * @return The file length, or -1 on error.
 */
int64_t Fs_Load(const char *filename, void **buffer) {
	int64_t len;

	typedef struct {
		byte *data;
		int64_t len;
	} fs_block_t;

	file_t *file;
	if ((file = Fs_OpenRead(filename))) {
		GList *list = NULL;
		len = 0;

		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("Fs_LoadFile: %s\n", Fs_LastError());
		}

		while (!Fs_Eof(file)) {
			fs_block_t *b = Z_Malloc(sizeof(fs_block_t));

			b->data = Z_LinkMalloc(FS_FILE_BUFFER, b);
			b->len = Fs_Read(file, b->data, 1, FS_FILE_BUFFER);

			if (b->len == -1) {
				Com_Error(ERR_DROP, "Fs_LoadFile: %s\n", Fs_LastError());
			}

			list = g_list_append(list, b);
			len += b->len;
		}

		if (buffer) {
			if (len > 0) {
				byte *buf = *buffer = Z_Malloc(len + 1);

				GList *e = list;
				while (e) {
					fs_block_t *b = (fs_block_t *) e->data;

					memcpy(buf, b->data, b->len);
					buf += (ptrdiff_t) b->len;

					e = e->next;
				}

#ifdef FS_LOAD_DEBUG
				g_hash_table_insert(fs_state.loaded_files, *buffer, (gpointer) Z_CopyString(filename));
#endif
			} else {
				*buffer = NULL;
			}
		}

		g_list_free_full(list, Z_Free);
		Fs_Close(file);
	} else {
		len = -1;

		if (buffer) {
			*buffer = NULL;
		}
	}

	return len;
}

/*
 * @brief Frees the specified buffer allocated by Fs_LoadFile.
 */
void Fs_Free(void *buffer) {

	if (buffer) {
#ifdef FS_LOAD_DEBUG
		if (!g_hash_table_remove(fs_state.loaded_files, buffer)) {
			Com_Warn("Fs_Free: Invalid buffer\n");
		}
#endif
		Z_Free(buffer);
	}
}

/*
 * @brief Renames the specified source to the given destination.
 */
bool Fs_Rename(const char *source, const char *dest) {
	const char *dir = Fs_WriteDir();

	return rename(va("%s/%s", dir, source), va("%s/%s", dir, dest)) == 0;
}

/*
 * @brief Unlinks (deletes) the specified file.
 */
bool Fs_Unlink(const char *filename) {

	if (!strcmp(Fs_WriteDir(), Fs_RealDir(filename))) {
		return unlink(filename) == 0;
	}

	return false;
}

typedef struct {
	char pattern[MAX_QPATH];
	fs_enumerate_func function;
	char dir[MAX_QPATH];
} fs_enumerate_t;

static fs_enumerate_t fs_enumerate;

/*
 * @brief Enumeration helper for Fs_Enumerate.
 */
static void Fs_Enumerate_(void *data, const char *dir, const char *filename) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s%s", dir, filename);

	if (GlobMatch(fs_enumerate.pattern, path)) {
		fs_enumerate.function(path, data);
	}
}

/*
 * @brief Enumerates files in the specified directory, calling the given function.
 */
void Fs_Enumerate(const char *pattern, fs_enumerate_func func, void *data) {

	g_strlcpy(fs_enumerate.pattern, pattern, sizeof(fs_enumerate.pattern));

	fs_enumerate.function = func;

	if (strchr(pattern, '/')) {
		Dirname(pattern, fs_enumerate.dir);
	} else {
		g_strlcpy(fs_enumerate.dir, "/", sizeof(fs_enumerate.dir));
	}

	PHYSFS_enumerateFilesCallback(fs_enumerate.dir, Fs_Enumerate_, data);
}

/*
 * @brief GHFunc for Fs_CompleteFile.
 */
static void Fs_CompleteFile_enumerate(const char *path, void *data) {
	GList **matches = (GList **) data;
	char match[MAX_QPATH];

	StripExtension(Basename(path), match);
	Com_Print("%s\n", match);

	*matches = g_list_prepend(*matches, Z_CopyString(match));
}

/*
 * @brief Console completion for file names.
 */
void Fs_CompleteFile(const char *pattern, GList **matches) {
	Fs_Enumerate(pattern, Fs_CompleteFile_enumerate, (void *) matches);
}

static void Fs_AddToSearchPath_enumerate(const char *path, void *data);

/*
 * @brief Adds the directory to the search path, loading all archives within it.
 */
void Fs_AddToSearchPath(const char *dir) {

	Com_Print("Adding path %s..\n", dir);

	if (PHYSFS_mount(dir, NULL, 1) == 0) {
		Com_Warn("Fs_AddToSearchPath: %s\n", PHYSFS_getLastError());
		return;
	}

	Fs_Enumerate("*.zip", Fs_AddToSearchPath_enumerate, (void *) dir);
}

/*
 * @brief Enumeration helper for Fs_AddToSearchPath. Adds all archive files for
 * the newly added filesystem mount point.
 */
static void Fs_AddToSearchPath_enumerate(const char *path, void *data) {
	const char *dir = (const char *)data;

	if (!strcmp(Fs_RealDir(path), dir)) {
		Fs_AddToSearchPath(va("%s%s", dir, path));
	}
}

/*
 * @brief Adds the user-specific search path, setting the write dir in the
 * process. This is where all files produced by the game are written to.
 */
static void Fs_AddUserSearchPath(const char *dir) {
	char gdir[MAX_OSPATH];

	g_snprintf(gdir, sizeof(gdir), "%s/%s", Sys_UserDir(), dir);

	Com_Print("Using %s for writing.\n", gdir);

	Fs_Mkdir(gdir);

	Fs_AddToSearchPath(gdir);
	PHYSFS_setWriteDir(gdir);
}

/*
 * @brief Sets the game path to a relative directory.
 */
void Fs_SetGame(const char *dir) {

	if (!dir || !*dir) {
		Com_Warn("Fs_SetGame: Missing game name\n");
		return;
	}

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":")) {
		Com_Warn("Fs_SetGame: Game should be a directory name, not a path.\n");
		return;
	}

	// iterate the current search path, removing those which are not base paths
	char **paths = PHYSFS_getSearchPath();
	char **path = paths;
	while (*path != NULL) {
		char **p = fs_state.base_search_paths;
		while (*p != NULL) {
			if (!strcmp(*path, *p)) {
				break;
			}
			p++;
		}
		if (!*p) {
			Com_Debug("Fs_SetGame: Removing %s\n", *path);
			PHYSFS_removeFromSearchPath(*path);
		}
		path++;
	}

	PHYSFS_freeList(paths);

	// now add new entries for the new game
	Fs_AddToSearchPath(va(PKGLIBDIR"/%s", dir));
	Fs_AddToSearchPath(va(PKGDATADIR"/%s", dir));
	Fs_AddUserSearchPath(dir);
}

/*
 * @brief Called to find where to write a file (demos, screenshots, etc)
 */
const char *Fs_WriteDir(void) {
	return PHYSFS_getWriteDir();
}

/*
 * @brief Returns the real directory name of the specified file.
 */
const char *Fs_RealDir(const char *filename) {
	return PHYSFS_getRealDir(filename);
}

/*
 * @brief Initializes the file subsystem.
 */
void Fs_Init(const char *argv0) {

	memset(&fs_state, 0, sizeof(fs_state_t));

	if (PHYSFS_init(argv0) == 0) {
		Com_Error(ERR_FATAL, "Fs_Init: %s\n", PHYSFS_getLastError());
	}

#if (__APPLE__ || __LINUX__)
	const char *path = Sys_ExecutablePath();
	if (path) {
		char *c;

		Com_Debug("Fs_Init: Resolved executable path: %s\n", path);
#ifdef __APPLE__
		if ((c = strstr(path, "Quake2World.app"))) {
			strcpy(c + strlen("Quake2World.app/Contents/"), "MacOS/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);

			strcpy(c + strlen("Quake2World.app/Contents/"), "Resources/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);
		}
#elif __LINUX__
		if ((c = strstr(path, "quake2world/bin"))) {
			strcpy(c + strlen("quake2world/"), "lib/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);

			strcpy(c + strlen("quake2world/"), "share/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);
		}
#endif
	}
#endif

	// add default to search path
	Fs_AddToSearchPath(PKGLIBDIR"/"DEFAULT_GAME);
	Fs_AddToSearchPath(PKGDATADIR"/"DEFAULT_GAME);

	// then add a '.quake2world/default' directory in home directory
	Fs_AddUserSearchPath(DEFAULT_GAME);

	// these paths will be retained across all game modules
	fs_state.base_search_paths = PHYSFS_getSearchPath();

#ifdef FS_LOAD_DEBUG
	fs_state.loaded_files = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, Z_Free);
#endif
}

#ifdef FS_LOAD_DEBUG
/*
 * @brief Prints the names of loaded (i.e. yet-to-be-freed) files.
 */
static void Fs_LoadedFiles_(gpointer key, gpointer value, gpointer data __attribute__((unused))) {
	Com_Print("Fs_PrintLoadedFiles: %s @ %p\n", (char *)value, key);
}
#endif

/*
 * @brief Shuts down the filesystem.
 */
void Fs_Shutdown(void) {

#ifdef FS_LOAD_DEBUG
	g_hash_table_foreach(fs_state.loaded_files, Fs_LoadedFiles_, NULL);
	g_hash_table_destroy(fs_state.loaded_files);
#endif

	PHYSFS_freeList(fs_state.base_search_paths);

	PHYSFS_deinit();
}
