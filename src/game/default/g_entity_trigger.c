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
#include "bg_pmove.h"

#define TRIGGERED 0x1
#define SHOOTABLE 0x2

/**
 * @brief
 */
static void G_Trigger_Init(g_entity_t *self) {

	if (!Vec3_Equal(self->s.angles, Vec3_Zero())) {
		G_SetMoveDir(self);
	}

	self->solid = SOLID_TRIGGER;
	self->locals.move_type = MOVE_TYPE_NONE;
	gi.SetModel(self, self->model);
	self->sv_flags = SVF_NO_CLIENT;
}

/**
 * @brief The wait time has passed, so set back up for another activation
 */
static void G_trigger_multiple_Wait(g_entity_t *ent) {
	ent->locals.next_think = 0;
}

/**
 * @brief
 */
static void G_trigger_multiple_Think(g_entity_t *ent) {

	if (ent->locals.next_think) {
		return; // already been triggered
	}

	G_UseTargets(ent, ent->locals.activator);

	if (ent->locals.wait > 0) {
		ent->locals.Think = G_trigger_multiple_Wait;
		ent->locals.next_think = g_level.time + ent->locals.wait * 1000;
	} else {
		ent->locals.Touch = NULL;
		ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
		ent->locals.Think = G_FreeEntity;
	}
}

/**
 * @brief
 */
static void G_trigger_multiple_Use(g_entity_t *ent, g_entity_t *other,
                                   g_entity_t *activator) {

	ent->locals.activator = activator;

	G_trigger_multiple_Think(ent);
}

/**
 * @brief
 */
static void G_trigger_multiple_Touch(g_entity_t *self, g_entity_t *other,
                                     const cm_bsp_plane_t *plane,
                                     const cm_bsp_texinfo_t *texinfo) {

	if (!other->client) {
		const _Bool isProjectile = other->owner && other->owner->client;
		if (isProjectile && (self->locals.spawn_flags & SHOOTABLE)) {
			// we're a shootable trigger, and we've been shot
		} else {
			return;
		}
	}

	if (!Vec3_Equal(self->locals.move_dir, Vec3_Zero())) {
		vec3_t forward;

		Vec3_Vectors(other->s.angles, &forward, NULL, NULL);

		if (Vec3_Dot(forward, self->locals.move_dir) < 0.0) {
			return;
		}
	}

	self->locals.activator = other;
	G_trigger_multiple_Think(self);
}

/**
 * @brief
 */
static void G_trigger_multiple_Enable(g_entity_t *self, g_entity_t *other,
                                      g_entity_t *activator) {
	self->solid = SOLID_TRIGGER;
	self->locals.Use = G_trigger_multiple_Use;
	gi.LinkEntity(self);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? triggered shootable
 Triggers multiple targets at fixed intervals.

 -------- Keys --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 wait : Interval in seconds between activations (default 0.2).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : If set, this trigger must be targeted before it will activate.
 shootable : If set, this trigger will fire when projectiles touch it.
 */
void G_trigger_multiple(g_entity_t *ent) {

	ent->locals.noise_index = gi.SoundIndex("misc/chat");

	if (!ent->locals.wait) {
		ent->locals.wait = 0.2;
	}

	ent->locals.Touch = G_trigger_multiple_Touch;
	ent->locals.move_type = MOVE_TYPE_NONE;
	ent->sv_flags |= SVF_NO_CLIENT;

	if (ent->locals.spawn_flags & TRIGGERED) {
		ent->solid = SOLID_NOT;
		ent->locals.Use = G_trigger_multiple_Enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->locals.Use = G_trigger_multiple_Use;
	}

	if (!Vec3_Equal(ent->s.angles, Vec3_Zero())) {
		G_SetMoveDir(ent);
	}

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/*QUAKED trigger_once (.5 .5 .5) ? triggered
 Triggers multiple targets once.

 -------- Keys --------
 delay : Delay in seconds between activation and firing of targets (default 0).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : If set, this trigger must be targeted before it will activate.
 */
void G_trigger_once(g_entity_t *ent) {
	ent->locals.wait = -1;
	G_trigger_multiple(ent);
}

/**
 * @brief
 */
static void G_trigger_relay_Use(g_entity_t *self, g_entity_t *other,
                                g_entity_t *activator) {
	G_UseTargets(self, activator);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
 A trigger that can not be touched, but must be triggered by another entity.

 -------- Keys --------
 delay : The delay in seconds between activation and firing of targets (default 0).
 message : An optional string to display when activated.
 target : The name of the entity or team to use on activation.
 killtarget : The name of the entity or team to kill on activation.
 targetname : The target name of this entity.
 */
void G_trigger_relay(g_entity_t *self) {
	self->locals.Use = G_trigger_relay_Use;
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
 Triggers targets once at level spawn.

 -------- Keys --------
 delay : The delay in seconds between activation and firing of targets (default 0.2).
 message : An optional message to display when this trigger fires.
 target : The name of the entity or team to use on activation.
 kill_target : The name of the entity or team to kill on activation.
 */
void G_trigger_always(g_entity_t *ent) {

	// we must have some delay to make sure our use targets are present
	if (ent->locals.delay < 0.2) {
		ent->locals.delay = 0.2;
	}

	G_UseTargets(ent, ent);
}

#define PUSH_ONCE 1
#define PUSH_EFFECT 2

/**
 * @brief
 */
static void G_trigger_push_Touch(g_entity_t *self, g_entity_t *other,
                                 const cm_bsp_plane_t *plane,
                                 const cm_bsp_texinfo_t *texinfo) {

	if (other->locals.move_type == MOVE_TYPE_WALK || other->locals.move_type == MOVE_TYPE_BOUNCE) {

		other->locals.velocity = Vec3_Scale(self->locals.move_dir, self->locals.speed * 10.0);

		if (other->client) {
			other->client->ps.pm_state.flags |= PMF_TIME_PUSHED;
			other->client->ps.pm_state.time = 240;
		}

		if (other->locals.push_time < g_level.time) {
			other->locals.push_time = g_level.time + 1500;
			gi.Sound(other, self->locals.move_info.sound_start, ATTEN_NORM, 0);
		}
	}

	if (self->locals.spawn_flags & PUSH_ONCE) {
		G_FreeEntity(self);
	}
}

/**
 * @brief Creates an effect trail for the specified entity.
 */
static void G_trigger_push_Effect(g_entity_t *self) {

	g_entity_t *ent = G_AllocEntity();

	ent->s.origin = Vec3_Add(self->mins, self->maxs);
	ent->s.origin = Vec3_Scale(ent->s.origin, 0.5);

	ent->locals.move_type = MOVE_TYPE_NONE;
	ent->s.trail = TRAIL_TELEPORTER;

	gi.LinkEntity(ent);
}

/*QUAKED trigger_push (.5 .5 .5) ? push_once push_effects
 Pushes the player in any direction. These are commonly used to make jump pads to send the player upwards. Using the angles key, you can project the player in any direction using "pitch yaw roll."

 -------- Keys --------
 angles : The direction to push the player in "pitch yaw roll" notation (e.g. -80 270 0).
 noise : The sound effect to play when the player is pushed (default "world/jumppad").
 speed : The speed with which to push the player (default 100).

 -------- Spawn flags --------
 push_once : If set, the pusher is freed after it is used once.
 push_effects : If set, emit particle effects to indicate that a pusher is here.
 */
void G_trigger_push(g_entity_t *self) {

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_push_Touch;

	const char *noise = g_game.spawn.noise ?: "world/jumppad";
	self->locals.move_info.sound_start = gi.SoundIndex(noise);

	if (!self->locals.speed) {
		self->locals.speed = 100;
	}

	gi.LinkEntity(self);

	if (self->locals.spawn_flags & PUSH_EFFECT) {
		G_trigger_push_Effect(self);
	}
}

/**
 * @brief
 */
static void G_trigger_hurt_Use(g_entity_t *self, g_entity_t *other,
                               g_entity_t *activator) {

	if (self->solid == SOLID_NOT) {
		self->solid = SOLID_TRIGGER;
	} else {
		self->solid = SOLID_NOT;
	}
	gi.LinkEntity(self);

	if (!(self->locals.spawn_flags & 2)) {
		self->locals.Use = NULL;
	}
}

/**
 * @brief
 */
static void G_trigger_hurt_Touch(g_entity_t *self, g_entity_t *other,
                                 const cm_bsp_plane_t *plane,
                                 const cm_bsp_texinfo_t *texinfo) {

	if (!other->locals.take_damage) { // deal with items that land on us

		if (other->locals.item) {
			if (other->locals.item->type == ITEM_FLAG) {
				G_ResetDroppedFlag(other);
			} else if (other->locals.item->type == ITEM_TECH) {
				G_ResetDroppedTech(other);
			} else {
				G_FreeEntity(other);
			}
		}

		G_Debug("%s\n", etos(other));
		return;
	}

	if (self->locals.timestamp > g_level.time) {
		return;
	}

	if (self->locals.spawn_flags & 16) {
		self->locals.timestamp = g_level.time + 1000;
	} else {
		self->locals.timestamp = g_level.time + 100;
	}

	const int16_t d = self->locals.damage;

	int32_t dflags = DMG_NO_ARMOR;

	if (self->locals.spawn_flags & 8) {
		dflags = DMG_NO_GOD;
	}

	G_Damage(other, self, NULL, Vec3_Zero(), Vec3_Zero(), Vec3_Zero(), d, d, dflags, MOD_TRIGGER_HURT);
}

/*QUAKED trigger_hurt (.5 .5 .5) ? start_off toggle ? no_protection slow
 Any player that touches this will be hurt by "dmg" points of damage every 100ms (very fast).

 -------- Keys --------
 dmg : The damage done every 100ms to any player who touches this entity (default 2).
 targetname : The target name of this entity, if it is to be triggered.

 -------- Spawn flags --------
 start_off : If set, this entity must be activated before it will hurt players.
 toggle : If set, this entity is toggled each time it is activated.
 ?
 no_protection : If set, armor will not be used to absorb damage inflicted by this entity.
 slow : Decreases the damage rate to once per second.
 */
void G_trigger_hurt(g_entity_t *self) {

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_hurt_Touch;

	if (!self->locals.damage) {
		self->locals.damage = 2;
	}

	if (self->locals.spawn_flags & 1) {
		self->solid = SOLID_NOT;
	} else {
		self->solid = SOLID_TRIGGER;
	}

	if (self->locals.spawn_flags & 2) {
		self->locals.Use = G_trigger_hurt_Use;
	}

	gi.LinkEntity(self);
}

/**
 * @brief
 */
static void G_trigger_exec_Touch(g_entity_t *self, g_entity_t *other,
                                 const cm_bsp_plane_t *plane,
                                 const cm_bsp_texinfo_t *texinfo) {

	if (self->locals.timestamp > g_level.time) {
		return;
	}

	self->locals.timestamp = g_level.time + self->locals.delay * 1000;

	if (self->locals.command) {
		gi.Cbuf(va("%s\n", self->locals.command));
	}

	else if (self->locals.script) {
		gi.Cbuf(va("exec %s\n", self->locals.script));
	}
}

/*QUAKED trigger_exec (0 1 0) ?
 Executes a console command or script file when activated.

 -------- Keys --------
 command : The console command(s) to execute.
 script : The script file (.cfg) to execute.
 delay : The delay in seconds between activation and execution of the commands.
 */
void G_trigger_exec(g_entity_t *self) {

	if (!self->locals.command && !self->locals.script) {
		G_Debug("No command or script at %s", vtos(self->s.origin));
		G_FreeEntity(self);
		return;
	}

	G_Trigger_Init(self);

	self->locals.Touch = G_trigger_exec_Touch;

	gi.LinkEntity(self);
}
