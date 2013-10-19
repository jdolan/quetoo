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

#ifndef __MEM_H__
#define __MEM_H__

#include "quake2world.h"

void Mem_Free(void *p);
void Mem_FreeTag(mem_tag_t tag);
void *Mem_TagMalloc(size_t size, mem_tag_t tag);
void *Mem_LinkMalloc(size_t size, void *parent);
void *Mem_Malloc(size_t size);
void *Mem_Link(void *parent, void *child);
size_t Mem_Size(void);
void Mem_Size_f(void);
char *Mem_CopyString(const char *in);
void Mem_Init(void);
void Mem_Shutdown(void);

#endif /* __MEM_H__ */
