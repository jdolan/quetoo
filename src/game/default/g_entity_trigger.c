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
 * @brief
 */
static void G_Trigger_Init(g_edict_t *self) {

	if (!VectorCompare(self->s.angles, vec3_origin))
		G_SetMoveDir(self->s.angles, self->locals.move_dir);

	self->solid = SOLID_TRIGGER;
	self->locals.move_type = MOVE_TYPE_NONE;
	gi.SetModel(self, self->model);
	self->sv_flags = SVF_NO_CLIENT;
}

/*
 * @brief The wait time has passed, so set back up for another activation
 */
static void G_trigger_multiple_wait(g_edict_t *ent) {
	ent->locals.next_think = 0;
}

/*
 * @brief
 */
static void G_trigger_multiple_think(g_edict_t *ent) {

	if (ent->locals.next_think)
		return; // already been triggered

	G_UseTargets(ent, ent->locals.activator);

	if (ent->locals.wait > 0) {
		ent->locals.think = G_trigger_multiple_wait;
		ent->locals.next_think = g_level.time + ent->locals.wait * 1000;
	} else { // we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->locals.touch = NULL;
		ent->locals.next_think = g_level.time + gi.frame_millis;
		ent->locals.think = G_FreeEdict;
	}
}

/*
 * @brief
 */
static void G_trigger_multiple_use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {

	ent->locals.activator = activator;

	G_trigger_multiple_think(ent);
}

/*
 * @brief
 */
static void G_trigger_multiple_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (self->locals.spawn_flags & 2)
		return;

	if (!VectorCompare(self->locals.move_dir, vec3_origin)) {

		if (DotProduct(other->client->locals.forward, self->locals.move_dir) < 0.0)
			return;
	}

	self->locals.activator = other;
	G_trigger_multiple_think(self);
}

/*
 * @brief
 */
static void G_trigger_multiple_enable(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	self->solid = SOLID_TRIGGER;
	self->locals.use = G_trigger_multiple_use;
	gi.LinkEntity(self);
}

/*QUAKED trigger_multiple(.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
 Variable sized repeatable trigger. Must be targeted at one or more entities.
 If "delay" is set, the trigger waits some time after activating before firing.
 "wait" : Seconds between triggerings.(.2 default)
 set "message" to text string
 */
void G_trigger_multiple(g_edict_t *ent) {

	ent->locals.noise_index = gi.SoundIndex("misc/chat");

	if (!ent->locals.wait)
		ent->locals.wait = 0.2;
	ent->locals.touch = G_trigger_multiple_touch;
	ent->locals.move_type = MOVE_TYPE_NONE;
	ent->sv_flags |= SVF_NO_CLIENT;

	if (ent->locals.spawn_flags & 4) {
		ent->solid = SOLID_NOT;
		ent->locals.use = G_trigger_multiple_enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->locals.use = G_trigger_multiple_use;
	}

	if (!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMoveDir(ent->s.angles, ent->locals.move_dir);

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
	ent->locals.wait = -1;
	G_trigger_multiple(ent);
}

/*
 * @brief
 */
static void G_trigger_relay_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	G_UseTargets(self, activator);
}

/*QUAKED trigger_relay(.5 .5 .5)(-8 -8 -8)(8 8 8)
 This fixed size trigger cannot be touched, it can only be fired by other events.
 */
void G_trigger_relay(g_edict_t *self) {
	self->locals.use = G_trigger_relay_use;
}

/*QUAKED trigger_always(.5 .5 .5)(-8 -8 -8)(8 8 8)
 This trigger will always fire. It is activated by the world.
 */
void G_trigger_always(g_edict_t *ent) {
	// we must have some delay to make sure our use targets are present
	if (ent->locals.delay < 0.2)
		ent->locals.delay = 0.2;

	G_UseTargets(ent, ent);
}

#define PUSH_ONCE 1
#define PUSH_EFFECT 2

/*
 * @brief
 */
static void G_trigger_push_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (other->locals.health > 0) {

		VectorScale(self->locals.move_dir, self->locals.speed * 10.0, other->locals.velocity);

		if (other->client) { // don't take falling damage immediately from this
			other->client->ps.pm_state.pm_flags |= PMF_PUSHED;
		}

		if (other->locals.push_time < g_level.time) {
			other->locals.push_time = g_level.time + 1500;
			gi.Sound(other, gi.SoundIndex("world/jumppad"), ATTN_NORM);
		}
	}

	if (self->locals.spawn_flags & PUSH_ONCE)
		G_FreeEdict(self);
}

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE PUSH_EFFECT
 Pushes the player (jump pads)
 "speed"		defaults to 100
 */
void G_trigger_push(g_edict_t *self) {
	g_edict_t *ent;

	G_Trigger_Init(self);

	self->locals.touch = G_trigger_push_touch;

	if (!self->locals.speed)
		self->locals.speed = 100;

	gi.LinkEntity(self);

	if (!(self->locals.spawn_flags & PUSH_EFFECT))
		return;

	// add a teleporter trail
	ent = G_Spawn();
	ent->solid = SOLID_TRIGGER;
	ent->locals.move_type = MOVE_TYPE_NONE;

	// uber hack to resolve origin
	VectorAdd(self->mins, self->maxs, ent->s.origin);
	VectorScale(ent->s.origin, 0.5, ent->s.origin);
	ent->s.effects = EF_TELEPORTER;

	gi.LinkEntity(ent);
}

/*
 * @brief
 */
static void G_trigger_hurt_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.LinkEntity(self);

	if (!(self->locals.spawn_flags & 2))
		self->locals.use = NULL;
}

/*
 * @brief
 */
static void G_trigger_hurt_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {
	int32_t dflags;

	if (!other->locals.take_damage) { // deal with items that land on us

		if (other->locals.item) {
			if (other->locals.item->type == ITEM_FLAG)
				G_ResetFlag(other);
			else
				G_FreeEdict(other);
		}

		gi.Debug("%s\n", other->class_name);
		return;
	}

	if (self->locals.timestamp > g_level.time)
		return;

	if (self->locals.spawn_flags & 16)
		self->locals.timestamp = g_level.time + 1000;
	else
		self->locals.timestamp = g_level.time + 100;

	if (self->locals.spawn_flags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = DAMAGE_NO_ARMOR;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, self->locals.dmg,
			dflags, MOD_TRIGGER_HURT);
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

	self->locals.touch = G_trigger_hurt_touch;

	if (!self->locals.dmg)
		self->locals.dmg = 5;

	if (self->locals.spawn_flags & 1)
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if (self->locals.spawn_flags & 2)
		self->locals.use = G_trigger_hurt_use;

	gi.LinkEntity(self);
}

/*
 * @brief
 */
static void G_trigger_exec_touch(g_edict_t *self, g_edict_t *other __attribute__((unused)), c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (self->locals.timestamp > g_level.time)
		return;

	self->locals.timestamp = g_level.time + self->locals.delay * 1000;

	if (self->locals.command)
		gi.AddCommandString(va("%s\n", self->locals.command));

	else if (self->locals.script)
		gi.AddCommandString(va("exec %s\n", self->locals.script));
}

/*
 * @brief A trigger which executes a command or script when touched.
 */
void G_trigger_exec(g_edict_t *self) {

	if (!self->locals.command && !self->locals.script) {
		gi.Debug("No command or script at %s", vtos(self->abs_mins));
		G_FreeEdict(self);
		return;
	}

	G_Trigger_Init(self);

	self->locals.touch = G_trigger_exec_touch;

	gi.LinkEntity(self);
}
