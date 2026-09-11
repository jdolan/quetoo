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
#define QUETOO_RELEASES_API_URL "https://api.github.com/repos/jdolan/quetoo/releases/latest"
#define QUETOO_DATA_BASE_URL    "https://quetoo-data.s3.amazonaws.com"

/**
 * @brief The installer lifecycle.
 */
typedef enum {
  INSTALLER_CHECKING,
  INSTALLER_UPDATE_AVAILABLE,
  INSTALLER_DOWNLOADING_UPDATE,
  INSTALLER_STAGING_UPDATE,
  INSTALLER_UPDATE_STAGED,
  INSTALLER_COMPARING,
  INSTALLER_DOWNLOADING,
  INSTALLER_COMMITTING,
  INSTALLER_CANCELLED,
  INSTALLER_DONE,
  INSTALLER_ERROR,
} installer_state_t;

/**
 * @brief The installer status snapshot.
 */
typedef struct {
	installer_state_t state;
  int32_t build_number;
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

/**
 * @brief Moves a staged update into place, and sweeps files displaced by a
 * previous one.
 * @details Called during shutdown, after the game modules are unloaded and
 * before the filesystem paths go away. Applying on the way out rather than on
 * the way in means the update completes in the session that fetched it, and
 * that nothing is loaded after the files move. A crash before this runs simply
 * leaves the staged update for the next clean exit.
 */
void Installer_ApplyPending(void);

void Installer_Shutdown(void);
