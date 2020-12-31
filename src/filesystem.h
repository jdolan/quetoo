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

#include "common.h"
#include "swap.h"
#include "sys.h"

const char *Fs_BaseDir(void);
_Bool Fs_Close(file_t *file);
_Bool Fs_Delete(const char *filename);
_Bool Fs_Eof(file_t *file);
_Bool Fs_Exists(const char *filename);
_Bool Fs_Flush(file_t *file);
const char *Fs_LastError(void);
_Bool Fs_Mkdir(const char *dir);
file_t *Fs_OpenAppend(const char *filename);
file_t *Fs_OpenRead(const char *filename);
file_t *Fs_OpenWrite(const char *filename);
int64_t Fs_Print(file_t *file, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int64_t Fs_Read(file_t *file, void *buffer, size_t size, size_t count);
_Bool Fs_ReadLine(file_t *file, char *buffer, size_t len);
_Bool Fs_Seek(file_t *file, int64_t offset);
int64_t Fs_FileLength(file_t *file);
int64_t Fs_Tell(file_t *file);
int64_t Fs_Write(file_t *file, const void *buffer, size_t size, size_t count);
int64_t Fs_Load(const char *filename, void **buffer);
int64_t Fs_LastModTime(const char *filename);
void Fs_Free(void *buffer);
_Bool Fs_Rename(const char *source, const char *dest);
_Bool Fs_Unlink(const char *filename);
void Fs_Enumerate(const char *pattern, Fs_Enumerator, void *data);
void Fs_CompleteFile(const char *pattern, GList **matches);
void Fs_AddToSearchPath(const char *dir);
void Fs_SetGame(const char *dir);
void Fs_SetWriteDir(const char *dir);
const char *Fs_WriteDir(void);
const char *Fs_RealDir(const char *filename);
const char *Fs_RealPath(const char *path);
void Fs_Init(const uint32_t flags);
void Fs_Shutdown(void);
