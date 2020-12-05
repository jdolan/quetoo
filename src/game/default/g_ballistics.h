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

#ifdef __GAME_LOCAL_H__
void G_Ripple(g_entity_t *ent, const vec3_t pos1, const vec3_t pos2, const float size, _Bool splash);

void G_BlasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                         int32_t speed, int32_t damage, int32_t knockback);
void G_BulletProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                        int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod);
void G_ShotgunProjectiles(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                          int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod);
void G_HyperblasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                              int32_t speed, int32_t damage, int32_t knockback);
void G_GrenadeProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                         int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer);
void G_RocketProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                        int32_t speed, int32_t damage, int32_t knockback, float damage_radius);
void G_LightningProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                           int32_t damage, int32_t knockback);
void G_RailgunProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                         int32_t damage, int32_t knockback);
void G_BfgProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir,
                     int32_t speed, int32_t damage, int32_t knockback, float damage_radius);
void G_HandGrenadeProjectile(g_entity_t *ent, g_entity_t *projectile, const vec3_t start,
                             const vec3_t dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius,
                             uint32_t timer);
void G_GrenadeProjectile_Touch(g_entity_t *self, g_entity_t *other,
                               const cm_bsp_plane_t *plane, const cm_bsp_texinfo_t *texinfo);
g_entity_t *G_HookProjectile(g_entity_t *self, const vec3_t start, const vec3_t dir);
#endif /* __GAME_LOCAL_H__ */
