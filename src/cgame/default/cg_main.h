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

#include "cg_types.h"

#ifdef __CG_LOCAL_H__

/**
 * @brief Local state to the cgame per server.
 */
typedef struct {
	vec_t hook_pull_speed;
	g_gameplay_t gameplay;
	uint8_t teams;
	_Bool ctf;
	_Bool match;
	uint8_t maxclients;
	uint8_t numclients;
} cg_state_t;

extern cg_state_t cg_state;

extern cvar_t *cg_add_emits;
extern cvar_t *cg_add_entities;
extern cvar_t *cg_add_particles;
extern cvar_t *cg_add_weather;
extern cvar_t *cg_bob;
extern cvar_t *cg_color;
extern cvar_t *cg_draw_blend;
extern cvar_t *cg_draw_blend_damage;
extern cvar_t *cg_draw_blend_liquid;
extern cvar_t *cg_draw_blend_pickup;
extern cvar_t *cg_draw_blend_powerup;
extern cvar_t *cg_draw_captures;
extern cvar_t *cg_draw_crosshair_color;
extern cvar_t *cg_draw_crosshair_pulse;
extern cvar_t *cg_draw_crosshair_scale;
extern cvar_t *cg_draw_crosshair;
extern cvar_t *cg_draw_held_flag;
extern cvar_t *cg_draw_held_tech;
extern cvar_t *cg_draw_frags;
extern cvar_t *cg_draw_deaths;
extern cvar_t *cg_draw_hud;
extern cvar_t *cg_draw_pickup;
extern cvar_t *cg_draw_powerups;
extern cvar_t *cg_draw_time;
extern cvar_t *cg_draw_target_name;
extern cvar_t *cg_draw_team_banner;
extern cvar_t *cg_draw_weapon;
extern cvar_t *cg_draw_weapon_alpha;
extern cvar_t *cg_draw_weapon_bob;
extern cvar_t *cg_draw_weapon_x;
extern cvar_t *cg_draw_weapon_y;
extern cvar_t *cg_draw_weapon_z;
extern cvar_t *cg_draw_vitals;
extern cvar_t *cg_draw_vitals_pulse;
extern cvar_t *cg_draw_vote;
extern cvar_t *cg_entity_bob;
extern cvar_t *cg_entity_pulse;
extern cvar_t *cg_entity_rotate;
extern cvar_t *cg_fov;
extern cvar_t *cg_fov_zoom;
extern cvar_t *cg_fov_interpolate;
extern cvar_t *cg_hand;
extern cvar_t *cg_handicap;
extern cvar_t *cg_helmet;
extern cvar_t *cg_hit_sound;
extern cvar_t *cg_hook_style;
extern cvar_t *cg_pants;
extern cvar_t *cg_particle_quality;
extern cvar_t *cg_predict;
extern cvar_t *cg_quick_join_max_ping;
extern cvar_t *cg_quick_join_min_clients;
extern cvar_t *cg_shirt;
extern cvar_t *cg_skin;
extern cvar_t *cg_third_person;
extern cvar_t *cg_third_person_chasecam;
extern cvar_t *cg_third_person_x;
extern cvar_t *cg_third_person_y;
extern cvar_t *cg_third_person_z;
extern cvar_t *cg_third_person_pitch;
extern cvar_t *cg_third_person_yaw;

extern cvar_t *g_gameplay;
extern cvar_t *g_teams;
extern cvar_t *g_ctf;
extern cvar_t *g_match;
extern cvar_t *g_ai_max_clients;

extern cg_import_t cgi;

cg_export_t *Cg_LoadCgame(cg_import_t *import);

#endif /* __CG_LOCAL_H__ */
