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

#include "s_types.h"

void S_LoadClientSamples(const char *model);

#ifdef __S_LOCAL_H__

void S_ListMedia_f(void);
void S_RegisterMedia(s_media_t *media);
void S_RegisterDependency(s_media_t *dependent, s_media_t *dependency);
s_media_t *S_FindMedia(const char *name, s_media_type_t type);
s_media_t *S_AllocMedia(const char *name, size_t size, s_media_type_t type);
void S_FreeMedia(void);
void S_BeginLoading(void);
void S_InitMedia(void);
void S_ShutdownMedia(void);

#endif /* __S_LOCAL_H__ */
