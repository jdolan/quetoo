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

#pragma once

#include "quetoo.h"

/**
 * @brief Extracts `archive` into `dest`, creating `dest` if it does not exist.
 * @details Dispatches on the archive's suffix rather than the host platform,
 * because the game data archive is a `.zip` everywhere while the engine
 * archives differ per platform. `.zip` is read in-process; `.dmg` and `.tar.gz`
 * are handed to `hdiutil`/`ditto` and `tar` respectively, which already handle
 * long names, symlinks, resource forks and mode bits correctly.
 * @param archive Path to the archive on the real filesystem.
 * @param dest Directory to extract into.
 * @return True on success. On failure `dest` holds an indeterminate subset of
 * the archive and the caller MUST discard it rather than use it.
 */
bool Archive_Extract(const char *archive, const char *dest);

/**
 * @brief Resolves an archive member name to a path beneath `dest`.
 * @details Archive member names are untrusted input; a crafted name can escape
 * the destination directory. Rejects absolute paths, drive prefixes, `..`
 * components and Windows device names, and normalizes separators.
 * @remarks Exposed for testing. `Archive_Extract` applies this to every member
 * of a `.zip`; the `hdiutil` and `tar` backends rely on those tools' equivalent
 * protections instead.
 * @return True if `name` is safe, in which case `out` holds the full path.
 */
bool Archive_SafePath(const char *dest, const char *name, char *out, size_t len);
