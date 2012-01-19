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

#ifndef __CL_EFFECT_H__
#define __CL_EFFECT_H__

#include "cl_types.h"

#ifdef __CL_LOCAL_H__
void Cl_BubbleTrail(const vec3_t start, const vec3_t end, float density);
void Cl_EntityEvent(entity_state_t *ent);
void Cl_TracerEffect(const vec3_t start, const vec3_t end);
void Cl_BulletEffect(const vec3_t org, const vec3_t dir);
void Cl_BurnEffect(const vec3_t org, const vec3_t dir, int scale);
void Cl_BloodEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_GibEffect(const vec3_t org, int count);
void Cl_SmokeFlash(entity_state_t *ent);
void Cl_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent);
void Cl_ExplosionEffect(const vec3_t org);
void Cl_ItemRespawnEffect(const vec3_t org);
void Cl_ItemPickupEffect(const vec3_t org);
void Cl_TeleporterTrail(const vec3_t org, cl_entity_t *ent);
void Cl_LogoutEffect(const vec3_t org);
void Cl_SparksEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_EnergyTrail(cl_entity_t *ent, float radius, int color);
void Cl_EnergyFlash(entity_state_t *ent, int color, int count);
void Cl_RocketTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_GrenadeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_HyperblasterTrail(cl_entity_t *ent);
void Cl_HyperblasterEffect(const vec3_t org);
void Cl_LightningEffect(const vec3_t org);
void Cl_LightningTrail(const vec3_t start, const vec3_t end);
void Cl_RailEffect(const vec3_t start, const vec3_t end, int flags, int color);
void Cl_BfgTrail(cl_entity_t *ent);
void Cl_BfgEffect(const vec3_t org);
void Cl_LoadEffects(void);
void Cl_AddParticles(void);
r_particle_t *Cl_AllocParticle(void);
void Cl_ClearEffects(void);
#endif /* __CL_LOCAL_H__ */

#endif /* __CL_EFFECT_H__ */

