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
int32_t Sv_InstallerFrame(const InstallerStatus *s);
void Sv_Frame(const uint32_t msec);

#if defined(__SV_LOCAL_H__)
extern Cvar *sv_demo_list;
extern Cvar *sv_enforce_time;
extern Cvar *sv_guid;
extern Cvar *sv_hostname;
extern Cvar *sv_map;
extern Cvar *sv_map_list;
extern Cvar *sv_map_list_shuffle;
extern Cvar *sv_master;
extern Cvar *sv_max_clients;
extern Cvar *sv_max_entities;
extern Cvar *sv_min_clients;
extern Cvar *sv_public;
extern Cvar *sv_stats_url;
extern Cvar *sv_timeout;

// per-level and static server structures
extern Server sv;
extern ServerStatic svs;

// current client / player edict
extern ServerClient *sv_client;
extern GameEntity *sv_player;

const char *Sv_StatusString(void);
const char *Sv_NetaddrToString(const ServerClient *cl);
void Sv_KickClient(ServerClient *cl, const char *msg);
void Sv_DropClient(ServerClient *cl);
bool Sv_UserInfoChanged(ServerClient *cl);

#endif
