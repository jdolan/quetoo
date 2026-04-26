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

#include "cl_local.h"

cl_static_t cls;

/**
 * @brief Null client stub: returns whether the installer is complete.
 */
int32_t Cl_InstallerFrame(const installer_status_t *in) {
  return in->state >= INSTALLER_DONE;
}

/**
 * @brief Null client stub: no-op disconnect.
 */
void Cl_Disconnect(void) {

}

/**
 * @brief Null client stub: no-op drop handler.
 */
void Cl_Drop(const char *text) {

}

/**
 * @brief Null client stub: no-op frame tick.
 */
void Cl_Frame(const uint32_t msec) {

}

/**
 * @brief Null client stub: minimal init that clears the client state.
 */
void Cl_Init(void) {
  memset(&cls, 0, sizeof(cls));
}

/**
 * @brief Null client stub: no-op shutdown.
 */
void Cl_Shutdown(void) {

}
