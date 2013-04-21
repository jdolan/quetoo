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

#include <physfs.h>

#include "common.h"
#include "swap.h"
#include "sys.h"

typedef PHYSFS_sint64 int64_t;
typedef PHYSFS_File file_t;

typedef void (*fs_enumerate_func)(const char *path, void *data);

bool Fs_Close(file_t *f);
bool Fs_Exists(const char *filename);
const char *Fs_LastError(void);
bool Fs_Mkdir(const char *dir);
file_t *Fs_OpenAppend(const char *filename);
file_t *Fs_OpenRead(const char *filename);
file_t *Fs_OpenWrite(const char *filename);
int64_t Fs_Print(file_t *file, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int64_t Fs_Read(file_t *file, void *buffer, size_t size, size_t count);
bool Fs_ReadLine(file_t *file, char *buffer, size_t len);
bool Fs_Seek(file_t *file, size_t offset);
int64_t Fs_Tell(file_t *file);
int64_t Fs_Write(file_t *file, void *buffer, size_t size, size_t count);
int64_t Fs_Load(const char *filename, void **buffer);
void Fs_Free(void *buffer);
bool Fs_Rename(const char *source, const char *dest);
bool Fs_Unlink(const char *filename);
void Fs_Enumerate(const char *pattern, fs_enumerate_func, void *data);
void Fs_CompleteFile(const char *pattern, GList **matches);
void Fs_AddToSearchPath(const char *dir);
void Fs_SetGame(const char *dir);
const char *Fs_WriteDir(void);
const char *Fs_RealDir(const char *filename);
void Fs_Init(const char *argv0);
void Fs_Shutdown(void);

#endif /* __FILESYSTEM_H__ */
