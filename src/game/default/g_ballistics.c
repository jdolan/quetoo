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
 * @brief Adds a fraction of the player's velocity to any projectiles they emit. This
 * is more realistic, but very strange feeling in Quake, so we keep the fraction
 * quite low.
 */
static void G_PlayerProjectile(g_edict_t *ent, const vec3_t scale) {
	vec3_t tmp;
	int32_t i;

	if (!ent->owner)
		return;

	for (i = 0; i < 3; i++) {
		tmp[i] = ent->owner->locals.velocity[i] * g_player_projectile->value * scale[i];
	}

	VectorAdd(tmp, ent->locals.velocity, ent->locals.velocity);
}

/*
 * @brief Returns true if the entity is facing a wall at close proximity.
 */
static _Bool G_ImmediateWall(g_edict_t *ent, vec3_t end) {
	c_trace_t tr;

	tr = gi.Trace(ent->s.origin, NULL, NULL, end, ent, MASK_SOLID);

	return tr.fraction < 1.0;
}

/*
 * @brief Returns true if the specified surface appears structural.
 */
static _Bool G_IsStructural(g_edict_t *ent, c_bsp_surface_t *surf) {

	if (!ent || ent->client || ent->locals.take_damage)
		return false; // we hit nothing, or something we damaged

	if (!surf || (surf->flags & SURF_SKY))
		return false; // we hit nothing, or the sky

	// don't leave marks on moving world objects
	return G_IsStationary(ent);
}

/*
 * @brief Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(const vec3_t start, c_trace_t *tr) {
	vec3_t dir, pos;

	if (VectorCompare(tr->end, start))
		return;

	VectorSubtract(tr->end, start, dir);
	VectorNormalize(dir);
	VectorMA(tr->end, -2, dir, pos);

	if (gi.PointContents(pos) & MASK_WATER)
		VectorCopy(pos, tr->end);
	else {
		const c_trace_t trace = gi.Trace(pos, NULL, NULL, start, tr->ent, MASK_WATER);
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
static void G_Tracer(vec3_t start, vec3_t end) {
	vec3_t dir, mid;
	float len;

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
static void G_BulletMark(vec3_t org, c_bsp_plane_t *plane, c_bsp_surface_t *surf) {

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
static void G_BurnMark(vec3_t org, c_bsp_plane_t *plane, c_bsp_surface_t *surf __attribute__((unused)), byte scale) {

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
static void G_BlasterProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (other == self->owner)
		return;

	if (other->locals.take_damage) { // damage what we hit
		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
				self->locals.dmg, self->locals.knockback, 0, MOD_BLASTER);
	} else {
		if (G_IsStructural(other, surf)) {
			vec3_t origin;

			VectorMA(self->s.origin, 16.0, plane->normal, origin);
			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_BLASTER);
			gi.WritePosition(origin);
			gi.WritePosition(plane->normal);
			gi.WriteByte(self->s.client);
			gi.Multicast(origin, MULTICAST_PHS);
		}
	}

	G_FreeEdict(self);
}

/*
 * @brief
 */
void G_BlasterProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t speed, int32_t damage,
		int32_t knockback) {
	g_edict_t *blast;
	const vec3_t scale = { 0.25, 0.25, 0.15 };

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	blast = G_Spawn();
	VectorCopy(start, blast->s.origin);
	VectorCopy(start, blast->s.old_origin);
	VectorCopy(dir, blast->locals.move_dir);
	VectorAngles(dir, blast->s.angles);
	VectorScale(dir, speed, blast->locals.velocity);
	blast->locals.move_type = MOVE_TYPE_FLY;
	blast->clip_mask = MASK_SHOT;
	blast->solid = SOLID_MISSILE;
	blast->s.effects = EF_BLASTER;
	blast->owner = ent;
	blast->locals.touch = G_BlasterProjectile_Touch;
	blast->locals.next_think = g_level.time + 8000;
	blast->locals.think = G_FreeEdict;
	blast->locals.dmg = damage;
	blast->locals.knockback = knockback;
	blast->locals.class_name = "blaster";

	// set the color, overloading the client byte
	if (ent->client) {
		blast->s.client = ent->client->locals.persistent.color;
	} else {
		blast->s.client = 0;
	}

	G_PlayerProjectile(blast, scale);

	gi.LinkEntity(blast);
}

/*
 * @brief
 */
void G_BulletProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int32_t damage,
		int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod) {
	c_trace_t tr;
	vec3_t dir;
	vec3_t forward, right, up;
	vec3_t end;
	float r;
	float u;

	tr = gi.Trace(ent->s.origin, NULL, NULL, start, ent, MASK_SHOT);
	if (tr.fraction == 1.0) {
		VectorAngles(aimdir, dir);
		AngleVectors(dir, forward, right, up);

		r = Randomc() * hspread;
		u = Randomc() * vspread;
		VectorMA(start, 8192.0, forward, end);
		VectorMA(end, r, right, end);
		VectorMA(end, u, up, end);

		tr = gi.Trace(start, NULL, NULL, end, ent, MASK_SHOT);
	}

	// send trails and marks
	if (tr.fraction < 1.0) {

		if (tr.ent->locals.take_damage) { // bleed and damage the enemy
			G_Damage(tr.ent, ent, ent, aimdir, tr.end, tr.plane.normal, damage, knockback,
					DAMAGE_BULLET, mod);
		} else { // leave an impact mark on the wall
			if (G_IsStructural(tr.ent, tr.surface)) {
				G_BulletMark(tr.end, &tr.plane, tr.surface);
			}
		}

		G_Tracer(start, tr.end);

		if ((gi.PointContents(start) & MASK_WATER) || (gi.PointContents(tr.end) & MASK_WATER))
			G_BubbleTrail(start, &tr);
	}
}

/*
 * @brief
 */
void G_ShotgunProjectiles(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t damage,
		int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod) {
	int32_t i;

	for (i = 0; i < count; i++)
		G_BulletProjectile(ent, start, dir, damage, knockback, hspread, vspread, mod);
}

/*
 * @brief
 */
static void G_GrenadeProjectile_Explode(g_edict_t *self) {
	vec3_t origin;

	if (self->locals.enemy) { // direct hit
		float d, k, dist;
		vec3_t v, dir;

		VectorAdd(self->locals.enemy->mins, self->locals.enemy->maxs, v);
		VectorMA(self->locals.enemy->s.origin, 0.5, v, v);
		VectorSubtract(self->s.origin, v, v);

		dist = VectorLength(v);
		d = self->locals.dmg - 0.5 * dist;
		k = self->locals.knockback - 0.5 * dist;

		VectorSubtract(self->locals.enemy->s.origin, self->s.origin, dir);

		G_Damage(self->locals.enemy, self, self->owner, dir, self->s.origin, vec3_origin, (int) d,
				(int) k, DAMAGE_RADIUS, MOD_GRENADE);
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, self->locals.enemy, self->locals.dmg, self->locals.knockback,
			self->locals.dmg_radius, MOD_GRENADE_SPLASH);

	if (G_IsStationary(self->locals.ground_entity))
		VectorMA(self->s.origin, 16.0, self->locals.plane.normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (G_IsStationary(self->locals.ground_entity)) {
		G_BurnMark(self->s.origin, &self->locals.plane, self->locals.surf, 20);
	}

	G_FreeEdict(self);
}

/*
 * @brief
 */
static void G_GrenadeProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEdict(self);
		return;
	}

	if (G_IsStructural(other, surf)) {
		self->locals.plane = *plane;
		self->locals.surf = surf;
	}

	if (!other->locals.take_damage) { // bounce
		if (g_level.time - self->locals.touch_time > 200) {
			VectorScale(self->locals.velocity, 1.25, self->locals.velocity);
			gi.Sound(self, g_level.media.grenade_hit_sound, ATTN_NORM);
			self->locals.touch_time = g_level.time;
		}
		return;
	}

	self->locals.enemy = other;
	G_GrenadeProjectile_Explode(self);
}

/*
 * @brief
 */
void G_GrenadeProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int32_t speed,
		int32_t damage, int32_t knockback, float damage_radius, uint32_t timer) {
	g_edict_t *grenade;
	vec3_t dir;
	vec3_t forward, right, up, start_bounds;
	const vec3_t mins = { -3.0, -3.0, -3.0 };
	const vec3_t maxs = { 3.0, 3.0, 3.0 };
	const vec3_t scale = { 0.3, 0.3, 0.3 };

	VectorMA(start, VectorLength(maxs), aimdir, start_bounds);

	if (G_ImmediateWall(ent, start_bounds))
		VectorCopy(ent->s.origin, start);

	VectorAngles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);

	VectorCopy(dir, grenade->s.angles);
	grenade->s.angles[PITCH] += 90.0;

	VectorScale(aimdir, speed, grenade->locals.velocity);
	VectorMA(grenade->locals.velocity, 200.0 + Randomc() * 10.0, up, grenade->locals.velocity);
	VectorMA(grenade->locals.velocity, Randomc() * 30.0, right, grenade->locals.velocity);

	grenade->locals.avelocity[0] = -300.0 + 10 * Randomc();
	grenade->locals.avelocity[1] = 50.0 * Randomc();
	grenade->locals.avelocity[2] = 25.0 * Randomc();

	grenade->locals.move_type = MOVE_TYPE_TOSS;
	grenade->clip_mask = MASK_SHOT;
	grenade->solid = SOLID_MISSILE;
	grenade->s.effects = EF_GRENADE;
	VectorCopy(mins, grenade->mins);
	VectorCopy(maxs, grenade->maxs);
	grenade->s.model1 = g_level.media.grenade_model;
	grenade->owner = ent;
	grenade->locals.touch = G_GrenadeProjectile_Touch;
	grenade->locals.touch_time = g_level.time;
	grenade->locals.next_think = g_level.time + timer;
	grenade->locals.think = G_GrenadeProjectile_Explode;
	grenade->locals.dmg = damage;
	grenade->locals.knockback = knockback;
	grenade->locals.dmg_radius = damage_radius;
	grenade->locals.class_name = "grenade";

	G_PlayerProjectile(grenade, scale);

	gi.LinkEntity(grenade);
}

/*
 * @brief
 */
static void G_RocketProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {
	vec3_t origin;

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEdict(self);
		return;
	}

	// calculate position for the explosion entity
	if (plane && surf)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (other->locals.take_damage) { // direct hit
		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
				self->locals.dmg, self->locals.knockback, 0, MOD_ROCKET);
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, other, self->locals.dmg, self->locals.knockback,
			self->locals.dmg_radius, MOD_ROCKET_SPLASH);

	if (G_IsStructural(other, surf)) { // leave a burn mark
		VectorMA(self->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 20);
	}

	G_FreeEdict(self);
}

/*
 * @brief
 */
void G_RocketProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t speed, int32_t damage,
		int32_t knockback, float damage_radius) {
	const vec3_t scale = { 0.25, 0.25, 0.15 };
	g_edict_t *rocket;

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	rocket = G_Spawn();
	VectorCopy(start, rocket->s.origin);
	VectorCopy(start, rocket->s.old_origin);
	VectorCopy(dir, rocket->locals.move_dir);
	VectorAngles(dir, rocket->s.angles);
	VectorScale(dir, speed, rocket->locals.velocity);
	VectorSet(rocket->locals.avelocity, 0.0, 0.0, 600.0);
	rocket->locals.move_type = MOVE_TYPE_FLY;
	rocket->clip_mask = MASK_SHOT;
	rocket->solid = SOLID_MISSILE;
	rocket->s.effects = EF_ROCKET;
	rocket->s.model1 = g_level.media.rocket_model;
	rocket->owner = ent;
	rocket->locals.touch = G_RocketProjectile_Touch;
	rocket->locals.next_think = g_level.time + 8000;
	rocket->locals.think = G_FreeEdict;
	rocket->locals.dmg = damage;
	rocket->locals.dmg_radius = damage_radius;
	rocket->locals.knockback = knockback;
	rocket->s.sound = g_level.media.rocket_fly_sound;
	rocket->locals.class_name = "rocket";

	G_PlayerProjectile(rocket, scale);

	gi.LinkEntity(rocket);
}

/*
 * @brief
 */
static void G_HyperblasterProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {
	vec3_t origin;
	vec3_t v;

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEdict(self);
		return;
	}

	// calculate position for the explosion entity
	if (plane && surf)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_HYPERBLASTER);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (other->locals.take_damage) {
		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
				self->locals.dmg, self->locals.knockback, DAMAGE_ENERGY, MOD_HYPERBLASTER);
	} else {
		if (G_IsStructural(other, surf)) { // leave a burn mark
			VectorMA(self->s.origin, 2.0, plane->normal, origin);
			G_BurnMark(origin, plane, surf, 10);

			VectorSubtract(self->s.origin, self->owner->s.origin, v);

			if (VectorLength(v) < 32.0) { // hyperblaster climb
				G_Damage(self->owner, self, self->owner, vec3_origin, self->s.origin,
						plane->normal, self->locals.dmg * 0.06, 0, DAMAGE_ENERGY, MOD_HYPERBLASTER);

				self->owner->locals.velocity[2] += 80.0;
			}
		}
	}

	G_FreeEdict(self);
}

/*
 * @brief
 */
void G_HyperblasterProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t speed,
		int32_t damage, int32_t knockback) {
	g_edict_t *bolt;
	const vec3_t scale = { 0.5, 0.5, 0.25 };

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	bolt = G_Spawn();
	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	VectorAngles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->locals.velocity);
	bolt->locals.move_type = MOVE_TYPE_FLY;
	bolt->clip_mask = MASK_SHOT;
	bolt->solid = SOLID_MISSILE;
	bolt->s.effects = EF_HYPERBLASTER;
	bolt->owner = ent;
	bolt->locals.touch = G_HyperblasterProjectile_Touch;
	bolt->locals.next_think = g_level.time + 6000;
	bolt->locals.think = G_FreeEdict;
	bolt->locals.dmg = damage;
	bolt->locals.knockback = knockback;
	bolt->locals.class_name = "bolt";

	G_PlayerProjectile(bolt, scale);

	gi.LinkEntity(bolt);
}

/*
 * @brief
 */
static void G_LightningProjectile_Discharge(g_edict_t *self) {
	g_edict_t *ent;
	int32_t i, d;

	// find all clients in the same water area and kill them
	for (i = 0; i < sv_max_clients->integer; i++) {

		ent = &g_game.edicts[i + 1];

		if (!ent->in_use)
			continue;

		if (!ent->locals.take_damage) // dead or spectator
			continue;

		if (gi.inPVS(self->s.origin, ent->s.origin)) {

			if (ent->locals.water_level) {

				// we always kill ourselves, we inflict a lot of damage but
				// we don't necessarily kill everyone else
				d = ent == self ? 999 : 50 * ent->locals.water_level;

				G_Damage(ent, self, self->owner, vec3_origin, ent->s.origin, vec3_origin, d, 100,
						DAMAGE_NO_ARMOR, MOD_LIGHTNING_DISCHARGE);
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
static _Bool G_LightningProjectile_Expire(g_edict_t *self) {

	if (self->locals.timestamp < g_level.time - 101)
		return true;

	if (self->owner->locals.dead)
		return true;

	return false;
}

/*
 * @brief
 */
static void G_LightningProjectile_Think(g_edict_t *self) {
	vec3_t forward, right, up;
	vec3_t start, end;
	vec3_t water_start;
	c_trace_t tr;

	if (G_LightningProjectile_Expire(self)) {
		self->owner->locals.lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	// re-calculate end points based on owner's movement
	G_InitProjectile(self->owner, forward, right, up, start);

	if (G_ImmediateWall(self->owner, start)) // resolve start
		VectorCopy(self->owner->s.origin, start);

	if (gi.PointContents(start) & MASK_WATER) { // discharge and return
		G_LightningProjectile_Discharge(self);
		self->owner->locals.lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	VectorMA(start, 800.0, forward, end); // resolve end
	VectorMA(end, 10.0 * sin(g_level.time / 4.0), up, end);
	VectorMA(end, 10.0 * Randomc(), right, end);

	tr = gi.Trace(start, NULL, NULL, end, self, MASK_SHOT | MASK_WATER);

	if (tr.contents & MASK_WATER) { // entered water, play sound, leave trail
		VectorCopy(tr.end, water_start);

		if (!self->locals.water_level) {
			gi.PositionedSound(water_start, g_game.edicts, gi.SoundIndex("world/water_in"),
					ATTN_NORM);
			self->locals.water_level = 1;
		}

		tr = gi.Trace(water_start, NULL, NULL, end, self, MASK_SHOT);
		G_BubbleTrail(water_start, &tr);
	} else {
		if (self->locals.water_level) { // exited water, play sound, no trail
			gi.PositionedSound(water_start, g_game.edicts, gi.SoundIndex("world/water_out"),
					ATTN_NORM);
			self->locals.water_level = 0;
		}
	}

	if (self->locals.dmg) { // shoot, removing our damage until it is renewed
		if (tr.ent->locals.take_damage) { // try to damage what we hit
			G_Damage(tr.ent, self, self->owner, forward, tr.end, tr.plane.normal, self->locals.dmg,
					self->locals.knockback, DAMAGE_ENERGY, MOD_LIGHTNING);
		} else { // or leave a mark
			if ((tr.contents & CONTENTS_SOLID) && G_IsStructural(tr.ent, tr.surface))
				G_BurnMark(tr.end, &tr.plane, tr.surface, 8);
		}
		self->locals.dmg = 0;
	}

	VectorCopy(start, self->s.origin); // update endpoints
	VectorCopy(tr.end, self->s.old_origin);

	gi.LinkEntity(self);

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
void G_LightningProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t damage,
		int32_t knockback) {
	g_edict_t *light;

	light = ent->locals.lightning;

	if (!light) { // ensure a valid lightning entity exists
		light = G_Spawn();

		VectorCopy(start, light->s.origin);
		VectorMA(start, 800.0, dir, light->s.old_origin);
		light->solid = SOLID_NOT;
		light->locals.move_type = MOVE_TYPE_THINK;
		light->owner = ent;
		light->locals.think = G_LightningProjectile_Think;
		light->locals.knockback = knockback;
		light->s.client = ent - g_game.edicts; // player number, for client prediction fix
		light->s.effects = EF_BEAM | EF_LIGHTNING;
		light->s.sound = g_level.media.lightning_fly_sound;
		light->locals.class_name = "lightning";

		gi.LinkEntity(light);
		ent->locals.lightning = light;
	}

	// set the damage and think time
	light->locals.dmg = damage;
	light->locals.timestamp = light->locals.next_think = g_level.time;
}

/*
 * @brief
 */
void G_RailgunProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int32_t damage,
		int32_t knockback) {
	vec3_t from;
	vec3_t end;
	c_trace_t tr;
	g_edict_t *ignore;
	vec3_t water_start;
	byte color;
	_Bool water = false;
	int32_t content_mask = MASK_SHOT | MASK_WATER;

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	VectorMA(start, 8192.0, aimdir, end);
	VectorCopy(start, from);
	ignore = ent;

	// are we starting in water?
	if (gi.PointContents(start) & MASK_WATER) {
		content_mask &= ~MASK_WATER;
		VectorCopy(start, water_start);
		water = true;
	}

	memset(&tr, 0, sizeof(tr));

	while (ignore) {
		tr = gi.Trace(from, NULL, NULL, end, ignore, content_mask);

		if ((tr.contents & MASK_WATER) && !water) {

			content_mask &= ~MASK_WATER;
			water = true;

			VectorCopy(tr.end, water_start);

			gi.PositionedSound(water_start, g_game.edicts, gi.SoundIndex("world/water_in"),
					ATTN_NORM);

			ignore = ent;
			continue;
		}

		if (tr.ent->client || tr.ent->solid == SOLID_BOX)
			ignore = tr.ent;
		else
			ignore = NULL;

		if ((tr.ent != ent) && (tr.ent->locals.take_damage)) {
			if (tr.ent->client && ((int) g_level.gameplay == INSTAGIB))
				damage = 1000; // be sure to cause a kill
			G_Damage(tr.ent, ent, ent, aimdir, tr.end, tr.plane.normal, damage, knockback, 0,
					MOD_RAILGUN);
		}

		VectorCopy(tr.end, from);
	}

	// use team colors, or client's color
	if (g_level.teams || g_level.ctf) {
		if (ent->client->locals.persistent.team == &g_team_good)
			color = EFFECT_COLOR_BLUE;
		else
			color = EFFECT_COLOR_RED;
	} else
		color = ent->client->locals.persistent.color;

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
	if (G_IsStructural(tr.ent, tr.surface)) {
		VectorMA(tr.end, -1.0, aimdir, tr.end);
		G_BurnMark(tr.end, &tr.plane, tr.surface, 12);
	}
}

/*
 * @brief
 */
static void G_BfgProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf) {
	vec3_t origin;

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEdict(self);
		return;
	}

	// calculate position for the explosion entity
	if (plane && surf)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BFG);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (other->locals.take_damage) // hurt what we hit
		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, plane->normal,
				self->locals.dmg, self->locals.knockback, DAMAGE_ENERGY, MOD_BFG_BLAST);

	// and anything nearby too
	G_RadiusDamage(self, self->owner, other, self->locals.dmg, self->locals.knockback,
			self->locals.dmg_radius, MOD_BFG_BLAST);

	if (G_IsStructural(other, surf)) { // leave a burn mark
		VectorMA(self->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 30);
	}

	G_FreeEdict(self);
}

/*
 * @brief
 */
static void G_BfgProjectile_Think(g_edict_t *self) {

	// radius damage
	G_RadiusDamage(self, self->owner, self->owner, self->locals.dmg * 10 * gi.frame_seconds,
			self->locals.knockback * 10 * gi.frame_seconds, self->locals.dmg_radius, MOD_BFG_LASER);

	// linear, clamped acceleration
	if (VectorLength(self->locals.velocity) < 1000.0)
		VectorScale(self->locals.velocity, 1.0 + (0.5 * gi.frame_seconds), self->locals.velocity);

	self->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
void G_BfgProjectiles(g_edict_t *ent, vec3_t start, vec3_t dir, int32_t speed, int32_t damage,
		int32_t knockback, float damage_radius) {
	g_edict_t *bfg;
	vec3_t angles, right, up, r, u;
	int32_t i;
	float s;
	const vec3_t scale = { 0.15, 0.15, 0.05 };

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	// calculate up and right vectors
	VectorAngles(dir, angles);
	AngleVectors(angles, NULL, right, up);

	for (i = 0; i < 8; i++) {

		bfg = G_Spawn();
		VectorCopy(start, bfg->s.origin);

		// start with the forward direction
		VectorCopy(dir, bfg->locals.move_dir);

		// and scatter randomly along the up and right vectors
		VectorScale(right, Randomc() * 0.35, r);
		VectorScale(up, Randomc() * 0.01, u);

		VectorAdd(bfg->locals.move_dir, r, bfg->locals.move_dir);
		VectorAdd(bfg->locals.move_dir, u, bfg->locals.move_dir);

		// finalize the direction and resolve angles, velocity, ..
		VectorAngles(bfg->locals.move_dir, bfg->s.angles);

		s = speed + (0.2 * speed * Randomc());
		VectorScale(bfg->locals.move_dir, s, bfg->locals.velocity);

		bfg->locals.move_type = MOVE_TYPE_FLY;
		bfg->clip_mask = MASK_SHOT;
		bfg->solid = SOLID_MISSILE;
		bfg->s.effects = EF_BFG;
		bfg->owner = ent;
		bfg->locals.touch = G_BfgProjectile_Touch;
		bfg->locals.think = G_BfgProjectile_Think;
		bfg->locals.next_think = g_level.time + gi.frame_millis;
		bfg->locals.dmg = damage;
		bfg->locals.knockback = knockback;
		bfg->locals.dmg_radius = damage_radius;
		bfg->locals.class_name = "bfg projectile";

		G_PlayerProjectile(bfg, scale);

		gi.LinkEntity(bfg);
	}
}
