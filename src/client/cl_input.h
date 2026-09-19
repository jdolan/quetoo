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

#if defined(__CL_LOCAL_H__)

extern Cvar *m_sensitivity;
extern Cvar *m_sensitivity_zoom;
extern Cvar *m_interpolate;
extern Cvar *m_invert;
extern Cvar *m_pitch;
extern Cvar *m_yaw;

void Cl_ClearInput(void);
void Cl_InitInput(void);
void Cl_HandleEvents(void);
void Cl_Look(PlayerMoveCmd *cmd);
void Cl_Move(PlayerMoveCmd *cmd);
void Cl_KeyDown(InputButton *b);
void Cl_KeyUp(InputButton *b);
float Cl_KeyState(InputButton *key, uint32_t cmd_msec);

#endif
