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

#include <ctype.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_process.h>

#include "archive.h"
#include "console.h"

#include "deps/minizip/miniz.h"

#if defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#endif

/**
 * @brief Windows reserves these names in every directory, with or without an
 * extension. Creating one from archive content is never legitimate.
 */
static const char *archive_reserved_names[] = {
  "CON", "PRN", "AUX", "NUL",
  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
  "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};

/**
 * @brief Returns true if `component` names a Windows reserved device.
 */
static bool Archive_IsReservedName(const char *component, size_t len) {

  size_t stem = 0;
  while (stem < len && component[stem] != '.') {
    stem++;
  }

  for (size_t i = 0; i < lengthof(archive_reserved_names); i++) {
    const char *reserved = archive_reserved_names[i];
    if (stem == q_strlen(reserved) && q_strncasecmp(component, reserved, stem) == 0) {
      return true;
    }
  }

  return false;
}

bool Archive_SafePath(const char *dest, const char *name, char *out, size_t len) {

  if (*name == '\0') {
    return false;
  }

  if (*name == '/' || *name == '\\') {
    return false;
  }

  if (isalpha((unsigned char) name[0]) && name[1] == ':') {
    return false;
  }

  char normalized[MAX_OS_PATH];
  if (q_strlcpy(normalized, name, sizeof(normalized)) >= sizeof(normalized)) {
    return false;
  }

  for (char *c = normalized; *c; c++) {
    if (*c == '\\') {
      *c = '/';
    }
  }

  const char *component = normalized;
  while (*component) {

    const char *end = q_strchr(component, '/');
    const size_t clen = end ? (size_t) (end - component) : q_strlen(component);

    if (clen == 2 && q_strncmp(component, "..", 2) == 0) {
      return false;
    }

    if (clen && Archive_IsReservedName(component, clen)) {
      return false;
    }

    if (clen && (component[clen - 1] == '.' || component[clen - 1] == ' ')) {
      return false;
    }

    if (!end) {
      break;
    }

    component = end + 1;
  }

  if (q_snprintf(out, len, "%s/%s", dest, normalized) >= (int32_t) len) {
    return false;
  }

  return true;
}

/**
 * @brief Creates the parent directory of `path`, including intermediates.
 */
static bool Archive_CreateParent(const char *path) {

  char dir[MAX_OS_PATH];
  q_strlcpy(dir, path, sizeof(dir));

  char *slash = q_strrchr(dir, '/');
  if (!slash) {
    return true;
  }

  *slash = '\0';
  return SDL_CreateDirectory(dir);
}

/**
 * @brief Runs `args` to completion, logging its output if it fails.
 * @return True if the process exited zero.
 */
static bool Archive_Spawn(const char * const *args) {

  SDL_Process *process = SDL_CreateProcess(args, true);
  if (!process) {
    Com_Warn("Failed to run %s: %s\n", args[0], SDL_GetError());
    return false;
  }

  int exit_code = -1;
  size_t length = 0;
  void *output = SDL_ReadProcess(process, &length, &exit_code);

  if (output == NULL) {
    SDL_WaitProcess(process, true, &exit_code);
  }

  if (exit_code != 0) {
    Com_Warn("%s exited %d\n", args[0], exit_code);
    if (output && length) {
      Com_Warn("%s: %.*s\n", args[0], (int32_t) length, (const char *) output);
    }
  }

  SDL_free(output);
  SDL_DestroyProcess(process);

  return exit_code == 0;
}

/**
 * @brief Extracts a `.zip` with the vendored miniz, sanitizing every member.
 */
static bool Archive_ExtractZip(const char *archive, const char *dest) {

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));

  if (!mz_zip_reader_init_file(&zip, archive, 0)) {
    Com_Warn("Failed to open %s: %s\n", archive, mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
    return false;
  }

  bool success = true;
  const mz_uint count = mz_zip_reader_get_num_files(&zip);

  for (mz_uint i = 0; i < count; i++) {

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
      Com_Warn("Failed to stat member %u of %s\n", i, archive);
      success = false;
      break;
    }

    char path[MAX_OS_PATH];
    if (!Archive_SafePath(dest, stat.m_filename, path, sizeof(path))) {
      Com_Warn("Refusing unsafe archive member: %s\n", stat.m_filename);
      success = false;
      break;
    }

    if (stat.m_is_directory) {
      if (!SDL_CreateDirectory(path)) {
        Com_Warn("Failed to create %s: %s\n", path, SDL_GetError());
        success = false;
        break;
      }
      continue;
    }

    if (!Archive_CreateParent(path)) {
      Com_Warn("Failed to create directory for %s: %s\n", path, SDL_GetError());
      success = false;
      break;
    }

    if (!mz_zip_reader_extract_to_file(&zip, i, path, 0)) {
      Com_Warn("Failed to extract %s: %s\n", stat.m_filename,
               mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
      success = false;
      break;
    }
  }

  mz_zip_reader_end(&zip);
  return success;
}

#if defined(__APPLE__)

/**
 * @brief Copies every non-symlink entry of `mount` into `dest` with `ditto`.
 * @details The disk image also carries an `/Applications` alias for drag
 * installs, which must not be followed. `ditto` rather than `cp` because it
 * preserves the extended attributes and resource forks that keep a bundle's
 * code signature valid.
 */
static bool Archive_DittoMount(const char *mount, const char *dest) {

  DIR *dir = opendir(mount);
  if (!dir) {
    Com_Warn("Failed to read %s\n", mount);
    return false;
  }

  bool success = true;

  const struct dirent *entry;
  while ((entry = readdir(dir))) {

    if (q_strcmp(entry->d_name, ".") == 0 || q_strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char src[MAX_OS_PATH];
    q_snprintf(src, sizeof(src), "%s/%s", mount, entry->d_name);

    struct stat st;
    if (lstat(src, &st) == 0 && S_ISLNK(st.st_mode)) {
      continue;
    }

    char dst[MAX_OS_PATH];
    q_snprintf(dst, sizeof(dst), "%s/%s", dest, entry->d_name);

    if (!Archive_Spawn((const char *[]) { "/usr/bin/ditto", src, dst, NULL })) {
      success = false;
      break;
    }
  }

  closedir(dir);
  return success;
}

/**
 * @brief Extracts a `.dmg` by mounting it at a path we choose.
 * @details Passing `-mountpoint` avoids parsing `hdiutil`'s output for the
 * volume it picked, and avoids colliding with an already-mounted volume of the
 * same name. The image is always detached, including on failure.
 */
static bool Archive_ExtractDmg(const char *archive, const char *dest) {

  char mount[MAX_OS_PATH];
  q_snprintf(mount, sizeof(mount), "%s/.mount", dest);

  if (!SDL_CreateDirectory(mount)) {
    Com_Warn("Failed to create %s: %s\n", mount, SDL_GetError());
    return false;
  }

  if (!Archive_Spawn((const char *[]) {
        "/usr/bin/hdiutil", "attach", "-nobrowse", "-readonly", "-noverify",
        "-mountpoint", mount, archive, NULL
      })) {
    SDL_RemovePath(mount);
    return false;
  }

  const bool success = Archive_DittoMount(mount, dest);

  if (!Archive_Spawn((const char *[]) { "/usr/bin/hdiutil", "detach", mount, NULL })) {
    Archive_Spawn((const char *[]) { "/usr/bin/hdiutil", "detach", "-force", mount, NULL });
  }

  SDL_RemovePath(mount);
  return success;
}

#endif

#if !defined(_WIN32)

/**
 * @brief Extracts a `.tar.gz` with the system `tar`.
 * @details `--strip-components` is deliberately not used; it is absent from
 * busybox tar, so callers treat the archive's own root directory as the staged
 * root instead. `tar` is addressed absolutely rather than through `PATH`, which
 * an update must not be willing to follow.
 */
static bool Archive_ExtractTarGz(const char *archive, const char *dest) {

  const char *tar = SDL_GetPathInfo("/usr/bin/tar", NULL) ? "/usr/bin/tar" : "/bin/tar";

  return Archive_Spawn((const char *[]) { tar, "-xzf", archive, "-C", dest, NULL });
}

#endif

/**
 * @brief Returns true if `path` ends with `suffix`, ignoring case.
 */
static bool Archive_HasSuffix(const char *path, const char *suffix) {

  const size_t plen = q_strlen(path), slen = q_strlen(suffix);
  if (plen < slen) {
    return false;
  }

  return q_strncasecmp(path + plen - slen, suffix, slen) == 0;
}

bool Archive_Extract(const char *archive, const char *dest) {

  if (!SDL_CreateDirectory(dest)) {
    Com_Warn("Failed to create %s: %s\n", dest, SDL_GetError());
    return false;
  }

  if (Archive_HasSuffix(archive, ".zip")) {
    return Archive_ExtractZip(archive, dest);
  }

#if defined(__APPLE__)
  if (Archive_HasSuffix(archive, ".dmg")) {
    return Archive_ExtractDmg(archive, dest);
  }
#endif

#if !defined(_WIN32)
  if (Archive_HasSuffix(archive, ".tar.gz") || Archive_HasSuffix(archive, ".tgz")) {
    return Archive_ExtractTarGz(archive, dest);
  }
#endif

  Com_Warn("Unsupported archive on this platform: %s\n", archive);
  return false;
}
