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

#include "sv_types.h"

#if defined(__SV_LOCAL_H__)
void Sv_LoadDemo(void);
void Sv_FreeDemo(void);
void Sv_SendDemoSetup(sv_client_t *cl);
size_t Sv_GetDemoFrame(byte *buffer);
bool Sv_SendDemoPacket(sv_client_t *cl, byte *buffer, size_t size);
void Sv_SeekDemo(int32_t millis);
void Sv_SendDemoInfo(void);
void Sv_DemoSeek_f(void);
void Sv_DemoSeekRelative_f(void);
void Sv_DemoPause_f(void);
#endif
