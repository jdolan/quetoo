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
#include <SDL3/SDL.h>

#include <Objectively/HashTable.h>
#include <Objectively/List.h>
#include <Objectively/Vector.h>

#include "console.h"
#include "filesystem.h"

#define FS_FILE_BUFFER (1024 * 1024 * 2)

#define MAX_COMMAND_LINE_PATHS 8

typedef struct {

  /**
   * @brief The `FS_`* flags.
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
   * @brief The bundled resources directory, or empty if not running from a bundle.
   */
  char resources_dir[MAX_OS_PATH];

  /**
   * @brief The base search paths (all those present after invoking `Fs_Init`).
   * When calling `Fs_SetGameDir`, all paths following the base paths are
   * unloaded.
   */
  char **base_search_paths;

  /**
   * @brief Search paths given on the command line with `-p` or `--path`.
   * @details These are roots that may contain game directories, exactly like
   * the install directories, so that a development tree laid out the same way
   * resolves modules and assets the same way.
   */
  char command_line_paths[MAX_COMMAND_LINE_PATHS][MAX_OS_PATH];
  size_t num_command_line_paths;

  /**
   * @brief An explicit write directory given on the command line with
   * `-w` or `--wpath`, overriding the per-game user directory that
   * `Fs_SetGame` would otherwise select. Empty if none was given.
   * @remarks Deliberately not one of `command_line_paths`: a root resolves
   * modules, and this is where server-named downloads are written.
   */
  char write_dir_override[MAX_OS_PATH];

  /**
   * @brief For debugging purposes, track all loaded files to ensure that
   * they are freed (`Fs_Free`) in all code paths.
   */
  HashTable *loaded_files;
} fs_state_t;

static fs_state_t fs_state;

/**
 * @brief Adds a command line search path, remembering it as a root so that
 * `Fs_SetGame` can resolve game directories beneath it.
 */
static void Fs_AddCommandLinePath(const char *path) {

  Fs_AddToSearchPath(path);

  if (fs_state.num_command_line_paths == MAX_COMMAND_LINE_PATHS) {
    Com_Warn("Ignoring %s; only %d command line paths are supported\n", path, MAX_COMMAND_LINE_PATHS);
    return;
  }

  q_strlcpy(fs_state.command_line_paths[fs_state.num_command_line_paths++], path, MAX_OS_PATH);
}

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
bool Fs_Close(file_t *file) {
  return PHYSFS_close((PHYSFS_File *) file) ? true : false;
}

/**
 * @brief Deletes the file from the configured write directory.
 */
bool Fs_Delete(const char *filename) {
  return PHYSFS_delete(filename) == 0;
}

/**
 * @return True if the end of the file has been reached, false otherwise.
 */
bool Fs_Eof(file_t *file) {
  return PHYSFS_eof((PHYSFS_File *) file) ? true : false;
}

/**
 * @return True if the specified filename exists on the search path.
 */
bool Fs_Exists(const char *filename) {
  return PHYSFS_exists(filename) ? true : false;
}

/**
 * @return True if the file flushed successfully, false otherwise.
 */
bool Fs_Flush(file_t *file) {
  return PHYSFS_flush((PHYSFS_File *) file) ? true : false;
}

/**
 * @return The last error message resulting from filesystem operations.
 */
const char *Fs_LastError(void) {
  return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

/**
 * @brief Creates the specified directory (and any ancestors) in `Fs_WriteDir`.
 */
bool Fs_Mkdir(const char *dir) {
  return PHYSFS_mkdir(dir) ? true : false;
}

/**
 * @brief Opens the specified file for appending.
 */
file_t *Fs_OpenAppend(const char *filename) {
  char dir[MAX_OS_PATH];
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
  char dir[MAX_OS_PATH];
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

  return Fs_Write(file, string, 1, q_strlen(string));
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
bool Fs_ReadLine(file_t *file, char *buffer, size_t len) {
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
bool Fs_Seek(file_t *file, int64_t offset) {
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
 * allocated if non-`NULL`. Returns the file length, or -1 if it is unable to be
 * read. Be sure to free the buffer when finished with `Fs_Free`.
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

          $(fs_state.loaded_files, set, *buffer,
                    (void *) Mem_CopyString(filename));
        } else {
          *buffer = NULL;
        }
      }
    } else {

      List *list = $(alloc(List), init);
      list->destroy = Mem_Free;
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

        $(list, append, chunk);
        len += chunk->len;
      }

      if (buffer) {
        if (len > 0) {
          byte *buf = *buffer = Mem_TagMalloc(len + 1, MEM_TAG_FS);

          const ListNode *e = list->head;
          while (e) {
            fs_chunk_t *b = e->element;

            memcpy(buf, b->data, b->len);
            buf += (ptrdiff_t) b->len;

            e = e->next;
          }

          $(fs_state.loaded_files, set, *buffer,
                    (void *) Mem_CopyString(filename));
        } else {

          *buffer = NULL;
        }
      }

      release(list);
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
 * @brief Frees the specified buffer allocated by `Fs_LoadFile`.
 */
void Fs_Free(void *buffer) {

  if (buffer) {
    if (!$(fs_state.loaded_files, get, buffer)) {
      Com_Warn("Invalid buffer\n");
    } else {
      $(fs_state.loaded_files, remove, buffer);
    }
    Mem_Free(buffer);
  }
}

/**
 * @brief Renames the specified source to the given destination.
 */
bool Fs_Rename(const char *source, const char *dest) {
  const char *dir = Fs_WriteDir();

  const char *src = va("%s/%s", dir, source);
  const char *dst = va("%s/%s", dir, dest);

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
bool Fs_Unlink(const char *filename) {

  if (!q_strcmp(Fs_WriteDir(), Fs_RealDir(filename))) {
    return unlink(filename) == 0;
  }

  return false;
}

/**
 * @brief `Fs_Enumerate` context.
 */
typedef struct {
  char dir[MAX_QPATH];
  const char *pattern;
  Fs_Enumerator function;
  void *data;
} fs_enumerate_t;

/**
 * @brief `PHYSFS_EnumerateCallback` for `Fs_Enumerate`.
 */
static int32_t Fs_Enumerate_(void *data, const char *dir, const char *filename) {
  const fs_enumerate_t *enumerator = data;

  char path[MAX_QPATH];
  q_snprintf(path, sizeof(path), "%s%s", dir, filename);

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

  if (q_strchr(pattern, '/')) {
    Dirname(pattern, enumerator.dir);
  } else {
    q_strlcpy(enumerator.dir, "/", sizeof(enumerator.dir));
  }

  PHYSFS_enumerate(enumerator.dir, Fs_Enumerate_, &enumerator);
}

static void Fs_CompleteFile_enumerate(const char *path, void *data) {
  List *matches = data;

  char name[MAX_OS_PATH];
  StripExtension(Basename(path), name);

  Con_AutocompleteMatch(matches, name, NULL);
}

/**
 * @brief Console completion for file names.
 */
void Fs_CompleteFile(const char *pattern, List *matches) {
  Fs_Enumerate(pattern, Fs_CompleteFile_enumerate, matches);
}

/**
 * @return True if `path` is itself one of the roots that hold game directories.
 */
static bool Fs_IsRoot(const char *path) {

  for (size_t p = 0; p < fs_state.num_command_line_paths; p++) {
    if (!q_strcmp(path, fs_state.command_line_paths[p])) {
      return true;
    }
  }

  if (!q_strcmp(path, fs_state.lib_dir)) {
    return true;
  }
  
  if (!q_strcmp(path, fs_state.data_dir)) {
    return true;
  }
  
  if (*fs_state.resources_dir && !q_strcmp(path, fs_state.resources_dir)) {
    return true;
  }
  
  return false;
}

/**
 * @brief Adds every game directory matching `pattern` beneath `root` to `matches`.
 */
static void Fs_CompleteGame_root(const char *root, const char *pattern, List *matches) {

  int32_t count;
  char **games = SDL_GlobDirectory(root, pattern, SDL_GLOB_CASEINSENSITIVE, &count);
  if (!games) {
    return;
  }

  for (int32_t i = 0; i < count; i++) {
    const char *path = va("%s/%s", root, games[i]);

    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY && !Fs_IsRoot(path)) {
      Con_AutocompleteMatch(matches, games[i], NULL);
    }
  }

  SDL_free(games);
}

/**
 * @brief Console completion for game names.
 */
void Fs_CompleteGame(const char *pattern, List *matches) {

  for (size_t p = 0; p < fs_state.num_command_line_paths; p++) {
    Fs_CompleteGame_root(fs_state.command_line_paths[p], pattern, matches);
  }

  Fs_CompleteGame_root(fs_state.lib_dir, pattern, matches);
  Fs_CompleteGame_root(Sys_UserDir(), pattern, matches);
  Fs_CompleteGame_root(fs_state.data_dir, pattern, matches);

  if (*fs_state.resources_dir) {
    Fs_CompleteGame_root(fs_state.resources_dir, pattern, matches);
  }
}

static void Fs_AddToSearchPath_enumerate(const char *path, void *data);

/**
 * @brief Concatenates the `NULL`-terminated list of path components and adds the
 * resulting path to the search path.
 */
void Fs_AddToSearchPath(const char *path) {

  SDL_PathInfo info;
  if (SDL_GetPathInfo(path, &info)) {
    Com_Print("Adding path %s..\n", path);

    const bool is_dir = (info.type == SDL_PATHTYPE_DIRECTORY);

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
 * @brief Variadic arguments version of `Fs_AddToSearchPath`.
 */
void Fs_AddToSearchPathv(const char *dir, ...) {
  char path[MAX_OS_PATH] = "";

  va_list args;
  va_start(args, dir);

  while (dir) {
    q_strlcat(path, dir, sizeof(path));

    dir = va_arg(args, const char *);
    if (dir) {
      q_strlcat(path, "/", sizeof(path));
    }
  }

  va_end(args);

  Fs_AddToSearchPath(path);
}

/**
 * @brief Enumeration helper for `Fs_AddToSearchPath`. Adds all archive files for
 * the newly added filesystem mount point.
 */
static void Fs_AddToSearchPath_enumerate(const char *path, void *data) {

  const char *real_dir = Fs_RealDir(path);
  const char *enum_dir = data;

  if (!q_strcmp(real_dir, enum_dir)) {
    Fs_AddToSearchPathv(real_dir, path + 1, NULL);
  }
}

/**
 * @brief Adds the user-specific search path, setting the write dir in the
 * process. This is where all files produced by the game are written to.
 * @details If `-w` or `--wpath` was given on the command line, that path is
 * used as the write dir instead of the per-game user directory; the user
 * directory is still mounted as a read-only fallback for configs and such
 * that may already live there.
 */
static void Fs_AddUserSearchPath(const char *dir) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s", Sys_UserDir(), dir);

  if (!SDL_CreateDirectory(path)) {
    Com_Warn("Failed to create %s\n", path);
    return;
  }

  Fs_AddToSearchPath(path);

  if (*fs_state.write_dir_override) {
    Fs_SetWriteDir(fs_state.write_dir_override);
  } else {
    Fs_SetWriteDir(path);
  }
}

/**
 * @brief Mounts every root that may hold `dir`'s assets.
 */
static void Fs_AddGameSearchPath(const char *dir) {

  if (*fs_state.resources_dir) {
    Fs_AddToSearchPathv(fs_state.resources_dir, dir, NULL);
  }

  Fs_AddToSearchPathv(fs_state.lib_dir, dir, NULL);
  Fs_AddToSearchPathv(fs_state.data_dir, dir, NULL);

  for (size_t p = 0; p < fs_state.num_command_line_paths; p++) {
    Fs_AddToSearchPathv(fs_state.command_line_paths[p], dir, NULL);
  }
}


/**
 * @brief Points the search path at the given game directory.
 * @param game The game directory to mount.
 * @param cgame The game directory providing the client game, or `NULL`.
 * @return True if the search path now reflects `game`, false if the name was rejected.
 */
bool Fs_SetGame(const char *game, const char *cgame) {

  if (!Com_IsValidGame(game)) {
    return false;
  }

  Com_Debug(DEBUG_FILESYSTEM, "Setting game: %s\n", game);

  // iterate the current search path, removing those which are not base paths
  char **paths = PHYSFS_getSearchPath();
  for (char **path = paths; *path; path++) {
    char **p = fs_state.base_search_paths;
    while (*p != NULL) {
      if (!q_strcmp(*path, *p)) {
        break;
      }
      p++;
    }
    if (!*p) {
      Com_Debug(DEBUG_FILESYSTEM, "Removing %s\n", *path);
      if (PHYSFS_unmount(*path) == 0) {
        Com_Warn("%s: %s\n", *path, Fs_LastError());
      }
    }
  }

  PHYSFS_freeList(paths);

  const bool provider = Com_IsValidGame(cgame)
      && q_strcmp(cgame, game)
      && q_strcmp(cgame, DEFAULT_GAME);

  if (provider) {
    Fs_AddGameSearchPath(cgame);
    Fs_AddToSearchPathv(Sys_UserDir(), cgame, NULL);
  }

  // the install and command-line roots for DEFAULT_GAME were already mounted
  // permanently by Fs_Init as the base fallback layer; mounting them again here
  // would just duplicate them. A provider is the exception: it was mounted above
  // those base layers, so the game itself has to go above it in turn
  if (q_strcmp(game, DEFAULT_GAME) || provider) {
    Fs_AddGameSearchPath(game);
  }

  Fs_AddUserSearchPath(game);
  return true;
}

/**
 * @brief Resolves the real, on-disk path of the module `name` within game directory `game`.
 * @param game The game directory name, e.g. @c "ctf".
 * @param name The library file name, e.g. @c "cgame.so".
 * @param path A buffer to receive the resolved path, if found.
 * @param len The size of `path`.
 * @return True if `path` was resolved, false otherwise.
 */
bool Fs_FindLibrary(const char *game, const char *name, char *path, size_t len) {

  if (!Com_IsValidGame(game)) {
    return false;
  }

  const char *roots[MAX_COMMAND_LINE_PATHS + 3];
  size_t num_roots = 0;

  roots[num_roots++] = Sys_UserDir();

  for (size_t p = fs_state.num_command_line_paths; p > 0; p--) {
    roots[num_roots++] = fs_state.command_line_paths[p - 1];
  }

  roots[num_roots++] = fs_state.data_dir;
  roots[num_roots++] = fs_state.lib_dir;

  if (*fs_state.resources_dir) {
    roots[num_roots++] = fs_state.resources_dir;
  }

  for (size_t r = 0; r < num_roots; r++) {
    q_snprintf(path, len, "%s/%s/%s", roots[r], game, name);

    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE) {
      return true;
    }
  }

  *path = '\0';
  return false;
}

/**
 * @brief Sets the [user-specific] target directory for writing files.
 */
void Fs_SetWriteDir(const char *dir) {

  SDL_PathInfo dir_info;
  if (SDL_GetPathInfo(dir, &dir_info)) {
    if (dir_info.type != SDL_PATHTYPE_DIRECTORY) {
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

  q_snprintf(real_path, sizeof(real_path), "%s/", Fs_WriteDir());

  const char *in = path;
  char *out = real_path + q_strlen(real_path);

  while (*in && (size_t) (out - real_path) < (sizeof(real_path) - 1)) {
    if (*in == '/') {
      *out = '/';
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

  q_strlcpy(fs_state.bin_dir, BINDIR, MAX_OS_PATH);
  q_strlcpy(fs_state.lib_dir, PKGLIBDIR, MAX_OS_PATH);
  q_strlcpy(fs_state.data_dir, PKGDATADIR, MAX_OS_PATH);

  const char *path = Sys_ExecutablePath();
  if (path) {
    char *c;

    Com_Debug(DEBUG_FILESYSTEM, "Resolved executable path: %s\n", path);

#if defined(__APPLE__)
    if ((c = q_strstr(path, "Quetoo.app"))) {
      *(c + q_strlen("Quetoo.app")) = '\0';
      q_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

      if (q_strstr(fs_state.base_dir, "AppTranslocation")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
          "Move Quetoo to Applications",
          "Quetoo cannot run from this location.\n\n"
          "Please move Quetoo.app to your Applications folder and try again.",
          NULL);
        Com_Error(ERROR_FATAL,
          "Quetoo cannot run from this location.\n\n"
          "Please move Quetoo.app to your Applications folder and try again.\n");
      }

      /*
       * The macOS paths for game data are a little funky. The .app bundle ships with a copy of
       * `quetoo-data` so that users don't have to sit through a lengthy download the first time
       * they launch the game. But this copy is immutable, signed, and must never be updated by the
       * in-game installer, or Gatekeeper would hassle the user when they relaunch the game.
       *
       * So, we set `data_dir` to `Sys_UserDir()/share`. This writes updated official game content to
       * the user's home, without conflating it with true user-owned data like screenshots, configs,
       * custom maps etc. Then, we append `Contents/Resources` to the search path, allowing the
       * game to fall back on the read-only copy of quetoo-data that it originally came with.
       */

      q_snprintf(fs_state.bin_dir, MAX_OS_PATH, "%s/Contents/MacOS", fs_state.base_dir);
      q_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s/Contents/MacOS/lib/quetoo", fs_state.base_dir);
      q_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s/share", Sys_UserDir());

      // Ensure data_dir/default exists so PhysFS will mount it. On first launch
      // this directory tree doesn't exist yet, and Fs_AddToSearchPath silently
      // skips non-existent paths, leaving the installer with nowhere to write.
      char data_default[MAX_OS_PATH];
      q_snprintf(data_default, MAX_OS_PATH, "%s/%s", fs_state.data_dir, DEFAULT_GAME);
      SDL_CreateDirectory(data_default);

      q_snprintf(fs_state.resources_dir, MAX_OS_PATH, "%s/Contents/Resources", fs_state.base_dir);
      Fs_AddToSearchPathv(fs_state.resources_dir, NULL);
      Fs_AddToSearchPathv(fs_state.resources_dir, DEFAULT_GAME, NULL);
    }
#elif defined(__linux__)
    if ((c = q_strstr(path, "/bin/"))) {
      *c = '\0';
      q_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

      char bin_dir[MAX_OS_PATH];
      q_snprintf(bin_dir, MAX_OS_PATH, "%s/bin", fs_state.base_dir);

      if (q_strcmp(bin_dir, fs_state.bin_dir) != 0) {
        q_strlcpy(fs_state.bin_dir, bin_dir, MAX_OS_PATH);
        q_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s/lib/quetoo", fs_state.base_dir);
        q_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s/share/quetoo", fs_state.base_dir);
      }
    }
#elif defined(_WIN32)
    if ((c = q_strstr(path, "\\bin\\"))) {
      *c = '\0';
      q_strlcpy(fs_state.base_dir, path, sizeof(fs_state.base_dir));

      q_snprintf(fs_state.bin_dir, MAX_OS_PATH, "%s\\bin", fs_state.base_dir);
      q_snprintf(fs_state.lib_dir, MAX_OS_PATH, "%s\\lib", fs_state.base_dir);
      q_snprintf(fs_state.data_dir, MAX_OS_PATH, "%s\\share", fs_state.base_dir);
    }
#endif
  }

  Fs_AddToSearchPathv(fs_state.lib_dir, NULL);
  Fs_AddToSearchPathv(fs_state.data_dir, NULL);

  Fs_AddToSearchPathv(fs_state.lib_dir, DEFAULT_GAME, NULL);
  Fs_AddToSearchPathv(fs_state.data_dir, DEFAULT_GAME, NULL);

  // finally add any paths specified on the command line
  int32_t i;
  for (i = 1; i < Com_Argc(); i++) {

    if (!q_strcmp(Com_Argv(i), "-p") || !q_strcmp(Com_Argv(i), "--path")) {
      Fs_AddCommandLinePath(Com_Argv(i + 1));
      continue;
    }

    if (!q_strcmp(Com_Argv(i), "-w") || !q_strcmp(Com_Argv(i), "--wpath")) {
      // mounted, but deliberately not remembered as a root: roots resolve
      // modules, and the engine writes server-named downloads here
      Fs_AddToSearchPath(Com_Argv(i + 1));
      q_strlcpy(fs_state.write_dir_override, Com_Argv(i + 1), sizeof(fs_state.write_dir_override));
      continue;
    }
  }

  // as with the install directories, the default game beneath them is a base
  // path, present whichever game is later selected
  for (size_t p = 0; p < fs_state.num_command_line_paths; p++) {
    Fs_AddToSearchPathv(fs_state.command_line_paths[p], DEFAULT_GAME, NULL);
  }

  // these paths will be retained across all game modules
  fs_state.base_search_paths = PHYSFS_getSearchPath();

  fs_state.loaded_files = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
  fs_state.loaded_files->destroyValue = Mem_Free;
}

/**
 * @brief Prints the names of loaded (i.e. yet-to-be-freed) files.
 */
static void Fs_LoadedFiles_(const HashTable *table, ident key, ident value, ident data) {
  Com_Print("Fs_PrintLoadedFiles: %s @ %p\n", (char *) value, key);
}

/**
 * @brief Shuts down the filesystem.
 */
void Fs_Shutdown(void) {

  if (PHYSFS_isInit() == 0) {
    return;
  }

  $(fs_state.loaded_files, enumerate, Fs_LoadedFiles_, NULL);
  release(fs_state.loaded_files);

  PHYSFS_freeList(fs_state.base_search_paths);

  PHYSFS_deinit();
}
