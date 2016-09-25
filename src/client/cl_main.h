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

#ifndef __CL_MAIN_H__
#define __CL_MAIN_H__

#include "cl_types.h"

extern cvar_t *cl_async;
extern cvar_t *cl_chat_sound;
extern cvar_t *cl_draw_counters;
extern cvar_t *cl_draw_net_graph;
extern cvar_t *cl_editor;
extern cvar_t *cl_ignore;
extern cvar_t *cl_max_fps;
extern cvar_t *cl_max_pps;
extern cvar_t *cl_predict;
extern cvar_t *cl_team_chat_sound;
extern cvar_t *cl_timeout;
extern cvar_t *cl_view_size;

// user_info
extern cvar_t *active;
extern cvar_t *color;
extern cvar_t *hand;
extern cvar_t *message_level;
extern cvar_t *name;
extern cvar_t *password;
extern cvar_t *rate;
extern cvar_t *skin;
extern cvar_t *handicap;

void Cl_Disconnect(void);
void Cl_Frame(const uint32_t msec);
void Cl_Init(void);
void Cl_Shutdown(void);

#ifdef __CL_LOCAL_H__

extern cvar_t *qport;

extern cvar_t *rcon_password;
extern cvar_t *rcon_address;

extern cvar_t *cl_show_net_messages;
extern cvar_t *cl_show_renderer_stats;
extern cvar_t *cl_show_sound_stats;

extern cl_client_t cl;
extern cl_static_t cls;

void Cl_SendDisconnect(void);
void Cl_Reconnect_f(void);

#endif /* __CL_LOCAL_H__ */

#endif /* __CL_MAIN_H__ */

