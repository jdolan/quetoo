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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "common.h"

/**
 * @brief The @c Fs_Stat file types.
 */
typedef enum {
  FS_UNKNOWN = -1,
  FS_REGULAR,
  FS_DIRECTORY,
  FS_SYMLINK,
  FS_OTHER,
} FsFileType;

/**
 * @brief The @c Fs_Stat return type.
 */
typedef struct {
  FsFileType type;
  int64_t size;
  int64_t created;
  int64_t modified;
  int64_t accessed;
} FsStat;

const char *Fs_BaseDir(void);
const char *Fs_BinDir(void);
const char *Fs_LibDir(void);
const char *Fs_DataDir(void);
bool Fs_Close(File *file);
bool Fs_Delete(const char *filename);
bool Fs_Eof(File *file);
bool Fs_Exists(const char *filename);
bool Fs_Flush(File *file);
bool Fs_Stat(const char *filename, FsStat *out);
const char *Fs_LastError(void);
bool Fs_Mkdir(const char *dir);
File *Fs_OpenAppend(const char *filename);
File *Fs_OpenRead(const char *filename);
File *Fs_OpenWrite(const char *filename);
int64_t Fs_Print(File *file, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int64_t Fs_Read(File *file, void *buffer, size_t size, size_t count);
bool Fs_ReadLine(File *file, char *buffer, size_t len);
bool Fs_Seek(File *file, int64_t offset);
int64_t Fs_FileLength(File *file);
int64_t Fs_Tell(File *file);
int64_t Fs_Write(File *file, const void *buffer, size_t size, size_t count);
bool Fs_WriteAt(const char *filename, const void *data, size_t size, int64_t offset);
int64_t Fs_Load(const char *filename, void **buffer);
int64_t Fs_LastModTime(const char *filename);
void Fs_Free(void *buffer);
bool Fs_Rename(const char *source, const char *dest);
bool Fs_Unlink(const char *filename);
void Fs_Enumerate(const char *pattern, Fs_Enumerator, void *data);
void Fs_CompleteFile(const char *pattern, List *matches);
void Fs_CompleteGame(const char *pattern, List *matches);
void Fs_AddToSearchPath(const char *path);
void Fs_AddToSearchPathv(const char *dir, ...) __attribute__((sentinel));
bool Fs_SetGame(const char *game, const char *cgame);
bool Fs_FindLibrary(const char *game, const char *name, char *path, size_t len);
void Fs_SetWriteDir(const char *dir);
const char *Fs_WriteDir(void);
const char *Fs_RealDir(const char *filename);
const char *Fs_RealPath(const char *path);
void Fs_Init(const uint32_t flags);
void Fs_Shutdown(void);
