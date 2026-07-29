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

#if defined(__GAME_LOCAL_H__)
bool G_OnSameTeam(const g_client_t *a, const g_client_t *b);
bool G_CanDamage(const g_entity_t *targ, const g_entity_t *inflictor);
vec3_t G_GetOrigin(const g_entity_t *ent);

void G_Damage(const g_damage_t *damage);

void G_RadiusDamage(g_entity_t *inflictor, g_entity_t *attacker, g_entity_t *ignore, int32_t damage,
                    int32_t knockback, float radius, g_means_of_death mod);
#endif /* __GAME_LOCAL_H__ */
