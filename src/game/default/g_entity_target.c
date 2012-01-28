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
 * G_target_speaker_use
 */
static void G_target_speaker_use(g_edict_t *ent, g_edict_t *other,
		g_edict_t *activator) {

	if (ent->spawn_flags & 3) { // looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0; // turn it off
		else
			ent->s.sound = ent->noise_index; // start it
	} else { // normal sound
		// use a positioned_sound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.PositionedSound(ent->s.origin, ent, ent->noise_index,
				ent->attenuation);
	}
}

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
 "noise"		wav file to play
 "attenuation"
 -1 = none, send to whole level
 1 = normal fighting sounds
 2 = idle sound level
 3 = ambient sound level

 Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

 Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
 Multiple identical looping sounds will just increase volume without any speed cost.
 */
void G_target_speaker(g_edict_t *ent) {
	char buffer[MAX_QPATH];

	if (!g_game.spawn.noise) {
		gi.Debug("target_speaker with no noise set at %s\n",
				vtos(ent->s.origin));
		return;
	}

	if (!strstr(g_game.spawn.noise, ""))
		snprintf(buffer, sizeof(buffer), "%s", g_game.spawn.noise);
	else
		strncpy(buffer, g_game.spawn.noise, sizeof(buffer));

	ent->noise_index = gi.SoundIndex(buffer);

	if (!ent->attenuation)
		ent->attenuation = ATTN_NORM;
	else if (ent->attenuation == -1) // use -1 so 0 defaults to 1
		ent->attenuation = ATTN_NONE;

	// check for looping sound
	if (ent->spawn_flags & 1)
		ent->s.sound = ent->noise_index;

	ent->use = G_target_speaker_use;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.LinkEntity(ent);
}

/*
 * G_target_explosion_explode
 */
static void G_target_explosion_explode(g_edict_t *self) {
	float save;

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_EXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.Multicast(self->s.origin, MULTICAST_PHS);

	G_RadiusDamage(self, self->activator, NULL, self->dmg, self->dmg,
			self->dmg + 40, MOD_EXPLOSIVE);

	save = self->delay;
	self->delay = 0;
	G_UseTargets(self, self->activator);
	self->delay = save;
}

/*
 * G_target_explosion_use
 */
static void G_target_explosion_use(g_edict_t *self, g_edict_t *other,
		g_edict_t *activator) {
	self->activator = activator;

	if (!self->delay) {
		G_target_explosion_explode(self);
		return;
	}

	self->think = G_target_explosion_explode;
	self->next_think = g_level.time + self->delay;
}

/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
 Spawns an explosion temporary entity when used.

 "delay"		wait this long before going off
 "dmg"		how much radius damage should be done, defaults to 0
 */
void G_target_explosion(g_edict_t *ent) {
	ent->use = G_target_explosion_use;
	ent->sv_flags = SVF_NO_CLIENT;
}

/*
 * G_target_splash_think
 */
static void G_target_splash_think(g_edict_t *self) {

	gi.WriteByte(SV_CMD_TEMP_ENTITY);
	gi.WriteByte(TE_SPARKS);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(self->move_dir);
	gi.Multicast(self->s.origin, MULTICAST_PVS);

	self->next_think = g_level.time + (frand() * 3);
}

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
 Creates a particle splash effect.
 */
void G_target_splash(g_edict_t *self) {

	G_SetMoveDir(self->s.angles, self->move_dir);

	self->solid = SOLID_NOT;
	self->think = G_target_splash_think;
	self->next_think = g_level.time + (frand() * 3);

	gi.LinkEntity(self);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
 */
void G_target_string(g_edict_t *self) {

	if (!self->message)
		self->message = "";

	// the rest is handled by G_UseTargets
}
