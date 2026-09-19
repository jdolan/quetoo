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

#if defined(__G_LOCAL_H__)
void G_BlasterProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, uint32_t mod);
void G_BulletProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod);
void G_NailProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, uint32_t mod);
void G_ShotgunProjectiles(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod);
void G_HyperblasterProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback);
void G_GrenadeProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer, uint32_t mod);
void G_QuakeGrenadeProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer);
void G_RocketProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t mod);
void G_QuakeRocketProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius);
void G_LightningProjectile(GameEntity *ent, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, int32_t mod, int32_t discharge_mod);
void G_BeamProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, uint32_t mod, uint8_t trail, uint16_t sound);
GameEntity *G_FindBeamProjectile(const GameEntity *emitter);
void G_FreeBeamProjectile(GameEntity *emitter);
void G_RailgunProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t damage, int32_t knockback, uint32_t mod);
void G_BfgProjectile(GameEntity *emitter, GameEntity *attacker, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius);
void G_HandGrenadeProjectile(GameEntity *ent, GameEntity *projectile, const Vec3 start, const Vec3 dir, int32_t speed, int32_t damage, int32_t knockback, float damage_radius, uint32_t timer);
void G_GrenadeProjectile_Touch(GameEntity *ent, GameEntity *other, const CmTrace *trace);
#endif
