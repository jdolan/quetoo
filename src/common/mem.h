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

#include "quetoo.h"

void Mem_Free(void *p);
void Mem_FreeTag(mem_tag_t tag);
void *Mem_TagMalloc(size_t size, mem_tag_t tag);
void *Mem_LinkMalloc(size_t size, void *parent);
void *Mem_Malloc(size_t size);
void *Mem_Realloc(void *p, size_t size);
void *Mem_Link(void *parent, void *child);
size_t Mem_Size(void);
char *Mem_TagCopyString(const char *in, mem_tag_t tag);
char *Mem_CopyString(const char *in);
void Mem_Check(void *p);

/**
 * @brief Struct used for return values of Mem_Stats
 */
typedef struct {
	mem_tag_t	tag; // tag
	size_t		size; // total size in bytes
	size_t		count; // number of blocks
} mem_stat_t;

GArray *Mem_Stats(void);

void Mem_Init(void);
void Mem_Shutdown(void);
