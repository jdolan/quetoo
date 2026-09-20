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
void Sv_SpawnEntities(const char *name, const CmEntity *props);
void Sv_LinkEntity(GameEntity *ent);
void Sv_UnlinkEntity(GameEntity *ent);
size_t Sv_BoxEntities(const Box3 bounds, GameEntity **list, size_t len, uint32_t type);
int32_t Sv_PointContents(const Vec3 p);
int32_t Sv_BoxContents(const Box3 bounds);
CmTrace Sv_Trace(const Vec3 start, const Vec3 end, const Box3 bounds, const GameEntity *skip, int32_t contents);
CmTrace Sv_Clip(const Vec3 start, const Vec3 end, const Box3 bounds, const GameEntity *test, int32_t contents);

#endif
