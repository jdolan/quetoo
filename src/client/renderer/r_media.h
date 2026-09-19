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

#include "r_types.h"

void R_BeginLoading(void);
void R_EndLoading(void);

#if defined(__R_LOCAL_H__)

typedef void (*R_MediaEnumerator)(const RenderMedia *media, void *data);
void R_EnumerateMedia(R_MediaEnumerator enumerator, void *data);
void R_ListMedia_f(void);
RenderMedia *R_RegisterDependency(RenderMedia *dependent, RenderMedia *dependency);
RenderMedia *R_RegisterMedia(RenderMedia *media);
RenderMedia *R_FindMedia(const char *name, RenderMediaType type);
RenderMedia *R_AllocMedia(const char *name, size_t size, RenderMediaType type);
void R_FreeMedia(RenderMedia *media);
void R_InitMedia(void);
void R_ShutdownMedia(void);

#endif
