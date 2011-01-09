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
 * PLATS
 *
 * movement options:
 *
 * linear
 * smooth start, hard stop
 * smooth start, smooth stop
 *
 * start
 * end
 * acceleration
 * speed
 * deceleration
 * begin sound
 * end sound
 * target fired when reaching end
 * wait at end
 *
 * object characteristics that use move segments
 * ---------------------------------------------
 * movetype_push, or movetype_stop
 * action when touched
 * action when blocked
 * action when used
 * 	disabled?
 * auto trigger spawning
 */

#define PLAT_LOW_TRIGGER	1

#define STATE_TOP			0
#define STATE_BOTTOM		1
#define STATE_UP			2
#define STATE_DOWN			3

#define DOOR_START_OPEN		1
#define DOOR_REVERSE		2
#define DOOR_CRUSHER		4
#define DOOR_TOGGLE			32
#define DOOR_X_AXIS			64
#define DOOR_Y_AXIS			128


// Support routines for movement (changes in origin using velocity)

static void Move_Done(edict_t *ent){
	VectorClear(ent->velocity);
	ent->move_info.done(ent);
}

static void Move_Final(edict_t *ent){

	if(ent->move_info.remaining_distance == 0){
		Move_Done(ent);
		return;
	}

	VectorScale(ent->move_info.dir, ent->move_info.remaining_distance / gi.server_frame, ent->velocity);

	ent->think = Move_Done;
	ent->next_think = g_level.time + gi.server_frame;
}

static void Move_Begin(edict_t *ent){
	float frames;

	if((ent->move_info.speed * gi.server_frame) >= ent->move_info.remaining_distance){
		Move_Final(ent);
		return;
	}
	VectorScale(ent->move_info.dir, ent->move_info.speed, ent->velocity);
	frames = floor((ent->move_info.remaining_distance / ent->move_info.speed) / gi.server_frame);
	ent->move_info.remaining_distance -= frames * ent->move_info.speed * gi.server_frame;
	ent->next_think = g_level.time + (frames * gi.server_frame);
	ent->think = Move_Final;
}

static void Think_AccelMove(edict_t *ent);

static void Move_Calc(edict_t *ent, vec3_t dest, void(*done)(edict_t*)){

	VectorClear(ent->velocity);
	VectorSubtract(dest, ent->s.origin, ent->move_info.dir);
	ent->move_info.remaining_distance = VectorNormalize(ent->move_info.dir);
	ent->move_info.done = done;

	if(ent->move_info.speed == ent->move_info.accel && ent->move_info.speed == ent->move_info.decel){
		if(g_level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent)){
			Move_Begin(ent);
		} else {
			ent->next_think = g_level.time + gi.server_frame;
			ent->think = Move_Begin;
		}
	} else {
		// accelerative
		ent->move_info.current_speed = 0;
		ent->think = Think_AccelMove;
		ent->next_think = g_level.time + gi.server_frame;
	}
}


// Support routines for angular movement (changes in angle using avelocity)

static void AngleMove_Done(edict_t *ent){
	VectorClear(ent->avelocity);
	ent->move_info.done(ent);
}

static void AngleMove_Final(edict_t *ent){
	vec3_t move;

	if(ent->move_info.state == STATE_UP)
		VectorSubtract(ent->move_info.end_angles, ent->s.angles, move);
	else
		VectorSubtract(ent->move_info.start_angles, ent->s.angles, move);

	if(VectorCompare(move, vec3_origin)){
		AngleMove_Done(ent);
		return;
	}

	VectorScale(move, 1.0 / gi.server_frame, ent->avelocity);

	ent->think = AngleMove_Done;
	ent->next_think = g_level.time + gi.server_frame;
}

static void AngleMove_Begin(edict_t *ent){
	vec3_t destdelta;
	float len;
	float traveltime;
	float frames;

	// set destdelta to the vector needed to move
	if(ent->move_info.state == STATE_UP)
		VectorSubtract(ent->move_info.end_angles, ent->s.angles, destdelta);
	else
		VectorSubtract(ent->move_info.start_angles, ent->s.angles, destdelta);

	// calculate length of vector
	len = VectorLength(destdelta);

	// divide by speed to get time to reach dest
	traveltime = len / ent->move_info.speed;

	if(traveltime < gi.server_frame){
		AngleMove_Final(ent);
		return;
	}

	frames = floor(traveltime / gi.server_frame);

	// scale the destdelta vector by the time spent traveling to get velocity
	VectorScale(destdelta, 1.0 / traveltime, ent->avelocity);

	// set next_think to trigger a think when dest is reached
	ent->next_think = g_level.time + frames * gi.server_frame;
	ent->think = AngleMove_Final;
}

static void AngleMove_Calc(edict_t *ent, void(*done)(edict_t*)){

	VectorClear(ent->avelocity);

	ent->move_info.done = done;

	if(g_level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent)){
		AngleMove_Begin(ent);
	} else {
		ent->next_think = g_level.time + gi.server_frame;
		ent->think = AngleMove_Begin;
	}
}


/*
 * Think_AccelMove
 *
 * The team has completed a frame of movement, so change the speed for the next frame
 */
#define AccelerationDistance(target, rate) (target * ((target / rate) + 1) / 2)

static void plat_CalcAcceleratedMove(g_move_info_t *move_info){
	float accel_dist;
	float decel_dist;

	move_info->move_speed = move_info->speed;

	if(move_info->remaining_distance < move_info->accel){
		move_info->current_speed = move_info->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance(move_info->speed, move_info->accel);
	decel_dist = AccelerationDistance(move_info->speed, move_info->decel);

	if((move_info->remaining_distance - accel_dist - decel_dist) < 0){
		float f;

		f = (move_info->accel + move_info->decel) / (move_info->accel * move_info->decel);
		move_info->move_speed = (-2 + sqrt(4 - 4 * f * (-2 * move_info->remaining_distance))) / (2 * f);
		decel_dist = AccelerationDistance(move_info->move_speed, move_info->decel);
	}

	move_info->decel_distance = decel_dist;
}

static void plat_Accelerate(g_move_info_t *move_info){
	// are we decelerating?
	if(move_info->remaining_distance <= move_info->decel_distance){
		if(move_info->remaining_distance < move_info->decel_distance){
			if(move_info->next_speed){
				move_info->current_speed = move_info->next_speed;
				move_info->next_speed = 0;
				return;
			}
			if(move_info->current_speed > move_info->decel)
				move_info->current_speed -= move_info->decel;
		}
		return;
	}

	// are we at full speed and need to start decelerating during this move?
	if(move_info->current_speed == move_info->move_speed)
		if((move_info->remaining_distance - move_info->current_speed) < move_info->decel_distance){
			float p1_distance;
			float p2_distance;
			float distance;

			p1_distance = move_info->remaining_distance - move_info->decel_distance;
			p2_distance = move_info->move_speed * (1.0 - (p1_distance / move_info->move_speed));
			distance = p1_distance + p2_distance;
			move_info->current_speed = move_info->move_speed;
			move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance / distance);
			return;
		}

	// are we accelerating?
	if(move_info->current_speed < move_info->speed){
		float old_speed;
		float p1_distance;
		float p1_speed;
		float p2_distance;
		float distance;

		old_speed = move_info->current_speed;

		// figure simple acceleration up to move_speed
		move_info->current_speed += move_info->accel;
		if(move_info->current_speed > move_info->speed)
			move_info->current_speed = move_info->speed;

		// are we accelerating throughout this entire move?
		if((move_info->remaining_distance - move_info->current_speed) >= move_info->decel_distance)
			return;

		// during this move we will accelrate from current_speed to move_speed
		// and cross over the decel_distance; figure the average speed for the
		// entire move
		p1_distance = move_info->remaining_distance - move_info->decel_distance;
		p1_speed = (old_speed + move_info->move_speed) / 2.0;
		p2_distance = move_info->move_speed * (1.0 - (p1_distance / p1_speed));
		distance = p1_distance + p2_distance;
		move_info->current_speed = (p1_speed * (p1_distance / distance)) + (move_info->move_speed * (p2_distance / distance));
		move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance / distance);
		return;
	}

	// we are at constant velocity(move_speed)
	return;
}

static void Think_AccelMove(edict_t *ent){

	ent->move_info.remaining_distance -= ent->move_info.current_speed;

	if(ent->move_info.current_speed == 0)  // starting or blocked
		plat_CalcAcceleratedMove(&ent->move_info);

	plat_Accelerate(&ent->move_info);

	// will the entire move complete on next frame?
	if(ent->move_info.remaining_distance <= ent->move_info.current_speed){
		Move_Final(ent);
		return;
	}

	VectorScale(ent->move_info.dir, ent->move_info.current_speed * gi.server_hz, ent->velocity);
	ent->next_think = g_level.time + gi.server_frame;
	ent->think = Think_AccelMove;
}


static void plat_go_down(edict_t *ent);

static void plat_hit_top(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_end)
			gi.Sound(ent, ent->move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->move_info.state = STATE_TOP;

	ent->think = plat_go_down;
	ent->next_think = g_level.time + 3;
}

static void plat_hit_bottom(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_end)
			gi.Sound(ent, ent->move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->move_info.state = STATE_BOTTOM;
}

static void plat_go_down(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_start)
			gi.Sound(ent, ent->move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->move_info.sound_middle;
	}
	ent->move_info.state = STATE_DOWN;
	Move_Calc(ent, ent->move_info.end_origin, plat_hit_bottom);
}

static void plat_go_up(edict_t *ent){
	if(!(ent->flags & FL_TEAMSLAVE)){
		if(ent->move_info.sound_start)
			gi.Sound(ent, ent->move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->move_info.sound_middle;
	}
	ent->move_info.state = STATE_UP;
	Move_Calc(ent, ent->move_info.start_origin, plat_hit_top);
}

static void plat_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	if(self->move_info.state == STATE_UP)
		plat_go_down(self);
	else if(self->move_info.state == STATE_DOWN)
		plat_go_up(self);
}


static void Use_Plat(edict_t *ent, edict_t *other, edict_t *activator){
	if(ent->think)
		return;  // already down
	plat_go_down(ent);
}


static void Touch_Plat_Center(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(!other->client)
		return;

	if(other->health <= 0)
		return;

	ent = ent->enemy;  // now point at the plat, not the trigger
	if(ent->move_info.state == STATE_BOTTOM)
		plat_go_up(ent);
	else if(ent->move_info.state == STATE_TOP)
		ent->next_think = g_level.time + 1;  // the player is still on the plat, so delay going down
}

static void plat_spawn_inside_trigger(edict_t *ent){
	edict_t *trigger;
	vec3_t tmin, tmax;

	// middle trigger
	trigger = G_Spawn();
	trigger->touch = Touch_Plat_Center;
	trigger->movetype = MOVETYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->enemy = ent;

	tmin[0] = ent->mins[0] + 25;
	tmin[1] = ent->mins[1] + 25;
	tmin[2] = ent->mins[2];

	tmax[0] = ent->maxs[0] - 25;
	tmax[1] = ent->maxs[1] - 25;
	tmax[2] = ent->maxs[2] + 8;

	tmin[2] = tmax[2] - (ent->pos1[2] - ent->pos2[2] + st.lip);

	if(ent->spawnflags & PLAT_LOW_TRIGGER)
		tmax[2] = tmin[2] + 8;

	if(tmax[0] - tmin[0] <= 0){
		tmin[0] = (ent->mins[0] + ent->maxs[0]) * 0.5;
		tmax[0] = tmin[0] + 1;
	}
	if(tmax[1] - tmin[1] <= 0){
		tmin[1] = (ent->mins[1] + ent->maxs[1]) * 0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy(tmin, trigger->mins);
	VectorCopy(tmax, trigger->maxs);

	gi.LinkEntity(trigger);
}


/*QUAKED func_plat(0 .5 .8) ? PLAT_LOW_TRIGGER

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out
disabled in the extended position until it is trigger, when it will lower
and become a normal plat.

"speed"	overrides default 200.
"accel" overrides default 500
"lip"	overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves,
instead of being implicitly determined by the model's height.
*/
void G_func_plat(edict_t *ent){
	float f;

	VectorClear(ent->s.angles);
	ent->solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;

	gi.SetModel(ent, ent->model);

	ent->blocked = plat_blocked;

	if(!ent->speed)
		ent->speed = 200;
	ent->speed *= 0.5;

	if(!ent->accel)
		ent->accel = 50;
	ent->accel *= 0.1;

	if(!ent->decel)
		ent->decel = 50;
	ent->decel *= 0.1;

	// normalize move_info based on server rate, stock q2 rate was 10hz
	f = 100.0 / (gi.server_hz * gi.server_hz);
	ent->speed *= f;
	ent->accel *= f;
	ent->decel *= f;

	if(!ent->dmg)
		ent->dmg = 2;

	if(!st.lip)
		st.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	VectorCopy(ent->s.origin, ent->pos1);
	VectorCopy(ent->s.origin, ent->pos2);

	if(st.height)  // use the specified height
		ent->pos2[2] -= st.height;
	else  // or derive it from the model height
		ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - st.lip;

	ent->use = Use_Plat;

	plat_spawn_inside_trigger(ent);  // the "start moving" trigger

	if(ent->targetname){
		ent->move_info.state = STATE_UP;
	} else {
		VectorCopy(ent->pos2, ent->s.origin);
		gi.LinkEntity(ent);
		ent->move_info.state = STATE_BOTTOM;
	}

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	ent->move_info.sound_start = gi.SoundIndex("world/plat_start");
	ent->move_info.sound_middle = gi.SoundIndex("world/plat_mid");
	ent->move_info.sound_end = gi.SoundIndex("world/plat_end");
}


/*QUAKED func_rotating(0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP ANIMATED ANIMATED_FAST
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"speed" determines how fast it moves; default value is 100.
"dmg"	damage to inflict when blocked(2 default)

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
*/

static void rotating_blocked(edict_t *self, edict_t *other){
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void rotating_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(self->avelocity[0] || self->avelocity[1] || self->avelocity[2])
		G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void rotating_use(edict_t *self, edict_t *other, edict_t *activator){
	if(!VectorCompare(self->avelocity, vec3_origin)){
		self->s.sound = 0;
		VectorClear(self->avelocity);
		self->touch = NULL;
	} else {
		self->s.sound = self->move_info.sound_middle;
		VectorScale(self->movedir, self->speed, self->avelocity);
		if(self->spawnflags & 16)
			self->touch = rotating_touch;
	}
}

void G_func_rotating(edict_t *ent){
	ent->solid = SOLID_BSP;
	if(ent->spawnflags & 32)
		ent->movetype = MOVETYPE_STOP;
	else
		ent->movetype = MOVETYPE_PUSH;

	// set the axis of rotation
	VectorClear(ent->movedir);
	if(ent->spawnflags & 4)
		ent->movedir[2] = 1.0;
	else if(ent->spawnflags & 8)
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if(ent->spawnflags & 2)
		VectorNegate(ent->movedir, ent->movedir);

	if(!ent->speed)
		ent->speed = 100;

	if(!ent->dmg)
		ent->dmg = 2;

	ent->use = rotating_use;
	if(ent->dmg)
		ent->blocked = rotating_blocked;

	if(ent->spawnflags & 1)
		ent->use(ent, NULL, NULL);

	if(ent->spawnflags & 64)
		ent->s.effects |= EF_ANIMATE;
	if(ent->spawnflags & 128)
		ent->s.effects |= EF_ANIMATE_FAST;

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/*
 *
 * BUTTONS
 *
 */

/*QUAKED func_button(0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"sounds"
1) silent
2) steam metal
3) wooden clunk
4) metallic click
5) in-out
*/

static void button_done(edict_t *self){
	self->move_info.state = STATE_BOTTOM;
}

static void button_return(edict_t *self){
	self->move_info.state = STATE_DOWN;

	Move_Calc(self, self->move_info.start_origin, button_done);

	self->s.frame = 0;

	if(self->health)
		self->takedamage = true;
}

static void button_wait(edict_t *self){
	self->move_info.state = STATE_TOP;

	G_UseTargets(self, self->activator);
	self->s.frame = 1;
	if(self->move_info.wait >= 0){
		  self->next_think = g_level.time + self->move_info.wait;
		  self->think = button_return;
	}
}

static void button_fire(edict_t *self){
	if(self->move_info.state == STATE_UP || self->move_info.state == STATE_TOP)
		  return;

	self->move_info.state = STATE_UP;
	if(self->move_info.sound_start && !(self->flags & FL_TEAMSLAVE))
		  gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
	Move_Calc(self, self->move_info.end_origin, button_wait);
}

static void button_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;
	button_fire(self);
}

static void button_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(!other->client)
		  return;

	if(other->health <= 0)
		  return;

	self->activator = other;
	button_fire(self);
}

static void button_killed(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	self->activator = attacker;
	self->health = self->max_health;
	self->takedamage = false;
	button_fire(self);
}

void G_func_button(edict_t *ent){
	vec3_t abs_movedir;
	float dist;

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_STOP;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	if(ent->sounds != 1)
		  ent->move_info.sound_start = gi.SoundIndex("world/switch");

	if(!ent->speed)
		  ent->speed = 40;
	if(!ent->accel)
		  ent->accel = ent->speed;
	if(!ent->decel)
		  ent->decel = ent->speed;

	if(!ent->wait)
		  ent->wait = 3;
	if(!st.lip)
		  st.lip = 4;

	VectorCopy(ent->s.origin, ent->pos1);
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	dist = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	VectorMA(ent->pos1, dist, ent->movedir, ent->pos2);

	ent->use = button_use;

	if(ent->health){
		  ent->max_health = ent->health;
		  ent->die = button_killed;
		  ent->takedamage = true;
	} else if(! ent->targetname)
		  ent->touch = button_touch;

	ent->move_info.state = STATE_BOTTOM;

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	gi.LinkEntity(ent);
}

/*
 *
 * DOORS
 *
 * spawn a trigger surrounding the entire team unless it is
 * already targeted by another
 *
 */

/*QUAKED func_door(0 .5 .8) ? START_OPEN x CRUSHER ANIMATED TOGGLE ANIMATED_FAST
TOGGLE		wait in both the start and end states for a trigger event.
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered(not useful for touch or takedamage doors).

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed(100 default)
"wait"		wait before returning(3 default, -1 = never return)
"lip"		lip remaining at end of move(8 default)
"dmg"		damage to inflict when blocked(2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

static void func_door_use_areaportals(edict_t *self, qboolean open){
	edict_t *t = NULL;

	if(!self->target)
		  return;

	while((t = G_Find(t, FOFS(targetname), self->target))){
		  if(strcasecmp(t->classname, "func_areaportal") == 0){
			  gi.SetAreaPortalState(t->style, open);
		  }
	}
}

static void func_door_go_down(edict_t *self);

static void func_door_hit_top(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_end)
			gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->move_info.state = STATE_TOP;
	if(self->spawnflags & DOOR_TOGGLE)
		return;
	if(self->move_info.wait >= 0){
		self->think = func_door_go_down;
		self->next_think = g_level.time + self->move_info.wait;
	}
}

static void func_door_hit_bottom(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_end)
			gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->move_info.state = STATE_BOTTOM;
	func_door_use_areaportals(self, false);
}

static void func_door_go_down(edict_t *self){
	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}
	if(self->max_health){
		self->takedamage = true;
		self->health = self->max_health;
	}

	self->move_info.state = STATE_DOWN;
	if(strcmp(self->classname, "func_door") == 0)
		Move_Calc(self, self->move_info.start_origin, func_door_hit_bottom);
	else if(strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc(self, func_door_hit_bottom);
}

static void func_door_go_up(edict_t *self, edict_t *activator){
	if(self->move_info.state == STATE_UP)
		return;  // already going up

	if(self->move_info.state == STATE_TOP){  // reset top wait time
		if(self->move_info.wait >= 0)
			self->next_think = g_level.time + self->move_info.wait;
		return;
	}

	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}
	self->move_info.state = STATE_UP;
	if(strcmp(self->classname, "func_door") == 0)
		Move_Calc(self, self->move_info.end_origin, func_door_hit_top);
	else if(strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc(self, func_door_hit_top);

	G_UseTargets(self, activator);
	func_door_use_areaportals(self, true);
}

static void func_door_use(edict_t *self, edict_t *other, edict_t *activator){
	edict_t *ent;

	if(self->flags & FL_TEAMSLAVE)
		return;

	if(self->spawnflags & DOOR_TOGGLE){
		if(self->move_info.state == STATE_UP || self->move_info.state == STATE_TOP){
			// trigger all paired doors
			for(ent = self; ent; ent = ent->teamchain){
				ent->message = NULL;
				ent->touch = NULL;
				func_door_go_down(ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for(ent = self; ent; ent = ent->teamchain){
		ent->message = NULL;
		ent->touch = NULL;
		func_door_go_up(ent, activator);
	}
}

static void Touch_DoorTrigger(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(other->health <= 0)
		return;

	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 1.0;

	func_door_use(self->owner, other, other);
}

static void Think_CalcMoveSpeed(edict_t *self){
	edict_t *ent;
	float min;
	float time;
	float newspeed;
	float ratio;
	float dist;

	if(self->flags & FL_TEAMSLAVE)
		return;  // only the team master does this

	// find the smallest distance any member of the team will be moving
	min = fabsf(self->move_info.distance);
	for(ent = self->teamchain; ent; ent = ent->teamchain){
		dist = fabsf(ent->move_info.distance);
		if(dist < min)
			min = dist;
	}

	time = min / self->move_info.speed;

	// adjust speeds so they will all complete at the same time
	for(ent = self; ent; ent = ent->teamchain){
		newspeed = fabsf(ent->move_info.distance) / time;
		ratio = newspeed / ent->move_info.speed;
		if(ent->move_info.accel == ent->move_info.speed)
			ent->move_info.accel = newspeed;
		else
			ent->move_info.accel *= ratio;
		if(ent->move_info.decel == ent->move_info.speed)
			ent->move_info.decel = newspeed;
		else
			ent->move_info.decel *= ratio;
		ent->move_info.speed = newspeed;
	}
}

static void Think_SpawnDoorTrigger(edict_t *ent){
	edict_t *other;
	vec3_t mins, maxs;

	if(ent->flags & FL_TEAMSLAVE)
		return;  // only the team leader spawns a trigger

	VectorCopy(ent->absmin, mins);
	VectorCopy(ent->absmax, maxs);

	for(other = ent->teamchain; other; other = other->teamchain){
		AddPointToBounds(other->absmin, mins, maxs);
		AddPointToBounds(other->absmax, mins, maxs);
	}

	// expand
	mins[0] -= 60;
	mins[1] -= 60;
	maxs[0] += 60;
	maxs[1] += 60;

	other = G_Spawn();
	VectorCopy(mins, other->mins);
	VectorCopy(maxs, other->maxs);
	other->owner = ent;
	other->solid = SOLID_TRIGGER;
	other->movetype = MOVETYPE_NONE;
	other->touch = Touch_DoorTrigger;
	gi.LinkEntity(other);

	if(ent->spawnflags & DOOR_START_OPEN)
		func_door_use_areaportals(ent, true);

	Think_CalcMoveSpeed(ent);
}

static void func_door_blocked(edict_t *self, edict_t *other){
	edict_t *ent;

	if(!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);

	if(self->spawnflags & DOOR_CRUSHER)
		return;

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if(self->move_info.wait >= 0){
		if(self->move_info.state == STATE_DOWN){
			for(ent = self->teammaster; ent; ent = ent->teamchain)
				func_door_go_up(ent, ent->activator);
		} else {
			for(ent = self->teammaster; ent; ent = ent->teamchain)
				func_door_go_down(ent);
		}
	}
}

static void func_door_killed(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	edict_t *ent;

	for(ent = self->teammaster; ent; ent = ent->teamchain){
		ent->health = ent->max_health;
		ent->takedamage = false;
	}

	func_door_use(self->teammaster, attacker, attacker);
}

static void func_door_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 5.0;

	if(self->message && strlen(self->message))
		gi.ClientCenterPrint(other, "%s", self->message);

	gi.Sound(other, gi.SoundIndex("misc/chat"), ATTN_NORM);
}

void G_func_door(edict_t *ent){
	vec3_t abs_movedir;

	if(ent->sounds != 1){
		ent->move_info.sound_start = gi.SoundIndex("world/door_start");
		ent->move_info.sound_end = gi.SoundIndex("world/door_end");
	}

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->blocked = func_door_blocked;
	ent->use = func_door_use;

	if(!ent->speed)
		ent->speed = 100;
	ent->speed *= 2;

	if(!ent->accel)
		ent->accel = ent->speed;
	if(!ent->decel)
		ent->decel = ent->speed;

	if(!ent->wait)
		ent->wait = 3;
	if(!st.lip)
		st.lip = 8;
	if(!ent->dmg)
		ent->dmg = 2;

	// calculate second position
	VectorCopy(ent->s.origin, ent->pos1);
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	ent->move_info.distance = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	VectorMA(ent->pos1, ent->move_info.distance, ent->movedir, ent->pos2);

	// if it starts open, switch the positions
	if(ent->spawnflags & DOOR_START_OPEN){
		VectorCopy(ent->pos2, ent->s.origin);
		VectorCopy(ent->pos1, ent->pos2);
		VectorCopy(ent->s.origin, ent->pos1);
	}

	ent->move_info.state = STATE_BOTTOM;

	if(ent->health){
		ent->takedamage = true;
		ent->die = func_door_killed;
		ent->max_health = ent->health;
	} else if(ent->targetname && ent->message){
		gi.SoundIndex("misc/chat");
		ent->touch = func_door_touch;
	}

	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->pos1, ent->move_info.start_origin);
	VectorCopy(ent->s.angles, ent->move_info.start_angles);
	VectorCopy(ent->pos2, ent->move_info.end_origin);
	VectorCopy(ent->s.angles, ent->move_info.end_angles);

	if(ent->spawnflags & 16)
		ent->s.effects |= EF_ANIMATE;
	if(ent->spawnflags & 64)
		ent->s.effects |= EF_ANIMATE_FAST;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if(!ent->team)
		ent->teammaster = ent;

	gi.LinkEntity(ent);

	ent->next_think = g_level.time + gi.server_frame;
	if(ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_door_rotating(0 .5 .8) ? START_OPEN REVERSE CRUSHER ANIMATED TOGGLE X_AXIS Y_AXIS
TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered(not useful for touch or takedamage doors).

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed(100 default)
"wait"		wait before returning(3 default, -1 = never return)
"dmg"		damage to inflict when blocked(2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

void G_func_door_rotating(edict_t *ent){
	VectorClear(ent->s.angles);

	// set the axis of rotation
	VectorClear(ent->movedir);
	if(ent->spawnflags & DOOR_X_AXIS)
		ent->movedir[2] = 1.0;
	else if(ent->spawnflags & DOOR_Y_AXIS)
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if(ent->spawnflags & DOOR_REVERSE)
		VectorNegate(ent->movedir, ent->movedir);

	if(!st.distance){
		gi.Debug("%s at %s with no distance set\n", ent->classname, vtos(ent->s.origin));
		st.distance = 90;
	}

	VectorCopy(ent->s.angles, ent->pos1);
	VectorMA(ent->s.angles, st.distance, ent->movedir, ent->pos2);
	ent->move_info.distance = st.distance;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->blocked = func_door_blocked;
	ent->use = func_door_use;

	if(!ent->speed)
		ent->speed = 100;
	if(!ent->accel)
		ent->accel = ent->speed;
	if(!ent->decel)
		ent->decel = ent->speed;

	if(!ent->wait)
		ent->wait = 3;
	if(!ent->dmg)
		ent->dmg = 2;

	if(ent->sounds != 1){
		ent->move_info.sound_start = gi.SoundIndex("world/door_start");
		ent->move_info.sound_end = gi.SoundIndex("world/door_end");
	}

	// if it starts open, switch the positions
	if(ent->spawnflags & DOOR_START_OPEN){
		VectorCopy(ent->pos2, ent->s.angles);
		VectorCopy(ent->pos1, ent->pos2);
		VectorCopy(ent->s.angles, ent->pos1);
		VectorNegate(ent->movedir, ent->movedir);
	}

	if(ent->health){
		ent->takedamage = true;
		ent->die = func_door_killed;
		ent->max_health = ent->health;
	}

	if(ent->targetname && ent->message){
		gi.SoundIndex("misc/chat");
		ent->touch = func_door_touch;
	}

	ent->move_info.state = STATE_BOTTOM;
	ent->move_info.speed = ent->speed;
	ent->move_info.accel = ent->accel;
	ent->move_info.decel = ent->decel;
	ent->move_info.wait = ent->wait;
	VectorCopy(ent->s.origin, ent->move_info.start_origin);
	VectorCopy(ent->pos1, ent->move_info.start_angles);
	VectorCopy(ent->s.origin, ent->move_info.end_origin);
	VectorCopy(ent->pos2, ent->move_info.end_angles);

	if(ent->spawnflags & 16)
		ent->s.effects |= EF_ANIMATE;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if(!ent->team)
		ent->teammaster = ent;

	gi.LinkEntity(ent);

	ent->next_think = g_level.time + gi.server_frame;
	if(ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;
}


/*QUAKED func_water(0 .5 .8) ? START_OPEN
func_water is a moveable water brush.  It must be targeted to operate.  Use a non-water texture at your own risk.

START_OPEN causes the water to move to its destination when spawned and operate in reverse.

"angle"		determines the opening direction(up or down only)
"speed"		movement speed(25 default)
"wait"		wait before returning(-1 default, -1 = TOGGLE)
"lip"		lip remaining at end of move(0 default)
*/

void G_func_water(edict_t *self){
	vec3_t abs_movedir;

	G_SetMovedir(self->s.angles, self->movedir);
	self->movetype = MOVETYPE_PUSH;
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	// calculate second position
	VectorCopy(self->s.origin, self->pos1);
	abs_movedir[0] = fabsf(self->movedir[0]);
	abs_movedir[1] = fabsf(self->movedir[1]);
	abs_movedir[2] = fabsf(self->movedir[2]);
	self->move_info.distance = abs_movedir[0] * self->size[0] + abs_movedir[1] * self->size[1] + abs_movedir[2] * self->size[2] - st.lip;
	VectorMA(self->pos1, self->move_info.distance, self->movedir, self->pos2);

	// if it starts open, switch the positions
	if(self->spawnflags & DOOR_START_OPEN){
		VectorCopy(self->pos2, self->s.origin);
		VectorCopy(self->pos1, self->pos2);
		VectorCopy(self->s.origin, self->pos1);
	}

	VectorCopy(self->pos1, self->move_info.start_origin);
	VectorCopy(self->s.angles, self->move_info.start_angles);
	VectorCopy(self->pos2, self->move_info.end_origin);
	VectorCopy(self->s.angles, self->move_info.end_angles);

	self->move_info.state = STATE_BOTTOM;

	if(!self->speed)
		self->speed = 25;
	self->move_info.accel = self->move_info.decel = self->move_info.speed = self->speed;

	if(!self->wait)
		self->wait = -1;
	self->move_info.wait = self->wait;

	self->use = func_door_use;

	if(self->wait == -1)
		self->spawnflags |= DOOR_TOGGLE;

	self->classname = "func_door";

	gi.LinkEntity(self);
}


#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

/*QUAKED func_train(0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
noise	looping sound to play when the train is in motion

*/
static void func_train_next(edict_t *self);

static void func_train_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	if(!self->dmg)
		return;

	self->touch_time = g_level.time + 0.5;
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void func_train_wait(edict_t *self){
	if(self->target_ent->pathtarget){
		char *savetarget;
		edict_t *ent;

		ent = self->target_ent;
		savetarget = ent->target;
		ent->target = ent->pathtarget;
		G_UseTargets(ent, self->activator);
		ent->target = savetarget;

		// make sure we didn't get killed by a killtarget
		if(!self->inuse)
			return;
	}

	if(self->move_info.wait){
		if(self->move_info.wait > 0){
			self->next_think = g_level.time + self->move_info.wait;
			self->think = func_train_next;
		} else if(self->spawnflags & TRAIN_TOGGLE)  // && wait < 0
		{
			func_train_next(self);
			self->spawnflags &= ~TRAIN_START_ON;
			VectorClear(self->velocity);
			self->next_think = 0;
		}

		if(!(self->flags & FL_TEAMSLAVE)){
			if(self->move_info.sound_end)
				gi.Sound(self, self->move_info.sound_end, ATTN_IDLE);
			self->s.sound = 0;
		}
	} else {
		func_train_next(self);
	}

}

static void func_train_next(edict_t *self){
	edict_t *ent;
	vec3_t dest;
	qboolean first;

	first = true;
again:
	if(!self->target)
		return;

	ent = G_PickTarget(self->target);
	if(!ent){
		gi.Debug("train_next: bad target %s\n", self->target);
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if(ent->spawnflags & 1){
		if(!first){
			gi.Debug("connected teleport path_corners, see %s at %s\n", ent->classname, vtos(ent->s.origin));
			return;
		}
		first = false;
		VectorSubtract(ent->s.origin, self->mins, self->s.origin);
		VectorCopy(self->s.origin, self->s.old_origin);
		self->s.event = EV_TELEPORT;
		gi.LinkEntity(self);
		goto again;
	}

	self->move_info.wait = ent->wait;
	self->target_ent = ent;

	if(!(self->flags & FL_TEAMSLAVE)){
		if(self->move_info.sound_start)
			gi.Sound(self, self->move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->move_info.sound_middle;
	}

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->move_info.start_origin);
	VectorCopy(dest, self->move_info.end_origin);
	Move_Calc(self, dest, func_train_wait);
	self->spawnflags |= TRAIN_START_ON;
}

static void func_train_resume(edict_t *self){
	edict_t *ent;
	vec3_t dest;

	ent = self->target_ent;

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->move_info.start_origin);
	VectorCopy(dest, self->move_info.end_origin);
	Move_Calc(self, dest, func_train_wait);
	self->spawnflags |= TRAIN_START_ON;
}

static void func_train_find(edict_t *self){
	edict_t *ent;

	if(!self->target){
		gi.Debug("train_find: no target\n");
		return;
	}
	ent = G_PickTarget(self->target);
	if(!ent){
		gi.Debug("train_find: target %s not found\n", self->target);
		return;
	}
	self->target = ent->target;

	VectorSubtract(ent->s.origin, self->mins, self->s.origin);
	gi.LinkEntity(self);

	// if not triggered, start immediately
	if(!self->targetname)
		self->spawnflags |= TRAIN_START_ON;

	if(self->spawnflags & TRAIN_START_ON){
		self->next_think = g_level.time + gi.server_frame;
		self->think = func_train_next;
		self->activator = self;
	}
}

static void func_train_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;

	if(self->spawnflags & TRAIN_START_ON){
		if(!(self->spawnflags & TRAIN_TOGGLE))
			return;
		self->spawnflags &= ~TRAIN_START_ON;
		VectorClear(self->velocity);
		self->next_think = 0;
	} else {
		if(self->target_ent)
			func_train_resume(self);
		else
			func_train_next(self);
	}
}

void G_func_train(edict_t *self){
	self->movetype = MOVETYPE_PUSH;

	VectorClear(self->s.angles);
	self->blocked = func_train_blocked;
	if(self->spawnflags & TRAIN_BLOCK_STOPS)
		self->dmg = 0;
	else {
		if(!self->dmg)
			self->dmg = 100;
	}
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	if(st.noise)
		self->move_info.sound_middle = gi.SoundIndex(st.noise);

	if(!self->speed)
		self->speed = 100;

	self->move_info.speed = self->speed;
	self->move_info.accel = self->move_info.decel = self->move_info.speed;

	self->use = func_train_use;

	gi.LinkEntity(self);

	if(self->target){
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->next_think = g_level.time + gi.server_frame;
		self->think = func_train_find;
	} else {
		gi.Debug("func_train without a target at %s\n", vtos(self->absmin));
	}
}


/*QUAKED func_timer(0.3 0.1 0.6)(-8 -8 -8)(8 8 8) START_ON
"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and(wait + random)

"delay"			delay before first firing when turned on, default is 0

"pausetime"		additional delay used only the very first time
				and only if spawned with START_ON

These can used but not touched.
*/
static void func_timer_think(edict_t *self){
	G_UseTargets(self, self->activator);
	self->next_think = g_level.time + self->wait + crandom() * self->random;
}

static void func_timer_use(edict_t *self, edict_t *other, edict_t *activator){
	self->activator = activator;

	// if on, turn it off
	if(self->next_think){
		self->next_think = 0;
		return;
	}

	// turn it on
	if(self->delay)
		self->next_think = g_level.time + self->delay;
	else
		func_timer_think(self);
}

void G_func_timer(edict_t *self){
	if(!self->wait)
		self->wait = 1.0;

	self->use = func_timer_use;
	self->think = func_timer_think;

	if(self->random >= self->wait){
		self->random = self->wait - gi.server_frame;
		gi.Debug("func_timer at %s has random >= wait\n", vtos(self->s.origin));
	}

	if(self->spawnflags & 1){
		self->next_think = g_level.time + 1.0 + st.pausetime + self->delay + self->wait + crandom() * self->random;
		self->activator = self;
	}

	self->svflags = SVF_NOCLIENT;
}


/*QUAKED func_conveyor(0 .5 .8) ? START_ON TOGGLE
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed	default 100
*/

static void func_conveyor_use(edict_t *self, edict_t *other, edict_t *activator){
	if(self->spawnflags & 1){
		self->speed = 0;
		self->spawnflags &= ~1;
	} else {
		self->speed = self->count;
		self->spawnflags |= 1;
	}

	if(!(self->spawnflags & 2))
		self->count = 0;
}

void G_func_conveyor(edict_t *self){
	if(!self->speed)
		self->speed = 100;

	if(!(self->spawnflags & 1)){
		self->count = self->speed;
		self->speed = 0;
	}

	self->use = func_conveyor_use;

	gi.SetModel(self, self->model);
	self->solid = SOLID_BSP;
	gi.LinkEntity(self);
}


/*QUAKED func_door_secret(0 .5 .8) ? always_shoot 1st_left 1st_down
A secret door.  Slide back and then to the side.

open_once		doors never closes
1st_left		1st move is left of arrow
1st_down		1st move is down from arrow
always_shoot	door is shootebale even if targeted

"angle"		determines the direction
"dmg"		damage to inflic when blocked(default 2)
"wait"		how long to hold in the open position(default 5, -1 means hold)
*/

#define SECRET_ALWAYS_SHOOT	1
#define SECRET_1ST_LEFT		2
#define SECRET_1ST_DOWN		4

static void func_door_secret_move1(edict_t *self);
static void func_door_secret_move2(edict_t *self);
static void func_door_secret_move3(edict_t *self);
static void func_door_secret_move4(edict_t *self);
static void func_door_secret_move5(edict_t *self);
static void func_door_secret_move6(edict_t *self);
static void func_door_secret_done(edict_t *self);

static void func_door_secret_use(edict_t *self, edict_t *other, edict_t *activator){
	// make sure we're not already moving
	if(!VectorCompare(self->s.origin, vec3_origin))
		return;

	Move_Calc(self, self->pos1, func_door_secret_move1);
	func_door_use_areaportals(self, true);
}

static void func_door_secret_move1(edict_t *self){
	self->next_think = g_level.time + 1.0;
	self->think = func_door_secret_move2;
}

static void func_door_secret_move2(edict_t *self){
	Move_Calc(self, self->pos2, func_door_secret_move3);
}

static void func_door_secret_move3(edict_t *self){
	if(self->wait == -1)
		return;
	self->next_think = g_level.time + self->wait;
	self->think = func_door_secret_move4;
}

static void func_door_secret_move4(edict_t *self){
	Move_Calc(self, self->pos1, func_door_secret_move5);
}

static void func_door_secret_move5(edict_t *self){
	self->next_think = g_level.time + 1.0;
	self->think = func_door_secret_move6;
}

static void func_door_secret_move6(edict_t *self){
	Move_Calc(self, vec3_origin, func_door_secret_done);
}

static void func_door_secret_done(edict_t *self){
	if(!(self->targetname) ||(self->spawnflags & SECRET_ALWAYS_SHOOT)){
		self->health = 0;
		self->takedamage = true;
	}
	func_door_use_areaportals(self, false);
}

static void func_door_secret_blocked(edict_t *self, edict_t *other){
	if(!other->client)
		return;

	if(g_level.time < self->touch_time)
		return;

	self->touch_time = g_level.time + 0.5;
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MOD_CRUSH);
}

static void func_door_secret_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	self->takedamage = false;
	func_door_secret_use(self, attacker, attacker);
}

void G_func_door_secret(edict_t *ent){
	vec3_t forward, right, up;
	float side;
	float width;
	float length;

	ent->move_info.sound_start = gi.SoundIndex("world/door_start");
	ent->move_info.sound_end = gi.SoundIndex("world/door_end");

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->blocked = func_door_secret_blocked;
	ent->use = func_door_secret_use;

	if(!(ent->targetname) ||(ent->spawnflags & SECRET_ALWAYS_SHOOT)){
		ent->health = 0;
		ent->takedamage = true;
		ent->die = func_door_secret_die;
	}

	if(!ent->dmg)
		ent->dmg = 2;

	if(!ent->wait)
		ent->wait = 5;

	ent->move_info.accel =
		ent->move_info.decel =
			ent->move_info.speed = 50;

	// calculate positions
	AngleVectors(ent->s.angles, forward, right, up);
	VectorClear(ent->s.angles);
	side = 1.0 -(ent->spawnflags & SECRET_1ST_LEFT);
	if(ent->spawnflags & SECRET_1ST_DOWN)
		width = fabsf(DotProduct(up, ent->size));
	else
		width = fabsf(DotProduct(right, ent->size));
	length = fabsf(DotProduct(forward, ent->size));
	if(ent->spawnflags & SECRET_1ST_DOWN)
		VectorMA(ent->s.origin, -1 * width, up, ent->pos1);
	else
		VectorMA(ent->s.origin, side * width, right, ent->pos1);
	VectorMA(ent->pos1, length, forward, ent->pos2);

	if(ent->health){
		ent->takedamage = true;
		ent->die = func_door_killed;
		ent->max_health = ent->health;
	} else if(ent->targetname && ent->message){
		gi.SoundIndex("misc/chat");
		ent->touch = func_door_touch;
	}

	ent->classname = "func_door";

	gi.LinkEntity(ent);
}

/*QUAKED func_killbox(1 0 0) ?
Kills everything inside when fired, regardless of protection.
*/
static void func_killbox_use(edict_t *self, edict_t *other, edict_t *activator){
	G_KillBox(self);
}

void G_func_killbox(edict_t *ent){
	gi.SetModel(ent, ent->model);
	ent->use = func_killbox_use;
	ent->svflags = SVF_NOCLIENT;
}
