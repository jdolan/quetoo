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

void Z_Free(void *p);
void Z_FreeTag(z_tag_t tag);
void *Z_TagMalloc(size_t size, z_tag_t tag);
void *Z_LinkMalloc(size_t size, void *parent);
void *Z_Malloc(size_t size);
size_t Z_Size(void);
void Z_Size_f(void);
char *Z_CopyString(const char *in);
void Z_Init(void);
void Z_Shutdown(void);

#endif /* __MEM_H__ */
