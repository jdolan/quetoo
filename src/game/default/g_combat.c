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

	// bmodels need special checking because their origin is 0,0,0
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
static void G_SpawnDamage(int32_t type, vec3_t origin, vec3_t normal, int32_t damage) {

	while (damage > 0) {
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(type);
		gi.WritePosition(origin);
		gi.WriteDir(normal);
		gi.Multicast(origin, MULTICAST_PVS);

		damage -= 50;
	}
}

/*
 * @brief
 */
static int32_t G_CheckArmor(g_edict_t *ent, vec3_t point, vec3_t normal, int32_t damage,
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

	G_SpawnDamage(TE_BLOOD, point, normal, saved);

	return saved;
}

#define QUAD_DAMAGE_FACTOR 2.5
#define QUAD_KNOCKBACK_FACTOR 2.0

/*
 * @brief targ		entity that is being damaged
 * inflictor	entity that is causing the damage
 * attacker	entity that caused the inflictor to damage targ
 * 	example: targ=player2, inflictor=rocket, attacker=player1
 *
 * dir			direction of the attack
 * point       point at which the damage is being inflicted
 * normal		normal vector from that point
 * damage		amount of damage being inflicted
 * knockback	force to be applied against targ as a result of the damage
 *
 * dflags		these flags are used to control how G_Damage works
 * 	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
 * 	DAMAGE_NO_ARMOR			armor does not protect from this damage
 * 	DAMAGE_ENERGY			damage is from an energy based weapon
 * 	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
 * 	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
 */
void G_Damage(g_edict_t *targ, g_edict_t *inflictor, g_edict_t *attacker, vec3_t dir, vec3_t point,
		vec3_t normal, int16_t damage, int16_t knockback, int32_t dflags, int32_t mod) {

	g_client_t *client;
	int16_t take;
	int16_t save;
	int16_t asave;
	int32_t te_sparks;
	vec_t scale;

	if (!targ->locals.take_damage)
		return;

	if (targ != attacker && targ->client->locals.respawn_protection_time > g_level.time)
		return;

	if (!inflictor) // use world
		inflictor = &g_game.edicts[0];

	if (!attacker) // use world
		attacker = &g_game.edicts[0];

	// quad damage affects both damage and knockback
	if (attacker->client
			&& attacker->client->locals.persistent.inventory[g_level.media.quad_damage]) {
		damage = (int32_t) (damage * QUAD_DAMAGE_FACTOR);
		knockback = (int32_t) (knockback * QUAD_KNOCKBACK_FACTOR);
	}

	// friendly fire avoidance
	if (targ != attacker && (g_level.teams || g_level.ctf)) {
		if (G_OnSameTeam(targ, attacker)) { // target and attacker are on same team

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
	if (targ == attacker && g_level.gameplay > 1)
		damage = 0;

	means_of_death = mod;

	client = targ->client;

	VectorNormalize(dir);

	// calculate velocity change due to knockback
	if (knockback && (targ->locals.move_type == MOVE_TYPE_WALK)) {
		vec3_t kvel;
		vec_t mass;

		if (targ->locals.mass < 50.0)
			mass = 50.0;
		else
			mass = targ->locals.mass;

		scale = 1000.0; // default knockback scale

		if (targ == attacker) { // weapon jump hacks
			if (mod == MOD_BFG_BLAST)
				scale = 300.0;
			else if (mod == MOD_ROCKET_SPLASH)
				scale = 1400.0;
			else if (mod == MOD_GRENADE)
				scale = 1200.0;
		}

		VectorScale(dir, scale * (vec_t)knockback / mass, kvel);
		VectorAdd(targ->locals.velocity, kvel, targ->locals.velocity);
	}

	take = damage;
	save = 0;

	// check for god mode
	if ((targ->locals.flags & FL_GOD_MODE) && !(dflags & DAMAGE_NO_PROTECTION)) {
		take = 0;
		save = damage;
		G_SpawnDamage(TE_BLOOD, point, normal, save);
	}

	asave = G_CheckArmor(targ, point, normal, take, dflags);
	take -= asave;

	// treat cheat savings the same as armor
	asave += save;

	// do the damage
	if (take) {
		if (client)
			G_SpawnDamage(TE_BLOOD, point, normal, take);
		else {
			// impact effects for things we can hurt which shouldn't bleed
			if (dflags & DAMAGE_BULLET)
				te_sparks = TE_BULLET;
			else
				te_sparks = TE_SPARKS;

			G_SpawnDamage(te_sparks, point, normal, take);
		}

		targ->locals.health = targ->locals.health - take;

		if (targ->locals.health <= 0) {
			if (targ->locals.Die) {
				targ->locals.Die(targ, inflictor, attacker, take, point);
			} else {
				gi.Debug("No die function for %s\n", targ->class_name);
			}
			return;
		}
	}

	// invoke the pain callback
	if ((take || knockback) && targ->locals.Pain)
		targ->locals.Pain(targ, attacker, take, knockback);

	// add to the damage inflicted on a player this frame
	if (client) {
		client->locals.damage_armor += asave;
		client->locals.damage_health += take;

		vec_t kick = (asave + take) / 30.0;

		if (kick > 1.0) {
			kick = 1.0;
		}

		G_ClientDamageKick(targ, dir, kick);

		if (attacker->client && attacker->client != client) {
			attacker->client->locals.damage_inflicted += take;
		}
	}
}

/*
 * @brief
 */
void G_RadiusDamage(g_edict_t *inflictor, g_edict_t *attacker, g_edict_t *ignore, int32_t damage,
		int32_t knockback, vec_t radius, int32_t mod) {
	g_edict_t *ent;
	vec_t d, k, dist;
	vec3_t dir;

	ent = NULL;

	while ((ent = G_FindRadius(ent, inflictor->s.origin, radius)) != NULL) {

		if (ent == ignore)
			continue;

		if (!ent->locals.take_damage)
			continue;

		VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
		dist = VectorNormalize(dir);

		d = damage - 0.5 * dist;
		k = knockback - 0.5 * dist;

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
