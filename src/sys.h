/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "filesystem.h"

#include <dirent.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <pwd.h>
#endif

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

int Sys_Milliseconds(void);

const char *Sys_GetCurrentUser(void);

void Sys_Mkdir(const char *path);
const char *Sys_FindFirst(const char *path);
const char *Sys_FindNext(void);
void Sys_FindClose(void);

void Sys_OpenLibrary(const char *name, void **handle);
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point, void *params);
void Sys_CloseLibrary(void **handle);

void Sys_Quit(void);
void Sys_Backtrace(void);
void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Sys_Signal(int s);

#endif /* __SYS_H__ */
