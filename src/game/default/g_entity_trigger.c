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
static void G_trigger_multiple_Wait(g_edict_t *ent) {
	ent->locals.next_think = 0;
}

/*
 * @brief
 */
static void G_trigger_multiple_Think(g_edict_t *ent) {

	if (ent->locals.next_think)
		return; // already been triggered

	G_UseTargets(ent, ent->locals.activator);

	if (ent->locals.wait > 0) {
		ent->locals.Think = G_trigger_multiple_Wait;
		ent->locals.next_think = g_level.time + ent->locals.wait * 1000;
	} else { // we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->locals.Touch = NULL;
		ent->locals.next_think = g_level.time + gi.frame_millis;
		ent->locals.Think = G_FreeEdict;
	}
}

/*
 * @brief
 */
static void G_trigger_multiple_Use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {

	ent->locals.activator = activator;

	G_trigger_multiple_Think(ent);
}

/*
 * @brief
 */
static void G_trigger_multiple_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (!VectorCompare(self->locals.move_dir, vec3_origin)) {

		if (DotProduct(other->client->locals.forward, self->locals.move_dir) < 0.0)
			return;
	}

	self->locals.activator = other;
	G_trigger_multiple_Think(self);
}

/*
 * @brief
 */
static void G_trigger_multiple_Enable(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	self->solid = SOLID_TRIGGER;
	self->locals.Use = G_trigger_multiple_Use;
	gi.LinkEdict(self);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? TRIGGERED
 Triggers multiple targets at fixed intervals.
 -------- KEYS --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 wait : Interval in seconds between activations (default 0.2).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 TRIGGERED : If set, this trigger must be targeted before it will activate.
 */
void G_trigger_multiple(g_edict_t *ent) {

	ent->locals.noise_index = gi.SoundIndex("misc/chat");

	if (!ent->locals.wait)
		ent->locals.wait = 0.2;
	ent->locals.Touch = G_trigger_multiple_Touch;
	ent->locals.move_type = MOVE_TYPE_NONE;
	ent->sv_flags |= SVF_NO_CLIENT;

	if (ent->locals.spawn_flags & 1) {
		ent->solid = SOLID_NOT;
		ent->locals.Use = G_trigger_multiple_Enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->locals.Use = G_trigger_multiple_Use;
	}

	if (!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMoveDir(ent->s.angles, ent->locals.move_dir);

	gi.SetModel(ent, ent->model);
	gi.LinkEdict(ent);
}

/*QUAKED trigger_once (.5 .5 .5) ? TRIGGERED
 Triggers multiple targets once.
 -------- KEYS --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 TRIGGERED : If set, this trigger must be targeted before it will activate.
 */
void G_trigger_once(g_edict_t *ent) {
	ent->locals.wait = -1;
	G_trigger_multiple(ent);
}

/*
 * @brief
 */
static void G_trigger_relay_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	G_UseTargets(self, activator);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
 A trigger that can not be touched, but must be triggered by another entity.
 -------- KEYS --------
 delay : The delay in seconds before using targets (default 0).
 message : An optional message to display the first time this trigger fires.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity.
 */
void G_trigger_relay(g_edict_t *self) {
	self->locals.Use = G_trigger_relay_Use;
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
 Triggers targets once at level spawn.
 -------- KEYS --------
 delay : The delay in seconds before using targets (default 0.2).
 message : An optional message to display when this trigger fires.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
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
static void G_trigger_push_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
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

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE PUSH_EFFECTS
 Pushes the player in any direction. These are commonly used to make jump pads to send the player upwards. Using the angles key, you can project the player in any direction using "pitch yaw roll."
 -------- KEYS --------
 angles : The direction to push the player in "pitch yaw roll" notation (e.g. -80 270 0).
 speed : The speed with which to push the player (default 100).
 -------- SPAWNFLAGS --------
 PUSH_ONCE : If set, the pusher is freed after it is used once.
 PUSH_EFFECTS : If set, emit particle effects to indicate that a pusher is here.
 */
void G_trigger_push(g_edict_t *self) {
	g_edict_t *ent;

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_push_Touch;

	if (!self->locals.speed)
		self->locals.speed = 100;

	gi.LinkEdict(self);

	if (!(self->locals.spawn_flags & PUSH_EFFECT))
		return;

	// add a teleporter trail
	ent = G_Spawn();
	ent->class_name = "trigger_push_effects";
	ent->solid = SOLID_TRIGGER;
	ent->locals.move_type = MOVE_TYPE_NONE;

	// uber hack to resolve origin
	VectorAdd(self->mins, self->maxs, ent->s.origin);
	VectorScale(ent->s.origin, 0.5, ent->s.origin);
	ent->s.effects = EF_TELEPORTER;

	gi.LinkEdict(ent);
}

/*
 * @brief
 */
static void G_trigger_hurt_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (self->solid == SOLID_NOT)
		self->solid = SOLID_TRIGGER;
	else
		self->solid = SOLID_NOT;
	gi.LinkEdict(self);

	if (!(self->locals.spawn_flags & 2))
		self->locals.Use = NULL;
}

/*
 * @brief
 */
static void G_trigger_hurt_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
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

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg,
			self->locals.dmg, dflags, MOD_TRIGGER_HURT);
}

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE - NO_PROTECTION SLOW
 Any player that touches this will be hurt by "dmg" points of damage every 100ms (very fast).
 -------- KEYS --------
 dmg : The damage done every 100ms to any player who touches this entity (default 2).
 targetname : The target name of this entity, if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_OFF : If set, this entity must be activated before it will hurt players.
 TOGGLE : If set, this entity is toggled each time it is activated.
 -
 NO_PROTECTION : If set, armor will not be used to absorb damage inflicted by this entity.
 SLOW : Decreases the damage rate to once per second.
 */
void G_trigger_hurt(g_edict_t *self) {

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_hurt_Touch;

	if (!self->locals.dmg)
		self->locals.dmg = 2;

	if (self->locals.spawn_flags & 1)
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if (self->locals.spawn_flags & 2)
		self->locals.Use = G_trigger_hurt_Use;

	gi.LinkEdict(self);
}

/*
 * @brief
 */
static void G_trigger_exec_Touch(g_edict_t *self, g_edict_t *other __attribute__((unused)), c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (self->locals.timestamp > g_level.time)
		return;

	self->locals.timestamp = g_level.time + self->locals.delay * 1000;

	if (self->locals.command)
		gi.AddCommandString(va("%s\n", self->locals.command));

	else if (self->locals.script)
		gi.AddCommandString(va("exec %s\n", self->locals.script));
}

/*QUAKED trigger_exec (1 0 1)
 Executes a console command or script file when activated.
 -------- KEYS --------
 command : The console command(s) to execute.
 script : The script file (.cfg) to execute.
 delay : The delay in seconds between activation and execution of the commands.
 */
void G_trigger_exec(g_edict_t *self) {

	if (!self->locals.command && !self->locals.script) {
		gi.Debug("No command or script at %s", vtos(self->abs_mins));
		G_FreeEdict(self);
		return;
	}

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_exec_Touch;

	gi.LinkEdict(self);
}
