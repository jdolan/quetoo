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

#if defined(__CG_LOCAL_H__)
#if defined(G_HOOK)
float Cg_GetHookPullSpeed(void);
#endif

extern Cvar *cg_add_atmospheric;
extern Cvar *cg_add_decals;
extern Cvar *cg_add_entities;
extern Cvar *cg_add_flares;
extern Cvar *cg_add_lights;
extern Cvar *cg_add_sprites;
extern Cvar *cg_add_weather;
extern Cvar *cg_bob;
extern Cvar *cg_draw_blend;
extern Cvar *cg_draw_blend_damage;
extern Cvar *cg_draw_blend_liquid;
extern Cvar *cg_draw_blend_pickup;
extern Cvar *cg_draw_blend_powerup;
extern Cvar *cg_draw_crosshair;
extern Cvar *cg_draw_crosshair_alpha;
extern Cvar *cg_draw_crosshair_color;
extern Cvar *cg_draw_crosshair_health;
extern Cvar *cg_draw_crosshair_pulse;
extern Cvar *cg_draw_crosshair_scale;
extern Cvar *cg_draw_diagnostics;
extern Cvar *cg_draw_fps;
extern Cvar *cg_draw_hud;
extern Cvar *cg_draw_ping;
extern Cvar *cg_draw_ping_warn;
extern Cvar *cg_hud;
extern Cvar *cg_draw_target_name;
extern Cvar *cg_draw_weapon;
extern Cvar *cg_draw_weapon_alpha;
extern Cvar *cg_draw_weapon_bob;
extern Cvar *cg_draw_weapon_x;
extern Cvar *cg_draw_weapon_y;
extern Cvar *cg_draw_weapon_z;
extern Cvar *cg_draw_vitals_pulse;
extern Cvar *cg_entity_bob;
extern Cvar *cg_entity_rotate;
extern Cvar *cg_force_skin;
extern Cvar *cg_fov;
extern Cvar *cg_fov_zoom;
extern Cvar *cg_fov_interpolate;
extern Cvar *cg_hit_sound;
extern Cvar *cg_predict;
extern Cvar *cg_quick_join_max_ping;
extern Cvar *cg_quick_join_min_clients;
extern Cvar *cg_sprite_physics;
extern Cvar *cg_camera_mode;
extern Cvar *cg_third_person;
extern Cvar *cg_third_person_x;
extern Cvar *cg_third_person_y;
extern Cvar *cg_third_person_z;
extern Cvar *cg_third_person_pitch;
extern Cvar *cg_third_person_yaw;

extern Cvar *cg_auto_switch;
extern Cvar *cg_color;
extern Cvar *cg_hand;
extern Cvar *cg_helmet;
#if defined(G_HOOK)
extern Cvar *cg_hook_style;
#endif
extern Cvar *cg_pants;
extern Cvar *cg_shirt;
extern Cvar *cg_skin;

extern ClientGameImport cgi;

CGAME_EXPORT ClientGameExport *Cg_LoadCgame(ClientGameImport *import);

#endif
