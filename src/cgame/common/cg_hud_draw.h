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

/**
 * @brief Shared HUD drawing primitives.
 *
 * @details These know nothing about which stats a module shows or how it lays
 * them out; they draw what they are handed. Composition - which vitals, which
 * counters, in what order - belongs to each module's own cg_hud.c, because the
 * HUD is the most visible thing a mod changes.
 */

#define HUD_COLOR_STAT      color_white
#define HUD_COLOR_STAT_MED  color_yellow
#define HUD_COLOR_STAT_LOW  color_red

#define HUD_PIC_HEIGHT      64

#define HUD_HEALTH_MED      75
#define HUD_HEALTH_LOW      25

#define HUD_ARMOR_MED       50
#define HUD_ARMOR_LOW       25

#define HUD_POWERUP_LOW     5

typedef struct {

  struct {
    uint32_t time;
    int16_t pickup;
  } pulse;

  struct {
    uint32_t hit_sound_time;
  } damage;

  struct {
    uint32_t pickup_time;
    uint32_t damage_time;
    int16_t pickup;
  } blend;

  struct {
    int16_t bit, used_bit;
    uint32_t time, bar_time;
    int16_t num;
    bool has[WEAPON_TOTAL];
  } weapon;

  int16_t chase_target;
} cg_hud_state_t;

extern cg_hud_state_t cg_hud_state;

extern r_image_t *cg_pickup_blend_image;
extern r_image_t *cg_quad_blend_image;
extern r_image_t *cg_invisibility_blend_image;
extern r_image_t *cg_invulnerability_blend_image;
extern r_image_t *cg_damage_blend_image;
extern r_image_t *cg_select_weapon_image;
extern cvar_t *cg_select_weapon_alpha;
extern cvar_t *cg_select_weapon_delay;
extern cvar_t *cg_select_weapon_fade;
extern cvar_t *cg_select_weapon_interval;

void Cg_DrawIcon(const int32_t x, const int32_t y, const r_image_t *image, const color_t color);
void Cg_DrawVital(int32_t x, int32_t ch, const int16_t value, const r_image_t *icon, int16_t med, int16_t low);
int32_t Cg_DrawPowerup(int32_t y, const int16_t value, const r_image_t *icon, const int32_t ch);
void Cg_DrawBlend(const player_state_t *ps);
void Cg_DrawCrosshair(const player_state_t *ps);
void Cg_DrawCenterPrint(const player_state_t *ps);
void Cg_DrawTargetName(const player_state_t *ps);
void Cg_DrawSelectWeapon(const player_state_t *ps);
bool Cg_AttemptSelectWeapon(const player_state_t *ps);
void Cg_ParseCenterPrint(void);
void Cg_InitHud(void);
void Cg_LoadHudMedia(void);
void Cg_ClearHud(void);

#endif
