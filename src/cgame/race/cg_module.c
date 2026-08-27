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


#include "cg_local.h"

/**
 * @brief Race draws the grappling hook, a feature of the common sources that
 * `Cg_Init` installs from the define in this module's Makefile.am. The run
 * timer, the HUD arrangement and the training tools it owns install their
 * hooks from here.
 */
void Cg_Module_Init(void) {
}

/**
 * @brief Nothing of its own to release yet; the hook feature owns what it loads.
 */
void Cg_Module_Shutdown(void) {
}
