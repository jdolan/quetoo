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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "g_types.h"

#if defined(__G_LOCAL_H__)
bool G_PickupWeapon(GameClient *cl, GameEntity *other);
void G_UseBestWeapon(GameClient *cl);
void G_UseWeapon(GameClient *cl, const GameItem *item);
GameEntity *G_DropWeapon(GameClient *cl, const GameItem *item);
GameEntity *G_TossWeapon(GameClient *cl);
void G_FireBlaster(GameClient *cl);
void G_FireShotgun(GameClient *cl);
void G_FireSuperShotgun(GameClient *cl);
void G_FireMachinegun(GameClient *cl);
void G_FireHyperblaster(GameClient *cl);
void G_FireRocketLauncher(GameClient *cl);
void G_FireHandGrenade(GameClient *cl);
void G_FireGrenadeLauncher(GameClient *cl);
void G_FireLightning(GameClient *cl);
void G_FireRailgun(GameClient *cl);
void G_FireBfg(GameClient *cl);
void G_FireQuakeShotgun(GameClient *cl);
void G_FireQuakeSuperShotgun(GameClient *cl);
void G_FireQuakeNailgun(GameClient *cl);
void G_FireQuakeSuperNailgun(GameClient *cl);
void G_FireQuakeGrenadeLauncher(GameClient *cl);
void G_FireQuakeRocketLauncher(GameClient *cl);
void G_FireQuakeThunderbolt(GameClient *cl);
void G_WorldMuzzleFlash(const Vec3 org, const Vec3 dir, GameMuzzleFlash flash, uint8_t client);
void G_ClientWeaponThink(GameClient *cl);
#endif
