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

#ifndef __SV_SEND_H__
#define __SV_SEND_H__

#include "sv_types.h"

#ifdef __SV_LOCAL_H__
typedef enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} sv_redirect_t;

#define SV_OUTPUTBUF_LENGTH	(MAX_MSG_SIZE - 16)
extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void Sv_FlushRedirect(int target, char *outputbuf);
void Sv_SendClientMessages(void);
void Sv_Unicast(g_edict_t *ent, boolean_t reliable);
void Sv_Multicast(vec3_t origin, multicast_t to);
void Sv_PositionedSound(vec3_t origin, g_edict_t *entity, unsigned short index, unsigned short atten);
void Sv_ClientPrint(g_edict_t *ent, int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Sv_ClientCenterPrint(g_edict_t *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastPrint(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastCommand(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif /* __SV_LOCAL_H__ */

#endif /* __SV_SEND_H__ */
