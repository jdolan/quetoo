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

#define LIGHT_START_ON 1

/**
 * @brief For singular lights, simply toggle them. For teamed lights,
 * advance through the team, toggling two at a time.
 */
static void G_target_light_Cycle(g_entity_t *self) {

	g_entity_t *master = self->locals.team_master;
	if (master) {
		G_Debug("Cycling %s\n", etos(master->locals.enemy));

		master->locals.enemy->s.effects ^= EF_LIGHT;
		master->locals.enemy = master->locals.enemy->locals.team_next;

		if (master->locals.enemy == NULL) {
			master->locals.enemy = master;
		}

		master->locals.enemy->s.effects ^= EF_LIGHT;
	} else {
		self->s.effects ^= EF_LIGHT;
	}
}

/**
 * @brief
 */
static void G_target_light_Use(g_entity_t *self, g_entity_t *other, g_entity_t *activator) {

	if (self->locals.delay) {
		self->locals.Think = G_target_light_Cycle;
		self->locals.next_think = g_level.time + self->locals.delay * 1000.0;
	} else {
		G_target_light_Cycle(self);
	}

	if (self->locals.wait) {
		self->locals.Think = G_target_light_Cycle;
		self->locals.next_think = g_level.time + (self->locals.delay + self->locals.wait) * 1000.0;
	}
}

/*QUAKED target_light (1 1 1) (-4 -4 -4) (4 4 4) start_on
 Emits a user-defined light when used. Lights can be chained with teams.

 -------- Keys --------
 _color : The light color (default 1.0 1.0 1.0).
 light : The radius of the light in units (default 300).
 delay : The delay before activating, in seconds (default 0).
 targetname : The target name of this entity.
 team : The team name for alternating lights.
 wait : If specified, an additional cycle will fire after this interval.

 -------- Spawn flags --------
 start_on : The light will start on.

 -------- Notes --------
 Use this entity to add switched lights. Use the wait key to synchronize
 color cycles with other entities.
*/
void G_target_light(g_entity_t *self) {

	if (Vec3_Equal(self->locals.color, Vec3_Zero())) {
		self->locals.color = Vec3_One();
	}

	self->locals.light = self->locals.light ?: 300.f;

	self->s.color = Color_Color32(Color3fv(self->locals.color));
	self->s.termination.x = self->locals.light;

	if (self->locals.spawn_flags & LIGHT_START_ON) {
		self->s.effects |= EF_LIGHT;
	}

	self->locals.enemy = self;
	self->locals.Use = G_target_light_Use;

	gi.LinkEntity(self);
}

#define SPEAKER_LOOP_ON 1
#define SPEAKER_LOOP_OFF 2

#define SPEAKER_LOOP (SPEAKER_LOOP_ON | SPEAKER_LOOP_OFF)

/**
 * @brief
 */
static void G_target_speaker_Use(g_entity_t *ent, g_entity_t *other, g_entity_t *activator) {

	if (ent->locals.spawn_flags & SPEAKER_LOOP) { // looping sound toggles
		if (ent->s.sound) {
			ent->s.sound = 0;
		} else {
			ent->s.sound = ent->locals.sound;
		}
	} else { // intermittent sound
		G_MulticastSound(&(const g_play_sound_t) {
			.index = ent->locals.sound,
			.origin = &ent->s.origin,
			.atten = ent->locals.atten
		}, MULTICAST_PHS, NULL);
	}
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) loop_on loop_off
 Plays a sound each time it is used, or in loop if requested.

 -------- Keys --------
 sound : The name of the sample to play, e.g. voices/haunting.
 atten : The attenuation level; higher levels drop off more quickly (default 1):
    0 : No attenuation, send the sound to the entire level.
    1 : Normal attenuation, hearable to all those in PHS of entity.
    2 : Idle attenuation, hearable only by those near to entity.
    3 : Static attenuation, hearable only by those very close to entity.
 targetname : The target name of this entity.

 -------- Spawn flags --------
 loop_on : The sound can be toggled, and will play in loop until used.
 loop_off : The sound can be toggled, and will begin playing when used.

 -------- Notes --------
 Use this entity only when a sound must be triggered by another entity. For
 ambient sounds, use the client-side version, misc_sound.
*/
void G_target_speaker(g_entity_t *ent) {

	const char *sound = gi.EntityValue(ent->def, "sound")->string;
	if (!strlen(sound)) {
		gi.Warn("No sound specified for %s\n", etos(ent));
		return;
	}

	ent->locals.sound = gi.SoundIndex(sound);

	const cm_entity_t *atten = gi.EntityValue(ent->def, "atten");
	if (atten->parsed & ENTITY_INTEGER) {
		ent->locals.atten = atten->integer;
	} else {
		ent->locals.atten = SOUND_ATTEN_LINEAR;
	}

	const int32_t spawn_flags = gi.EntityValue(ent->def, "spawnflags")->integer;

	// check for looping sound
	if (spawn_flags & SPEAKER_LOOP_ON) {
		ent->s.sound = ent->locals.sound;
	}

	ent->locals.Use = G_target_speaker_Use;

	// link the entity so the server can determine who to send updates to
	gi.LinkEntity(ent);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
 Displays a center-printed message to the player when used.
 -------- KEYS --------
 message : The message to display.
 targetname : The target name of this entity.
 */
void G_target_string(g_entity_t *self) {

	if (!self->locals.message) {
		self->locals.message = "";
	}

	// the rest is handled by G_UseTargets
}
