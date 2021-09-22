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

/**
 * @brief Adds a fraction of the player's velocity to the given projectile.
 */
static void G_PlayerProjectile(g_entity_t *ent, const float scale) {

	if (ent->owner) {
		const float s = scale * g_player_projectile->value;
		ent->locals.velocity = Vec3_Fmaf(ent->locals.velocity, s, ent->owner->locals.velocity);
	} else {
		G_Debug("No owner for %s\n", etos(ent));
	}
}

/**
 * @brief Returns true if the entity is facing a wall at too close proximity
 * for the specified projectile.
 */
static _Bool G_ImmediateWall(g_entity_t *ent, g_entity_t *projectile) {

	const cm_trace_t tr = gi.Trace(ent->s.origin, projectile->s.origin, projectile->bounds,
	                               ent, CONTENTS_MASK_SOLID);

	return tr.fraction < 1.0;
}

/**
 * @brief Returns true if the specified entity takes damage.
 */
static _Bool G_TakesDamage(g_entity_t *ent) {
	return (ent && ent->locals.take_damage);
}

/**
 * @brief Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(const vec3_t start, cm_trace_t *tr) {
	vec3_t dir, pos;

	if (Vec3_Equal(tr->end, start)) {
		return;
	}

	dir = Vec3_Subtract(tr->end, start);
	dir = Vec3_Normalize(dir);
	pos = Vec3_Fmaf(tr->end, -2.f, dir);

	if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
		tr->end = pos;
	} else {
		const cm_trace_t trace = gi.Trace(pos, start, Box3_Zero(), tr->ent, CONTENTS_MASK_LIQUID);
		tr->end = trace.end;
	}

	pos = Vec3_Add(start, tr->end);
	pos = Vec3_Scale(pos, .5f);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BUBBLES);
	gi.WritePosition(start);
	gi.WritePosition(tr->end);
	gi.Multicast(pos, MULTICAST_PHS, NULL);
}

/**
 * @brief Used to add tracer trails to bullet shots.
 */
static void G_Tracer(const vec3_t start, const vec3_t end) {
	vec3_t dir, mid;
	float len;

	dir = Vec3_Subtract(end, start);
	len = Vec3_Length(dir);

	if (len < 128.f) {
		return;
	}

	dir = Vec3_Normalize(dir);
	mid = Vec3_Fmaf(end, -len + (Randomf() * .05f * len), dir);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_TRACER);
	gi.WritePosition(mid);
	gi.WritePosition(end);
	gi.Multicast(start, MULTICAST_PHS, NULL);

	if (!gi.inPHS(start, end)) { // send to both PHS's
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_TRACER);
		gi.WritePosition(mid);
		gi.WritePosition(end);
		gi.Multicast(end, MULTICAST_PHS, NULL);
	}
}

/**
 * @brief Emit a water ripple and optional splash effect.
 * @details The caller must pass either an entity, or two points to trace between.
 * @param ent The entity entering the water, or NULL.
 * @param pos1 The start position to trace for liquid from.
 * @param pos2 The end position to trace for liquid to.
 * @param size The ripple size, or 0.0 to use the entity's size.
 * @param splash True to emit a splash effect, false otherwise.
 */
void G_Ripple(g_entity_t *ent, const vec3_t pos1, const vec3_t pos2, float size, _Bool splash) {
	vec3_t start, end;

	if (ent) {

		if (g_level.time - ent->locals.ripple_time < 400) {
			return;
		}

		ent->locals.ripple_time = g_level.time;

		start = Vec3_Add(ent->s.origin, ent->bounds.maxs);
		end = Vec3_Add(ent->s.origin, ent->bounds.mins);

		start.x = end.x = (start.x + end.x) * 0.5;
		start.y = end.y = (start.y + end.y) * 0.5;

		start.z += 16.0;
		end.z -= 16.0;

	} else {
		start = pos1;
		end = pos2;
	}

	const cm_trace_t tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_LIQUID);
	if (tr.start_solid || tr.fraction == 1.0) {
		G_Debug("%s failed to resolve water\n", etos(ent));
		return;
	}

	float viscosity;

	if (tr.side->contents & CONTENTS_SLIME) {
		viscosity = 20.0;
	} else if (tr.side->contents & CONTENTS_LAVA) {
		viscosity = 30.0;
	} else if (tr.side->contents & CONTENTS_WATER) {
		viscosity = 10.0;
	} else {
		G_Debug("%s failed to resolve water type\n", etos(ent));
		return;
	}

	vec3_t pos, dir;

	pos = Vec3_Add(tr.end, Vec3_Up());
	dir = tr.plane.normal;

	if (ent && size == 0.0) {
		if (ent->locals.ripple_size) {
			size = ent->locals.ripple_size;
		} else {
			size = Clampf(Box3_Distance(ent->bounds), 12.0, 64.0);
		}
	}

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_RIPPLE);
	gi.WritePosition(pos);
	gi.WriteDir(dir);
	gi.WriteByte((uint8_t) size);
	gi.WriteByte((uint8_t) viscosity);
	gi.WriteByte((uint8_t) splash);

	gi.Multicast(pos, MULTICAST_PVS, NULL);

	if (!(tr.side->contents & CONTENTS_TRANSLUCENT)) {
		pos = Vec3_Add(tr.end, Vec3_Down());
		dir = Vec3_Negate(dir);

		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_RIPPLE);
		gi.WritePosition(pos);
		gi.WriteDir(dir);
		gi.WriteByte((uint8_t) size);
		gi.WriteByte((uint8_t) viscosity);
		gi.WriteByte((uint8_t) false);

		gi.Multicast(pos, MULTICAST_PVS, NULL);
	}
}


/**
 * @brief Used to add impact marks on surfaces hit by bullets.
 */
static void G_BulletImpact(const cm_trace_t *trace) {

	if (trace->side->surface & SURF_ALPHA_TEST) {
		return;
	}

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_BULLET);
	gi.WritePosition(trace->end);
	gi.WriteDir(trace->plane.normal);

	gi.Multicast(trace->end, MULTICAST_PHS, NULL);
}

/**
 * @brief
 */
static void G_BlasterProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	if (!(trace->side->surface & SURF_SKY)) {

		G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, trace->plane.normal,
		         self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_BLASTER);

		if (G_IsStructural(trace->side)) {

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_BLASTER);
			gi.WritePosition(self->s.origin);
			gi.WriteDir(trace->plane.normal);
			gi.WriteByte(self->owner->s.number);
			gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
		}
	}

	G_FreeEntity(self);
}

/**
 * @brief
 */
void G_BlasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
                         int32_t damage, int32_t knockback) {

	const box3_t bounds = Box3f(2.f, 2.f, 2.f);

	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->s.angles = Vec3_Euler(dir);
	projectile->locals.velocity = Vec3_Scale(dir, speed);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + 8000;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.Touch = G_BlasterProjectile_Touch;
	projectile->s.client = ent->s.client; // player number, for trail color
	projectile->s.trail = TRAIL_BLASTER;

	gi.LinkEntity(projectile);
}

/**
 * @brief
 */
void G_BulletProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage,
                        int32_t knockback, int32_t hspread, int32_t vspread, int32_t mod) {

	cm_trace_t tr = gi.Trace(ent->s.origin, start, Box3f(1.f, 1.f, 1.f), ent, CONTENTS_MASK_CLIP_PROJECTILE);
	if (tr.fraction == 1.0) {
		vec3_t angles, forward, right, up, end;

		angles = Vec3_Euler(dir);
		Vec3_Vectors(angles, &forward, &right, &up);

		end = Vec3_Fmaf(start, MAX_WORLD_DIST, forward);
		end = Vec3_Fmaf(end, RandomRangef(-hspread, hspread), right);
		end = Vec3_Fmaf(end, RandomRangef(-vspread, vspread), up);

		tr = gi.Trace(start, end, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);

		G_Tracer(start, tr.end);
	}

	if (tr.fraction < 1.0) {

		G_Damage(tr.ent, ent, ent, dir, tr.end, tr.plane.normal, damage, knockback, DMG_BULLET, mod);

		if (G_IsStructural(tr.side)) {
			G_BulletImpact(&tr);
		}

		if (gi.PointContents(start) & CONTENTS_MASK_LIQUID) {
			G_Ripple(NULL, tr.end, start, 8.0, false);
			G_BubbleTrail(start, &tr);
		} else if (gi.PointContents(tr.end) & CONTENTS_MASK_LIQUID) {
			G_Ripple(NULL, start, tr.end, 8.0, true);
			G_BubbleTrail(start, &tr);
		}
	}
}

/**
 * @brief
 */
void G_ShotgunProjectiles(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage,
						  int32_t knockback, int32_t hspread, int32_t vspread, int32_t count, int32_t mod) {

	for (int32_t i = 0; i < count; i++) {
		G_BulletProjectile(ent, start, dir, damage, knockback, hspread, vspread, mod);
	}
}

#define HAND_GRENADE 1
#define HAND_GRENADE_HELD 2

/**
 * @brief
 */
static void G_GrenadeProjectile_Explode(g_entity_t *self) {
	int32_t mod;

	if (self->locals.enemy) { // direct hit

		vec3_t v;
		v = Box3_Center(self->locals.enemy->bounds);
		v = Vec3_Subtract(self->s.origin, v);

		const float dist = Vec3_Length(v);
		const int32_t d = self->locals.damage - 0.5 * dist;
		const int32_t k = self->locals.knockback - 0.5 * dist;

		const vec3_t dir = Vec3_Subtract(self->locals.enemy->s.origin, self->s.origin);

		mod = self->locals.spawn_flags & HAND_GRENADE ? MOD_HANDGRENADE : MOD_GRENADE;

		G_Damage(self->locals.enemy, self, self->owner, dir, self->s.origin, Vec3_Zero(), d, k, DMG_RADIUS, mod);
	}

	if (self->locals.spawn_flags & HAND_GRENADE) {
		if (self->locals.spawn_flags & HAND_GRENADE_HELD) {
			mod = MOD_HANDGRENADE_KAMIKAZE;
		} else {
			mod = MOD_HANDGRENADE_SPLASH;
		}
	} else {
		mod = MOD_GRENADE_SPLASH;
	}

	// hurt anything else nearby
	G_RadiusDamage(self, self->owner, self->locals.enemy, self->locals.damage,
	               self->locals.knockback, self->locals.damage_radius, mod);

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(Vec3_Up());
	gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);

	G_FreeEntity(self);
}

/**
 * @brief
 */
void G_GrenadeProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	if (!G_TakesDamage(other)) { // bounce off of structural solids

		if (G_IsStructural(trace->side)) {
			if (g_level.time - self->locals.touch_time > 200) {
				if (Vec3_Length(self->locals.velocity) > 40.0) {
					gi.Sound(self, g_media.sounds.grenade_hit, SOUND_ATTEN_LINEAR, (int8_t) (Randomf() * 5.0));
					self->locals.touch_time = g_level.time;
				}
			}
		} else if (G_IsSky(trace->side)) {
			G_FreeEntity(self);
		}

		return;
	}

	self->locals.enemy = other;
	G_GrenadeProjectile_Explode(self);
}

/**
 * @brief
 */
void G_GrenadeProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
						 int32_t damage, int32_t knockback, float damage_radius, uint32_t timer) {

	const box3_t bounds = Box3f(6.f, 6.f, 6.f);

	vec3_t forward, right, up;

	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->s.angles = Vec3_Euler(dir);

	Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
	projectile->locals.velocity = Vec3_Scale(dir, speed);

	projectile->locals.velocity = Vec3_Fmaf(projectile->locals.velocity, RandomRangef(90.f, 110.f), up);
	projectile->locals.velocity = Vec3_Fmaf(projectile->locals.velocity, RandomRangef(-10.f, 10.f), right);

	G_PlayerProjectile(projectile, 0.33);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.avelocity.x = RandomRangef(-310.f, -290.f);
	projectile->locals.avelocity.y = RandomRangef(-50.f, 50.f);
	projectile->locals.avelocity.z = RandomRangef(-25.f, 25.f);
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
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

	gi.LinkEntity(projectile);
}

// tossing a hand grenade
void G_HandGrenadeProjectile(g_entity_t *ent, g_entity_t *projectile,
                             vec3_t const start, const vec3_t dir, int32_t speed, int32_t damage,
							 int32_t knockback, float damage_radius, uint32_t timer) {

	const box3_t bounds = Box3f(4.f, 4.f, 4.f);

	vec3_t forward, right, up;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->s.angles = Vec3_Euler(dir);

	Vec3_Vectors(projectile->s.angles, &forward, &right, &up);
	projectile->locals.velocity = Vec3_Scale(dir, speed);

	projectile->locals.velocity = Vec3_Fmaf(projectile->locals.velocity, RandomRangef(190.f, 210.f), up);
	projectile->locals.velocity = Vec3_Fmaf(projectile->locals.velocity, RandomRangef(-10.f, 10.f), right);

	// add some of the player's velocity to the projectile
	G_PlayerProjectile(projectile, 0.33);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->locals.spawn_flags = HAND_GRENADE;

	// if client is holding it, let the nade know it's being held
	if (ent->client->locals.grenade_hold_time) {
		projectile->locals.spawn_flags |= HAND_GRENADE_HELD;
	}

	projectile->locals.avelocity.x = RandomRangef(-310.f, -290.f);
	projectile->locals.avelocity.y = RandomRangef(-50.f, 50.f);
	projectile->locals.avelocity.z = RandomRangef(-25.f, 25.f);
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.next_think = g_level.time + timer;
	projectile->solid = SOLID_BOX;
	projectile->sv_flags &= ~SVF_NO_CLIENT;
	projectile->locals.move_type = MOVE_TYPE_BOUNCE;
	projectile->locals.Think = G_GrenadeProjectile_Explode;
}

/**
 * @brief
 */
static void G_RocketProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	if (!G_IsSky(trace->side)) {

		if (G_IsStructural(trace->side) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, trace->plane.normal,
			         self->locals.damage, self->locals.knockback, 0, MOD_ROCKET);

			G_RadiusDamage(self, self->owner, other, self->locals.damage, self->locals.knockback,
			               self->locals.damage_radius, MOD_ROCKET_SPLASH);

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_EXPLOSION);
			gi.WritePosition(self->s.origin);
			gi.WriteDir(trace->plane.normal);
			gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
		}
	}

	G_FreeEntity(self);
}

/**
 * @brief
 */
void G_RocketProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
						int32_t damage, int32_t knockback, float damage_radius) {

	const box3_t bounds = Box3f(8.f, 8.f, 8.f);

	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->s.angles = Vec3_Euler(dir);
	projectile->locals.velocity = Vec3_Scale(dir, speed);
	projectile->locals.avelocity = Vec3(0.0, 0.0, 600.0);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.ripple_size = 32.0;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + 8000;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.Touch = G_RocketProjectile_Touch;
	projectile->s.model1 = g_media.models.rocket;
	projectile->s.sound = g_media.sounds.rocket_fly;
	projectile->s.trail = TRAIL_ROCKET;

	gi.LinkEntity(projectile);
}

/**
 * @brief
 */
static void G_HyperblasterProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	if (!G_IsSky(trace->side)) {

		if (G_IsStructural(trace->side) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, trace->plane.normal,
			         self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_HYPERBLASTER);

			if (G_IsStructural(trace->side)) {

				vec3_t v;
				v = Vec3_Subtract(self->s.origin, self->owner->s.origin);

				if (Vec3_Length(v) < 32.0) { // hyperblaster climbing
					G_Damage(self->owner, self, self->owner,
							 Vec3_Zero(), self->s.origin, trace->plane.normal,
					        g_balance_hyperblaster_climb_damage->integer, 0, DMG_ENERGY, MOD_HYPERBLASTER_CLIMB);

					self->owner->locals.velocity.z += g_balance_hyperblaster_climb_knockback->value;
				}
			}

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_HYPERBLASTER);
			gi.WritePosition(self->s.origin);
			gi.WriteDir(trace->plane.normal);
			gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
		}
	}

	G_FreeEntity(self);
}

/**
 * @brief
 */
void G_HyperblasterProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
							  int32_t damage, int32_t knockback) {

	const box3_t bounds = Box3f(6.f, 6.f, 6.f);

	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->s.angles = Vec3_Euler(dir);
	projectile->locals.velocity = Vec3_Scale(dir, speed);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.knockback = knockback;
	projectile->locals.ripple_size = 22.0;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + 6000;
	projectile->locals.Think = G_FreeEntity;
	projectile->locals.Touch = G_HyperblasterProjectile_Touch;
	projectile->s.trail = TRAIL_HYPERBLASTER;

	gi.LinkEntity(projectile);
}

/**
 * @brief
 */
static void G_LightningProjectile_Discharge(g_entity_t *self) {

	// kill ourselves
	G_Damage(self->owner, self, self->owner,
			 Vec3_Zero(), self->s.origin, Vec3_Zero(),
			 9999, 100, DMG_NO_ARMOR, MOD_LIGHTNING_DISCHARGE);

	g_entity_t *ent = g_game.entities;

	// and ruin the pool party for everyone else too
	for (int32_t i = 0; i < ge.num_entities; i++, ent++) {

		if (ent == self->owner) {
			continue;
		}

		if (!G_IsMeat(ent)) {
			continue;
		}

		if (gi.inPVS(self->s.origin, ent->s.origin)) {

			if (ent->locals.water_level) {
				const int32_t dmg = 50 * ent->locals.water_level;

				G_Damage(ent, self, self->owner, Vec3_Zero(), ent->s.origin, Vec3_Zero(),
						 dmg, 100, DMG_NO_ARMOR, MOD_LIGHTNING_DISCHARGE);
			}
		}
	}

	// send discharge event
	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_LIGHTNING_DISCHARGE);
	gi.WritePosition(self->s.origin);
}

/**
 * @brief
 */
static _Bool G_LightningProjectile_Expire(g_entity_t *self) {

	if (self->locals.timestamp < g_level.time - (SECONDS_TO_MILLIS(g_balance_lightning_refire->value) + 1)) {
		return true;
	}

	if (self->owner->locals.dead) {
		return true;
	}

	return false;
}

/**
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
	G_InitProjectile(self->owner, &forward, &right, &up, &start, 1.0);
	self->s.origin = start;

	if (G_ImmediateWall(self->owner, self)) { // resolve start
		start = self->owner->s.origin;
	}

	if (gi.PointContents(start) & CONTENTS_MASK_LIQUID) { // discharge and return
		G_LightningProjectile_Discharge(self);
		G_FreeEntity(self);
		return;
	}

	end = Vec3_Fmaf(start, g_balance_lightning_length->value, forward); // resolve end
	end = Vec3_Fmaf(end, 2.f * sinf(g_level.time / 4.f), up);
	end = Vec3_Fmaf(end, RandomRangef(-2.f, 2.f), right);

	tr = gi.Trace(start, end, Box3_Zero(), self, CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID);

	if (tr.side && tr.side->contents & CONTENTS_MASK_LIQUID) { // entered water, play sound, leave trail
		water_start = tr.end;

		if (!self->locals.water_level) {
			gi.PositionedSound(water_start, NULL, g_media.sounds.water_in, SOUND_ATTEN_LINEAR, 0);
			self->locals.water_level = WATER_FEET;
		}

		tr = gi.Trace(water_start, end, Box3_Zero(), self, CONTENTS_MASK_CLIP_PROJECTILE);
		G_BubbleTrail(water_start, &tr);

		G_Ripple(NULL, start, end, 16.f, true);
	} else {
		if (self->locals.water_level) { // exited water, play sound, no trail
			gi.PositionedSound(start, NULL, g_media.sounds.water_out, SOUND_ATTEN_LINEAR, 0);
			self->locals.water_level = WATER_NONE;
		}
	}

	// clear the angles for impact effects
	self->s.angles = Vec3_Zero();
	self->s.animation1 = LIGHTNING_NO_HIT;

	if (self->locals.damage) { // shoot, removing our damage until it is renewed
		if (G_TakesDamage(tr.ent)) { // try to damage what we hit
			G_Damage(tr.ent, self, self->owner, forward, tr.end, tr.plane.normal,
			         self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_LIGHTNING);
			self->locals.damage = 0;
		} else { // or leave a mark
			if (tr.side && tr.side->contents & CONTENTS_MASK_SOLID) {
				if (G_IsStructural(tr.side)) {
					self->s.angles = Vec3_Euler(tr.plane.normal);
					self->s.animation1 = LIGHTNING_SOLID_HIT;
				}
			}
		}
	}

	self->s.origin = start; // update end points
	self->s.termination = tr.end;

	gi.LinkEntity(self);

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
void G_LightningProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage,
						   int32_t knockback) {

	g_entity_t *projectile = NULL;

	while ((projectile = G_Find(projectile, EOFS(class_name), __func__))) {
		if (projectile->owner == ent) {
			break;
		}
	}

	if (!projectile) { // ensure a valid lightning entity exists
		projectile = G_AllocEntity();

		projectile->s.origin = start;

		if (G_ImmediateWall(ent, projectile)) {
			projectile->s.origin = ent->s.origin;
		}

		projectile->s.termination = Vec3_Fmaf(start, g_balance_lightning_length->value, dir);

		projectile->owner = ent;
		projectile->solid = SOLID_NOT;
		projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
		projectile->locals.move_type = MOVE_TYPE_THINK;
		projectile->locals.Think = G_LightningProjectile_Think;
		projectile->locals.knockback = knockback;
		projectile->s.client = ent->s.client; // player number, for client prediction fix
		projectile->s.effects = EF_BEAM;
		projectile->s.sound = g_media.sounds.lightning_fly;
		projectile->s.trail = TRAIL_LIGHTNING;

		gi.LinkEntity(projectile);
	}

	// set the damage and think time
	projectile->locals.damage = damage;
	projectile->locals.next_think = g_level.time + 1;
	projectile->locals.timestamp = g_level.time;
	projectile->locals.water_level = WATER_NONE;
}

/**
 * @brief
 */
void G_RailgunProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t damage,
						 int32_t knockback) {
	vec3_t pos, end;

	pos = start;

	cm_trace_t tr = gi.Trace(ent->s.origin, pos, Box3_Zero(), ent, CONTENTS_MASK_CLIP_PROJECTILE);
	if (tr.fraction < 1.0) {
		pos = ent->s.origin;
	}

	int32_t content_mask = CONTENTS_MASK_CLIP_PROJECTILE | CONTENTS_MASK_LIQUID;
	_Bool liquid = false;

	// are we starting in water?
	if (gi.PointContents(pos) & CONTENTS_MASK_LIQUID) {
		content_mask &= ~CONTENTS_MASK_LIQUID;
		liquid = true;
	}

	end = Vec3_Fmaf(pos, MAX_WORLD_DIST, dir);

	G_Ripple(NULL, pos, end, 24.0, true);

	g_entity_t *ignore = ent;
	while (ignore) {
		tr = gi.Trace(pos, end, Box3_Zero(), ignore, content_mask);
		if (!tr.ent) {
			break;
		}

		if ((tr.side->contents & CONTENTS_MASK_LIQUID) && !liquid) {

			content_mask &= ~CONTENTS_MASK_LIQUID;
			liquid = true;

			gi.PositionedSound(tr.end, NULL, g_media.sounds.water_in, SOUND_ATTEN_LINEAR, 0);

			ignore = ent;
			continue;
		}

		if (tr.ent->client || tr.ent->solid == SOLID_BOX) {
			ignore = tr.ent;
		} else {
			ignore = NULL;
		}

		// we've hit something, so damage it
		if ((tr.ent != ent) && G_TakesDamage(tr.ent)) {
			G_Damage(tr.ent, ent, ent, dir, tr.end, tr.plane.normal, damage, knockback, 0,
			         MOD_RAILGUN);
		}

		pos = tr.end;
	}

	// send rail trail
	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_RAIL);
	gi.WritePosition(start);
	gi.WritePosition(tr.end);
	gi.WriteDir(tr.plane.normal);
	gi.WriteLong(tr.side ? tr.side->surface : 0);
	gi.WriteByte(ent->s.number);

	gi.Multicast(start, MULTICAST_PHS, NULL);

	if (!gi.inPHS(start, tr.end)) { // send to both PHS's
		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_RAIL);
		gi.WritePosition(start);
		gi.WritePosition(tr.end);
		gi.WriteDir(tr.plane.normal);
		gi.WriteLong(tr.side ? tr.side->surface : 0);
		gi.WriteByte(ent->s.number);

		gi.Multicast(tr.end, MULTICAST_PHS, NULL);
	}
}

/**
 * @brief
 */
static void G_BfgProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	if (!G_IsSky(trace->side)) {

		if (G_IsStructural(trace->side) || G_IsMeat(other)) {

			G_Damage(other, self, self->owner, self->locals.velocity, self->s.origin, trace->plane.normal,
			         self->locals.damage, self->locals.knockback, DMG_ENERGY, MOD_BFG_BLAST);

			G_RadiusDamage(self, self->owner, other, self->locals.damage, self->locals.knockback,
			               self->locals.damage_radius, MOD_BFG_BLAST);

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_BFG);
			gi.WritePosition(self->s.origin);
			gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
		}
	}

	G_FreeEntity(self);
}

/**
 * @brief
 */
static void G_BfgProjectile_Think(g_entity_t *self) {

	const int32_t frame_damage = self->locals.damage * QUETOO_TICK_SECONDS;
	const int32_t frame_knockback = self->locals.knockback * QUETOO_TICK_SECONDS;

	g_entity_t *ent = NULL;
	while ((ent = G_FindRadius(ent, self->s.origin, self->locals.damage_radius)) != NULL) {

		if (ent == self || ent == self->owner) {
			continue;
		}

		if (!ent->locals.take_damage) {
			continue;
		}

		if (!G_CanDamage(ent, self)) {
			continue;
		}

		const vec3_t end = G_GetOrigin(ent);
		const vec3_t dir = Vec3_Subtract(end, self->s.origin);
		const float dist = Vec3_Length(dir);
		const vec3_t normal = Vec3_Normalize(Vec3_Negate(dir));

		const float f = 1.0 - dist / self->locals.damage_radius;

		G_Damage(ent, self, self->owner, dir, ent->s.origin, normal,
				 frame_damage * f, frame_knockback * f, DMG_RADIUS, MOD_BFG_LASER);

		gi.WriteByte(SV_CMD_TEMP_ENTITY);
		gi.WriteByte(TE_BFG_LASER);
		gi.WriteShort(self->s.number);
		gi.WriteShort(ent->s.number);
		gi.Multicast(self->s.origin, MULTICAST_PVS, NULL);
	}

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
void G_BfgProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir, int32_t speed,
					 int32_t damage, int32_t knockback, float damage_radius) {

	const box3_t bounds = Box3f(24.f, 24.f, 24.f);

	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->bounds = bounds;
	projectile->locals.velocity = Vec3_Scale(dir, speed);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	projectile->locals.damage = damage;
	projectile->locals.damage_radius = damage_radius;
	projectile->locals.knockback = knockback;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	projectile->locals.Think = G_BfgProjectile_Think;
	projectile->locals.Touch = G_BfgProjectile_Touch;
	projectile->s.trail = TRAIL_BFG;

	gi.LinkEntity(projectile);
}

/**
 * @brief
 */
static void G_HookProjectile_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other == self->owner) {
		return;
	}

	if (other->solid < SOLID_DEAD) {
		return;
	}

	if (trace == NULL) {
		return;
	}

	self->s.sound = 0;

	if (!G_IsSky(trace->side)) {

		if (G_IsStructural(trace->side) || (G_IsMeat(other) && G_OnSameTeam(other, self->owner))) {

			self->locals.velocity = Vec3_Zero();
			self->locals.avelocity = Vec3_Zero();

			self->owner->client->locals.hook_pull = true;

			self->locals.move_type = MOVE_TYPE_THINK;
			self->solid = SOLID_NOT;
			self->bounds = Box3_Zero();
			self->locals.enemy = other;

			gi.LinkEntity(self);

			self->owner->client->ps.pm_state.hook_position = self->s.origin;

			if (self->owner->client->locals.persistent.hook_style != HOOK_PULL) {
				const float distance = Vec3_Distance(self->owner->s.origin, self->s.origin);

				self->owner->client->ps.pm_state.hook_length = Clampf(distance, PM_HOOK_MIN_DIST, g_hook_distance->value);
			}

			gi.Sound(self, g_media.sounds.hook_hit, SOUND_ATTEN_LINEAR, RandomRangei(-4, 5));

			gi.WriteByte(SV_CMD_TEMP_ENTITY);
			gi.WriteByte(TE_HOOK_IMPACT);
			gi.WritePosition(self->s.origin);
			gi.WriteDir(trace->plane.normal);
			gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
		} else {

			gi.Sound(self, g_media.sounds.hook_gibhit, SOUND_ATTEN_LINEAR, RandomRangei(-4, 5));

			/*
			if (g_hook_auto_refire->integer) {
				G_ClientHookThink(self->owner, true);
			} else {*/
				self->locals.velocity = Vec3_Normalize(self->locals.velocity);

				G_Damage(other, self, self->owner,
						 self->locals.velocity, self->s.origin, Vec3_Zero(),
						 5, 0, 0, MOD_HOOK);

				G_ClientHookDetach(self->owner);
//			}
		}
	} else {
		/* Currently disabled due to bugs
		if (g_hook_auto_refire->integer) {
			G_ClientHookThink(self->owner, true);
		} else {
		*/
			G_ClientHookDetach(self->owner);
//		}
	}
}

/**
 * @brief
 */
static void G_HookTrail_Think(g_entity_t *ent) {

	const g_entity_t *hook = ent->locals.target_ent;
	g_entity_t *player = ent->owner;

	vec3_t forward, right, up, org;

	G_InitProjectile(player, &forward, &right, &up, &org, -1.0);

	ent->s.origin = org;
	ent->s.termination = hook->s.origin;

	vec3_t distance;
	distance = Vec3_Subtract(org, hook->s.origin);

	if (Vec3_Length(distance) > g_hook_distance->value) {

		G_ClientHookDetach(player);
		return;
	}

	ent->locals.next_think = g_level.time + 1;
	gi.LinkEntity(ent);
}

/**
 * @brief
 */
static void G_HookProjectile_Think(g_entity_t *ent) {

	// if we're attached to something, copy velocities
	if (ent->locals.enemy) {
		g_entity_t *mover = ent->locals.enemy;
		vec3_t move, amove, inverse_amove, forward, right, up, rotate, translate, delta;

		move = Vec3_Scale(mover->locals.velocity, QUETOO_TICK_SECONDS);
		amove = Vec3_Scale(mover->locals.avelocity, QUETOO_TICK_SECONDS);

		if (!Vec3_Equal(move, Vec3_Zero()) || !Vec3_Equal(amove, Vec3_Zero())) {
			inverse_amove = Vec3_Negate(amove);
			Vec3_Vectors(inverse_amove, &forward, &right, &up);

			// translate the pushed entity
			ent->s.origin = Vec3_Add(ent->s.origin, move);

			// then rotate the movement to comply with the pusher's rotation
			translate = Vec3_Subtract(ent->s.origin, mover->s.origin);

			rotate.x = Vec3_Dot(translate, forward);
			rotate.y = -Vec3_Dot(translate, right);
			rotate.z = Vec3_Dot(translate, up);

			delta = Vec3_Subtract(rotate, translate);

			ent->s.origin = Vec3_Add(ent->s.origin, delta);

			// FIXME: any way we can have the hook move on all axis?
			ent->s.angles.y += amove.y;
			ent->locals.target_ent->s.angles.y += amove.y;

			gi.LinkEntity(ent);

			ent->owner->client->ps.pm_state.hook_position = ent->s.origin;
		}

		if ((ent->owner->client->locals.persistent.hook_style == HOOK_PULL && Vec3_LengthSquared(ent->owner->locals.velocity) > 128.0) ||
			ent->locals.knockback != ent->owner->client->ps.pm_state.hook_length) {
			ent->s.sound = g_media.sounds.hook_pull;
			ent->locals.knockback = ent->owner->client->ps.pm_state.hook_length;
		} else {
			ent->s.sound = 0;
		}
	}

	ent->locals.next_think = g_level.time + 1;
}

/**
 * @brief
 */
g_entity_t *G_HookProjectile(g_entity_t *ent, const vec3_t start, const vec3_t dir) {
	g_entity_t *projectile = G_AllocEntity();
	projectile->owner = ent;

	projectile->s.origin = start;
	projectile->s.angles = Vec3_Euler(dir);
	projectile->locals.velocity = Vec3_Scale(dir, g_hook_speed->value);
	projectile->locals.avelocity = Vec3(0, 0, 500);

	if (G_ImmediateWall(ent, projectile)) {
		projectile->s.origin = ent->s.origin;
	}

	projectile->solid = SOLID_PROJECTILE;
	projectile->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	projectile->locals.move_type = MOVE_TYPE_FLY;
	projectile->locals.Touch = G_HookProjectile_Touch;
	projectile->s.model1 = g_media.models.hook;
	projectile->locals.Think = G_HookProjectile_Think;
	projectile->locals.next_think = g_level.time + 1;
	projectile->s.sound = g_media.sounds.hook_fly;

	gi.LinkEntity(projectile);

	g_entity_t *trail = G_AllocEntity();

	projectile->locals.target_ent = trail;
	trail->locals.target_ent = projectile;

	trail->owner = ent;
	trail->solid = SOLID_NOT;
	trail->locals.clip_mask = CONTENTS_MASK_CLIP_PROJECTILE;
	trail->locals.move_type = MOVE_TYPE_THINK;
	trail->s.client = ent->s.client; // player number, for client prediction fix
	trail->s.effects = EF_BEAM;
	trail->s.trail = TRAIL_HOOK;
	trail->locals.Think = G_HookTrail_Think;
	trail->locals.next_think = g_level.time + 1;

	G_HookTrail_Think(trail);

	// angle is used for rendering on client side
	trail->s.angles = projectile->s.angles;

	gi.LinkEntity(trail);
	return projectile;
}
