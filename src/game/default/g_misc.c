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

static void G_func_areaportal_use(edict_t *ent, edict_t *other, edict_t *activator){
	ent->count ^= 1;  // toggle state
	gi.SetAreaPortalState(ent->style, ent->count);
}

/*QUAKED func_areaportal(0 0 0) ?

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void G_func_areaportal(edict_t *ent){
	ent->use = G_func_areaportal_use;
	ent->count = 0;  // always start closed;
}


/*QUAKED info_null(0 0.5 0)(-4 -4 -4)(4 4 4)
Used as a positional target for spotlights, etc.
*/
void G_info_null(edict_t *self){
	G_FreeEdict(self);
}


/*QUAKED info_notnull(0 0.5 0)(-4 -4 -4)(4 4 4)
Used as a positional target for lightning.
*/
void G_info_notnull(edict_t *self){
	VectorCopy(self->s.origin, self->abs_mins);
	VectorCopy(self->s.origin, self->abs_maxs);
}


/*QUAKED func_wall(0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON
This is just a solid wall if not inhibited

TRIGGER_SPAWN	the wall will not be present until triggered
				it will then blink in to existence; it will
				kill anything that was in it's way

TOGGLE			only valid for TRIGGER_SPAWN walls
				this allows the wall to be turned on and off

START_ON		only valid for TRIGGER_SPAWN walls
				the wall will initially be present
*/

static void G_func_wall_use(edict_t *self, edict_t *other, edict_t *activator){
	if(self->solid == SOLID_NOT){
		self->solid = SOLID_BSP;
		self->sv_flags &= ~SVF_NOCLIENT;
		G_KillBox(self);
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NOCLIENT;
	}
	gi.LinkEntity(self);

	if(!(self->spawn_flags & 2))
		self->use = NULL;
}

void G_func_wall(edict_t *self){
	self->move_type = MOVE_TYPE_PUSH;
	gi.SetModel(self, self->model);

	// just a wall
	if((self->spawn_flags & 7) == 0){
		self->solid = SOLID_BSP;
		gi.LinkEntity(self);
		return;
	}

	// it must be TRIGGER_SPAWN
	if(!(self->spawn_flags & 1)){
		gi.Debug("func_wall missing TRIGGER_SPAWN\n");
		self->spawn_flags |= 1;
	}

	// yell if the spawnflags are odd
	if(self->spawn_flags & 4){
		if(!(self->spawn_flags & 2)){
			gi.Debug("func_wall START_ON without TOGGLE\n");
			self->spawn_flags |= 2;
		}
	}

	self->use = G_func_wall_use;

	if(self->spawn_flags & 4){
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NOCLIENT;
	}
	gi.LinkEntity(self);
}


/*QUAKED func_object(0 .5 .8) ? TRIGGER_SPAWN
This is solid bmodel that will fall if it's support it removed.
*/

static void G_func_object_touch(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	// only squash thing we fall on top of
	if(!plane)
		return;
	if(plane->normal[2] < 1.0)
		return;
	if(!other->take_damage)
		return;
	G_Damage(other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void G_func_object_release(edict_t *self){
	self->move_type = MOVE_TYPE_TOSS;
	self->touch = G_func_object_touch;
}

static void G_func_object_use(edict_t *self, edict_t *other, edict_t *activator){
	self->solid = SOLID_BSP;
	self->sv_flags &= ~SVF_NOCLIENT;
	self->use = NULL;
	G_KillBox(self);
	G_func_object_release(self);
}

void G_func_object(edict_t *self){
	gi.SetModel(self, self->model);

	self->mins[0] += 1;
	self->mins[1] += 1;
	self->mins[2] += 1;
	self->maxs[0] -= 1;
	self->maxs[1] -= 1;
	self->maxs[2] -= 1;

	if(!self->dmg)
		self->dmg = 100;

	if(self->spawn_flags == 0){
		self->solid = SOLID_BSP;
		self->move_type = MOVE_TYPE_PUSH;
		self->think = G_func_object_release;
		self->next_think = g_level.time + 2 * gi.server_frame;
	} else {
		self->solid = SOLID_NOT;
		self->move_type = MOVE_TYPE_PUSH;
		self->use = G_func_object_use;
		self->sv_flags |= SVF_NOCLIENT;
	}

	self->clip_mask = MASK_PLAYERSOLID;

	gi.LinkEntity(self);
}


/*QUAKED target_string(0 0 1)(-8 -8 -8)(8 8 8)
 *
 * TODO: gi.CenterPrint + think delay?
 */
static void G_target_string_use(edict_t *self, edict_t *other, edict_t *activator){
	/*edict_t *e;
	int n, l;
	char c;

	l = strlen(self->message);
	for(e = self->team_master; e; e = e->team_chain){
		if(!e->count)
			continue;
		n = e->count - 1;
		if(n > l){
			e->s.frame1 = 12;
			continue;
		}

		c = self->message[n];
		if(c >= '0' && c <= '9')
			e->s.frame1 = c - '0';
		else if(c == '-')
			e->s.frame1 = 10;
		else if(c == ':')
			e->s.frame1 = 11;
		else
			e->s.frame1 = 12;
	}*/
}

void G_target_string(edict_t *self){
	if(!self->message)
		self->message = "";
	self->use = G_target_string_use;
}


static void G_teleporter_touch(edict_t *self, edict_t *other, c_plane_t *plane, c_surface_t *surf){
	edict_t *dest;
	float speed;
	vec3_t forward;
	int i;

	if(!other->client)
		return;

	dest = G_Find(NULL, FOFS(target_name), self->target);

	if(!dest){
		gi.Debug("G_teleporter_touch: Couldn't find destination.\n");
		return;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	gi.UnlinkEntity(other);

	VectorCopy(dest->s.origin, other->s.origin);
	VectorCopy(dest->s.origin, other->s.old_origin);
	other->s.origin[2] += 10.0;

	// clear the velocity and hold them in place briefly
	other->velocity[2] = 0.0;
	speed = VectorLength(other->velocity);
	VectorClear(other->velocity);

	other->client->ps.pmove.pm_time = 20;  // hold time
	other->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

	// draw the teleport splash at source and on the player
	self->s.event = EV_TELEPORT;
	other->s.event = EV_TELEPORT;

	// set angles
	for(i = 0; i < 3; i++){
		other->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT(dest->s.angles[i] - other->client->cmd_angles[i]);
	}

	AngleVectors(dest->s.angles, forward, NULL, NULL);
	VectorScale(forward, speed, other->velocity);
	other->velocity[2] = 150.0;

	VectorClear(other->client->cmd_angles);
	VectorClear(other->client->angles);
	VectorClear(other->s.angles);

	G_KillBox(other);  // telefrag anyone in our spot

	gi.LinkEntity(other);
}


/*QUAKED misc_teleporter(1 0 0)(-32 -32 -24)(32 32 -16)
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
*/
void G_misc_teleporter(edict_t *ent){
	vec3_t v;

	if(!ent->target){
		gi.Debug("G_misc_teleporter: No target specified.\n");
		G_FreeEdict(ent);
		return;
	}

	ent->solid = SOLID_TRIGGER;
	ent->move_type = MOVE_TYPE_NONE;

	if(ent->model){  // model form, trigger_teleporter
		gi.SetModel(ent, ent->model);
		ent->sv_flags = SVF_NOCLIENT;
	}
	else {  // or model-less form, misc_teleporter
		VectorSet(ent->mins, -32.0, -32.0, -24.0);
		VectorSet(ent->maxs,  32.0,  32.0, -16.0);

		VectorCopy(ent->s.origin, v);
		v[2] -= 16.0;

		// add effect if ent is not burried and effect is not inhibited
		if(!gi.PointContents(v) && !(ent->spawn_flags & 4)){
			ent->s.effects = EF_TELEPORTER;
			ent->s.sound = gi.SoundIndex("world/teleport_hum");
		}
	}

	ent->touch = G_teleporter_touch;

	gi.LinkEntity(ent);
}


/*QUAKED misc_teleporter_dest(1 0 0)(-32 -32 -24)(32 32 -16)
Point teleporters at these.
*/
void G_misc_teleporter_dest(edict_t *ent){
	G_ProjectSpawn(ent);
}

