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

#ifdef __SV_LOCAL_H__
void Sv_InitWorld(void);
void Sv_LinkEntity(g_entity_t *ent);
void Sv_UnlinkEntity(g_entity_t *ent);
size_t Sv_BoxEntities(const box3_t bounds, g_entity_t **list, const size_t len,
                      const uint32_t type);
int32_t Sv_PointContents(const vec3_t p);
cm_trace_t Sv_Trace(const vec3_t start, const vec3_t end, const box3_t bounds,
                    const g_entity_t *skip, const int32_t contents);
cm_trace_t Sv_Clip(const vec3_t start, const vec3_t end, const box3_t bounds,
                   const g_entity_t *test, const int32_t contents);

#endif /* __SV_LOCAL_H__ */
