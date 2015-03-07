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

#ifndef __SYS_H__
#define __SYS_H__

#include "quetoo.h"

uint32_t Sys_Milliseconds(void);

const char *Sys_ExecutablePath(void);
const char *Sys_Username(void);
const char *Sys_UserDir(void);

void Sys_OpenLibrary(const char *name, void **handle);
void Sys_CloseLibrary(void **handle);
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point, void *params);

void Sys_Backtrace(void);
void Sys_Signal(int32_t s) __attribute__((noreturn));

#endif /* __SYS_H__ */
