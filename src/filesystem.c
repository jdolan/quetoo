/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include <physfs.h>

#include "filesystem.h"

#define FS_FILE_BUFFER (1024 * 1024 * 2)

typedef struct fs_state_s {

	/**
	 * @brief The FS_* flags.
	 */
	uint32_t flags;

	/**
	 * @brief The base directory of the install, if running from a bundled
	 * application. On Windows, this will always be set. On Mac and Linux, it
	 * is set for the .app and .tgz distributables.
	 */
	char base_dir[MAX_OS_PATH];

	/**
	 * @brief The base search paths (all those present after invoking Fs_Init).
	 * When calling Fs_SetGameDir, all paths following the base paths are
	 * unloaded.
	 */
	char **base_search_paths;

	/**
	 * @brief For debugging purposes, track all loaded files to ensure that
	 * they are freed (Fs_Free) in all code paths.
	 */
	GHashTable *loaded_files;
} fs_state_t;

static fs_state_t fs_state;

/**
 * @return The base directory, if running from a bundled application.
 */
const char *Fs_BaseDir(void) {
	return fs_state.base_dir;
}

/**
 * @brief Closes the file.
 *
 * @return True on successful flush and close, false otherwise.
 */
_Bool Fs_Close(file_t *file) {
	return PHYSFS_close((PHYSFS_File *) file) ? true : false;
}

/**
 * @brief Deletes the file from the configured write directory.
 */
_Bool Fs_Delete(const char *filename) {
	return PHYSFS_delete(filename) == 0;
}

/**
 * @return True if the end of the file has been reached, false otherwise.
 */
_Bool Fs_Eof(file_t *file) {
	return PHYSFS_eof((PHYSFS_File *) file) ? true : false;
}

/**
 * @return True if the specified filename exists on the search path.
 */
_Bool Fs_Exists(const char *filename) {
	return PHYSFS_exists(filename) ? true : false;
}

/**
 * @return True if the file flushed successfully, false otherwise.
 */
_Bool Fs_Flush(file_t *file) {
	return PHYSFS_flush((PHYSFS_File *) file) ? true : false;
}

/**
 * @return The last error message resulting from filesystem operations.
 */
const char *Fs_LastError(void) {
	return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

/**
 * @brief Creates the specified directory (and any ancestors) in Fs_WriteDir.
 */
_Bool Fs_Mkdir(const char *dir) {
	return PHYSFS_mkdir(dir) ? true : false;
}

/**
 * @brief Opens the specified file for appending.
 */
file_t *Fs_OpenAppend(const char *filename) {
	char dir[MAX_QPATH];
	PHYSFS_File *file;

	Dirname(filename, dir);
	Fs_Mkdir(dir);

	if ((file = PHYSFS_openAppend(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("%s: %s\n", filename, Fs_LastError());
		}
	}

	return (file_t *) file;
}

/**
 * @brief Opens the specified file for reading.
 */
file_t *Fs_OpenRead(const char *filename) {
	PHYSFS_File *file;

	if ((file = PHYSFS_openRead(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("%s: %s\n", filename, Fs_LastError());
		}
	}

	return (file_t *) file;
}

/**
 * @brief Opens the specified file for writing.
 */
file_t *Fs_OpenWrite(const char *filename) {
	char dir[MAX_QPATH];
	PHYSFS_File *file;

	Dirname(filename, dir);
	Fs_Mkdir(dir);

	if ((file = PHYSFS_openWrite(filename))) {
		if (!PHYSFS_setBuffer(file, FS_FILE_BUFFER)) {
			Com_Warn("%s: %s\n", filename, Fs_LastError());
		}
	}

	return (file_t *) file;
}

/**
 * @brief Prints the specified formatted string to the given file.
 *
 * @return The number of characters written, or -1 on failure.
 */
int64_t Fs_Print(file_t *file, const char *fmt, ...) {
	static char string[MAX_PRINT_MSG];
	va_list args;

	va_start(args, fmt);
	vsnprintf(string, sizeof(string), fmt, args);
	va_end(args);

	return Fs_Write(file, string, 1, strlen(string));
}

/**
 * @brief Reads from the specified file.
 *
 * @return The number of objects read, or -1 on failure.
 */
int64_t Fs_Read(file_t *file, void *buffer, size_t size, size_t count) {
	return PHYSFS_readBytes((PHYSFS_File *) file, buffer, (PHYSFS_uint64) size * (PHYSFS_uint64) count) / size;
}

/**
 * @brief Reads a line from the specified file. The newline character is
 * omitted from the returned, null-terminated string.
 *
 * @return True on success, false on failures.
 */
_Bool Fs_ReadLine(file_t *file, char *buffer, size_t len) {
	size_t i;
	char *c;

	for (i = 0, c = buffer; i < len - 1; i++, c++) {

		if (Fs_Read(file, c, 1, 1) != 1) {
			break;
		}

		if (*c == '\n') {
			i++;
			break;
		}
	}

	*c = '\0';
	return i ? true : false;
}

/**
 * @brief Seeks to the specified offset.
 */
_Bool Fs_Seek(file_t *file, size_t offset) {
	return PHYSFS_seek((PHYSFS_File *) file, offset) ? true : false;
}

/**
 * @brief Get the length of a file in bytes
 */
int64_t Fs_FileLength(file_t *file) {
	return PHYSFS_fileLength((PHYSFS_File *) file);
}

/**
 * @return The current file offset.
 */
int64_t Fs_Tell(file_t *file) {
	return PHYSFS_tell((PHYSFS_File *) file);
}

/**
 * @brief Writes to the specified file.
 *
 * @return The number of objects read, or -1 on failure.
 */
int64_t Fs_Write(file_t *file, const void *buffer, size_t size, size_t count) {
	return PHYSFS_writeBytes((PHYSFS_File *) file, buffer, (PHYSFS_uint64) size * (PHYSFS_uint64) count) / size;
}

/**
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
		const int64_t buffer_length = Fs_FileLength(file);

		// if we can calculate the length, we can pull it easily
		if (buffer_length != -1) {

			len = buffer_length;

			if (buffer) {
				if (len > 0) {
					byte *buf = *buffer = Mem_TagMalloc(len + 1, MEM_TAG_FS);
					const int64_t read = Fs_Read(file, buf, 1, len);

					if (read != len) {
						Com_Error(ERROR_DROP, "%s: %s\n", filename, Fs_LastError());
					}

					g_hash_table_insert(fs_state.loaded_files, *buffer,
										(gpointer) Mem_CopyString(filename));
				} else {

					*buffer = NULL;
				}
			}
		} else {

			GList *list = NULL;
			len = 0;

			while (!Fs_Eof(file)) {
				fs_block_t *b = Mem_TagMalloc(sizeof(fs_block_t), MEM_TAG_FS);

				b->data = Mem_LinkMalloc(FS_FILE_BUFFER, b);
				b->len = Fs_Read(file, b->data, 1, FS_FILE_BUFFER);

				if (b->len == -1) {
					Com_Error(ERROR_DROP, "%s: %s\n", filename, Fs_LastError());
				}

				list = g_list_append(list, b);
				len += b->len;
			}

			if (buffer) {
				if (len > 0) {
					byte *buf = *buffer = Mem_TagMalloc(len + 1, MEM_TAG_FS);

					GList *e = list;
					while (e) {
						fs_block_t *b = (fs_block_t *) e->data;

						memcpy(buf, b->data, b->len);
						buf += (ptrdiff_t) b->len;

						e = e->next;
					}

					g_hash_table_insert(fs_state.loaded_files, *buffer,
										(gpointer) Mem_CopyString(filename));
				} else {

					*buffer = NULL;
				}
			}

			g_list_free_full(list, Mem_Free);
		}

		Fs_Close(file);
	} else {

		len = -1;

		if (buffer) {
			*buffer = NULL;
		}
	}

	return len;
}

/**
 * @brief Frees the specified buffer allocated by Fs_LoadFile.
 */
void Fs_Free(void *buffer) {

	if (buffer) {
		if (!g_hash_table_remove(fs_state.loaded_files, buffer)) {
			Com_Warn("Invalid buffer\n");
		}
		Mem_Free(buffer);
	}
}

/**
 * @brief Renames the specified source to the given destination.
 */
_Bool Fs_Rename(const char *source, const char *dest) {
	const char *dir = Fs_WriteDir();

	const char *src = va("%s"G_DIR_SEPARATOR_S"%s", dir, source);
	const char *dst = va("%s"G_DIR_SEPARATOR_S"%s", dir, dest);

	return rename(src, dst) == 0;
}

/**
 * @brief Fetch the "last modified" time for the specified file.
 */
int64_t Fs_LastModTime(const char *filename) {
	PHYSFS_Stat stat;
	PHYSFS_stat(filename, &stat);
	return stat.modtime;
}


/**
 * @brief Unlinks (deletes) the specified file.
 */
_Bool Fs_Unlink(const char *filename) {

	if (!g_strcmp0(Fs_WriteDir(), Fs_RealDir(filename))) {
		return unlink(filename) == 0;
	}

	return false;
}

typedef struct {
	char dir[MAX_QPATH];
	const char *pattern;
	Fs_Enumerator function;
	void *data;
} fs_enumerate_t;

/**
 * @brief Enumeration helper for Fs_Enumerate.
 */
static int32_t Fs_Enumerate_(void *data, const char *dir, const char *filename) {
	char path[MAX_QPATH];
	const fs_enumerate_t *en = data;

	g_snprintf(path, sizeof(path), "%s%s", dir, filename);

	if (GlobMatch(en->pattern, path, GLOB_FLAGS_NONE)) {
		en->function(path, en->data);
	}

	return 1;
}

/**
 * @brief Enumerates files matching `pattern`, calling the given function.
 */
void Fs_Enumerate(const char *pattern, Fs_Enumerator func, void *data) {
	fs_enumerate_t en = {
		.pattern = pattern,
		.function = func,
		.data = data,
	};

	if (strchr(pattern, '/')) {
		Dirname(pattern, en.dir);
	} else {
		g_strlcpy(en.dir, "/", sizeof(en.dir));
	}

	PHYSFS_enumerate(en.dir, Fs_Enumerate_, &en);
}

/**
 * @brief OS compare, wee.
 */
static int32_t Fs_CompleteFile_compare(const void *a, const void *b) {
	const com_autocomplete_match_t *ma = (const com_autocomplete_match_t *) a;
	const com_autocomplete_match_t *mb = (const com_autocomplete_match_t *) b;

#if defined(WIN32)
	return stricmp(ma->name, mb->name);
#else
	return g_strcmp0(ma->name, mb->name);
#endif
}

/**
 * @brief GHFunc for Fs_CompleteFile.
 */
static void Fs_CompleteFile_enumerate(const char *path, void *data) {
	GList **matches = (GList **) data;
	char match[MAX_OS_PATH];

	StripExtension(Basename(path), match);
	com_autocomplete_match_t temp_match = {
		.name = match,
		.description = NULL
	};

	if (!g_list_find_custom(*matches, &temp_match, Fs_CompleteFile_compare)) {
		*matches = g_list_insert_sorted(*matches, Com_AllocMatch(match, NULL), Fs_CompleteFile_compare);
	}
}

/**
 * @brief Console completion for file names.
 */
void Fs_CompleteFile(const char *pattern, GList **matches) {

	Fs_Enumerate(pattern, Fs_CompleteFile_enumerate, (void *) matches);
}

static void Fs_AddToSearchPath_enumerate(const char *path, void *data);

/**
 * @brief Adds the directory to the search path, conditionally loading all
 * archives within it.
 */
void Fs_AddToSearchPath(const char *dir) {

	if (g_file_test(dir, G_FILE_TEST_EXISTS)) {
		Com_Print("Adding path %s..\n", dir);

		const _Bool is_dir = g_file_test(dir, G_FILE_TEST_IS_DIR);

		if (PHYSFS_mount(dir, NULL, !is_dir) == 0) {
			Com_Warn("%s: %s\n", dir, Fs_LastError());
			return;
		}

		if ((fs_state.flags & FS_AUTO_LOAD_ARCHIVES) && is_dir) {
			Fs_Enumerate("*.pak", Fs_AddToSearchPath_enumerate, (void *) dir);
			Fs_Enumerate("*.pk3", Fs_AddToSearchPath_enumerate, (void *) dir);
		}
	} else {
		Com_Debug(DEBUG_FILESYSTEM, "Failed to stat %s\n", dir);
	}
}

/**
 * @brief Enumeration helper for Fs_AddToSearchPath. Adds all archive files for
 * the newly added filesystem mount point.
 */
static void Fs_AddToSearchPath_enumerate(const char *path, void *data) {
	const char *dir = (const char *) data;

	if (!g_strcmp0(Fs_RealDir(path), dir)) {
		Fs_AddToSearchPath(va("%s%s", dir, path));
	}
}

/**
 * @brief Adds the user-specific search path, setting the write dir in the
 * process. This is where all files produced by the game are written to.
 */
static void Fs_AddUserSearchPath(const char *dir) {
	char path[MAX_OS_PATH];

	g_snprintf(path, sizeof(path), "%s"G_DIR_SEPARATOR_S"%s", Sys_UserDir(), dir);

	if (g_mkdir_with_parents(path, 0755)) {
		Com_Warn("Failed to create %s\n", path);
		return;
	}

	Fs_AddToSearchPath(path);

	Fs_SetWriteDir(path);
}

/**
 * @brief Sets the game path to a relative directory.
 */
void Fs_SetGame(const char *dir) {

	if (!dir || !*dir) {
		Com_Warn("Missing game name\n");
		return;
	}

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":")) {
		Com_Warn("Game should be a directory name, not a path (%s)\n", dir);
		return;
	}

	Com_Debug(DEBUG_FILESYSTEM, "Setting game: %s\n", dir);

	// iterate the current search path, removing those which are not base paths
	char **paths = PHYSFS_getSearchPath();
	char **path = paths;
	while (*path != NULL) {
		char **p = fs_state.base_search_paths;
		while (*p != NULL) {
			if (!g_strcmp0(*path, *p)) {
				break;
			}
			p++;
		}
		if (!*p) {
			Com_Debug(DEBUG_FILESYSTEM, "Removing %s\n", *path);
			if (PHYSFS_unmount(*path) == 0) {
				Com_Warn("%s: %s\n", *path, Fs_LastError());
				return;
			}
		}
		path++;
	}

	PHYSFS_freeList(paths);

	// now add new entries for the new game
	Fs_AddToSearchPath(va(PKGLIBDIR G_DIR_SEPARATOR_S "%s", dir));
	Fs_AddToSearchPath(va(PKGDATADIR G_DIR_SEPARATOR_S "%s", dir));

	Fs_AddUserSearchPath(dir);
}

/**
 * @brief Sets the [user-specific] target directory for writing files.
 */
void Fs_SetWriteDir(const char *dir) {

	if (g_file_test(dir, G_FILE_TEST_EXISTS)) {
		if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
			Com_Warn("%s exists but is not a directory\n", dir);
			return;
		}
	} else {
		Com_Warn("%s does not exist\n", dir);
		return;
	}

	if (PHYSFS_setWriteDir(dir)) {
		Com_Print("Using %s for writing.\n", dir);
	} else {
		Com_Warn("Failed to set: %s\n", dir);
	}
}

/**
 * @brief Called to find where to write a file (demos, screenshots, etc)
 */
const char *Fs_WriteDir(void) {
	return PHYSFS_getWriteDir();
}

/**
 * @brief Returns the real directory name of the specified file.
 */
const char *Fs_RealDir(const char *filename) {
	return PHYSFS_getRealDir(filename);
}

/**
 * @return The real path name of the specified file or directory.
 */
const char *Fs_RealPath(const char *path) {
	static char real_path[MAX_OS_PATH];

	g_snprintf(real_path, sizeof(real_path), "%s%s", Fs_WriteDir(), G_DIR_SEPARATOR_S);

	const char *in = path;
	char *out = real_path + strlen(real_path);

	while (*in && (size_t) (out - real_path) < (sizeof(real_path) - 1)) {
		if (*in == '/') {
			*out = G_DIR_SEPARATOR;
		} else {
			*out = *in;
		}
		out++;
		in++;
	}
	*out = '\0';

	return real_path;
}

/**
 * @brief Initializes the file subsystem.
 */
void Fs_Init(const uint32_t flags) {

	memset(&fs_state, 0, sizeof(fs_state_t));

	if (PHYSFS_init(Com_Argv(0)) == 0) {
		Com_Error(ERROR_FATAL, "%s\n", Fs_LastError());
	}

	fs_state.flags = flags;

	PHYSFS_permitSymbolicLinks(true);

	const char *path = Sys_ExecutablePath();
	if (path) {
		char *c;

		Com_Debug(DEBUG_FILESYSTEM, "Resolved executable path: %s\n", path);

#if defined(__APPLE__)
		if ((c = strstr(path, "Quetoo.app"))) {
			*(c + strlen("Quetoo.app")) = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			strcpy(c + strlen("Quetoo.app"), "/Contents/MacOS/lib/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);

			strcpy(c + strlen("Quetoo.app"), "/Contents/Resources/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);
		}
#elif defined(__linux__)
		if ((c = strstr(path, "quetoo/bin"))) {
			*(c + strlen("quetoo")) = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			strcpy(c + strlen("quetoo"), "/lib/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);

			strcpy(c + strlen("quetoo"), "/share/"DEFAULT_GAME);
			Fs_AddToSearchPath(path);
		}
#elif defined(_WIN32)
		if ((c = strstr(path, "\\bin\\"))) {
			*c = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			strcpy(c, "\\lib\\"DEFAULT_GAME);
			Fs_AddToSearchPath(path);

			strcpy(c, "\\share\\"DEFAULT_GAME);
			Fs_AddToSearchPath(path);
		}
#endif
	}

	// if the base directory was not resolved, add the default search paths
	if (strlen(fs_state.base_dir)) {
		Com_Debug(DEBUG_FILESYSTEM, "Resolved base dir: %s\n", fs_state.base_dir);
	} else { // trailing slash is added to "fix" links on Linux, possibly causes issues on other platforms?
		Fs_AddToSearchPath(PKGLIBDIR G_DIR_SEPARATOR_S DEFAULT_GAME G_DIR_SEPARATOR_S);
		Fs_AddToSearchPath(PKGDATADIR G_DIR_SEPARATOR_S DEFAULT_GAME G_DIR_SEPARATOR_S);
	}

	// then add the game directory in the user's home directory
	Fs_AddUserSearchPath(DEFAULT_GAME);

	// finally add any paths specified on the command line
	int32_t i;
	for (i = 1; i < Com_Argc(); i++) {

		if (!g_strcmp0(Com_Argv(i), "-p") || !g_strcmp0(Com_Argv(i), "-path")) {
			Fs_AddToSearchPath(Com_Argv(i + 1));
			continue;
		}

		if (!g_strcmp0(Com_Argv(i), "-w") || !g_strcmp0(Com_Argv(i), "-wpath")) {
			Fs_AddToSearchPath(Com_Argv(i + 1));
			Fs_SetWriteDir(Com_Argv(i + 1));
			continue;
		}
	}

	// these paths will be retained across all game modules
	fs_state.base_search_paths = PHYSFS_getSearchPath();

	fs_state.loaded_files = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, Mem_Free);
}

/**
 * @brief Prints the names of loaded (i.e. yet-to-be-freed) files.
 */
static void Fs_LoadedFiles_(gpointer key, gpointer value, gpointer data) {
	Com_Print("Fs_PrintLoadedFiles: %s @ %p\n", (char *) value, key);
}

/**
 * @brief Shuts down the filesystem.
 */
void Fs_Shutdown(void) {

	g_hash_table_foreach(fs_state.loaded_files, Fs_LoadedFiles_, NULL);
	g_hash_table_destroy(fs_state.loaded_files);

	PHYSFS_freeList(fs_state.base_search_paths);

	PHYSFS_deinit();
}
