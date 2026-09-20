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

#include "s_types.h"

void S_InitStage(SoundStage *stage);
void S_RenderStage(SoundStage *stage);
void S_Init(void);
void S_Shutdown(void);
void S_Stop(void);

#if defined(__S_LOCAL_H__)
extern Cvar *s_getError;

void S_GetError_(const char *function, const char *msg);

#define S_GetError(msg) { \
  if (s_getError->integer) { \
    S_GetError_(__func__, msg); \
  } \
}

#endif
