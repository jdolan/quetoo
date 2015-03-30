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

#include "g_local.h"

/*
 * @brief Adds a fraction of the player's velocity to any projectiles they
 * emit. This is more realistic, but very strange feeling in Quake, so the
 * scale is generally kept quite low.
 */
static void G_PlayerProjectile(g_entity_t *ent, const vec_t scale) {

	if (ent->owner) {
		const vec_t s = scale * g_player_projectile->value;
		VectorMA(ent->locals.velocity, s, ent->owner->locals.velocity, ent->locals.velocity);
	} else {
		gi.Debug("No owner for %s\n", etos(ent));
	}
}

/*
 * @brief Returns true if the entity is facing a wall at too close proximity
 * for the specified projectile.
 */
static _Bool G_ImmediateWall(g_entity_t *ent, g_entity_t *projectile) {

	const cm_trace_t tr = gi.Trace(ent->s.origin, projectile->s.origin, projectile->mins,
			projectile->maxs, ent, MASK_SOLID);

	return tr.fraction < 1.0;
}

/*
 * @brief Returns true if the specified entity takes damage.
 */
static _Bool G_TakesDamage(g_entity_t *ent) {
	return (ent && ent->locals.take_damage);
}

/*
 * @brief Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(const vec3_t start, cm_trace_t *tr) {
	vec3_t dir, pos;

	if (VectorCompare(tr->end, start))
		return;

	VectorSubtract(tr->end, start, dir);
	VectorNormalize(dir);
	VectorMA(tr->end, -2, dir, pos);

	if (gi.PointContents(pos) & MASK_LIQUID)
		VectorCopy(pos, tr->end);
	else {
		const cm_trace_t trace = gi.Trace(pos, start, NULL, NULL, tr->ent, MASK_LIQUID);
		VectorCopy(trace.end, tr->end);
	}

	VectorAdd(start, tr->end, pos);
	VectorScale(pos, 0.5, pos);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BUBBLES);
	gi.WritePosition(start);
	gi.WritePosition(tr->end);
	gi.Multicast(pos, MULTICAST_PHS);
}

/*
 * @brief Used to add tracer trails to bullet shots.
 */
static void G_Tracer(const vec3_t start, const vec3_t end) {
	vec3_t dir, mid;
	vec_t len;

	VectorSubtract(end, start, dir);
	len = VectorNormalize(dir);

	if (len < 128.0)
		return;

	VectorMA(end, -len + (Randomf() * 0.05 * len), dir, mid);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_TRACER);
	gi.WritePosition(mid);
	gi.WritePosition(end);
	gi.Multicast(start, MULTICAST_PHS);

	if (!gi.inPHS(start, end)) { // send to both PHS's
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_TRACER);
		gi.WritePosition(mid);
		gi.WritePosition(end);
		gi.Multicast(end, MULTICAST_PHS);
	}
}

/*
 * @brief Used to add impact marks on surfaces hit by bullets.
 */
static void G_BulletMark(vec3_t org, cm_bsp_plane_t *plane, cm_bsp_surface_t *surf) {

	if (surf->flags & SURF_ALPHA_TEST)
		return;

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BULLET);
	gi.WritePosition(org);
	gi.WriteDir(plane->normal);

	gi.Multicast(org, MULTICAST_PHS);
}

/*
 * @brief Used to add burn marks on surfaces hit by projectiles.
 */
static void G_BurnMark(vec3_t org, const cm_bsp_plane_t *plane,
		const cm_bsp_surface_t *surf __attribute__((unused)), uint8_t scale) {

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BURN);
	gi.WritePosition(org);
	gi.WriteDir(plane->normal);
	gi.WriteByte(scale);

	gi.Multicast(org, MULTICAST_PHS);
}

/*
 * @brief
 */
static void G_BlasterProjectile_Touch(g_entity_t *self, g_entity_t *other,
		const cm_bsp_plane_t *plane, const cm_bsp_surface_t *surf) {

	if (other == self->owner)
		return;

	if (other->solid < SOLID_DEAD)
		return;

	if (!G_IsSky(surf)) {

		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
				self->locals.damage, self->locals.knockback, 0, MOD_BLASTER);

		if (G_IsStructural(other, surf)) {
			vec3_t origin;

			VectorMA(self->s.origin, 4.0, plane->normal, origin);
			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_BLASTER);
			gi.WritePosition(origin);
			gi.WritePosition(plane->normal);
			gi.WriteByte(self->s.client);
			gi.Multicast(origin, MULTICAST_PHS);
		}
	}

	G_FreeEntity(self);
}

/*
 * @brief
 */
void G_BlasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
		int16_t damage, int16_t knockback) {

	const vec3_t mins = { -1.0, -1.0, -1.0 };
	const vec3_t maxs = { 1.0, 1.0, 1.0 };

	g_entity_t *projectile = G_AllocEntity(__func__);
	projectile->owner = ent;

	VectorCopy(start, projectile->s.origin);
	VectorCopy(mins, projectile->mins);
	VectorCopy(maxs, projectile->maxs);
	VectorAngles(dir, projectile->s.angles);
	VectorScale(dir, speed, projectile->locals.velocity);

	G_PlayerProjectile(projectile, 0.25);

	if (G_ImmediateWall(ent, projectile))
		VectorCopy(ent->s.origin, projectile->s.origin);

	projectile->solid = SOLID_BOX;
	projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + 8000;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.Touch = G_BlasterProjectile_Touch;
	projectile->s.trail = TRAIL_BLASTER;

	// set the color, overloading the client byte
	if (ent->client) {
		projectile->s.client = ent->client->locals.persistent.color;
	} else {
		projectile->s.client = 0;
	}

	gi.LinkEntity(projectile);
}

/*
 * @brief
 */
void G_BulletProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int16_t damage,
		int16_t knockback, uint16_t hspread, uint16_t vspread, uint32_t mod) {

	cm_trace_t tr = gi.Trace(ent->s.origin, start, NULL, NULL, ent, MASK_CLIP_PROJECTILE);
	if (tr.fraction == 1.0) {
		vec3_t angles, forward, right, up, end;

		VectorAngles(dir, angles);
		AngleVectors(angles, forward, right, up);

		VectorMA(start, MAX_WORLD_DIST, forward, end);
		VectorMA(end, Randomc() * hspread, right, end);
		VectorMA(end, Randomc() * vspread, up, end);

		tr = gi.Trace(start, end, NULL, NULL, ent, MASK_CLIP_PROJECTILE);

		G_Tracer(start, tr.end);
	}

	if (tr.fraction < 1.0) {

		G_Damage(tr.ent, ent, ent, dir, tr.end, tr.plane.normal, damage, knockback, DMG_BULLET, mod);

		if (G_IsStructural(tr.ent, tr.surface)) {
			G_BulletMark(tr.end, &tr.plane, tr.surface);
		}

		if ((gi.PointContents(start) & MASK_LIQUID) || (gi.PointContents(tr.end) & MASK_LIQUID))
			G_BubbleTrail(start, &tr);
	}
}

/*
 * @brief
 */
void G_ShotgunProjectiles(g_entity_t *ent, const vec3_t start, const vec3_t dir, int16_t damage,
		int16_t knockback, int32_t hspread, int32_t vspread, int32_t count, uint32_t mod) {
	int32_t i;

	for (i = 0; i < count; i++)
		G_BulletProjectile(ent, start, dir, damage, knockback, hspread, vspread, mod);
}

/*
 * @brief
 */
static void G_GrenadeProjectile_Explode(g_entity_t *self) {
	vec3_t origin;

	if (self->locals.enemy) { // direct hit
		vec_t d, k, dist;
		vec3_t v, dir;

		VectorAdd(self->locals.enemy->mins, self->locals.enemy->maxs, v);
		VectorMA(self->locals.enemy->s.origin, 0.5, v, v);
		VectorSubtract(self->s.origin, v, v);

		dist = VectorLength(v);
		d = self->locals.damage - 0.5 * dist;
		k = self->locals.knockback - 0.5 * dist;

		VectorSubtract(self->locals.enemy->s.origin, self->s.origin, dir);

		G_Damage(self->locals.enemy, self, self->owner, dir, self->s.origin, vec3_origin,
				(int16_t) d, (int16_t) k, DMG_RADIUS, MOD_GRENADE);
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, self->locals.enemy, self->locals.damage,
			self->locals.knockback, self->locals.damage_radius, MOD_GRENADE_SPLASH);

	const g_entity_t *ent = self->locals.ground_entity;
	const cm_bsp_plane_t *plane = &self->locals.ground_plane;
	const cm_bsp_surface_t *surf = self->locals.ground_surface;

	if (ent)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (ent) {
		if (G_IsStructural(ent, surf) && G_IsStationary(ent)) {
			G_BurnMark(self->s.origin, plane, surf, 20);
		}
	}
	
	G_FreeEntity(self);
}

/*
 * @brief mostly a copy of the grenade launcher version but with different
 * means of death messages
 */
static void G_HandGrenadeProjectile_Explode(g_entity_t *self) {
	vec3_t origin;
	uint32_t mod = 0; 

	if (self->locals.enemy) { // direct hit
	
		vec_t d, k, dist;
		vec3_t v, dir;

		VectorAdd(self->locals.enemy->mins, self->locals.enemy->maxs, v);
		VectorMA(self->locals.enemy->s.origin, 0.5, v, v);
		VectorSubtract(self->s.origin, v, v);

		dist = VectorLength(v);
		d = self->locals.damage - 0.5 * dist;
		k = self->locals.knockback - 0.5 * dist;

		VectorSubtract(self->locals.enemy->s.origin, self->s.origin, dir);

		G_Damage(self->locals.enemy, self, self->owner, dir, self->s.origin, vec3_origin,
				(int16_t) d, (int16_t) k, DMG_RADIUS, MOD_HANDGRENADE_HIT);
	}
	
	// they never tossed it
	if (self->locals.held_grenade) {
		mod = MOD_HANDGRENADE_KAMIKAZE;
	} else {
		mod = MOD_HANDGRENADE_SPLASH;
	}
	
	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, self->locals.enemy, self->locals.damage,
			self->locals.knockback, self->locals.damage_radius, mod);

	const g_entity_t *ent = self->locals.ground_entity;
	const cm_bsp_plane_t *plane = &self->locals.ground_plane;
	const cm_bsp_surface_t *surf = self->locals.ground_surface;

	if (ent)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (ent) {
		if (G_IsStructural(ent, surf) && G_IsStationary(ent)) {
			G_BurnMark(self->s.origin, plane, surf, 20);
		}
	}

	G_FreeEntity(self);
}

/*
 * @brief
 */
void G_GrenadeProjectile_Touch(g_entity_t *self, g_entity_t *other,
	const cm_bsp_plane_t *plane __attribute__((unused)), const cm_bsp_surface_t *surf) {

	
	if (other == self->owner)
		return;

	if (other->solid < SOLID_DEAD)
		return;

	if (!G_TakesDamage(other)) { // bounce off of structural solids

		if (G_IsStructural(other, surf)) {
			if (g_level.time - self->locals.touch_time > 200) {
				if (VectorLength(self->locals.velocity) > 40.0) {
					gi.Sound(self, g_media.sounds.grenade_hit, ATTEN_NORM);
					self->locals.touch_time = g_level.time;
				}
			}
		} else if (G_IsSky(surf)) {
			G_FreeEntity(self);
		}

		return;
	}

	self->locals.enemy = other;
	if (g_strcmp0(self->class_name, "ammo_grenades") == 0)
		G_HandGrenadeProjectile_Explode(self);
	else
		G_GrenadeProjectile_Explode(self);
}

/*
 * @brief
 */
void G_GrenadeProjectile(g_entity_t *ent, vec3_t const start, const vec3_t dir, int32_t speed,
		int16_t damage, int16_t knockback, vec_t damage_radius, uint32_t timer) {

	vec3_t forward, right, up;

	g_entity_t *projectile = G_AllocEntity(__func__);
	projectile->owner = ent;

	VectorCopy(start, projectile->s.origin);
	VectorAngles(dir, projectile->s.angles);

	AngleVectors(projectile->s.angles, forward, right, up);
	VectorScale(dir, speed, projectile->locals.velocity);

	VectorMA(projectile->locals.velocity, 200.0 + Randomc() * 10.0, up, projectile->locals.velocity);
	VectorMA(projectile->locals.velocity, Randomc() * 10.0, right, projectile->locals.velocity);

	G_PlayerProjectile(projectile, 0.33);

	if (G_ImmediateWall(ent, projectile))
		VectorCopy(ent->s.origin, projectile->s.origin);

	projectile->solid = SOLID_BOX;
	projectile->locals.avelocity[0] = -300.0 + 10 * Randomc();
	projectile->locals.avelocity[1] = 50.0 * Randomc();
	projectile->locals.avelocity[2] = 25.0 * Randomc();
	projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_BOUNCE;
	projectile->locals.next_think = g_level.time + timer;
	projectile->locals.take_damage = true;
	projectile->locals.Think = G_GrenadeProjectile_Explode;
	projectile->locals.Touch = G_GrenadeProjectile_Touch;
	projectile->locals.touch_time = g_level.time;
	projectile->s.trail = TRAIL_GRENADE;
	projectile->s.model1 = g_media.models.grenade;

	// Giblet testing
	// projectile->s.model1 = gi.ModelIndex(va("models/gibs/gib%02d/tris.obj", (Random() % 3) + 1));
	// projectile->locals.next_think = 0;

	gi.LinkEntity(projectile);
}

// tossing a hand grenade
void G_HandGrenadeProjectile(g_entity_t *ent, g_entity_t *projectile, 
	vec3_t const start, const vec3_t dir, int32_t speed, int16_t damage, 
	int16_t knockback, vec_t damage_radius, uint32_t timer) {
	
	vec3_t forward, right, up;

	VectorCopy(start, projectile->s.origin);
	VectorAngles(dir, projectile->s.angles);

	AngleVectors(projectile->s.angles, forward, right, up);
	VectorScale(dir, speed, projectile->locals.velocity);

	VectorMA(
		projectile->locals.velocity, 
		200.0 + Randomc() * 10.0, 
		up, 
		projectile->locals.velocity
	);
	
	VectorMA(
		projectile->locals.velocity, 
		Randomc() * 10.0, 
		right, 
		projectile->locals.velocity
	);
	
	// add some of the player's velocity to the projectile
	G_PlayerProjectile(projectile, 0.33);

	if (G_ImmediateWall(ent, projectile))
		VectorCopy(ent->s.origin, projectile->s.origin);

	// if client is holding it, let the nade know it's being held
	if (ent->client->locals.grenade_hold_time)
		projectile->locals.held_grenade = true;
	
	projectile->locals.avelocity[0] = -300.0 + 10 * Randomc();
	projectile->locals.avelocity[1] = 50.0 * Randomc();
	projectile->locals.avelocity[2] = 25.0 * Randomc();
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.next_think = g_level.time + timer;
	projectile->locals.Think = G_HandGrenadeProjectile_Explode;
}
/*
 * @brief
 */
static void G_RocketProjectile_Touch(g_entity_t *self, g_entity_t *other,
		const cm_bsp_plane_t *plane, const cm_bsp_surface_t *surf) {

	if (other == self->owner)
		return;

	if (other->solid < SOLID_DEAD)
		return;

	if (!G_IsSky(surf)) {

		if (G_IsStructural(other, surf) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
					self->locals.damage, self->locals.knockback, 0, MOD_ROCKET);

			G_RadiusDamage(self, self->owner, other, self->locals.damage, self->locals.knockback,
					self->locals.damage_radius, MOD_ROCKET_SPLASH);

			vec3_t origin;
			if (G_IsStructural(other, surf))
				VectorMA(self->s.origin, 16.0, plane->normal, origin);
			else
				VectorCopy(self->s.origin, origin);

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_EXPLOSION);
			gi.WritePosition(origin);
			gi.Multicast(origin, MULTICAST_PHS);

			if (G_IsStructural(other, surf) && G_IsStationary(other)) {
				VectorMA(self->s.origin, 2.0, plane->normal, origin);
				G_BurnMark(origin, plane, surf, 20);
			}
		}
	}

	G_FreeEntity(self);
}

/*
 * @brief
 */
void G_RocketProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
		int16_t damage, int16_t knockback, vec_t damage_radius) {

	g_entity_t *projectile = G_AllocEntity(__func__);
	projectile->owner = ent;

	VectorCopy(start, projectile->s.origin);
	VectorAngles(dir, projectile->s.angles);
	VectorScale(dir, speed, projectile->locals.velocity);
	VectorSet(projectile->locals.avelocity, 0.0, 0.0, 600.0);

	G_PlayerProjectile(projectile, 0.125);

	if (G_ImmediateWall(ent, projectile))
		VectorCopy(ent->s.origin, projectile->s.origin);

	projectile->solid = SOLID_BOX;
	projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + 8000;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.Touch = G_RocketProjectile_Touch;
	projectile->s.model1 = g_media.models.rocket;
	projectile->s.sound = g_media.sounds.rocket_fly;
	projectile->s.trail = TRAIL_ROCKET;

	gi.LinkEntity(projectile);
}

/*
 * @brief
 */
static void G_HyperblasterProjectile_Touch(g_entity_t *self, g_entity_t *other,
		const cm_bsp_plane_t *plane, const cm_bsp_surface_t *surf) {

	if (other == self->owner)
		return;

	if (other->solid < SOLID_DEAD)
		return;

	if (!G_IsSky(surf)) {

		if (G_IsStructural(other, surf) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
					self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_HYPERBLASTER);

			vec3_t origin;
			if (G_IsStructural(other, surf)) {
				VectorMA(self->s.origin, 16.0, plane->normal, origin);
			} else if (G_IsMeat(other)) {
				VectorCopy(self->s.origin, origin);
			}

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_HYPERBLASTER);
			gi.WritePosition(origin);
			gi.Multicast(origin, MULTICAST_PHS);

			if (G_IsStructural(other, surf)) {

				vec3_t v;
				VectorSubtract(self->s.origin, self->owner->s.origin, v);

				if (VectorLength(v) < 32.0) { // hyperblaster climb
					G_Damage(self->owner, self, self->owner, NULL, self->s.origin, plane->normal,
							self->locals.damage * 0.06, 0, DMG_ENERGY, MOD_HYPERBLASTER);

					self->owner->locals.velocity[2] += 80.0;
				}

				if (G_IsStationary(other)) {
					VectorMA(self->s.origin, 2.0, plane->normal, origin);
					G_BurnMark(origin, plane, surf, 10);
				}
			}
		}
	}

	G_FreeEntity(self);
}

/*
 * @brief
 */
void G_HyperblasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
		int16_t damage, int16_t knockback) {

	g_entity_t *projectile = G_AllocEntity(__func__);
	projectile->owner = ent;

	VectorCopy(start, projectile->s.origin);
	VectorAngles(dir, projectile->s.angles);
	VectorScale(dir, speed, projectile->locals.velocity);

	G_PlayerProjectile(projectile, 0.25);

	if (G_ImmediateWall(ent, projectile))
		VectorCopy(ent->s.origin, projectile->s.origin);

	projectile->solid = SOLID_BOX;
	projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.next_think = g_level.time + 6000;
	projectile->locals.Touch = G_HyperblasterProjectile_Touch;
	projectile->s.trail = TRAIL_HYPERBLASTER;

	gi.LinkEntity(projectile);
}

/*
 * @brief
 */
static void G_LightningProjectile_Discharge(g_entity_t *self) {

	// kill ourselves
	G_Damage(self->owner, self, self->owner, NULL, self->s.origin, NULL, 9999, 100,
			DMG_NO_ARMOR, MOD_LIGHTNING_DISCHARGE);

	g_entity_t *ent = g_game.entities;

	// and ruin the pool party for everyone else too
	for (int32_t i = 0; i < g_max_entities->integer; i++, ent++) {

		if (ent == self->owner)
			continue;

		if (!G_IsMeat(ent))
			continue;

		if (gi.inPVS(self->s.origin, ent->s.origin)) {

			if (ent->locals.water_level) {
				const int16_t dmg = 50 * ent->locals.water_level;

				G_Damage(ent, self, self->owner, NULL, NULL, NULL, dmg, 100, DMG_NO_ARMOR,
						MOD_LIGHTNING_DISCHARGE);
			}
		}
	}

	// send discharge event
	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_LIGHTNING);
	gi.WritePosition(self->s.origin);
}

/*
 * @brief
 */
static _Bool G_LightningProjectile_Expire(g_entity_t *self) {

	if (self->locals.timestamp < g_level.time - 101)
		return true;

	if (self->owner->locals.dead)
		return true;

	return false;
}

/*
 * @brief
 */
static void G_LightningProjectile_Think(g_entity_t *self) {
	vec3_t forward, right, up;
	vec3_t start, end;
	vec3_t water_start;
	cm_trace_t tr;

	if (G_LightningProjectile_Expire(self)) {
		G_FreeEntity(self);
		return;
	}

	// re-calculate end points based on owner's movement
	G_InitProjectile(self->owner, forward, right, up, start);
	VectorCopy(start, self->s.origin);

	if (G_ImmediateWall(self->owner, self)) // resolve start
		VectorCopy(self->owner->s.origin, start);

	if (gi.PointContents(start) & MASK_LIQUID) { // discharge and return
		G_LightningProjectile_Discharge(self);
		G_FreeEntity(self);
		return;
	}

	VectorMA(start, 800.0, forward, end); // resolve end
	VectorMA(end, 10.0 * sin(g_level.time / 4.0), up, end);
	VectorMA(end, 10.0 * Randomc(), right, end);

	tr = gi.Trace(start, end, NULL, NULL, self, MASK_CLIP_PROJECTILE | MASK_LIQUID);

	if (tr.contents & MASK_LIQUID) { // entered water, play sound, leave trail
		VectorCopy(tr.end, water_start);

		if (!self->locals.water_level) {
			gi.PositionedSound(water_start, g_game.entities, g_media.sounds.water_in, ATTEN_NORM);
			self->locals.water_level = 1;
		}

		tr = gi.Trace(water_start, end, NULL, NULL, self, MASK_CLIP_PROJECTILE);
		G_BubbleTrail(water_start, &tr);
	} else {
		if (self->locals.water_level) { // exited water, play sound, no trail
			gi.PositionedSound(water_start, g_game.entities, g_media.sounds.water_out, ATTEN_NORM);
			self->locals.water_level = 0;
		}
	}

	if (self->locals.damage) { // shoot, removing our damage until it is renewed
		if (G_TakesDamage(tr.ent)) { // try to damage what we hit
			G_Damage(tr.ent, self, self->owner, forward, tr.end, tr.plane.normal,
					self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_LIGHTNING);
		} else { // or leave a mark
			if (tr.contents & MASK_SOLID) {
				if (G_IsStructural(tr.ent, tr.surface) && G_IsStationary(tr.ent))
					G_BurnMark(tr.end, &tr.plane, tr.surface, 8);
			}
		}
		self->locals.damage = 0;
	}

	VectorCopy(start, self->s.origin); // update end points
	VectorCopy(tr.end, self->s.termination);

	gi.LinkEntity(self);

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
void G_LightningProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int16_t damage,
		int16_t knockback) {

	g_entity_t *projectile = NULL;

	while ((projectile = G_Find(NULL, EOFS(class_name), __func__))) {
		if (projectile->owner == ent) {
			break;
		}
	}

	if (!projectile) { // ensure a valid lightning entity exists
		projectile = G_AllocEntity(__func__);

		VectorCopy(start, projectile->s.origin);

		if (G_ImmediateWall(ent, projectile))
			VectorCopy(ent->s.origin, projectile->s.origin);

		VectorMA(start, 800.0, dir, projectile->s.termination);

		projectile->owner = ent;
		projectile->solid = SOLID_NOT;
		projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
		projectile->locals.move_type = MOVE_TYPE_THINK;
		projectile->locals.Think = G_LightningProjectile_Think;
		projectile->locals.knockback = knockback;
		projectile->s.client = ent - g_game.entities - 1; // player number, for client prediction fix
		projectile->s.effects = EF_BEAM;
		projectile->s.sound = g_media.sounds.lightning_fly;
		projectile->s.trail = TRAIL_LIGHTNING;

		gi.LinkEntity(projectile);
	}

	// set the damage and think time
	projectile->locals.damage = damage;
	projectile->locals.next_think = g_level.time + 1;
	projectile->locals.timestamp = g_level.time;
}

/*
 * @brief
 */
void G_RailgunProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int16_t damage,
		int16_t knockback) {
	vec3_t pos, end, water_pos;

	VectorCopy(start, pos);

	if (gi.Trace(ent->s.origin, pos, NULL, NULL, ent, MASK_CLIP_PROJECTILE).fraction < 1.0) {
		VectorCopy(ent->s.origin, pos);
	}

	int32_t content_mask = MASK_CLIP_PROJECTILE | MASK_LIQUID;
	_Bool liquid = false;

	// are we starting in water?
	if (gi.PointContents(pos) & MASK_LIQUID) {
		VectorCopy(pos, water_pos);

		content_mask &= ~MASK_LIQUID;
		liquid = true;
	}

	VectorMA(pos, MAX_WORLD_DIST, dir, end);

	cm_trace_t tr;
	memset(&tr, 0, sizeof(tr));

	g_entity_t *ignore = ent;
	while (ignore) {
		tr = gi.Trace(pos, end, NULL, NULL, ignore, content_mask);
		if (!tr.ent) {
			break;
		}

		if ((tr.contents & MASK_LIQUID) && !liquid) {
			VectorCopy(tr.end, water_pos);

			content_mask &= ~MASK_LIQUID;
			liquid = true;

			gi.PositionedSound(water_pos, g_game.entities, g_media.sounds.water_in, ATTEN_NORM);

			ignore = ent;
			continue;
		}

		if (tr.ent->client || tr.ent->solid == SOLID_BOX)
			ignore = tr.ent;
		else
			ignore = NULL;

		// we've hit something, so damage it
		if ((tr.ent != ent) && G_TakesDamage(tr.ent)) {
			G_Damage(tr.ent, ent, ent, dir, tr.end, tr.plane.normal, damage, knockback, 0,
			MOD_RAILGUN);
		}

		VectorCopy(tr.end, pos);
	}

	uint8_t color;
	// use team colors, or client's color
	if (g_level.teams || g_level.ctf) {
		if (ent->client->locals.persistent.team == &g_team_good) {
			color = EFFECT_COLOR_BLUE;
		} else {
			color = EFFECT_COLOR_RED;
		}
	} else {
		color = ent->client->locals.persistent.color;
	}

	// send rail trail
	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_RAIL);
	gi.WritePosition(start);
	gi.WritePosition(tr.end);
	gi.WriteLong(tr.surface->flags);
	gi.WriteByte(color);

	gi.Multicast(start, MULTICAST_PHS);

	if (!gi.inPHS(start, tr.end)) { // send to both PHS's
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_RAIL);
		gi.WritePosition(start);
		gi.WritePosition(tr.end);
		gi.WriteLong(tr.surface->flags);
		gi.WriteByte(color);

		gi.Multicast(tr.end, MULTICAST_PHS);
	}

	// calculate position of burn mark
	if (G_IsStructural(tr.ent, tr.surface) && G_IsStationary(tr.ent)) {
		VectorMA(tr.end, -1.0, dir, tr.end);
		G_BurnMark(tr.end, &tr.plane, tr.surface, 12);
	}
}

/*
 * @brief
 */
static void G_BfgProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_bsp_plane_t *plane,
		const cm_bsp_surface_t *surf) {

	vec3_t pos;

	if (other == self->owner)
		return;

	if (other->solid < SOLID_DEAD)
		return;

	if (!G_IsSky(surf)) {

		if (G_IsStructural(other, surf) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
					self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_BFG_BLAST);

			G_RadiusDamage(self, self->owner, other, self->locals.damage, self->locals.knockback,
					self->locals.damage_radius, MOD_BFG_BLAST);

			vec3_t origin;
			if (G_IsStructural(other, surf))
				VectorMA(self->s.origin, 16.0, plane->normal, origin);
			else
				VectorCopy(self->s.origin, origin);

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_BFG);
			gi.WritePosition(origin);
			gi.Multicast(origin, MULTICAST_PHS);

			if (G_IsStructural(other, surf) && G_IsStationary(other)) {

				VectorMA(self->s.origin, 2.0, plane->normal, pos);
				G_BurnMark(pos, plane, surf, 30);
			}
		}
	}

	G_FreeEntity(self);
}

/*
 * @brief
 */
static void G_BfgProjectile_Think(g_entity_t *self) {

	const int16_t frame_damage = self->locals.damage * gi.frame_seconds;
	const int16_t frame_knockback = self->locals.knockback * gi.frame_seconds;

	g_entity_t *ent = NULL;
	while ((ent = G_FindRadius(ent, self->s.origin, self->locals.damage_radius)) != NULL) {
		vec3_t dir, normal;

		if (ent == self || ent == self->owner)
			continue;

		if (!ent->locals.take_damage)
			continue;

		if (!G_CanDamage(ent, self))
			continue;

		VectorSubtract(ent->s.origin, self->s.origin, dir);
		const vec_t dist = VectorNormalize(dir);
		VectorNegate(dir, normal);

		const vec_t f = 1.0 - dist / self->locals.damage_radius;

		G_Damage(ent, self, self->owner, dir, NULL, normal, frame_damage * f,
				frame_knockback * f, DMG_RADIUS, MOD_BFG_LASER);

		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_BFG_LASER);
		gi.WritePosition(self->s.origin);
		gi.WritePosition(ent->s.origin);
		gi.Multicast(self->s.origin, MULTICAST_PVS);
	}

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
void G_BfgProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
		int16_t damage, int16_t knockback, vec_t damage_radius) {

	g_entity_t *projectile = G_AllocEntity(__func__);
	projectile->owner = ent;

	VectorCopy(start, projectile->s.origin);
	VectorScale(dir, speed, projectile->locals.velocity);

	G_PlayerProjectile(projectile, 0.33);

	if (G_ImmediateWall(ent, projectile)) {
		VectorCopy(ent->s.origin, projectile->s.origin);
	}

	projectile->solid = SOLID_BOX;
	projectile->locals.clip_mask = MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + gi.frame_millis;
	projectile->locals.Think = G_BfgProjectile_Think;
	projectile->locals.Touch = G_BfgProjectile_Touch;
	projectile->s.trail = TRAIL_BFG;

	gi.LinkEntity(projectile);
}
