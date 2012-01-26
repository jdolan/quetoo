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

#ifndef __CL_MAIN_H__
#define __CL_MAIN_H__

#include "cl_types.h"

// settings and preferences
extern cvar_t *cl_async;
extern cvar_t *cl_bob;
extern cvar_t *cl_chat_sound;
extern cvar_t *cl_counters;
extern cvar_t *cl_fov;
extern cvar_t *cl_fov_zoom;
extern cvar_t *cl_ignore;
extern cvar_t *cl_max_fps;
extern cvar_t *cl_max_pps;
extern cvar_t *cl_net_graph;
extern cvar_t *cl_predict;
extern cvar_t *cl_team_chat_sound;
extern cvar_t *cl_third_person;
extern cvar_t *cl_timeout;
extern cvar_t *cl_view_size;
extern cvar_t *cl_weapon;
extern cvar_t *cl_weather;

// user_info
extern cvar_t *color;
extern cvar_t *message_level;
extern cvar_t *name;
extern cvar_t *password;
extern cvar_t *rate;
extern cvar_t *skin;

void Cl_LoadProgress(unsigned short percent);
void Cl_Disconnect(void);
void Cl_Frame(unsigned int msec);
void Cl_Init(void);
void Cl_Shutdown(void);

#ifdef __CL_LOCAL_H__

extern cvar_t *cl_add_entities;
extern cvar_t *cl_add_particles;
extern cvar_t *cl_add_emits;
extern cvar_t *cl_show_prediction_misses;
extern cvar_t *cl_show_net_messages;
extern cvar_t *cl_show_renderer_stats;

extern cvar_t *rcon_password;
extern cvar_t *rcon_address;

extern cvar_t *recording;

void Cl_SendDisconnect(void);
void Cl_Reconnect_f(void);
void Cl_RequestNextDownload(void);

#endif /* __CL_LOCAL_H__ */

#endif /* __CL_MAIN_H__ */

