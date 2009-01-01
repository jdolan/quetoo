/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "g_local.h"


/*QUAKED target_speaker(1 0 0)(-8 -8 -8)(8 8 8) looped-on looped-off reliable
"noise"		wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"	0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
Multiple identical looping sounds will just increase volume without any speed cost.
*/
static void Use_Target_Speaker(edict_t *ent, edict_t *other, edict_t *activator){
	int chan;

	if(ent->spawnflags & 3){  // looping sound toggles
		if(ent->s.sound)
			ent->s.sound = 0;  // turn it off
		else
			ent->s.sound = ent->noise_index;  // start it
	} else {  // normal sound
		if(ent->spawnflags & 4)
			chan = CHAN_VOICE | CHAN_RELIABLE;
		else
			chan = CHAN_VOICE;
		// use a positioned_sound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.PositionedSound(ent->s.origin, ent, chan, ent->noise_index, ent->volume, ent->attenuation, 0);
	}
}

void G_target_speaker(edict_t *ent){
	char buffer[MAX_QPATH];

	if(!st.noise){
		gi.Dprintf("target_speaker with no noise set at %s\n", vtos(ent->s.origin));
		return;
	}
	if(!strstr(st.noise, ".wav"))
		snprintf(buffer, sizeof(buffer), "%s.wav", st.noise);
	else
		strncpy(buffer, st.noise, sizeof(buffer));
	ent->noise_index = gi.SoundIndex(buffer);

	if(!ent->volume)
		ent->volume = 1.0;

	if(!ent->attenuation)
		ent->attenuation = 1.0;
	else if(ent->attenuation == -1)  // use -1 so 0 defaults to 1
		ent->attenuation = 0;

	// check for prestarted looping sound
	if(ent->spawnflags & 1)
		ent->s.sound = ent->noise_index;

	ent->use = Use_Target_Speaker;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.LinkEntity(ent);
}


/*QUAKED target_explosion(1 0 0)(-8 -8 -8)(8 8 8)
Spawns an explosion temporary entity when used.

"delay"		wait this long before going off
"dmg"		how much radius damage should be done, defaults to 0
*/
static void target_explosion_explode(edict_t *self){
	float save;

	gi.WriteByte(svc_temp_entity);
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

static void use_target_explosion(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;

	if(!self->delay){
		target_explosion_explode(self);
		return;
	}

	self->think = target_explosion_explode;
	self->nextthink = level.time + self->delay;
}

void G_target_explosion(edict_t *ent){
	ent->use = use_target_explosion;
	ent->svflags = SVF_NOCLIENT;
}


/*QUAKED target_splash(1 0 0)(-8 -8 -8)(8 8 8)
Creates a particle splash effect.
*/

static void target_splash_think(edict_t *self){

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPARKS);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(self->movedir);
	gi.Multicast(self->s.origin, MULTICAST_PVS);

	self->nextthink = level.time + (frand() * 3);
}

void G_target_splash(edict_t *self){

	G_SetMovedir(self->s.angles, self->movedir);

	self->solid = SOLID_NOT;
	self->think = target_splash_think;
	self->nextthink = level.time + (frand() * 3);

	gi.LinkEntity(self);
}
