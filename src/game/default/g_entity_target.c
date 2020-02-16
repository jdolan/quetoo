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
#define LIGHT_TOGGLE 2

/**
 * @brief
 */
static void G_target_light_Toggle(g_entity_t *self) {

	if ((self->s.effects & EF_LIGHT) == 0) {
		self->s.client = self->locals.colors[0];
		self->s.effects |= EF_LIGHT;
	} else {
		self->s.client = 0;
		self->s.effects &= ~EF_LIGHT;
	}
}

/**
 * @brief Cycles through state and colors for the given light entity.
 */
static void G_target_light_Cycle(g_entity_t *self) {

	if ((self->s.effects & EF_LIGHT) == 0) {
		G_target_light_Toggle(self);
	} else {
		if (self->s.client == self->locals.colors[0]) {
			if (self->locals.colors[1]) {
				self->s.client = self->locals.colors[1];
			} else if (self->locals.spawn_flags & LIGHT_TOGGLE) {
				G_target_light_Toggle(self);
			}
		} else {
			if (self->locals.spawn_flags & LIGHT_TOGGLE) {
				G_target_light_Toggle(self);
			} else {
				self->s.client = self->locals.colors[0];
			}
		}
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

/*QUAKED target_light (0 1 0) (-8 -8 -8) (8 8 8) start_on toggle
 Emits a user-defined light when used. Lights can cycle between two colors,
 and also be toggled on and off.

 -------- Keys --------
 colors : The color(s) to cycle through (1 - 255 paletted, "red", "green", etc..)
 delay : The delay before activating, in seconds (default 0).
 dmg : The radius of the light in units.
 targetname : The target name of this entity.
 wait : If specified, an additional cycle will fire after this interval.

 -------- Spawn flags --------
 start_on : The light starts on, with the first configured color.
 toggle : The light will include an off state in its cycle.

 -------- Notes --------
 Use this entity to add switched lights (toggle). Use the wait key to synchronize
 color cycles with other entities.
*/
void G_target_light(g_entity_t *self) {

	if (!g_game.spawn.colors) {
		g_game.spawn.colors = "white";
	}

	char *c = strchr(g_game.spawn.colors, ' ');
	if (c) {
		self->locals.colors[1] = G_ColorByName(c + 1, 0);
		*c = '\0';
	}

	if (!self->locals.damage) {
		self->locals.damage = 300;
	}

	self->locals.colors[0] = G_ColorByName(g_game.spawn.colors, PALETTE_COLOR_WHITE);
	self->s.termination.x = self->locals.damage; // radius

	self->locals.Use = G_target_light_Use;

	if (self->locals.spawn_flags & LIGHT_START_ON) {
		G_target_light_Cycle(self);
	}

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
			ent->s.sound = ent->locals.noise_index;
		}
	} else { // intermittent sound
		gi.PositionedSound(ent->s.origin, ent, ent->locals.noise_index, ent->locals.attenuation, 0);
	}
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) loop_on loop_off
 Plays a sound each time it is used, or in loop if requested.

 -------- Keys --------
 noise : The name of the sample to play, e.g. voices/haunting.
 attenuation : The attenuation level; higher levels drop off more quickly (default 1):
   -1 : No attenuation, send the sound to the entire level.
    1 : Normal attenuation, hearable to all those in PHS of entity.
    2 : Idle attenuation, hearable only by those near to entity.
    3 : Static attenuation, hearable only by those very close to entity.
 targetname : The target name of this entity.

 -------- Spawn flags --------
 loop_on : The sound can be toggled, and will play in loop until used.
 loop_off : The sound can be toggled, and will begin playing when used.

 -------- Notes --------
 Use this entity only when a sound must be triggered by another entity. For
 all other ambient sounds, use misc_emit.
*/
void G_target_speaker(g_entity_t *ent) {
	char buffer[MAX_QPATH];

	if (!g_game.spawn.noise) {
		gi.Debug("No noise at %s\n", vtos(ent->s.origin));
		return;
	}

	if (!strstr(g_game.spawn.noise, "")) {
		g_snprintf(buffer, sizeof(buffer), "%s", g_game.spawn.noise);
	} else {
		g_strlcpy(buffer, g_game.spawn.noise, sizeof(buffer));
	}

	ent->locals.noise_index = gi.SoundIndex(buffer);

	if (!ent->locals.attenuation) {
		ent->locals.attenuation = ATTEN_NORM;
	} else if (ent->locals.attenuation == -1) { // use -1 so 0 defaults to 1
		ent->locals.attenuation = ATTEN_NONE;
	}

	// check for looping sound
	if (ent->locals.spawn_flags & SPEAKER_LOOP_ON) {
		ent->s.sound = ent->locals.noise_index;
	}

	ent->locals.Use = G_target_speaker_Use;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
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
