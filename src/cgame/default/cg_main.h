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

#ifndef __CG_MAIN_H__
#define __CG_MAIN_H__

#include "cg_types.h"

#ifdef __CG_LOCAL_H__

extern cvar_t *cg_add_emits;
extern cvar_t *cg_add_entities;
extern cvar_t *cg_add_particles;
extern cvar_t *cg_add_weather;
extern cvar_t *cg_bob;
extern cvar_t *cg_draw_blend;
extern cvar_t *cg_draw_captures;
extern cvar_t *cg_draw_crosshair_color;
extern cvar_t *cg_draw_crosshair_pulse;
extern cvar_t *cg_draw_crosshair_scale;
extern cvar_t *cg_draw_crosshair;
extern cvar_t *cg_draw_frags;
extern cvar_t *cg_draw_deaths;
extern cvar_t *cg_draw_hud;
extern cvar_t *cg_draw_pickup;
extern cvar_t *cg_draw_time;
extern cvar_t *cg_draw_teambar;
extern cvar_t *cg_draw_weapon;
extern cvar_t *cg_draw_weapon_alpha;
extern cvar_t *cg_draw_weapon_x;
extern cvar_t *cg_draw_weapon_y;
extern cvar_t *cg_draw_weapon_z;
extern cvar_t *cg_draw_vitals;
extern cvar_t *cg_draw_vitals_pulse;
extern cvar_t *cg_draw_vote;
extern cvar_t *cg_fov;
extern cvar_t *cg_fov_zoom;
extern cvar_t *cg_fov_interpolate;
extern cvar_t *cg_third_person;
extern cvar_t *cg_third_person_yaw;

extern cg_import_t cgi;

cg_export_t *Cg_LoadCgame(cg_import_t *import);

#endif /* __CG_LOCAL_H__ */

#endif /* __CG_MAIN_H__ */

