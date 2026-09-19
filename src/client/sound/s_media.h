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

void S_BeginLoading(void);
void S_LoadClientModelSamples(const char *model, const char *soundSet);
void S_EndLoading(void);

#if defined(__S_LOCAL_H__)

void S_ListMedia_f(void);
void S_RegisterMedia(SoundMedia *media);
void S_RegisterDependency(SoundMedia *dependent, SoundMedia *dependency);
SoundMedia *S_FindMedia(const char *name, SoundMediaType type);
SoundMedia *S_AllocMedia(const char *name, size_t size, SoundMediaType type);
void S_InitMedia(void);
void S_ShutdownMedia(void);

#endif
