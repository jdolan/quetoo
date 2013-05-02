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

#ifndef __GAME_WEAPON_H__
#define __GAME_WEAPON_H__

#include "g_types.h"

#ifdef __GAME_LOCAL_H__
void G_ChangeWeapon(g_edict_t *ent);
_Bool G_PickupWeapon(g_edict_t *ent, g_edict_t *other);
void G_UseBestWeapon(g_client_t *client);
void G_UseWeapon(g_edict_t *ent, const g_item_t *item);
void G_DropWeapon(g_edict_t *ent, const g_item_t *item);
void G_TossWeapon(g_edict_t *ent);
void G_FireBlaster(g_edict_t *ent);
void G_FireShotgun(g_edict_t *ent);
void G_FireSuperShotgun(g_edict_t *ent);
void G_FireMachinegun(g_edict_t *ent);
void G_FireHyperblaster(g_edict_t *ent);
void G_FireRocketLauncher(g_edict_t *ent);
void G_FireGrenadeLauncher(g_edict_t *ent);
void G_FireLightning(g_edict_t *ent);
void G_FireRailgun(g_edict_t *ent);
void G_FireBfg(g_edict_t *ent);
void G_ClientWeaponThink(g_edict_t *ent);
#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_WEAPON_H__ */
