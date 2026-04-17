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

/**
 * @brief The phase of an in-progress data sync operation.
 */
typedef enum {
	INSTALLER_SYNC_IDLE,
	INSTALLER_SYNC_CHECKING,
	INSTALLER_SYNC_LISTING,
	INSTALLER_SYNC_DOWNLOADING,
	INSTALLER_SYNC_PRUNING,
	INSTALLER_SYNC_DONE,
	INSTALLER_SYNC_ERROR,
} installer_sync_phase_t;

/**
 * @brief Progress state for an in-progress data sync.
 * All fields are guarded by @c lock; hold it when reading or writing.
 */
typedef struct {
	SDL_Mutex *lock;
	installer_sync_phase_t phase;
	int32_t files_done;
	int32_t files_total;
	int32_t kbytes_done;
	int32_t kbytes_total;
	char current_file[MAX_OS_PATH];
	char error[MAX_STRING_CHARS];
} installer_sync_status_t;

int32_t Installer_CheckForUpdates(void);
void Installer_OpenReleasesPage(void);
void Installer_SyncData(void);
void Installer_Status(installer_sync_status_t *out);
