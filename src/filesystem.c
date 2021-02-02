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

typedef struct {
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
	 * @brief The binaries directory.
	 */
	char bin_dir[MAX_OS_PATH];

	/**
	 * @brief The shared libraries directory.
	 */
	char lib_dir[MAX_OS_PATH];

	/**
	 * @brief The data directory.
	 */
	char data_dir[MAX_OS_PATH];

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
 * @return The binaries directory.
 */
const char *Fs_BinDir(void) {
	return fs_state.bin_dir;
}

/**
 * @return The shared libraries directory.
 */
const char *Fs_LibDir(void) {
	return fs_state.lib_dir;
}

/**
 * @return The data directory.
 */
const char *Fs_DataDir(void) {
	return fs_state.data_dir;
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

	if (PHYSFS_isInit() == 0) {
		return NULL;
	}

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
_Bool Fs_Seek(file_t *file, int64_t offset) {
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
 * @return The number of objects written, or -1 on failure.
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

			typedef struct {
				byte *data;
				int64_t len;
			} fs_chunk_t;

			while (!Fs_Eof(file)) {
				fs_chunk_t *chunk = Mem_TagMalloc(sizeof(fs_chunk_t), MEM_TAG_FS);

				chunk->data = Mem_LinkMalloc(FS_FILE_BUFFER, chunk);
				chunk->len = Fs_Read(file, chunk->data, 1, FS_FILE_BUFFER);

				if (chunk->len == -1) {
					Com_Error(ERROR_DROP, "%s: %s\n", filename, Fs_LastError());
				}

				list = g_list_append(list, chunk);
				len += chunk->len;
			}

			if (buffer) {
				if (len > 0) {
					byte *buf = *buffer = Mem_TagMalloc(len + 1, MEM_TAG_FS);

					GList *e = list;
					while (e) {
						fs_chunk_t *b = (fs_chunk_t *) e->data;

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

/**
 * @brief Fs_Enumerate context.
 */
typedef struct {
	char dir[MAX_QPATH];
	const char *pattern;
	Fs_Enumerator function;
	void *data;
} fs_enumerate_t;

/**
 * @brief PHYSFS_EnumerateCallback for Fs_Enumerate.
 */
static int32_t Fs_Enumerate_(void *data, const char *dir, const char *filename) {
	const fs_enumerate_t *enumerator = data;

	char path[MAX_QPATH];
	g_snprintf(path, sizeof(path), "%s%s", dir, filename);

	if (GlobMatch(enumerator->pattern, path, GLOB_FLAGS_NONE)) {
		enumerator->function(path, enumerator->data);
	}

	return 1;
}

/**
 * @brief Enumerates files matching `pattern`, calling the given function.
 */
void Fs_Enumerate(const char *pattern, Fs_Enumerator func, void *data) {

	fs_enumerate_t enumerator = {
		.pattern = pattern,
		.function = func,
		.data = data,
	};

	if (strchr(pattern, '/')) {
		Dirname(pattern, enumerator.dir);
	} else {
		g_strlcpy(enumerator.dir, "/", sizeof(enumerator.dir));
	}

	PHYSFS_enumerate(enumerator.dir, Fs_Enumerate_, &enumerator);
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
 * @brief Concatenates the NULL-terminated list of path components and adds the
 * resulting path to the search path.
 */
void Fs_AddToSearchPath(const char *path) {

	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		Com_Print("Adding path %s..\n", path);

		const _Bool is_dir = g_file_test(path, G_FILE_TEST_IS_DIR);

		if (PHYSFS_mount(path, NULL, !is_dir) == 0) {
			Com_Warn("%s: %s\n", path, Fs_LastError());
			return;
		}

		if ((fs_state.flags & FS_AUTO_LOAD_ARCHIVES) && is_dir) {
			Fs_Enumerate("*.pk3", Fs_AddToSearchPath_enumerate, (void *) path);
		}
	} else {
		Com_Debug(DEBUG_FILESYSTEM, "Failed to stat %s\n", path);
	}
}

/**
 * @brief Variadic arguments version of Fs_AddToSearchPath.
 */
void Fs_AddToSearchPathv(const char *dir, ...) {
	char path[MAX_OS_PATH] = "";

	va_list args;
	va_start(args, dir);

	while (dir) {
		g_strlcat(path, dir, sizeof(path));

		dir = va_arg(args, const char *);
		if (dir) {
			g_strlcat(path, G_DIR_SEPARATOR_S, sizeof(path));
		}
	}

	va_end(args);

	Fs_AddToSearchPath(path);
}

/**
 * @brief Enumeration helper for Fs_AddToSearchPath. Adds all archive files for
 * the newly added filesystem mount point.
 */
static void Fs_AddToSearchPath_enumerate(const char *path, void *data) {

	const char *real_dir = Fs_RealDir(path);
	const char *enum_dir = data;

	if (!g_strcmp0(real_dir, enum_dir)) {
		Fs_AddToSearchPathv(real_dir, path + 1, NULL);
	}
}

/**
 * @brief Adds the user-specific search path, setting the write dir in the
 * process. This is where all files produced by the game are written to.
 */
static void Fs_AddUserSearchPath(const char *dir) {

	gchar *path = g_build_path(G_DIR_SEPARATOR_S, Sys_UserDir(), dir, NULL);

	if (g_mkdir_with_parents(path, 0755)) {
		Com_Warn("Failed to create %s\n", path);
		return;
	}

	Fs_AddToSearchPath(path);
	Fs_SetWriteDir(path);

	g_free(path);
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
	Fs_AddToSearchPathv(fs_state.lib_dir, dir, NULL);
	Fs_AddToSearchPathv(fs_state.data_dir, dir, NULL);

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

	PHYSFS_Version physfs_version;
	PHYSFS_getLinkedVersion(&physfs_version);

	Com_Debug(DEBUG_FILESYSTEM, "Initializing PhysFS %i.%i.%i...\n",
			  physfs_version.major, physfs_version.minor, physfs_version.patch);

	if (PHYSFS_init(Com_Argv(0)) == 0) {
		Com_Error(ERROR_FATAL, "%s\n", Fs_LastError());
	}

	fs_state.flags = flags;

	PHYSFS_permitSymbolicLinks(true);

	g_strlcpy(fs_state.bin_dir, BINDIR, MAX_OS_PATH);
	g_strlcpy(fs_state.lib_dir, PKGLIBDIR, MAX_OS_PATH);
	g_strlcpy(fs_state.data_dir, PKGDATADIR, MAX_OS_PATH);

	const char *path = Sys_ExecutablePath();
	if (path) {
		char *c;

		Com_Debug(DEBUG_FILESYSTEM, "Resolved executable path: %s\n", path);

#if defined(__APPLE__)
		if ((c = strstr(path, "Quetoo.app"))) {
			*(c + strlen("Quetoo.app")) = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			g_snprintf(fs_state.bin_dir, MAX_OS_PATH, "%s/Contents/MacOS", fs_state.base_dir);
			g_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s/Contents/MacOS/lib", fs_state.base_dir);
			g_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s/Contents/Resources", fs_state.base_dir);
		}
#elif defined(__linux__)
		if ((c = strstr(path, "quetoo/bin"))) {
			*(c + strlen("quetoo")) = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			g_snprintf(fs_state.bin_dir, MAX_OS_PATH, "%s/bin", fs_state.base_dir);
			g_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s/lib", fs_state.base_dir);
			g_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s/share", fs_state.base_dir);
		}
#elif defined(_WIN32)
		if ((c = strstr(path, "\\bin\\"))) {
			*c = '\0';
			g_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

			g_snprintf(fs_state.bin_dir, MAX_OS_PATH, "%s\\bin", fs_state.base_dir);
			g_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s\\lib", fs_state.base_dir);
			g_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s\\share", fs_state.base_dir);
		}
#endif
	}

	Fs_AddToSearchPathv(fs_state.lib_dir, NULL);
	Fs_AddToSearchPathv(fs_state.data_dir, NULL);

	Fs_AddToSearchPathv(fs_state.lib_dir, DEFAULT_GAME, NULL);
	Fs_AddToSearchPathv(fs_state.data_dir, DEFAULT_GAME, NULL);

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

	if (PHYSFS_isInit() == 0) {
		return;
	}

	g_hash_table_foreach(fs_state.loaded_files, Fs_LoadedFiles_, NULL);
	g_hash_table_destroy(fs_state.loaded_files);

	PHYSFS_freeList(fs_state.base_search_paths);

	PHYSFS_deinit();
}
