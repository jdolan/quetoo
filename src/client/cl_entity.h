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

/**
 * @brief For delta compression to be valid, the player's origin must be within
 * this distance from the previous frame. This is MAX_SPEED for one tick.
 */
#define MAX_DELTA_ORIGIN (2400.f * QUETOO_TICK_SECONDS)

void Cl_ParseFrame(void);
void Cl_Interpolate(void);
#endif /* __CL_LOCAL_H__ */
