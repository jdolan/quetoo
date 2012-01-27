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
 * G_Trigger_Init
 */
static void G_Trigger_Init(g_edict_t *self) {

	if (!VectorCompare(self->s.angles, vec3_origin))
		G_SetMoveDir(self->s.angles, self->move_dir);

	self->solid = SOLID_TRIGGER;
	self->move_type = MOVE_TYPE_NONE;
	gi.SetModel(self, self->model);
	self->sv_flags = SVF_NO_CLIENT;
}

/*
 * G_trigger_multiple_wait
 *
 * The wait time has passed, so set back up for another activation
 */
static void G_trigger_multiple_wait(g_edict_t *ent) {
	ent->next_think = 0;
}

/*
 * G_trigger_multiple_think
 */
static void G_trigger_multiple_think(g_edict_t *ent) {

	if (ent->next_think)
		return; // already been triggered

	G_UseTargets(ent, ent->activator);

	if (ent->wait > 0) {
		ent->think = G_trigger_multiple_wait;
		ent->next_think = g_level.time + ent->wait;
	} else { // we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->next_think = g_level.time + gi.server_frame;
		ent->think = G_FreeEdict;
	}
}

/*
 * G_trigger_multiple_use
 */
static void G_trigger_multiple_use(g_edict_t *ent, g_edict_t *other,
		g_edict_t *activator) {

	ent->activator = activator;

	G_trigger_multiple_think(ent);
}

/*
 * G_trigger_multiple_touch
 */
static void G_trigger_multiple_touch(g_edict_t *self, g_edict_t *other,
		c_bsp_plane_t *plane, c_bsp_surface_t *surf) {

	if (!other->client)
		return;

	if (self->spawn_flags & 2)
		return;

	if (!VectorCompare(self->move_dir, vec3_origin)) {
		vec3_t forward;

		AngleVectors(other->s.angles, forward, NULL, NULL);
		if (DotProduct(forward, self->move_dir) < 0)
			return;
	}

	self->activator = other;
	G_trigger_multiple_think(self);
}

/*
 * G_trigger_multiple_enable
 */
static void G_trigger_multiple_enable(g_edict_t *self, g_edict_t *other,
		g_edict_t *activator) {
	self->solid = SOLID_TRIGGER;
	self->use = G_trigger_multiple_use;
	gi.LinkEntity(self);
}

/*QUAKED trigger_multiple(.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
 Variable sized repeatable trigger.  Must be targeted at one or more entities.
 If "delay" is set, the trigger waits some time after activating before firing.
 "wait" : Seconds between triggerings.(.2 default)
 set "message" to text string
 */
void G_trigger_multiple(g_edict_t *ent) {

	ent->noise_index = gi.SoundIndex("misc/chat");

	if (!ent->wait)
		ent->wait = 0.2;
	ent->touch = G_trigger_multiple_touch;
	ent->move_type = MOVE_TYPE_NONE;
	ent->sv_flags |= SVF_NO_CLIENT;

	if (ent->spawn_flags & 4) {
		ent->solid = SOLID_NOT;
		ent->use = G_trigger_multiple_enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->use = G_trigger_multiple_use;
	}

	if (!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMoveDir(ent->s.angles, ent->move_dir);

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/*QUAKED trigger_once(.5 .5 .5) ? x x TRIGGERED
 Triggers once, then removes itself.
 You must set the key "target" to the name of another object in the level that has a matching "targetname".

 If TRIGGERED, this trigger must be triggered before it is live.

 "message"	string to be displayed when triggered
 */
void G_trigger_once(g_edict_t *ent) {
	ent->wait = -1;
	G_trigger_multiple(ent);
}

/*
 * G_trigger_relay_use
 */
static void G_trigger_relay_use(g_edict_t *self, g_edict_t *other,
		g_edict_t *activator) {
	G_UseTargets(self, activator);
}

/*QUAKED trigger_relay(.5 .5 .5)(-8 -8 -8)(8 8 8)
 This fixed size trigger cannot be touched, it can only be fired by other events.
 */
void G_trigger_relay(g_edict_t *self) {
	self->use = G_trigger_relay_use;
}

/*QUAKED trigger_always(.5 .5 .5)(-8 -8 -8)(8 8 8)
 This trigger will always fire.  It is activated by the world.
 */
void G_trigger_always(g_edict_t *ent) {
	// we must have some delay to make sure our use targets are present
	if (ent->delay < 0.2)
		ent->delay = 0.2;

	G_UseTargets(ent, ent);
}

#define PUSH_ONCE 1
#define PUSH_EFFECT 2

/*
 * G_trigger_push_touch
 */
static void G_trigger_push_touch(g_edict_t *self, g_edict_t *other,
		c_bsp_plane_t *plane, c_bsp_surface_t *surf) {

	if (!strcmp(other->class_name, "grenade") || other->health > 0) {

		VectorScale(self->move_dir, self->speed * 10.0, other->velocity);

		if (other->client) { // don't take falling damage immediately from this

			VectorCopy(other->velocity, other->client->old_velocity);
			other->client->ps.pmove.pm_flags |= (PMF_PUSHED | PMF_TIME_LAND);
			other->client->ps.pmove.pm_time = 10;

			if (other->push_time < g_level.time) {
				other->push_time = g_level.time + 1.5;
				gi.Sound(other, gi.SoundIndex("world/jumppad"), ATTN_NORM);
			}
		}
	}

	if (self->spawn_flags & PUSH_ONCE)
		G_FreeEdict(self);
}

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE PUSH_EFFECT
 Pushes the player (jump pads)
 "speed"		defaults to 100
 */
void G_trigger_push(g_edict_t *self) {
	g_edict_t *ent;

	G_Trigger_Init(self);

	self->touch = G_trigger_push_touch;

	if (!self->speed)
		self->speed = 100;

	gi.LinkEntity(self);

	if (!(self->spawn_flags & PUSH_EFFECT))
		return;

	// add a teleporter trail
	ent = G_Spawn();
	ent->solid = SOLID_TRIGGER;
	ent->move_type = MOVE_TYPE_NONE;

	// uber hack to resolve origin
	VectorAdd(self->mins, self->maxs, ent->s.origin);
	VectorScale(ent->s.origin, 0.5, ent->s.origin);
	ent->s.effects = EF_TELEPORTER;

	gi.LinkEntity(ent);
}

/*
 * G_trigger_hurt_use
 */
static void G_trigger_hurt_use(g_edict_t *self, g_edict_t *other,
		g_edict_t *activator) {

	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.LinkEntity(self);

	if (!(self->spawn_flags & 2))
		self->use = NULL;
}

/*
 * G_trigger_hurt_touch
 */
static void G_trigger_hurt_touch(g_edict_t *self, g_edict_t *other,
		c_bsp_plane_t *plane, c_bsp_surface_t *surf) {
	int dflags;

	if (!other->take_damage) { // deal with items that land on us

		if (other->item) {
			if (other->item->type == ITEM_FLAG)
				G_ResetFlag(other);
			else
				G_FreeEdict(other);
		}

		gi.Debug("hurt_touch: %s\n", other->class_name);
		return;
	}

	if (self->timestamp > g_level.time)
		return;

	if (self->spawn_flags & 16)
		self->timestamp = g_level.time + 1;
	else
		self->timestamp = g_level.time + 0.1;

	if (self->spawn_flags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = DAMAGE_NO_ARMOR;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin,
			self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT);
}

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
 Any entity that touches this will be hurt.

 It does dmg points of damage evert 100ms.

 SILENT			supresses playing the sound
 SLOW			changes the damage rate to once per second
 NO_PROTECTION	*nothing* stops the damage

 "dmg"			default 5 (whole numbers only)
 */
void G_trigger_hurt(g_edict_t *self) {

	G_Trigger_Init(self);

	self->touch = G_trigger_hurt_touch;

	if (!self->dmg)
		self->dmg = 5;

	if (self->spawn_flags & 1)
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if (self->spawn_flags & 2)
		self->use = G_trigger_hurt_use;

	gi.LinkEntity(self);
}

/*
 * G_trigger_exec_touch
 */
static void G_trigger_exec_touch(g_edict_t *self, g_edict_t *other,
		c_bsp_plane_t *plane, c_bsp_surface_t *surf) {

	if (self->timestamp > g_level.time)
		return;

	self->timestamp = g_level.time + self->delay;

	if (self->command)
		gi.AddCommandString(va("%s\n", self->command));

	else if (self->script)
		gi.AddCommandString(va("exec %s\n", self->script));
}

/*
 * A trigger which executes a command or script when touched.
 */
void G_trigger_exec(g_edict_t *self) {

	if (!self->command && !self->script) {
		gi.Debug("%s does not have a command or script", self->class_name);
		G_FreeEdict(self);
		return;
	}

	G_Trigger_Init(self);

	self->touch = G_trigger_exec_touch;

	gi.LinkEntity(self);
}
