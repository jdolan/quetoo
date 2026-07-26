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
bool G_PickupWeapon(g_client_t *cl, g_entity_t *other);
void G_UseBestWeapon(g_client_t *cl);
void G_UseWeapon(g_client_t *cl, const g_item_t *item);
g_entity_t *G_DropWeapon(g_client_t *cl, const g_item_t *item);
g_entity_t *G_TossWeapon(g_client_t *cl);
/**
 * @brief Return true when the attack input, refire timer, and ammunition allow
 * the active weapon to fire. Custom weapon Think callbacks should call this
 * before creating their projectile or trace.
 */
bool G_FireWeapon(g_client_t *cl);
/**
 * @brief Apply common post-fire bookkeeping: animation, mode timing
 * modifiers, refire time, ammunition consumption, and Quad feedback.
 */
void G_WeaponFired(g_client_t *cl, uint32_t interval, uint32_t ammo_needed);
/** @brief Publish a standard muzzle flash for a weapon firing event. */
void G_MuzzleFlash(g_entity_t *ent, g_muzzle_flash_t flash);
void G_FireBlaster(g_client_t *cl);
void G_FireShotgun(g_client_t *cl);
void G_FireSuperShotgun(g_client_t *cl);
void G_FireMachinegun(g_client_t *cl);
void G_FireHyperblaster(g_client_t *cl);
void G_FireRocketLauncher(g_client_t *cl);
void G_FireHandGrenade(g_client_t *cl);
void G_FireGrenadeLauncher(g_client_t *cl);
void G_FireLightning(g_client_t *cl);
void G_FireRailgun(g_client_t *cl);
void G_FireBfg(g_client_t *cl);
void G_FireQuakeShotgun(g_client_t *cl);
void G_FireQuakeSuperShotgun(g_client_t *cl);
void G_FireQuakeNailgun(g_client_t *cl);
void G_FireQuakeSuperNailgun(g_client_t *cl);
void G_FireQuakeGrenadeLauncher(g_client_t *cl);
void G_FireQuakeRocketLauncher(g_client_t *cl);
void G_FireQuakeThunderbolt(g_client_t *cl);
void G_ClientWeaponThink(g_client_t *cl);
void G_HookDetach(g_client_t *cl);
void G_HookThink(g_client_t *cl, const bool refire);
#endif /* __GAME_LOCAL_H__ */
