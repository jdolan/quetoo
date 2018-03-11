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

#ifdef __CL_LOCAL_H__

extern cvar_t *m_sensitivity;
extern cvar_t *m_sensitivity_zoom;
extern cvar_t *m_interpolate;
extern cvar_t *m_invert;
extern cvar_t *m_pitch;
extern cvar_t *m_yaw;

void Cl_ClearInput(void);
void Cl_InitInput(void);
void Cl_HandleEvents(void);
void Cl_Look(pm_cmd_t *cmd);
void Cl_Move(pm_cmd_t *cmd);
void Cl_KeyDown(button_t *b);
void Cl_KeyUp(button_t *b);
vec_t Cl_KeyState(button_t *key, uint32_t cmd_msec);

#endif /* __CL_LOCAL_H__ */
