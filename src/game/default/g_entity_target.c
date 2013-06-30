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
static void G_target_speaker_Use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (ent->locals.spawn_flags & 3) { // looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0; // turn it off
		else
			ent->s.sound = ent->locals.noise_index; // start it
	} else { // normal sound
		// use a positioned_sound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.PositionedSound(ent->s.origin, ent, ent->locals.noise_index, ent->locals.attenuation);
	}
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
 "noise"		wav file to play
 "attenuation"
 -1 = none, send to whole level
 1 = normal fighting sounds
 2 = idle sound level
 3 = ambient sound level

 Normal sounds play each time the target is used. The reliable flag can be set for crucial voiceovers.

 Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
 Multiple identical looping sounds will just increase volume without any speed cost.
 */
void G_target_speaker(g_edict_t *ent) {
	char buffer[MAX_QPATH];

	if (!g_game.spawn.noise) {
		gi.Debug("No noise at %s\n", vtos(ent->s.origin));
		return;
	}

	if (!strstr(g_game.spawn.noise, ""))
		g_snprintf(buffer, sizeof(buffer), "%s", g_game.spawn.noise);
	else
		g_strlcpy(buffer, g_game.spawn.noise, sizeof(buffer));

	ent->locals.noise_index = gi.SoundIndex(buffer);

	if (!ent->locals.attenuation)
		ent->locals.attenuation = ATTN_NORM;
	else if (ent->locals.attenuation == -1) // use -1 so 0 defaults to 1
		ent->locals.attenuation = ATTN_NONE;

	// check for looping sound
	if (ent->locals.spawn_flags & 1)
		ent->s.sound = ent->locals.noise_index;

	ent->locals.Use = G_target_speaker_Use;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.LinkEdict(ent);
}

/*
 * @brief
 */
static void G_target_explosion_Explode(g_edict_t *self) {
	vec_t save;

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.Multicast(self->s.origin, MULTICAST_PHS);

	G_RadiusDamage(self, self->locals.activator, NULL, self->locals.dmg, self->locals.dmg, self->locals.dmg + 40, MOD_EXPLOSIVE);

	save = self->locals.delay;
	self->locals.delay = 0;
	G_UseTargets(self, self->locals.activator);
	self->locals.delay = save;
}

/*
 * @brief
 */
static void G_target_explosion_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;

	if (!self->locals.delay) {
		G_target_explosion_Explode(self);
		return;
	}

	self->locals.Think = G_target_explosion_Explode;
	self->locals.next_think = g_level.time + self->locals.delay * 1000;
}

/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
 Spawns an explosion temporary entity when used.

 "delay"		wait this long before going off
 "dmg"		how much radius damage should be done, defaults to 0
 */
void G_target_explosion(g_edict_t *ent) {
	ent->locals.Use = G_target_explosion_Use;
	ent->sv_flags = SVF_NO_CLIENT;
}

/*
 * @brief
 */
static void G_target_splash_Think(g_edict_t *self) {

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_SPARKS);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(self->locals.move_dir);
	gi.Multicast(self->s.origin, MULTICAST_PVS);

	self->locals.next_think = g_level.time + (Randomf() * 3000);
}

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
 Creates a particle splash effect.
 */
void G_target_splash(g_edict_t *self) {

	G_SetMoveDir(self->s.angles, self->locals.move_dir);

	self->solid = SOLID_NOT;
	self->locals.Think = G_target_splash_Think;
	self->locals.next_think = g_level.time + (Randomf() * 3000);

	gi.LinkEdict(self);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
 */
void G_target_string(g_edict_t *self) {

	if (!self->locals.message)
		self->locals.message = "";

	// the rest is handled by G_UseTargets
}
