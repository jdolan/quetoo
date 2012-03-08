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

/**
 * G_PlayerProjectile
 *
 * Adds a fraction of the player's velocity to any projectiles they emit. This
 * is more realistic, but very strange feeling in Quake, so we keep the fraction
 * quite low.
 */
static void G_PlayerProjectile(g_edict_t *ent, const vec3_t scale) {
	vec3_t tmp;
	int i;

	if (!ent->owner)
		return;

	for (i = 0; i < 3; i++)
		tmp[i] = ent->owner->velocity[i] * g_player_projectile->value * scale[i];

	VectorAdd(tmp, ent->velocity, ent->velocity);
}

/**
 * G_ImmediateWall
 *
 * Returns true if the entity is facing a wall at close proximity.
 */
static boolean_t G_ImmediateWall(g_edict_t *ent, vec3_t end) {
	c_trace_t tr;

	tr = gi.Trace(ent->s.origin, NULL, NULL, end, ent, MASK_SOLID);

	return tr.fraction < 1.0;
}

/**
 * G_IsStructural
 *
 * Returns true if the specified surface appears structural.
 */
static boolean_t G_IsStructural(g_edict_t *ent, c_bsp_surface_t *surf) {

	if (!ent || ent->client || ent->take_damage)
		return false; // we hit nothing, or something we damaged

	if (!surf || (surf->flags & SURF_SKY))
		return false; // we hit nothing, or the sky

	// don't leave marks on moving world objects
	return G_IsStationary(ent);
}

/**
 * G_BubbleTrail
 *
 * Used to add generic bubble trails to shots.
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

/**
 * G_Tracer
 *
 * Used to add tracer trails to bullet shots.
 */
static void G_Tracer(vec3_t start, vec3_t end) {
	vec3_t dir, mid;
	float len;

	VectorSubtract(end, start, dir);
	len = VectorNormalize(dir);

	if (len < 128.0)
		return;

	VectorMA(end, -len + (frand() * 0.05 * len), dir, mid);

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

/**
 * G_BulletMark
 *
 * Used to add impact marks on surfaces hit by bullets.
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

/**
 * G_BurnMark
 *
 * Used to add burn marks on surfaces hit by projectiles.
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
 * G_BlasterProjectile_Touch
 */
static void G_BlasterProjectile_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane,
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (other == self->owner)
		return;

	if (other->take_damage) { // damage what we hit
		G_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal,
				self->dmg, self->knockback, 0, MOD_BLASTER);
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
 * G_BlasterProjectile
 */
void G_BlasterProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int speed, int damage,
		int knockback) {
	g_edict_t *blast;
	const vec3_t scale = { 0.25, 0.25, 0.15 };

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	blast = G_Spawn();
	VectorCopy(start, blast->s.origin);
	VectorCopy(start, blast->s.old_origin);
	VectorCopy(dir, blast->move_dir);
	VectorAngles(dir, blast->s.angles);
	VectorScale(dir, speed, blast->velocity);
	blast->move_type = MOVE_TYPE_FLY;
	blast->clip_mask = MASK_SHOT;
	blast->solid = SOLID_MISSILE;
	blast->s.effects = EF_BLASTER;
	blast->owner = ent;
	blast->touch = G_BlasterProjectile_Touch;
	blast->next_think = g_level.time + 8000;
	blast->think = G_FreeEdict;
	blast->dmg = damage;
	blast->knockback = knockback;
	blast->class_name = "blaster";

	// set the color, overloading the client byte
	if (ent->client)
		blast->s.client = ent->client->persistent.color;
	else
		blast->s.client = DEFAULT_WEAPON_EFFECT_COLOR;

	G_PlayerProjectile(blast, scale);

	gi.LinkEntity(blast);
}

/*
 * G_BulletProjectile
 */
void G_BulletProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int damage, int knockback,
		int hspread, int vspread, int mod) {
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

		r = crand() * hspread;
		u = crand() * vspread;
		VectorMA(start, 8192.0, forward, end);
		VectorMA(end, r, right, end);
		VectorMA(end, u, up, end);

		tr = gi.Trace(start, NULL, NULL, end, ent, MASK_SHOT);
	}

	// send trails and marks
	if (tr.fraction < 1.0) {

		if (tr.ent->take_damage) { // bleed and damage the enemy
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
 * G_ShotgunProjectiles
 */
void G_ShotgunProjectiles(g_edict_t *ent, vec3_t start, vec3_t dir, int damage, int knockback,
		int hspread, int vspread, int count, int mod) {
	int i;

	for (i = 0; i < count; i++)
		G_BulletProjectile(ent, start, dir, damage, knockback, hspread, vspread, mod);
}

/*
 * G_GrenadeProjectile_Explode
 */
static void G_GrenadeProjectile_Explode(g_edict_t *self) {
	vec3_t origin;

	if (self->enemy) { // direct hit
		float d, k, dist;
		vec3_t v, dir;

		VectorAdd(self->enemy->mins, self->enemy->maxs, v);
		VectorMA(self->enemy->s.origin, 0.5, v, v);
		VectorSubtract(self->s.origin, v, v);

		dist = VectorLength(v);
		d = self->dmg - 0.5 * dist;
		k = self->knockback - 0.5 * dist;

		VectorSubtract(self->enemy->s.origin, self->s.origin, dir);

		G_Damage(self->enemy, self, self->owner, dir, self->s.origin, vec3_origin, (int) d,
				(int) k, DAMAGE_RADIUS, MOD_GRENADE);
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, self->enemy, self->dmg, self->knockback, self->dmg_radius,
			MOD_GRENADE_SPLASH);

	if (G_IsStationary(self->ground_entity))
		VectorMA(self->s.origin, 16.0, self->plane.normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if (G_IsStationary(self->ground_entity)) {
		G_BurnMark(self->s.origin, &self->plane, self->surf, 20);
	}

	G_FreeEdict(self);
}

/*
 * G_GrenadeProjectile_Touch
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
		self->plane = *plane;
		self->surf = surf;
	}

	if (!other->take_damage) { // bounce
		if (g_level.time - self->touch_time > 200) {
			VectorScale(self->velocity, 1.25, self->velocity);
			gi.Sound(self, grenade_hit_index, ATTN_NORM);
			self->touch_time = g_level.time;
		}
		return;
	}

	self->enemy = other;
	G_GrenadeProjectile_Explode(self);
}

/*
 * G_GrenadeProjectile
 */
void G_GrenadeProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int speed, int damage,
		int knockback, float damage_radius, unsigned int timer) {
	g_edict_t *grenade;
	vec3_t dir;
	vec3_t forward, right, up, startBounds;
	const vec3_t mins = { -3.0, -3.0, -3.0 };
	const vec3_t maxs = { 3.0, 3.0, 3.0 };
	const vec3_t scale = { 0.3, 0.3, 0.3 };

	VectorMA(start, VectorLength(maxs), aimdir, startBounds);
	if (G_ImmediateWall(ent, startBounds))
		VectorCopy(ent->s.origin, start);

	VectorAngles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);

	VectorCopy(dir, grenade->s.angles);
	grenade->s.angles[PITCH] += 90.0;

	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200.0 + crand() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crand() * 10.0, right, grenade->velocity);

	grenade->avelocity[0] = -300.0 + 10 * crand();
	grenade->avelocity[1] = 50.0 * crand();
	grenade->avelocity[2] = 25.0 * crand();

	grenade->move_type = MOVE_TYPE_TOSS;
	grenade->clip_mask = MASK_SHOT;
	grenade->solid = SOLID_MISSILE;
	grenade->s.effects = EF_GRENADE;
	VectorCopy(mins, grenade->mins);
	VectorCopy(maxs, grenade->maxs);
	grenade->s.model1 = grenade_index;
	grenade->owner = ent;
	grenade->touch = G_GrenadeProjectile_Touch;
	grenade->touch_time = g_level.time;
	grenade->next_think = g_level.time + timer;
	grenade->think = G_GrenadeProjectile_Explode;
	grenade->dmg = damage;
	grenade->knockback = knockback;
	grenade->dmg_radius = damage_radius;
	grenade->class_name = "grenade";

	G_PlayerProjectile(grenade, scale);

	gi.LinkEntity(grenade);
}

/*
 * G_RocketProjectile_Touch
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

	if (other->take_damage) { // direct hit
		G_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal,
				self->dmg, self->knockback, 0, MOD_ROCKET);
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, other, self->dmg, self->knockback, self->dmg_radius,
			MOD_ROCKET_SPLASH);

	if (G_IsStructural(other, surf)) { // leave a burn mark
		VectorMA(self->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 20);
	}

	G_FreeEdict(self);
}

/*
 * G_RocketProjectile
 */
void G_RocketProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int speed, int damage,
		int knockback, float damage_radius) {
	const vec3_t scale = { 0.25, 0.25, 0.15 };
	g_edict_t *rocket;

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	rocket = G_Spawn();
	VectorCopy(start, rocket->s.origin);
	VectorCopy(start, rocket->s.old_origin);
	VectorCopy(dir, rocket->move_dir);
	VectorAngles(dir, rocket->s.angles);
	VectorScale(dir, speed, rocket->velocity);
	VectorSet(rocket->avelocity, 0.0, 0.0, 600.0);
	rocket->move_type = MOVE_TYPE_FLY;
	rocket->clip_mask = MASK_SHOT;
	rocket->solid = SOLID_MISSILE;
	rocket->s.effects = EF_ROCKET;
	rocket->s.model1 = rocket_index;
	rocket->owner = ent;
	rocket->touch = G_RocketProjectile_Touch;
	rocket->next_think = g_level.time + 8000;
	rocket->think = G_FreeEdict;
	rocket->dmg = damage;
	rocket->dmg_radius = damage_radius;
	rocket->knockback = knockback;
	rocket->s.sound = rocket_fly_index;
	rocket->class_name = "rocket";

	G_PlayerProjectile(rocket, scale);

	gi.LinkEntity(rocket);
}

/*
 * G_HyperblasterProjectile_Touch
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

	if (other->take_damage) {
		G_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal,
				self->dmg, self->knockback, DAMAGE_ENERGY, MOD_HYPERBLASTER);
	} else {
		if (G_IsStructural(other, surf)) { // leave a burn mark
			VectorMA(self->s.origin, 2.0, plane->normal, origin);
			G_BurnMark(origin, plane, surf, 10);

			VectorSubtract(self->s.origin, self->owner->s.origin, v);

			if (VectorLength(v) < 32.0) { // hyperblaster climb
				G_Damage(self->owner, self, self->owner, vec3_origin, self->s.origin,
						plane->normal, self->dmg * 0.06, 0, DAMAGE_ENERGY, MOD_HYPERBLASTER);

				self->owner->velocity[2] += 70.0;
			}
		}
	}

	G_FreeEdict(self);
}

/*
 * G_HyperblasterProjectile
 */
void G_HyperblasterProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int speed, int damage,
		int knockback) {
	g_edict_t *bolt;
	const vec3_t scale = { 0.5, 0.5, 0.25 };

	if (G_ImmediateWall(ent, start))
		VectorCopy(ent->s.origin, start);

	bolt = G_Spawn();
	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	VectorAngles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->velocity);
	bolt->move_type = MOVE_TYPE_FLY;
	bolt->clip_mask = MASK_SHOT;
	bolt->solid = SOLID_MISSILE;
	bolt->s.effects = EF_HYPERBLASTER;
	bolt->owner = ent;
	bolt->touch = G_HyperblasterProjectile_Touch;
	bolt->next_think = g_level.time + 3000;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->knockback = knockback;
	bolt->class_name = "bolt";

	G_PlayerProjectile(bolt, scale);

	gi.LinkEntity(bolt);
}

/*
 * G_LightningProjectile_Discharge
 */
static void G_LightningProjectile_Discharge(g_edict_t *self) {
	g_edict_t *ent;
	int i, d;

	// find all clients in the same water area and kill them
	for (i = 0; i < sv_max_clients->integer; i++) {

		ent = &g_game.edicts[i + 1];

		if (!ent->in_use)
			continue;

		if (!ent->take_damage) // dead or spectator
			continue;

		if (gi.inPVS(self->s.origin, ent->s.origin)) {

			if (ent->water_level) {

				// we always kill ourselves, we inflict a lot of damage but
				// we don't necessarily kill everyone else
				d = ent == self ? 999 : 50 * ent->water_level;

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
 * G_LightningProjectile_Expire
 */
static boolean_t G_LightningProjectile_Expire(g_edict_t *self) {

	if (self->timestamp < g_level.time - 101)
		return true;

	if (self->owner->dead)
		return true;

	return false;
}

/*
 * G_LightningProjectile_Think
 */
static void G_LightningProjectile_Think(g_edict_t *self) {
	vec3_t forward, right, up;
	vec3_t start, end;
	vec3_t water_start;
	c_trace_t tr;

	if (G_LightningProjectile_Expire(self)) {
		self->owner->lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	// re-calculate end points based on owner's movement
	G_InitProjectile(self->owner, forward, right, up, start);

	if (G_ImmediateWall(self->owner, start)) // resolve start
		VectorCopy(self->owner->s.origin, start);

	if (gi.PointContents(start) & MASK_WATER) { // discharge and return
		G_LightningProjectile_Discharge(self);
		self->owner->lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	VectorMA(start, 800.0, forward, end); // resolve end
	tr = gi.Trace(start, NULL, NULL, end, self, MASK_SHOT | MASK_WATER);

	if (tr.contents & MASK_WATER) { // entered water, play sound, leave trail
		VectorCopy(tr.end, water_start);

		if (!self->water_level) {
			gi.PositionedSound(water_start, g_game.edicts, gi.SoundIndex("world/water_in"),
					ATTN_NORM);
			self->water_level = 1;
		}

		tr = gi.Trace(water_start, NULL, NULL, end, self, MASK_SHOT);
		G_BubbleTrail(water_start, &tr);
	} else {
		if (self->water_level) { // exited water, play sound, no trail
			gi.PositionedSound(water_start, g_game.edicts, gi.SoundIndex("world/water_out"),
					ATTN_NORM);
			self->water_level = 0;
		}
	}

	if (self->dmg) { // shoot, removing our damage until it is renewed
		if (tr.ent->take_damage) { // try to damage what we hit
			G_Damage(tr.ent, self, self->owner, forward, tr.end, tr.plane.normal, self->dmg,
					self->knockback, DAMAGE_ENERGY, MOD_LIGHTNING);
		} else { // or leave a mark
			if ((tr.contents & CONTENTS_SOLID) && G_IsStructural(tr.ent, tr.surface))
				G_BurnMark(tr.end, &tr.plane, tr.surface, 8);
		}
		self->dmg = 0;
	}

	VectorCopy(start, self->s.origin); // update endpoints
	VectorCopy(tr.end, self->s.old_origin);

	gi.LinkEntity(self);

	self->next_think = g_level.time + gi.frame_millis;
}

/*
 * G_LightningProjectile
 */
void G_LightningProjectile(g_edict_t *ent, vec3_t start, vec3_t dir, int damage, int knockback) {
	g_edict_t *light;

	light = ent->lightning;

	if (!light) { // ensure a valid lightning entity exists
		light = G_Spawn();

		VectorCopy(start, light->s.origin);
		VectorMA(start, 800.0, dir, light->s.old_origin);
		light->solid = SOLID_NOT;
		light->move_type = MOVE_TYPE_THINK;
		light->owner = ent;
		light->think = G_LightningProjectile_Think;
		light->knockback = knockback;
		light->s.client = ent - g_game.edicts; // player number, for client prediction fix
		light->s.effects = EF_BEAM | EF_LIGHTNING;
		light->s.sound = lightning_fly_index;
		light->class_name = "lightning";

		gi.LinkEntity(light);
		ent->lightning = light;
	}

	// set the damage and think time
	light->dmg = damage;
	light->timestamp = light->next_think = g_level.time;
}

/*
 * G_RailgunProjectile
 */
void G_RailgunProjectile(g_edict_t *ent, vec3_t start, vec3_t aimdir, int damage, int knockback) {
	vec3_t from;
	vec3_t end;
	c_trace_t tr;
	g_edict_t *ignore;
	vec3_t water_start;
	byte color;
	boolean_t water = false;
	int content_mask = MASK_SHOT | MASK_WATER;

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

		if ((tr.ent != ent) && (tr.ent->take_damage)) {
			if (tr.ent->client && ((int) g_level.gameplay == INSTAGIB))
				damage = 9999; // be sure to cause a kill
			G_Damage(tr.ent, ent, ent, aimdir, tr.end, tr.plane.normal, damage, knockback, 0,
					MOD_RAILGUN);
		}

		VectorCopy(tr.end, from);
	}

	// use team colors, or client's color
	if (g_level.teams || g_level.ctf) {
		if (ent->client->persistent.team == &g_team_good)
			color = ColorByName("blue", 0);
		else
			color = ColorByName("red", 0);
	} else
		color = ent->client->persistent.color;

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
		G_BurnMark(tr.end, &tr.plane, tr.surface, 6);
	}
}

/*
 * G_BfgProjectile_Touch
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

	if (other->take_damage) // hurt what we hit
		G_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal,
				self->dmg, self->knockback, DAMAGE_ENERGY, MOD_BFG_BLAST);

	// and anything nearby too
	G_RadiusDamage(self, self->owner, other, self->dmg, self->knockback, self->dmg_radius,
			MOD_BFG_BLAST);

	if (G_IsStructural(other, surf)) { // leave a burn mark
		VectorMA(self->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 30);
	}

	G_FreeEdict(self);
}

/*
 * G_BfgProjectile_Think
 */
static void G_BfgProjectile_Think(g_edict_t *self) {

	// radius damage
	G_RadiusDamage(self, self->owner, self->owner, self->dmg * 10 * gi.frame_seconds,
			self->knockback * 10 * gi.frame_seconds, self->dmg_radius, MOD_BFG_LASER);

	// linear, clamped acceleration
	if (VectorLength(self->velocity) < 1000.0)
		VectorScale(self->velocity, 1.0 + (0.5 * gi.frame_seconds), self->velocity);

	self->next_think = g_level.time + gi.frame_millis;
}

/*
 * G_BfgProjectiles
 */
void G_BfgProjectiles(g_edict_t *ent, vec3_t start, vec3_t dir, int speed, int damage,
		int knockback, float damage_radius) {
	g_edict_t *bfg;
	vec3_t angles, right, up, r, u;
	int i;
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
		VectorCopy(dir, bfg->move_dir);

		// and scatter randomly along the up and right vectors
		VectorScale(right, crand() * 0.35, r);
		VectorScale(up, crand() * 0.01, u);

		VectorAdd(bfg->move_dir, r, bfg->move_dir);
		VectorAdd(bfg->move_dir, u, bfg->move_dir);

		// finalize the direction and resolve angles, velocity, ..
		VectorAngles(bfg->move_dir, bfg->s.angles);

		s = speed + (0.2 * speed * crand());
		VectorScale(bfg->move_dir, s, bfg->velocity);

		bfg->move_type = MOVE_TYPE_FLY;
		bfg->clip_mask = MASK_SHOT;
		bfg->solid = SOLID_MISSILE;
		bfg->s.effects = EF_BFG;
		bfg->owner = ent;
		bfg->touch = G_BfgProjectile_Touch;
		bfg->think = G_BfgProjectile_Think;
		bfg->next_think = g_level.time + gi.frame_millis;
		bfg->dmg = damage;
		bfg->knockback = knockback;
		bfg->dmg_radius = damage_radius;
		bfg->class_name = "bfg projectile";

		G_PlayerProjectile(bfg, scale);

		gi.LinkEntity(bfg);
	}
}
