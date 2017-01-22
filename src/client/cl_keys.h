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

#include "cl_types.h"

const char *Cl_KeyName(SDL_Scancode key);
SDL_Scancode Cl_KeyForName(const char *name);
SDL_Scancode Cl_KeyForBind(SDL_Scancode from, const char *bind);
void Cl_Bind(SDL_Scancode key, const char *bind);

#ifdef __CL_LOCAL_H__

void Cl_SetKeyDest(cl_key_dest_t dest);
void Cl_KeyEvent(const SDL_Event *event);
void Cl_WriteBindings(file_t *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);

#endif /* __CL_LOCAL_H__ */
