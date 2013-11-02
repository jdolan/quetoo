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

#include "g_local.h"

/*
 * @brief Returns true if ent1 and ent2 are on the same team.
 */
_Bool G_OnSameTeam(const g_edict_t *ent1, const g_edict_t *ent2) {

	if (!ent1->client || !ent2->client)
		return false;

	if (ent1->client->locals.persistent.spectator && ent2->client->locals.persistent.spectator)
		return true;

	if (!g_level.teams && !g_level.ctf)
		return false;

	return ent1->client->locals.persistent.team == ent2->client->locals.persistent.team;
}

/*
 * @brief Returns true if the inflictor can directly damage the target. Used for
 * explosions and melee attacks.
 */
_Bool G_CanDamage(g_edict_t *targ, g_edict_t *inflictor) {
	vec3_t dest;
	c_trace_t tr;

	// BSP sub-models need special checking because their origin is 0,0,0
	if (targ->locals.move_type == MOVE_TYPE_PUSH) {
		VectorAdd(targ->abs_mins, targ->abs_maxs, dest);
		VectorScale(dest, 0.5, dest);
		tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
		if (tr.fraction == 1.0)
			return true;
		if (tr.ent == targ)
			return true;
		return false;
	}

	tr = gi.Trace(inflictor->s.origin, targ->s.origin, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	tr = gi.Trace(inflictor->s.origin, dest, NULL, NULL, inflictor, MASK_SOLID);
	if (tr.fraction == 1.0)
		return true;

	return false;
}

/*
 * @brief
 */
static void G_SpawnDamage(g_temp_entity_t type, const vec3_t pos, const vec3_t normal,
		int16_t damage) {

	if (damage < 1)
		return;

	int16_t count = Clamp(damage / 50, 1, 4);

	while (count--) {
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(type);
		gi.WritePosition(pos);
		gi.WriteDir(normal);
		gi.Multicast(pos, MULTICAST_PVS);
	}
}

/*
 * @brief
 */
static int32_t G_CheckArmor(g_edict_t *ent, const vec3_t pos, const vec3_t normal, int16_t damage,
		int32_t dflags) {

	g_client_t *client;
	int32_t saved;

	if (damage < 1)
		return 0;

	if (damage < 2) // sometimes protect very small damage
		damage = Random() & 1;
	else
		damage *= 0.80; // mostly protect large damage

	client = ent->client;

	if (!client)
		return 0;

	if (dflags & DAMAGE_NO_ARMOR)
		return 0;

	if (damage > ent->client->locals.persistent.armor)
		saved = ent->client->locals.persistent.armor;
	else
		saved = damage;

	ent->client->locals.persistent.armor -= saved;

	G_SpawnDamage(TE_BLOOD, pos, normal, saved);

	return saved;
}

#define QUAD_DAMAGE_FACTOR 2.5
#define QUAD_KNOCKBACK_FACTOR 2.0

/*
 * @brief Damage routine. The inflictor imparts damage on the target on behalf
 * of the attacker.
 *
 * @param target The target may receive damage.
 * @param inflictor The entity inflicting the damage (projectile).
 * @param attacker The entity taking credit for the damage (client).
 * @param dir The direction of the attack.
 * @param pos The point at which damage is being inflicted.
 * @param normal The normal vector from that point.
 * @param damage The damage to be inflicted.
 * @param knockback Velocity added to target in the direction of the normal.
 * @param dflags Damage flags:
 *
 *  DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
 * 	DAMAGE_NO_ARMOR			armor does not protect from this damage
 * 	DAMAGE_ENERGY			damage is from an energy based weapon
 * 	DAMAGE_BULLET			damage is from a bullet
 * 	DAMAGE_NO_PROTECTION	kills god mode, armor, everything
 *
 * @param mod The means of death, used by the obituaries routine.
 */
void G_Damage(g_edict_t *target, g_edict_t *inflictor, g_edict_t *attacker, const vec3_t dir,
		const vec3_t pos, const vec3_t normal, int16_t damage, int16_t knockback, uint32_t dflags,
		uint32_t mod) {

	if (!target->locals.take_damage)
		return;

	if (target->client) {
		if (target->client->locals.respawn_protection_time > g_level.time)
			return;
	}

	if (!inflictor)
		inflictor = g_game.edicts;

	if (!attacker)
		attacker = g_game.edicts;

	if (attacker->client) {
		if (attacker->client->locals.persistent.inventory[g_level.media.quad_damage]) {
			damage *= QUAD_DAMAGE_FACTOR;
			knockback *= QUAD_KNOCKBACK_FACTOR;
		}
	}

	// friendly fire avoidance
	if (target != attacker && (g_level.teams || g_level.ctf)) {
		if (G_OnSameTeam(target, attacker)) { // target and attacker are on same team

			if (mod == MOD_TELEFRAG) { // telefrags can not be avoided
				mod |= MOD_FRIENDLY_FIRE;
			} else { // while everything else can
				if (g_friendly_fire->value)
					mod |= MOD_FRIENDLY_FIRE;
				else
					damage = 0;
			}
		}
	}

	// there is no self damage in instagib or arena, but there is knockback
	if (target == attacker && g_level.gameplay != GAME_DEATHMATCH)
		damage = 0;

	means_of_death = mod;

	g_client_t *client = target->client;

	// calculate velocity change due to knockback
	if (knockback && (target->locals.move_type == MOVE_TYPE_WALK)) {
		vec3_t knockback_vel, ndir;

		VectorCopy(dir, ndir);
		VectorNormalize(ndir);

		vec_t mass = Clamp(target->locals.mass, 50.0, 999.0);
		vec_t scale = 1000.0; // default knockback scale

		if (target == attacker) { // weapon jump hacks
			if (mod == MOD_BFG_BLAST)
				scale = 300.0;
			else if (mod == MOD_ROCKET_SPLASH)
				scale = 1400.0;
			else if (mod == MOD_GRENADE)
				scale = 1200.0;
		}

		VectorScale(ndir, scale * knockback / mass, knockback_vel);
		VectorAdd(target->locals.velocity, knockback_vel, target->locals.velocity);
	}

	int16_t inflicted = damage;
	int16_t saved = 0;

	// check for god mode
	if ((target->locals.flags & FL_GOD_MODE) && !(dflags & DAMAGE_NO_PROTECTION)) {
		inflicted = 0;
		saved = damage;
		G_SpawnDamage(TE_BLOOD, pos, normal, saved);
	}

	int16_t saved_armor = G_CheckArmor(target, pos, normal, inflicted, dflags);
	inflicted -= saved_armor;

	// treat cheat savings the same as armor
	saved_armor += saved;

	// do the damage
	if (inflicted) {
		if (client)
			G_SpawnDamage(TE_BLOOD, pos, normal, inflicted);
		else {
			// impact effects for things we can hurt which shouldn't bleed
			if (dflags & DAMAGE_BULLET)
				G_SpawnDamage(TE_BULLET, pos, normal, inflicted);
			else
				G_SpawnDamage(TE_SPARKS, pos, normal, inflicted);
		}

		target->locals.health = target->locals.health - inflicted;

		if (target->locals.health <= 0) {
			if (target->locals.Die) {
				target->locals.Die(target, inflictor, attacker, inflicted, pos);
			} else {
				gi.Debug("No die function for %s\n", target->class_name);
			}
			return;
		}
	}

	// invoke the pain callback
	if ((inflicted || knockback) && target->locals.Pain)
		target->locals.Pain(target, attacker, inflicted, knockback);

	// add to the damage inflicted on a player this frame
	if (client) {
		client->locals.damage_armor += saved_armor;
		client->locals.damage_health += inflicted;

		vec_t kick = (saved_armor + inflicted) / 30.0;

		if (kick > 1.0) {
			kick = 1.0;
		}

		G_ClientDamageKick(target, dir, kick);

		if (attacker->client && attacker->client != client) {
			attacker->client->locals.damage_inflicted += inflicted;
		}
	}
}

/*
 * @brief
 */
void G_RadiusDamage(g_edict_t *inflictor, g_edict_t *attacker, g_edict_t *ignore, int16_t damage,
		int16_t knockback, vec_t radius, uint32_t mod) {

	g_edict_t *ent = NULL;

	while ((ent = G_FindRadius(ent, inflictor->s.origin, radius)) != NULL) {
		vec3_t dir;

		if (ent == ignore)
			continue;

		if (!ent->locals.take_damage)
			continue;

		VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
		const vec_t dist = VectorNormalize(dir);

		vec_t d = damage - 0.5 * dist;
		const vec_t k = knockback - 0.5 * dist;

		if (d <= 0 && k <= 0) // too far away to be damaged
			continue;

		if (ent == attacker) { // reduce self damage
			if (mod == MOD_BFG_BLAST)
				d = d * 0.25;
			else
				d = d * 0.5;
		}

		if (!G_CanDamage(ent, inflictor))
			continue;

		G_Damage(ent, inflictor, attacker, dir, ent->s.origin, vec3_origin, (int32_t) d,
				(int32_t) k, DAMAGE_RADIUS, mod);
	}
}
