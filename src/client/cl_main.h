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

#include "cl_types.h"

extern Cvar *cl_chatSound;
extern Cvar *cl_maxFps;
extern Cvar *cl_noLerp;
extern Cvar *cl_teamChatSound;
extern Cvar *cl_timeout;

extern Cvar *guid;
extern Cvar *name;
extern Cvar *active;
extern Cvar *message_level;
extern Cvar *password;
extern Cvar *rate;

void Cl_Connect(const NetAddr *addr);
void Cl_Disconnect(void);
void Cl_Drop(const char *text);
int32_t Cl_InstallerFrame(const InstallerStatus *in);
void Cl_Frame(const uint32_t msec);
void Cl_Init(void);
void Cl_Shutdown(void);

extern RenderView clView;
extern SoundStage clStage;

#if defined(__CL_LOCAL_H__)

extern Cvar *qport;

extern Cvar *cl_drawNetMessages;

extern Client cl;
extern ClientStatic cls;

void Cl_SendDisconnect(void);
void Cl_Reconnect_f(void);
void Cl_ClearState(void);
void Cl_ForwardCmdToServer(void);

#endif
