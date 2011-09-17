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

#include "common.h"

#define Z_MAGIC 0x1d1d

typedef struct z_head_s {
	struct z_head_s	*prev, *next;
	short magic;
	short tag;  // for group free
	size_t size;
} z_head_t;

void Z_Init(void);
void Z_Shutdown(void);
void Z_Free(void *ptr);
void Z_FreeTags(int tag);
void *Z_TagMalloc(size_t size, int tag);
void *Z_Malloc(size_t size);
char *Z_CopyString(const char *in);

#endif /* __MEM_H__ */
