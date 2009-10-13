/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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
 * G_PlayerProjectile
 */
static void G_PlayerProjectile(edict_t *ent, vec3_t scale){
	vec3_t tmp;
	int i;

	if(!ent->owner)
		return;

	for(i = 0; i < 3; i++)
		tmp[i] = ent->owner->velocity[i] * g_playerprojectile->value * scale[i];

	VectorAdd(tmp, ent->velocity, ent->velocity);
}


/*
 * G_ImmediateWall
 */
static qboolean G_ImmediateWall(edict_t *ent, vec3_t dir){
	trace_t tr;
	vec3_t end;

	VectorMA(ent->s.origin, 30, dir, end);
	tr = gi.Trace(ent->s.origin, NULL, NULL, end, ent, MASK_SOLID);

	return tr.fraction < 1.0;
}


/*
 * G_IsStructural
 */
static qboolean G_IsStructural(edict_t *ent, csurface_t *surf){

	if(!ent || ent->client || ent->takedamage)
		return false;  // we hit nothing, or something we damaged

	if(!surf || (surf->flags & SURF_SKY))
		return false;  // we hit nothing, or the sky

	// don't leave marks on moving world objects
	return G_IsStationary(ent);
}


/*
 * G_BubbleTrail
 *
 * Used to add generic bubble trails to shots.
 */
static void G_BubbleTrail(vec3_t start, trace_t *tr){
	vec3_t dir, pos;

	if(VectorCompare(tr->endpos, start))
		return;

	VectorSubtract(tr->endpos, start, dir);
	VectorNormalize(dir);
	VectorMA(tr->endpos, -2, dir, pos);

	if(gi.PointContents(pos) & MASK_WATER)
		VectorCopy(pos, tr->endpos);
	else
		tr->endpos = gi.Trace(pos, NULL, NULL, start, tr->ent, MASK_WATER).endpos;

	VectorAdd(start, tr->endpos, pos);
	VectorScale(pos, 0.5, pos);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BUBBLETRAIL);
	gi.WritePosition(start);
	gi.WritePosition(tr->endpos);
	gi.Multicast(pos, MULTICAST_PVS);
}


/*
 * G_Tracer
 *
 * Used to add trails to bullet shots.
 */
static void G_Tracer(vec3_t start, vec3_t end){
	vec3_t dir, mid;
	float len;

	VectorSubtract(end, start, dir);
	len = VectorNormalize(dir);

	if(len < 128.0)
		return;

	VectorMA(end, -len + (frand() * 0.05 * len), dir, mid);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_TRACER);
	gi.WritePosition(mid);
	gi.WritePosition(end);
	gi.Multicast(mid, MULTICAST_PVS);
}


/*
 * G_BulletMark
 */
static void G_BulletMark(vec3_t org, cplane_t *plane, csurface_t *surf){

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BULLET);
	gi.WritePosition(org);
	gi.WriteDir(plane->normal);

	gi.Multicast(org, MULTICAST_PVS);
}


/*
 * G_BurnMark
 *
 * Used to add burn marks on surfaces hit by projectiles.
 */
static void G_BurnMark(vec3_t org, cplane_t *plane, csurface_t *surf, byte scale){

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BURN);
	gi.WritePosition(org);
	gi.WriteDir(plane->normal);
	gi.WriteByte(scale);

	gi.Multicast(org, MULTICAST_PVS);
}


/*
 * G_FireBullet
 *
 * Used by bullet and pellet weapons.
 */
void G_FireBullet(edict_t *self, vec3_t start, vec3_t aimdir,
		int damage, int knockback, int hspread, int vspread, int mod){
	trace_t tr;
	vec3_t dir;
	vec3_t forward, right, up;
	vec3_t end;
	float r;
	float u;
	vec3_t water_start;
	qboolean water = false;
	int content_mask = MASK_SHOT | MASK_WATER;

	tr = gi.Trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
	if(!(tr.fraction < 1.0)){
		VectorAngles(aimdir, dir);
		AngleVectors(dir, forward, right, up);

		r = crandom() * hspread;
		u = crandom() * vspread;
		VectorMA(start, 8192, forward, end);
		VectorMA(end, r, right, end);
		VectorMA(end, u, up, end);

		if(gi.PointContents(start) & MASK_WATER){
			water = true;
			VectorCopy(start, water_start);
			content_mask &= ~MASK_WATER;
		}

		tr = gi.Trace(start, NULL, NULL, end, self, content_mask);

		// see if we hit water
		if(tr.contents & MASK_WATER && !water){

			water = true;
			VectorCopy(tr.endpos, water_start);

			// change bullet's course when it enters water
			VectorSubtract(end, start, dir);
			VectorAngles(dir, dir);
			AngleVectors(dir, forward, right, up);
			r = crandom() * hspread * 2;
			u = crandom() * vspread * 2;
			VectorMA(water_start, 8192, forward, end);
			VectorMA(end, r, right, end);
			VectorMA(end, u, up, end);

			// re-trace ignoring water this time
			tr = gi.Trace(water_start, NULL, NULL, end, self, MASK_SHOT);
		}
	}

	// send trails and marks
	if(tr.fraction < 1.0){

		if(tr.ent->takedamage){  // bleed and damage the enemy
			G_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal,
					damage, knockback, DAMAGE_BULLET, mod);
		}
		else {  // leave an impact mark on the wall
			if(G_IsStructural(tr.ent, tr.surface)){
				G_BulletMark(tr.endpos, &tr.plane, tr.surface);
			}
		}
	}

	if(water){
		G_BubbleTrail(water_start, &tr);
		G_Tracer(start, water_start);
	}
	else {
		G_Tracer(start, tr.endpos);
	}
}


/*
 * G_FireShotgun
 *
 * Fires shotgun pellets.  Used by shotgun and super shotgun.
 */
void G_FireShotgun(edict_t *self, vec3_t start, vec3_t aimdir,
		int damage, int knockback, int hspread, int vspread, int count, int mod){
	int i;

	for(i = 0; i < count; i++)
		G_FireBullet(self, start, aimdir, damage, knockback, hspread, vspread, mod);
}


/*
 * G_FireGrenadeLauncher
 */
static void G_GrenadeExplode(edict_t *ent){
	vec3_t origin;

	if(ent->enemy){  // direct hit
		float d, k, dist;
		vec3_t v, dir;

		VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
		VectorMA(ent->enemy->s.origin, 0.5, v, v);
		VectorSubtract(ent->s.origin, v, v);

		dist = VectorLength(v);
		d = ent->dmg - 0.5 * dist;
		k = ent->knockback - 0.5 * dist;

		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);

		G_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin,
				(int)d, (int)k, DAMAGE_RADIUS, MOD_GRENADE);
	}

	// hurt anything else nearby
	G_RadiusDamage(ent, ent->owner, ent->enemy, ent->dmg, ent->knockback,
			ent->dmg_radius, MOD_G_SPLASH);

	if(G_IsStationary(ent->groundentity))
		VectorMA(ent->s.origin, 16.0, ent->plane.normal, origin);
	else
		VectorCopy(ent->s.origin, origin);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if(G_IsStationary(ent->groundentity))
		G_BurnMark(ent->s.origin, &ent->plane, &ent->surf, 20);

	G_FreeEdict(ent);
}

static void G_GrenadeTouch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf){
	float vel, dot;
	vec3_t dir;

	if(other == ent->owner)
		return;

	if(surf && (surf->flags & SURF_SKY)){
		G_FreeEdict(ent);
		return;
	}

	if(!other->takedamage){  // bounce or detonate, based on time

		VectorCopy(ent->velocity, dir);

		if((vel = VectorNormalize(dir)) < 250.0)
			return;

		dot = 0.0;

		if(plane && surf){

			ent->plane = *plane;
			ent->surf = *surf;

			dot = DotProduct(plane->normal, dir);
		}

		// we're live after a brief safety period for the owner
		if(dot < -0.25 && level.time - ent->touch_time > 0.25){
			ent->groundentity = other;
			G_GrenadeExplode(ent);
		}
		else
			gi.Sound(ent, grenade_hit_index, ATTN_NORM);

		return;
	}

	ent->enemy = other;
	G_GrenadeExplode(ent);
}

void G_FireGrenadeLauncher(edict_t *self, vec3_t start, vec3_t aimdir, int speed,
		int damage, int knockback, float damage_radius, float timer){
	edict_t *grenade;
	vec3_t dir;
	vec3_t forward, right, up;
	vec3_t mins = {-2, -2, -2};
	vec3_t maxs = { 2,  2,  2};
	vec3_t scale = {0.3, 0.3, 0.3};

	if(G_ImmediateWall(self, aimdir))
		VectorCopy(self->s.origin, start);

	VectorAngles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);
	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200.0 + crandom() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet(grenade->avelocity, crand(), crand(), crand());
	VectorScale(grenade->avelocity, 600.0, grenade->avelocity);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_MISSILE;
	grenade->s.effects = EF_GRENADE;
	VectorCopy(mins, grenade->mins);
	VectorCopy(maxs, grenade->maxs);
	grenade->s.modelindex = grenade_index;
	grenade->owner = self;
	grenade->touch = G_GrenadeTouch;
	grenade->touch_time = level.time;
	grenade->nextthink = level.time + timer;
	grenade->think = G_GrenadeExplode;
	grenade->dmg = damage;
	grenade->knockback = knockback;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	G_PlayerProjectile(grenade, scale);

	gi.LinkEntity(grenade);
}


/*
 * G_FireRocketLauncher
 */
static void G_RocketTouch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf){
	vec3_t origin;

	if(other == ent->owner)
		return;

	if(surf && (surf->flags & SURF_SKY)){
		G_FreeEdict(ent);
		return;
	}

	// calculate position for the explosion entity
	if(plane && surf)
		VectorMA(ent->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(ent->s.origin, origin);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PHS);

	if(other->takedamage){  // direct hit
		G_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
				plane->normal, ent->dmg, ent->knockback, 0, MOD_ROCKET);
	}

	// hurt anything else nearby
	G_RadiusDamage(ent, ent->owner, other, ent->dmg, ent->knockback,
			ent->dmg_radius, MOD_R_SPLASH);

	if(G_IsStructural(other, surf)){  // leave a burn mark
		VectorMA(ent->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 20);
	}

	G_FreeEdict(ent);
}

void G_FireRocketLauncher(edict_t *self, vec3_t start, vec3_t dir, int speed,
		int damage, int knockback, float damage_radius){
	edict_t *rocket;
	vec3_t dest;
	trace_t tr;
	vec3_t scale = {0.25, 0.25, 0.15};

	if(G_ImmediateWall(self, dir))
		VectorCopy(self->s.origin, start);

	rocket = G_Spawn();
	VectorCopy(start, rocket->s.origin);
	VectorCopy(start, rocket->s.old_origin);
	VectorCopy(dir, rocket->movedir);
	VectorAngles(dir, rocket->s.angles);
	VectorScale(dir, speed, rocket->velocity);
	rocket->movetype = MOVETYPE_FLYMISSILE;
	rocket->clipmask = MASK_SHOT;
	rocket->solid = SOLID_MISSILE;
	rocket->s.effects = EF_ROCKET;
	rocket->s.modelindex = rocket_index;
	rocket->owner = self;
	rocket->touch = G_RocketTouch;
	rocket->nextthink = level.time + 8.0;
	rocket->think = G_FreeEdict;
	rocket->dmg = damage;
	rocket->knockback = knockback;
	rocket->dmg_radius = damage_radius;
	rocket->s.sound = rocket_fly_index;
	rocket->classname = "rocket";

	VectorMA(start, 8192.0, dir, dest);
	tr = gi.Trace(start, NULL, NULL, dest, self, MASK_SHOT);
	VectorCopy(tr.endpos, rocket->dest);

	G_PlayerProjectile(rocket, scale);

	gi.LinkEntity(rocket);
}


/*
 * G_FireHyperblaster
 */
static void G_HyperblasterTouch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	vec3_t origin;
	vec3_t v;

	if(other == self->owner)
		return;

	if(surf && (surf->flags & SURF_SKY)){
		G_FreeEdict(self);
		return;
	}

	// calculate position for the explosion entity
	if(plane && surf)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_HYPERBLASTER);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PVS);

	if(other->takedamage){
		G_Damage(other, self, self->owner, self->velocity, self->s.origin,
				plane->normal, self->dmg, self->knockback, DAMAGE_ENERGY, MOD_HYPERBLASTER);
	} else {
		if(G_IsStructural(other, surf)){  // leave a burn mark
			VectorMA(self->s.origin, 2.0, plane->normal, origin);
			G_BurnMark(origin, plane, surf, 10);

			VectorSubtract(self->s.origin, self->owner->s.origin, v);

			if(VectorLength(v) < 32.0){  // hyperblaster climb
				G_Damage(self->owner, self, self->owner, vec3_origin, self->s.origin,
						plane->normal, self->dmg * 0.06, 0, DAMAGE_ENERGY, MOD_HYPERBLASTER);

				self->owner->velocity[2] += 70.0;
			}
		}
	}

	G_FreeEdict(self);
}

void G_FireHyperblaster(edict_t *self, vec3_t start, vec3_t dir,
		int speed, int damage, int knockback){
	edict_t *bolt;
	vec3_t scale = {0.5, 0.5, 0.25};

	if(G_ImmediateWall(self, dir))
		VectorCopy(self->s.origin, start);

	bolt = G_Spawn();
	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	VectorAngles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->velocity);
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_SHOT;
	bolt->solid = SOLID_MISSILE;
	bolt->s.effects = EF_HYPERBLASTER;
	bolt->owner = self;
	bolt->touch = G_HyperblasterTouch;
	bolt->nextthink = level.time + 3.0;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->knockback = knockback;
	bolt->classname = "bolt";

	G_PlayerProjectile(bolt, scale);

	gi.LinkEntity(bolt);
}


/*
 * G_FireLightning
 */
static void G_LightningDischarge(edict_t *self){
	edict_t *ent;
	int i, d;

	// find all clients in the same water area and kill them
	for(i = 0; i < sv_maxclients->value; i++){

		ent = &g_edicts[i + 1];

		if(!ent->inuse)
			continue;

		if(!ent->takedamage)  // dead or spectator
			continue;

		if(gi.inPVS(self->s.origin, ent->s.origin)){

			if(ent->waterlevel){

				// we always kill ourselves, we inflict a lot of damage but
				// we don't necessarily kill everyone else
				d = ent == self ? 999 : 50 * ent->waterlevel;

				G_Damage(ent, self, self->owner, vec3_origin, ent->s.origin, vec3_origin,
						d, 100, DAMAGE_NO_ARMOR, MOD_L_DISCHARGE);
			}
		}
	}

	// send discharge event
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_LIGHTNING);
	gi.WritePosition(self->s.origin);
}

static qboolean G_LightningExpire(edict_t *self){

	if(self->timestamp < level.time - 0.101)
		return true;

	if(self->owner->dead)
		return true;

	return false;
}

static void G_LightningThink(edict_t *self){
	vec3_t forward, right, offset;
	vec3_t start, end;
	vec3_t water_start;
	trace_t tr;

	if(G_LightningExpire(self)){
		self->owner->lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	// re-calculate endpoints based on owner's movement
	AngleVectors(self->owner->client->angles, forward, right, NULL);
	VectorSet(offset, 30.0, 6.0, self->owner->viewheight - 10.0);
	G_ProjectSource(self->owner->s.origin, offset, forward, right, start);

	if(G_ImmediateWall(self->owner, forward))  // resolve start
		VectorCopy(self->owner->s.origin, start);

	if(gi.PointContents(start) & MASK_WATER){  // discharge and return
		G_LightningDischarge(self);
		self->owner->lightning = NULL;
		G_FreeEdict(self);
		return;
	}

	VectorMA(start, 666.0, forward, end);  // resolve end
	tr = gi.Trace(start, NULL, NULL, end, self, MASK_SHOT | MASK_WATER);

	if(tr.contents & MASK_WATER){  // entered water, play sound, leave trail
		VectorCopy(tr.endpos, water_start);

		if(!self->waterlevel){
			gi.PositionedSound(water_start, g_edicts,
					gi.SoundIndex("world/water_in"), ATTN_NORM);
			self->waterlevel = 1;
		}

		tr = gi.Trace(water_start, NULL, NULL, end, self, MASK_SHOT);
		G_BubbleTrail(water_start, &tr);
	}
	else {
		if(self->waterlevel){  // exited water, play sound, no trail
			gi.PositionedSound(water_start, g_edicts,
					gi.SoundIndex("world/water_out"), ATTN_NORM);
			self->waterlevel = 0;
		}
	}

	if(self->timestamp <= level.time){  // shoot
		if(tr.ent->takedamage){  // try to damage what we hit
			G_Damage(tr.ent, self, self->owner, forward, tr.endpos, tr.plane.normal,
					self->dmg, self->knockback, DAMAGE_ENERGY, MOD_LIGHTNING);
		}
		else {  // or leave a mark
			if((tr.contents & CONTENTS_SOLID) && G_IsStructural(tr.ent, tr.surface))
				G_BurnMark(tr.endpos, &tr.plane, tr.surface, 8);
		}
	}

	VectorCopy(start, self->s.origin);  // update endpoints
	VectorCopy(tr.endpos, self->s.old_origin);

	gi.LinkEntity(self);

	self->nextthink = level.time + gi.serverframe;
}

void G_FireLightning(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int knockback){
	edict_t *light;

	light = self->lightning;

	if(!light){  // ensure a valid lightning entity exists
		light = G_Spawn();
		VectorCopy(start, light->s.origin);
		VectorCopy(start, light->s.old_origin);
		light->solid = SOLID_NOT;
		light->movetype = MOVETYPE_THINK;
		light->owner = self;
		light->think = G_LightningThink;
		light->dmg = damage;
		light->knockback = knockback;
		light->s.skinnum = self - g_edicts;  // player number, for client prediction fix
		light->s.effects = EF_BEAM | EF_LIGHTNING;
		light->s.sound = lightning_fly_index;
		light->classname = "lightning";

		gi.LinkEntity(light);
		self->lightning = light;
	}

	// set the damage and think time
	light->timestamp = light->nextthink = level.time;
}


/*
 * G_FireRailgun
 */
void G_FireRailgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int knockback){
	vec3_t from;
	vec3_t end;
	trace_t tr;
	edict_t *ignore;
	vec3_t water_start;
	byte color;
	qboolean water = false;
	int content_mask = MASK_SHOT | MASK_WATER;

	if(G_ImmediateWall(self, aimdir))
		VectorCopy(self->s.origin, start);

	VectorMA(start, 8192, aimdir, end);
	VectorCopy(start, from);
	ignore = self;

	// are we starting in water?
	if(gi.PointContents(start) & MASK_WATER){
		content_mask &= ~MASK_WATER;
		VectorCopy(start, water_start);
		water = true;
	}

	while(ignore){
		tr = gi.Trace(from, NULL, NULL, end, ignore, content_mask);

		if(tr.contents & MASK_WATER && !water){

			content_mask &= ~MASK_WATER;
			water = true;

			VectorCopy(tr.endpos, water_start);

			gi.PositionedSound(water_start, g_edicts,
					gi.SoundIndex("world/water_in"), ATTN_NORM);

			ignore = self;
			continue;
		}

		if(tr.ent->client || tr.ent->solid == SOLID_BBOX)
			ignore = tr.ent;
		else
			ignore = NULL;

		if((tr.ent != self) && (tr.ent->takedamage)){
			if(tr.ent->client && ((int)level.gameplay == INSTAGIB))
				damage = 9999;  // be sure to cause a kill
			G_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal,
					damage, knockback, 0, MOD_RAILGUN);
		}

		VectorCopy(tr.endpos, from);
	}

	// send rail trail
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_RAILTRAIL);
	gi.WritePosition(start);
	gi.WritePosition(tr.endpos);

	// use team colors, or client's color
	if(level.teams || level.ctf){
		if(self->client->locals.team == &good)
			color = ColorByName("blue", 0);
		else
			color = ColorByName("red", 0);
	}
	else
		color = self->client->locals.color;

	gi.WriteByte(color);

	gi.Multicast(self->s.origin, MULTICAST_PHS);

	// calculate position of burn mark
	if(G_IsStructural(tr.ent, tr.surface)){
		VectorMA(tr.endpos, -1, aimdir, tr.endpos);
		G_BurnMark(tr.endpos, &tr.plane, tr.surface, 6);
	}
}


/*
 * G_FireBFG
 */
static void G_BFGTouch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	vec3_t origin;

	if(other == self->owner)
		return;

	if(surf && (surf->flags & SURF_SKY)){
		G_FreeEdict(self);
		return;
	}

	// calculate position for the explosion entity
	if(plane && surf)
		VectorMA(self->s.origin, 16.0, plane->normal, origin);
	else
		VectorCopy(self->s.origin, origin);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BFG);
	gi.WritePosition(origin);
	gi.Multicast(origin, MULTICAST_PVS);

	if(other->takedamage)  // hurt what we hit
		G_Damage(other, self, self->owner, self->velocity, self->s.origin,
				plane->normal, self->dmg, self->knockback, DAMAGE_ENERGY, MOD_BFG_BLAST);

	// and anything nearby too
	G_RadiusDamage(self, self->owner, other, self->dmg, self->knockback,
			self->dmg_radius, MOD_BFG_BLAST);

	if(G_IsStructural(other, surf)){  // leave a burn mark
		VectorMA(self->s.origin, 2.0, plane->normal, origin);
		G_BurnMark(origin, plane, surf, 30);
	}

	G_FreeEdict(self);
}

static void G_BFGThink(edict_t *self){

	// radius damage
	G_RadiusDamage(self, self->owner, self->owner, self->dmg * 10.0 * gi.serverframe,
			self->knockback * 10 * gi.serverframe, self->dmg_radius, MOD_BFG_LASER);

	// linear, clamped acceleration
	if(VectorLength(self->velocity) < 1000.0)
		VectorScale(self->velocity, 1.0 + (0.5 * gi.serverframe), self->velocity);

	self->nextthink = level.time + gi.serverframe;
}

void G_FireBFG(edict_t *self, vec3_t start, vec3_t dir, int speed, int damage,
		int knockback, float damage_radius){
	edict_t *bfg;
	vec3_t angles, right, up, r, u;
	int i;
	float s;
	vec3_t scale = {0.15, 0.15, 0.05};

	if(G_ImmediateWall(self, dir))
		VectorCopy(self->s.origin, start);

	// calculate up and right vectors
	VectorAngles(dir, angles);
	AngleVectors(angles, NULL, right, up);

	for(i = 0; i < 8; i++){

		bfg = G_Spawn();
		VectorCopy(start, bfg->s.origin);

		// start with the forward direction
		VectorCopy(dir, bfg->movedir);

		// and scatter randomly along the up and right vectors
		VectorScale(right, crand() * 0.35, r);
		VectorScale(up, crand() * 0.01, u);

		VectorAdd(bfg->movedir, r, bfg->movedir);
		VectorAdd(bfg->movedir, u, bfg->movedir);

		// finalize the direction and resolve angles, velocity, ..
		VectorAngles(bfg->movedir, bfg->s.angles);

		s = speed + (0.2 * speed * crand());
		VectorScale(bfg->movedir, s, bfg->velocity);

		bfg->movetype = MOVETYPE_FLYMISSILE;
		bfg->clipmask = MASK_SHOT;
		bfg->solid = SOLID_MISSILE;
		bfg->s.effects = EF_BFG;
		bfg->owner = self;
		bfg->touch = G_BFGTouch;
		bfg->think = G_BFGThink;
		bfg->nextthink = level.time + gi.serverframe;
		bfg->dmg = damage;
		bfg->knockback = knockback;
		bfg->dmg_radius = damage_radius;
		bfg->classname = "bfg blast";

		G_PlayerProjectile(bfg, scale);

		gi.LinkEntity(bfg);
	}
}
