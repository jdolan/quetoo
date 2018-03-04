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
void G_PlayTechSound(g_entity_t *ent);
_Bool G_PickupWeapon(g_entity_t *ent, g_entity_t *other);
void G_UseBestWeapon(g_entity_t *ent);
void G_UseWeapon(g_entity_t *ent, const g_item_t *item);
g_entity_t *G_DropWeapon(g_entity_t *ent, const g_item_t *item);
g_entity_t *G_TossWeapon(g_entity_t *ent);
void G_FireBlaster(g_entity_t *ent);
void G_FireShotgun(g_entity_t *ent);
void G_FireSuperShotgun(g_entity_t *ent);
void G_FireMachinegun(g_entity_t *ent);
void G_FireHyperblaster(g_entity_t *ent);
void G_FireRocketLauncher(g_entity_t *ent);
void G_FireHandGrenade(g_entity_t *ent);
void G_FireGrenadeLauncher(g_entity_t *ent);
void G_FireLightning(g_entity_t *ent);
void G_FireRailgun(g_entity_t *ent);
void G_FireBfg(g_entity_t *ent);
void G_ClientWeaponThink(g_entity_t *ent);
void G_ClientHookDetach(g_entity_t *ent);
void G_ClientHookThink(g_entity_t *ent, const _Bool refire);
#endif /* __GAME_LOCAL_H__ */
