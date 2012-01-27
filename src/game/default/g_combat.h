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

#ifndef __G_COMBAT_H__
#define __G_COMBAT_H__

#ifdef __G_LOCAL_H__

boolean_t G_CanDamage(g_edict_t *targ, g_edict_t *inflictor);
void G_Damage(g_edict_t *targ, g_edict_t *inflictor, g_edict_t *attacker, vec3_t dir,
		vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
boolean_t G_OnSameTeam(g_edict_t *ent1, g_edict_t *ent2);
void G_RadiusDamage(g_edict_t *inflictor, g_edict_t *attacker, g_edict_t *ignore,
		int damage, int knockback, float radius, int mod);

#endif /* __G_LOCAL_H__ */

#endif /* G_COMBAT_H_ */
