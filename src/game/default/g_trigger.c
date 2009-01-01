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

static void InitTrigger(edict_t *self){

	if(!VectorCompare(self->s.angles, vec3_origin))
		G_SetMovedir(self->s.angles, self->movedir);

	self->solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_NONE;
	gi.SetModel(self, self->model);
	self->svflags = SVF_NOCLIENT;
}


// the wait time has passed, so set back up for another activation
static void multi_wait(edict_t *ent){
	ent->nextthink = 0;
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
static void multi_trigger(edict_t *ent){
	if(ent->nextthink)
		return;  // already been triggered

	G_UseTargets(ent, ent->activator);

	if(ent->wait > 0){
		ent->think = multi_wait;
		ent->nextthink = level.time + ent->wait;
	} else {  // we can't just remove(self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->nextthink = level.time + gi.serverframe;
		ent->think = G_FreeEdict;
	}
}

static void Use_Multi(edict_t *ent, edict_t *other, edict_t *activator){
	ent->activator = activator;
	multi_trigger(ent);
}

static void Touch_Multi(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(!other->client)
		return;

	if(self->spawnflags & 2)
		return;

	if(!VectorCompare(self->movedir, vec3_origin)){
		vec3_t forward;

		AngleVectors(other->s.angles, forward, NULL, NULL);
		if(DotProduct(forward, self->movedir) < 0)
			return;
	}

	self->activator = other;
	multi_trigger(self);
}

/*QUAKED trigger_multiple(.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings.(.2 default)
set "message" to text string
*/
static void trigger_enable(edict_t *self, edict_t *other, edict_t *activator){
	self->solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	gi.LinkEntity(self);
}

void G_trigger_multiple(edict_t *ent){

	ent->noise_index = gi.SoundIndex("misc/chat.wav");

	if(!ent->wait)
		ent->wait = 0.2;
	ent->touch = Touch_Multi;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;


	if(ent->spawnflags & 4){
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	if(!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMovedir(ent->s.angles, ent->movedir);

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}


/*QUAKED trigger_once(.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

"message"	string to be displayed when triggered
*/

void G_trigger_once(edict_t *ent){
	ent->wait = -1;
	G_trigger_multiple(ent);
}

/*QUAKED trigger_relay(.5 .5 .5)(-8 -8 -8)(8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
static void trigger_relay_use(edict_t *self, edict_t *other, edict_t *activator){
	G_UseTargets(self, activator);
}

void G_trigger_relay(edict_t *self){
	self->use = trigger_relay_use;
}


/*QUAKED trigger_always(.5 .5 .5)(-8 -8 -8)(8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void G_trigger_always(edict_t *ent){
	// we must have some delay to make sure our use targets are present
	if(ent->delay < 0.2)
		ent->delay = 0.2;

	G_UseTargets(ent, ent);
}


#define PUSH_ONCE 1
#define PUSH_EFFECT 2

static void trigger_push_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){

	if(strcmp(other->classname, "grenade") == 0 || other->health > 0){

		VectorScale(self->movedir, self->speed * 10, other->velocity);

		if(other->client){  // don't take falling damage immediately from this
			VectorCopy(other->velocity, other->client->oldvelocity);
			if(other->push_time < level.time){
				other->push_time = level.time + 1.5;
				gi.Sound(other, CHAN_AUTO, gi.SoundIndex("world/jumppad.wav"), 1, ATTN_NORM, 0);
			}
		}
	}

	if(self->spawnflags & PUSH_ONCE)
		G_FreeEdict(self);
}


/*QUAKED trigger_push(.5 .5 .5) ? PUSH_ONCE
Pushes the player (jump pads)
"speed"		defaults to 100
*/
void G_trigger_push(edict_t *self){
	edict_t *ent;

	InitTrigger(self);

	self->touch = trigger_push_touch;

	if(!self->speed)
		self->speed = 100;

	gi.LinkEntity(self);

	if(!(self->spawnflags & PUSH_EFFECT))
		return;

	// add a teleporter trail
	ent = G_Spawn();
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_NONE;

	// uber hack to resolve origin
	VectorAdd(self->mins, self->maxs, ent->s.origin);
	VectorScale(ent->s.origin, 0.5, ent->s.origin);
	ent->s.effects = EF_TELEPORTER;

	gi.LinkEntity(ent);
}


/*QUAKED trigger_hurt(.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)

*/
static void hurt_use(edict_t *self, edict_t *other, edict_t *activator){

	if(self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.LinkEntity(self);

	if(!(self->spawnflags & 2))
		self->use = NULL;
}


static void hurt_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	int dflags;

	if(!other->takedamage)
		return;

	if(self->timestamp > level.time)
		return;

	if(self->spawnflags & 16)
		self->timestamp = level.time + 1;
	else
		self->timestamp = level.time + 0.1;

	if(self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin,
			self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT);
}

void G_trigger_hurt(edict_t *self){

	InitTrigger(self);

	self->touch = hurt_touch;

	if(!self->dmg)
		self->dmg = 5;

	if(self->spawnflags & 1)
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if(self->spawnflags & 2)
		self->use = hurt_use;

	gi.LinkEntity(self);
}

