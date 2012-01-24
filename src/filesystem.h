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

#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "common.h"
#include "cmd.h"
#include "cvar.h"
#include "pak.h"
#include "swap.h"
#include "sys.h"

void Fs_Init(void);
void Fs_SetGame(const char *dir);
const char *Fs_Gamedir(void);
const char *Fs_FindFirst(const char *path, boolean_t absolute);
void Fs_ExecAutoexec(void);
int Fs_OpenFile(const char *file_name, FILE **file, file_mode_t mode);
void Fs_CloseFile(FILE *f);
int Fs_LoadFile(const char *path, void **buffer);
void Fs_AddPakfile(const char *pakfile);
size_t Fs_Write(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t Fs_Read(void *ptr, size_t size, size_t nmemb, FILE *stream);
int Fs_CompleteFile(const char *dir, const char *prefix, const char *suffix, const char *matches[]);

// a null buffer will just return the file length without loading
// a -1 length is not present
void Fs_ReadFile(void *buffer, int len, FILE *f);
void Fs_FreeFile(void *buffer);
void Fs_CreatePath(const char *path);
void Fs_GunzipFile(const char *path);

#endif /* __FILESYSTEM_H__ */
