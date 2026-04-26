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

#include <SDL3/SDL_mutex.h>

#define QUETOO_RELEASES_URL     "https://github.com/jdolan/quetoo/releases/latest"
#define QUETOO_VERSION_URL      "https://quetoo.s3.amazonaws.com/version"
#define QUETOO_DATA_BASE_URL    "https://quetoo-data.s3.amazonaws.com"

/**
 * @brief The installer lifecycle.
 */
typedef enum {
  INSTALLER_CHECKING,         ///< Fetches binary version from S3
  INSTALLER_UPDATE_AVAILABLE, ///< Binary is outdated; warns and stops
  INSTALLER_COMPARING,        ///< Fetches remote manifest.mf and diffs against local copy
  INSTALLER_DOWNLOADING,      ///< Parallel file downloads
  INSTALLER_COMMITTING,       ///< Prunes stale files and writes manifest.mf
  INSTALLER_CANCELLED,
  INSTALLER_DONE,
  INSTALLER_ERROR,
} installer_state_t;

/**
 * @brief The installer status snapshot.
 */
typedef struct {
	installer_state_t state;
  int32_t bin_version;
	int32_t files_done;
	int32_t files_total;
	int32_t kbytes_done;
	int32_t kbytes_total;
	char current_file[MAX_OS_PATH];
  char error[MAX_STRING_CHARS];
} installer_status_t;

/**
 * @brief Frame callback type for `Installer_Wait`.
 * @details Returning non-zero will terminate the installer process and resume startup.
 */
typedef int32_t (*Installer_FrameFunction)(const installer_status_t *status);

void Installer_Init(Installer_FrameFunction frame);
void Installer_Shutdown(void);
