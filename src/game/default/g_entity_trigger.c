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
 * @brief Initializes a trigger entity, setting its move direction, solid type, and model.
 */
static void G_Trigger_Init(g_entity_t *ent) {

  if (!Vec3_Equal(ent->s.angles, Vec3_Zero())) {
    G_SetMoveDir(ent);
  }

  ent->solid = SOLID_TRIGGER;
  ent->move_type = MOVE_TYPE_NONE;
  gi.SetModel(ent, ent->model);
  ent->sv_flags = SVF_NO_CLIENT;
}

/**
 * @brief The wait time has passed, so set back up for another activation
 */
static void G_trigger_multiple_Wait(g_entity_t *ent) {
  ent->next_think = 0;
}

/**
 * @brief Called after the wait period expires, re-enabling the trigger for another activation.
 */
static void G_trigger_multiple_Think(g_entity_t *ent) {

  if (ent->next_think) {
    return; // already been triggered
  }

  G_UseTargets(ent, ent->activator);

  if (ent->wait > 0) {
    ent->Think = G_trigger_multiple_Wait;
    ent->next_think = g_level.time + ent->wait * 1000;
  } else {
    ent->Touch = NULL;
    ent->next_think = g_level.time + QUETOO_TICK_MILLIS;
    ent->Think = G_FreeEntity;
  }
}

/**
 * @brief Fires the trigger's targets after the optional delay, then resets or removes the trigger.
 */
static void G_trigger_multiple_Use(g_entity_t *ent, g_entity_t *other,
                                   g_entity_t *activator) {

  ent->activator = activator;

  G_trigger_multiple_Think(ent);
}

/**
 * @brief Handles use activation of a trigger_multiple, delegating to the think function.
 */
static void G_trigger_multiple_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->client) {
    const bool isProjectile = other->owner && other->owner->client;
    if (isProjectile && (ent->spawn_flags & SHOOTABLE)) {
      // we're a shootable trigger, and we've been shot
    } else {
      return;
    }
  }

  if (!Vec3_Equal(ent->move_dir, Vec3_Zero())) {
    vec3_t forward;

    Vec3_Vectors(other->s.angles, &forward, NULL, NULL);

    if (Vec3_Dot(forward, ent->move_dir) < 0.0) {
      return;
    }
  }

  ent->activator = other;
  G_trigger_multiple_Think(ent);
}

/**
 * @brief Handles touch events on a trigger_multiple, activating it when a qualifying entity enters.
 */
static void G_trigger_multiple_Enable(g_entity_t *ent, g_entity_t *other,
                                      g_entity_t *activator) {
  ent->solid = SOLID_TRIGGER;
  ent->Use = G_trigger_multiple_Use;
  gi.LinkEntity(ent);
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

  ent->sound = gi.SoundIndex("misc/chat");

  if (!ent->wait) {
    ent->wait = 0.2;
  }

  ent->Touch = G_trigger_multiple_Touch;
  ent->move_type = MOVE_TYPE_NONE;
  ent->sv_flags |= SVF_NO_CLIENT;

  if (ent->spawn_flags & TRIGGERED) {
    ent->solid = SOLID_NOT;
    ent->Use = G_trigger_multiple_Enable;
  } else {
    ent->solid = SOLID_TRIGGER;
    ent->Use = G_trigger_multiple_Use;
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
  ent->wait = -1;
  G_trigger_multiple(ent);
}

/**
 * @brief Enables a previously dormant triggered trigger, making it solid and ready to activate.
 */
static void G_trigger_relay_Use(g_entity_t *ent, g_entity_t *other,
                                g_entity_t *activator) {
  G_UseTargets(ent, activator);
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
void G_trigger_relay(g_entity_t *ent) {
  ent->Use = G_trigger_relay_Use;
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
  if (ent->delay < 0.2) {
    ent->delay = 0.2;
  }

  G_UseTargets(ent, ent);
}

#define PUSH_ONCE 1
#define PUSH_EFFECT 2

/**
 * @brief Handles touch events on a trigger_push, applying velocity to the touching entity.
 */
static void G_trigger_push_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (other->move_type == MOVE_TYPE_WALK || other->move_type == MOVE_TYPE_BOUNCE) {

    other->velocity = Vec3_Scale(ent->move_dir, ent->speed * 10.0);

    if (other->client) {
      other->client->ps.pm_state.flags |= PMF_TIME_PUSHED;
      other->client->ps.pm_state.time = 240;
    }

    if (other->push_time < g_level.time) {
      other->push_time = g_level.time + 1500;
      G_MulticastSound(&(const g_play_sound_t) {
        .index = ent->move_info.sound_start,
        .origin = &other->s.origin,
        .atten = SOUND_ATTEN_SQUARE
      }, MULTICAST_PHS);
    }
  }

  if (ent->spawn_flags & PUSH_ONCE) {
    G_FreeEntity(ent);
  }
}

/**
 * @brief Creates an effect trail for the specified entity.
 */
static void G_trigger_push_Effect(g_entity_t *ent) {

  g_entity_t *effect = G_AllocEntity(__func__);

  effect->s.origin = Box3_Center(ent->bounds);

  effect->move_type = MOVE_TYPE_NONE;
  effect->s.trail = TRAIL_TELEPORTER;

  gi.LinkEntity(effect);
}

/*QUAKED trigger_push (.5 .5 .5) ? push_once push_effects
 Pushes the player in any direction. These are commonly used to make jump pads to send the player upwards. Using the angles key, you can project the player in any direction using "pitch yaw roll."

 -------- Keys --------
 angles : The direction to push the player in "pitch yaw roll" notation (e.g. -80 270 0).
 sound : The sound effect to play when the player is pushed (default "trigger/push").
 speed : The speed with which to push the player (default 100).

 -------- Spawn flags --------
 push_once : If set, the pusher is freed after it is used once.
 push_effects : If set, emit particle effects to indicate that a pusher is here.
 */
void G_trigger_push(g_entity_t *ent) {

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_push_Touch;

  const cm_entity_t *sound = gi.EntityValue(ent->def, "sound");
  if (sound->parsed & ENTITY_STRING) {
    ent->move_info.sound_start = gi.SoundIndex(sound->string);
  } else {
    ent->move_info.sound_start = gi.SoundIndex("trigger/push");
  }

  if (!ent->speed) {
    ent->speed = 100;
  }

  gi.LinkEntity(ent);

  if (ent->spawn_flags & PUSH_EFFECT) {
    G_trigger_push_Effect(ent);
  }
}

/**
 * @brief Handles use activation of a trigger_hurt, toggling its solidity on or off.
 */
static void G_trigger_hurt_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

  if (ent->solid == SOLID_NOT) {
    ent->solid = SOLID_TRIGGER;
  } else {
    ent->solid = SOLID_NOT;
  }

  gi.LinkEntity(ent);

  if (!(ent->spawn_flags & 2)) {
    ent->Use = NULL;
  }
}

/**
 * @brief Handles touch events on a trigger_hurt, dealing damage to entities that enter it.
 */
static void G_trigger_hurt_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (!other->take_damage) { // deal with items that land on us

    if (other->item) {
      if (other->item->type == ITEM_FLAG) {
        G_ResetDroppedFlag(other);
      } else if (other->item->type == ITEM_TECH) {
        G_ResetDroppedTech(other);
      } else {
        G_FreeEntity(other);
      }
    }

    G_Debug("%s\n", etos(other));
    return;
  }

  if (other->dead) {
    return;
  }

  if (ent->timestamp > g_level.time) {
    return;
  }

  if (ent->spawn_flags & 16) {
    ent->timestamp = g_level.time + 1000;
  } else {
    ent->timestamp = g_level.time + 100;
  }

  const int16_t d = ent->damage;

  int32_t dflags = DMG_NO_ARMOR;

  if (ent->spawn_flags & 8) {
    dflags = DMG_NO_GOD;
  }

  G_Damage(&(g_damage_t) {
    .target = other,
    .inflictor = ent,
    .attacker = NULL,
    .dir = Vec3_Zero(),
    .point = other->s.origin,
    .normal = Vec3_Zero(),
    .damage = d,
    .knockback = d >> 2,
    .flags = dflags,
    .mod = MOD_TRIGGER_HURT
  });
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
void G_trigger_hurt(g_entity_t *ent) {

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_hurt_Touch;

  if (!ent->damage) {
    ent->damage = 2;
  }

  if (ent->spawn_flags & 1) {
    ent->solid = SOLID_NOT;
  } else {
    ent->solid = SOLID_TRIGGER;
  }

  if (ent->spawn_flags & 2) {
    ent->Use = G_trigger_hurt_Use;
  }

  gi.LinkEntity(ent);
}

/**
 * @brief Handles touch events on a trigger_exec, executing a console command or script.
 */
static void G_trigger_exec_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

  if (ent->timestamp > g_level.time) {
    return;
  }

  ent->timestamp = g_level.time + ent->delay * 1000;

  const char *command = gi.EntityValue(ent->def, "command")->nullable_string;
  if (command) {
    gi.Cbuf(va("%s\n", command));
  }

  else {
    const char *script = gi.EntityValue(ent->def, "script")->nullable_string;
    if (script) {
      gi.Cbuf(va("exec %s\n", script));
    }
  }
}

/*QUAKED trigger_exec (0 1 0) ?
 Executes a console command or script file when activated.

 -------- Keys --------
 command : The console command(s) to execute.
 script : The script file (.cfg) to execute.
 delay : The delay in seconds between activation and execution of the commands.
 */
void G_trigger_exec(g_entity_t *ent) {

  const char *command = gi.EntityValue(ent->def, "command")->nullable_string;
  const char *script = gi.EntityValue(ent->def, "script")->nullable_string;
  if (!command && !script) {
    G_Debug("No command or script at %s", vtos(ent->s.origin));
    G_FreeEntity(ent);
    return;
  }

  G_Trigger_Init(ent);

  ent->Touch = G_trigger_exec_Touch;

  gi.LinkEntity(ent);
}
