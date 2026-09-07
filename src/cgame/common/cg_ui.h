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

#include <ObjectivelyMVC/Image.h>

#include "cg_types.h"

/**
 * @brief Loads the named pic as an Image: an SVG rasterized at the window's pixel density, so
 * that it is sharp at the size it is drawn, or else whatever raster `cgi.LoadSurface` finds.
 * @param name The pic name without extension, e.g. `pics/w_shotgun`.
 * @return The Image, or `NULL` if none was found. The caller owns a reference.
 */
Image *Cg_LoadImage(const char *name);

/**
 * @brief Initializes the user interface.
 */
void Cg_InitUi(void);

/**
 * @brief Installs the HUD, once the modules are initialized.
 */
void Cg_InitHudUi(void);
void Cg_ShutdownUi(void);
void Cg_ClearUi(void);
void Cg_UpdateLoading(const cl_loading_t loading);
int32_t Cg_UpdateInstaller(const installer_status_t *status);

#if defined(__CG_LOCAL_H__)
void Cg_BindCvar(const Inlet *inlet, ident obj);
#endif
