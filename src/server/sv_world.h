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
int Sv_AreaEdicts(vec3_t mins, vec3_t maxs, g_edict_t **list, int maxcount, int areatype);
int Sv_PointContents(vec3_t p);
c_trace_t Sv_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, g_edict_t *passedict, int contentmask);

#endif /* __SV_LOCAL_H__ */

#endif /* __SV_WORLD_H__ */
