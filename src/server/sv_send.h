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
void Sv_SendClientPackets(void);
void Sv_Unicast(const g_entity_t *ent, const _Bool reliable);
void Sv_Multicast(const vec3_t origin, multicast_t to, EntityFilterFunc filter);
void Sv_PositionedSound(const vec3_t origin, const g_entity_t *ent, int32_t index, sound_atten_t atten, int8_t pitch);
void Sv_ClientPrint(const g_entity_t *ent, int32_t level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Sv_BroadcastPrint(int32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastCommand(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif /* __SV_LOCAL_H__ */
