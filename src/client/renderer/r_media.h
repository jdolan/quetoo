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

#ifndef __R_MEDIA_H__
#define __R_MEDIA_H__

#include "r_types.h"

#ifdef __R_LOCAL_H__

void R_ListMedia_f(void);
void R_RegisterDependency(r_media_t *dependent, r_media_t *dependency);
void R_RegisterMedia(r_media_t *media);
r_media_t *R_FindMedia(const char *name);
r_media_t *R_MallocMedia(const char *name, size_t size);
void R_FreeMedia(void);
void R_BeginLoading(void);
void R_InitMedia(void);
void R_ShutdownMedia(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_MEDIA_H__ */
