/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __SV_WORLD_H__
#define __SV_WORLD_H__

#include "sv_types.h"

#ifdef __SV_LOCAL_H__
void Sv_InitWorld(void);
void Sv_LinkEdict(g_edict_t *ent);
void Sv_UnlinkEdict(g_edict_t *ent);
int32_t Sv_AreaEdicts(const vec3_t mins, const vec3_t maxs, g_edict_t **area_edicts, int32_t max_area_edicts, int32_t area_type);
int32_t Sv_PointContents(const vec3_t p);
c_trace_t Sv_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const g_edict_t *skip, const int32_t mask);

#endif /* __SV_LOCAL_H__ */

#endif /* __SV_WORLD_H__ */
