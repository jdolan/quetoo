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

#include "cm_types.h"

int32_t Cm_ClusterPVS(const int32_t cluster, byte *pvs);
int32_t Cm_ClusterPHS(const int32_t cluster, byte *phs);

void Cm_SetAreaPortalState(const int32_t portal_num, const _Bool open);
_Bool Cm_AreasConnected(const int32_t area1, const int32_t area2);

int32_t Cm_WriteAreaBits(const int32_t area, byte *out);
_Bool Cm_HeadnodeVisible(const int32_t head_node, const byte *vis);

extern _Bool cm_no_areas;
extern _Bool cm_no_vis;

#ifdef __CM_LOCAL_H__
void Cm_FloodAreas(void);
#endif /* __CM_LOCAL_H__ */
