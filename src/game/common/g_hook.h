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

#include "g_types.h"
#include "bg_pmove.h"

#if defined(__G_LOCAL_H__)

extern cvar_t *g_hook;
extern cvar_t *g_hook_auto_refire;
extern cvar_t *g_hook_distance;
extern cvar_t *g_hook_pull_speed;
extern cvar_t *g_hook_refire;
extern cvar_t *g_hook_sky;
extern cvar_t *g_hook_speed;
extern cvar_t *g_hook_style;

void G_Hook_Init(void);
void G_Hook_CheckState(void);

void G_HookDetach(g_client_t *cl);
void G_HookThink(g_client_t *cl, const bool refire);
g_entity_t *G_HookProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir);
void G_SetClientHookStyle(g_client_t *cl);

#endif
