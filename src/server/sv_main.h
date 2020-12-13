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

void Sv_Init(void);
void Sv_Shutdown(const char *msg);
void Sv_Frame(const uint32_t msec);

#ifdef __SV_LOCAL_H__
// cvars
extern cvar_t *sv_demo_list;
extern cvar_t *sv_download_url;
extern cvar_t *sv_enforce_time;
extern cvar_t *sv_hostname;
extern cvar_t *sv_max_clients;
extern cvar_t *sv_public;
extern cvar_t *sv_rcon_password;
extern cvar_t *sv_timeout;
extern cvar_t *sv_udp_download;

// per-level and static server structures
extern sv_server_t sv;
extern sv_static_t svs;

// current client / player edict
extern sv_client_t *sv_client;
extern g_entity_t *sv_player;

const char *Sv_StatusString(void);
const char *Sv_NetaddrToString(const sv_client_t *cl);
void Sv_KickClient(sv_client_t *cl, const char *msg);
void Sv_DropClient(sv_client_t *cl);
void Sv_UserInfoChanged(sv_client_t *cl);

#endif /* __SV_LOCAL_H__ */
