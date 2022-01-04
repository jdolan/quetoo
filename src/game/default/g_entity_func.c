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

/**
 * @brief
 */
static void G_MoveInfo_Linear_Done(g_entity_t *ent) {

	ent->s.origin = ent->locals.move_info.dest;

	ent->locals.velocity = Vec3_Zero();

	ent->locals.move_info.current_speed = 0.0;

	ent->locals.move_info.Done(ent);

	gi.LinkEntity(ent);
}

/**
 * @brief
 */
static void G_MoveInfo_Linear_Final(g_entity_t *ent) {
	g_move_info_t *move = &ent->locals.move_info;

	vec3_t dir;
	const float distance = Vec3_DistanceDir(move->dest, ent->s.origin, &dir);

	if (distance == 0.0 || Vec3_Dot(dir, move->dir) < 0.0) {
		G_MoveInfo_Linear_Done(ent);
		return;
	}

	ent->locals.Think = G_MoveInfo_Linear_Final;
	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief Starts a move with constant velocity. The entity will think again when it
 * has reached its destination.
 */
static void G_MoveInfo_Linear_Constant(g_entity_t *ent) {
	g_move_info_t *move = &ent->locals.move_info;

	const float distance = Vec3_Distance(move->dest, ent->s.origin);
	move->const_frames = distance / move->speed * QUETOO_TICK_RATE;

	ent->locals.velocity = Vec3_Scale(move->dir, move->speed);

	ent->locals.next_think = g_level.time + move->const_frames * QUETOO_TICK_MILLIS;
	ent->locals.Think = G_MoveInfo_Linear_Final;
}

/**
 * @brief Sets up a non-constant move, i.e. one that will accelerate near the beginning
 * and decelerate towards the end.
 */
static void G_MoveInfo_Linear_Accelerate(g_entity_t *ent) {
	g_move_info_t *move = &ent->locals.move_info;

	if (move->current_speed == 0.0) { // starting or restarting after being blocked

		// http://www.ajdesigner.com/constantacceleration/cavelocitya.php

		const float distance = Vec3_Distance(move->dest, ent->s.origin);

		const float accel_time = move->speed / move->accel;
		const float decel_time = move->speed / move->decel;

		move->accel_frames = accel_time * QUETOO_TICK_RATE;
		move->decel_frames = decel_time * QUETOO_TICK_RATE;

		const float avg_speed = move->speed * 0.5;

		float accel_distance = avg_speed * accel_time;
		float decel_distance = avg_speed * decel_time;

		if (accel_distance + decel_distance > distance) {
			G_Debug("Clamping acceleration for %s\n", etos(ent));

			const float scale = distance / (accel_distance + decel_distance);

			accel_distance *= scale;
			decel_distance *= scale;

			move->accel_frames = accel_distance / avg_speed * QUETOO_TICK_RATE;
			move->decel_frames = decel_distance / avg_speed * QUETOO_TICK_RATE;
		}

		const float const_distance = (distance - (accel_distance + decel_distance));
		const float const_time = const_distance / move->speed;

		move->const_frames = const_time * QUETOO_TICK_RATE;
	}

	// accelerate
	if (move->accel_frames) {
		move->current_speed += move->accel * QUETOO_TICK_SECONDS;
		if (move->current_speed > move->speed) {
			move->current_speed = move->speed;
		}
		move->accel_frames--;
	}

	// maintain speed
	else if (move->const_frames) {
		move->current_speed = move->speed;
		move->const_frames--;
	}

	// decelerate
	else if (move->decel_frames) {
		move->current_speed -= move->decel * QUETOO_TICK_SECONDS;
		if (move->current_speed < sqrtf(move->speed)) {
			move->current_speed = sqrtf(move->speed);
		}
		move->decel_frames--;
	}

	// done
	else {
		G_MoveInfo_Linear_Final(ent);
		return;
	}

	ent->locals.velocity = Vec3_Scale(move->dir, move->current_speed);

	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	ent->locals.Think = G_MoveInfo_Linear_Accelerate;
}

/**
 * @brief Sets up movement for the specified entity. Both constant and
 * accelerative movements are initiated through this function. Animations are
 * also kicked off here.
 */
static void G_MoveInfo_Linear_Init(g_entity_t *ent, const vec3_t dest, void (*Done)(g_entity_t *)) {
	g_move_info_t *move = &ent->locals.move_info;

	ent->locals.velocity = Vec3_Zero();
	move->current_speed = 0.0;

	move->dest = dest;
	move->dir = Vec3_Subtract(dest, ent->s.origin);
	move->dir = Vec3_Normalize(move->dir);

	move->Done = Done;

	if (move->accel == 0.0 && move->decel == 0.0) { // constant
		const g_entity_t *master = (ent->locals.flags & FL_TEAM_SLAVE) ? ent->locals.team_master : ent;
		if (g_level.current_entity == master) {
			G_MoveInfo_Linear_Constant(ent);
		} else {
			ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
			ent->locals.Think = G_MoveInfo_Linear_Constant;
		}
	} else { // accelerative
		ent->locals.Think = G_MoveInfo_Linear_Accelerate;
		ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	}
}

/**
 * @brief
 */
static void G_MoveInfo_Angular_Done(g_entity_t *ent) {

	ent->locals.avelocity = Vec3_Zero();
	ent->locals.move_info.Done(ent);
}

/**
 * @brief
 */
static void G_MoveInfo_Angular_Final(g_entity_t *ent) {
	g_move_info_t *move = &ent->locals.move_info;
	vec3_t delta;

	if (move->state == MOVE_STATE_GOING_UP) {
		delta = Vec3_Subtract(move->end_angles, ent->s.angles);
	} else {
		delta = Vec3_Subtract(move->start_angles, ent->s.angles);
	}

	if (Vec3_Equal(delta, Vec3_Zero())) {
		G_MoveInfo_Angular_Done(ent);
		return;
	}

	ent->locals.avelocity = Vec3_Scale(delta, 1.0 / QUETOO_TICK_SECONDS);

	ent->locals.Think = G_MoveInfo_Angular_Done;
	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_MoveInfo_Angular_Begin(g_entity_t *ent) {
	g_move_info_t *move = &ent->locals.move_info;
	vec3_t delta;

	// set move to the vector needed to move
	if (move->state == MOVE_STATE_GOING_UP) {
		delta = Vec3_Subtract(move->end_angles, ent->s.angles);
	} else {
		delta = Vec3_Subtract(move->start_angles, ent->s.angles);
	}

	// calculate length of vector
	const float len = Vec3_Length(delta);

	// divide by speed to get time to reach dest
	const float time = len / move->speed;

	if (time < QUETOO_TICK_SECONDS) {
		G_MoveInfo_Angular_Final(ent);
		return;
	}

	const float frames = floor(time / QUETOO_TICK_SECONDS);

	// scale the move vector by the time spent traveling to get velocity
	ent->locals.avelocity = Vec3_Scale(delta, 1.0 / time);

	// set next_think to trigger a think when dest is reached
	ent->locals.next_think = g_level.time + frames * QUETOO_TICK_MILLIS;
	ent->locals.Think = G_MoveInfo_Angular_Final;
}

/**
 * @brief
 */
static void G_MoveInfo_Angular_Init(g_entity_t *ent, void (*Done)(g_entity_t *)) {

	ent->locals.avelocity = Vec3_Zero();

	ent->locals.move_info.Done = Done;

	const g_entity_t *master = (ent->locals.flags & FL_TEAM_SLAVE) ? ent->locals.team_master : ent;
	if (g_level.current_entity == master) {
		G_MoveInfo_Angular_Begin(ent);
	} else {
		ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
		ent->locals.Think = G_MoveInfo_Angular_Begin;
	}
}

/**
 * @brief When a MOVE_TYPE_PUSH or MOVE_TYPE_STOP is blocked, deal with the
 * obstacle by damaging it.
 */
static void G_MoveType_Push_Blocked(g_entity_t *self, g_entity_t *other) {

	const vec3_t dir = self->locals.velocity;

	if (G_IsMeat(other)) {

		if (other->solid == SOLID_DEAD) {
			G_Damage(other, self, NULL, dir, other->s.origin, Vec3_Up(), 999, 0, DMG_NO_ARMOR, MOD_CRUSH);
			if (other->in_use) {
				if (other->client) {
					gi.WriteByte(SV_CMD_TEMP_ENTITY);
					gi.WriteByte(TE_GIB);
					gi.WritePosition(other->s.origin);
					gi.Multicast(other->s.origin, MULTICAST_PVS, NULL);

					other->solid = SOLID_NOT;
				} else {
					G_Gib(other);
				}
			}
		} else if (other->solid == SOLID_BOX) {
			G_Damage(other, self, NULL, dir, other->s.origin, Vec3_Up(), self->locals.damage, 0, DMG_NO_ARMOR, MOD_CRUSH);
		} else {
			G_Debug("Unhandled blocker: %s: %s\n", etos(self), etos(other));
		}
	} else {
		G_Damage(other, self, NULL, dir, other->s.origin, Vec3_Up(), 999, 0, 0, MOD_CRUSH);
		if (other->in_use) {
			G_Explode(other, 60, 60, 80.0, 0);
		}
	}
}

#define PLAT_LOW_TRIGGER	1

static void G_func_plat_GoingDown(g_entity_t *ent);

/**
 * @brief
 */
static void G_func_plat_Top(g_entity_t *ent) {

	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {

		if (ent->locals.move_info.sound_end) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = ent->locals.move_info.sound_end,
				.entity = ent,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		ent->s.sound = 0;
	}

	ent->locals.move_info.state = MOVE_STATE_TOP;

	ent->locals.Think = G_func_plat_GoingDown;
	ent->locals.next_think = g_level.time + 3000;
}

/**
 * @brief
 */
static void G_func_plat_Bottom(g_entity_t *ent) {

	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {

		if (ent->locals.move_info.sound_end) {
			vec3_t pos;

			pos = Box3_Center(ent->abs_bounds);
			pos.z = ent->abs_bounds.maxs.z;

			G_MulticastSound(&(const g_play_sound_t) {
				.index = ent->locals.move_info.sound_end,
				.origin = &pos,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		ent->s.sound = 0;
	}

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief
 */
static void G_func_plat_GoingDown(g_entity_t *ent) {

	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {

		if (ent->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = ent->locals.move_info.sound_start,
				.entity = ent,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		ent->s.sound = ent->locals.move_info.sound_middle;
	}

	ent->locals.move_info.state = MOVE_STATE_GOING_DOWN;
	G_MoveInfo_Linear_Init(ent, ent->locals.move_info.end_origin, G_func_plat_Bottom);
}

/**
 * @brief
 */
static void G_func_plat_GoingUp(g_entity_t *ent) {

	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {

		if (ent->locals.move_info.sound_start) {
			vec3_t pos;

			pos = Box3_Center(ent->abs_bounds);
			pos.z = ent->abs_bounds.maxs.z;

			G_MulticastSound(&(const g_play_sound_t) {
				.index = ent->locals.move_info.sound_start,
				.origin = &pos,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		ent->s.sound = ent->locals.move_info.sound_middle;
	}

	ent->locals.move_info.state = MOVE_STATE_GOING_UP;
	G_MoveInfo_Linear_Init(ent, ent->locals.move_info.start_origin, G_func_plat_Top);
}

/**
 * @brief
 */
static void G_func_plat_Blocked(g_entity_t *self, g_entity_t *other) {

	G_MoveType_Push_Blocked(self, other);

	if (self->locals.move_info.state == MOVE_STATE_GOING_UP) {
		G_func_plat_GoingDown(self);
	} else if (self->locals.move_info.state == MOVE_STATE_GOING_DOWN) {
		G_func_plat_GoingUp(self);
	}
}

/**
 * @brief
 */
static void G_func_plat_Use(g_entity_t *ent, g_entity_t *other,
                            g_entity_t *activator) {

	if (ent->locals.Think) {
		return; // already down
	}

	G_func_plat_GoingDown(ent);
}

/**
 * @brief
 */
static void G_func_plat_Touch(g_entity_t *ent, g_entity_t *other, const cm_trace_t *trace) {

	if (!other->client) {
		return;
	}

	if (other->locals.dead) {
		return;
	}

	ent = ent->locals.enemy; // now point at the plat, not the trigger

	if (ent->locals.move_info.state == MOVE_STATE_BOTTOM) {
		G_func_plat_GoingUp(ent);
	} else if (ent->locals.move_info.state == MOVE_STATE_TOP) {
		ent->locals.next_think = g_level.time + 1000; // the player is still on the plat, so delay going down
	}
}

/**
 * @brief
 */
static void G_func_plat_CreateTrigger(g_entity_t *ent) {
	g_entity_t *trigger;

	// middle trigger
	trigger = G_AllocEntity();
	trigger->locals.Touch = G_func_plat_Touch;
	trigger->locals.move_type = MOVE_TYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->locals.enemy = ent;

	box3_t bounds = Box3_Expand3(ent->bounds, Vec3(-25.f, -25.f, 0.f));
	bounds.maxs.z += 8;

	bounds.mins.z = bounds.maxs.z - (ent->locals.pos1.z - ent->locals.pos2.z + ent->locals.lip);

	if (ent->locals.spawn_flags & PLAT_LOW_TRIGGER) {
		bounds.maxs.z = bounds.mins.z + 8.0;
	}

	if (bounds.maxs.x - bounds.mins.x <= 0) {
		bounds.mins.x = (ent->bounds.mins.x + ent->bounds.maxs.x) * 0.5;
		bounds.maxs.x = bounds.mins.x + 1;
	}
	if (bounds.maxs.y - bounds.mins.y <= 0) {
		bounds.mins.y = (ent->bounds.mins.y + ent->bounds.maxs.y) * 0.5;
		bounds.maxs.y = bounds.mins.y + 1;
	}

	trigger->bounds = bounds;

	gi.LinkEntity(trigger);
}

/*QUAKED func_plat (0 .5 .8) ? low_trigger
 Rising platform the player can ride to reach higher places. Platforms must be placed in the raised position, so they will operate and be lit correctly, but they spawn in the lowered position. If the platform is the target of a trigger or button, it will start out disabled and in the extended position.

 -------- Keys --------
 speed : The speed with which the platform moves (default 200).
 accel : The platform acceleration (default 500).
 lip : The lip remaining at end of move (default 8 units).
 height : If set, this will determine the extent of the platform's movement, rather than implicitly using the platform's height.
 sounds : The sound set for the platform (0 default, 1 stone, -1 silent).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 low_trigger : If set, the touch field for this platform will only exist at the lower position.
 */
void G_func_plat(g_entity_t *ent) {

	ent->s.angles = Vec3_Zero();

	ent->solid = SOLID_BSP;
	ent->locals.move_type = MOVE_TYPE_PUSH;

	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_plat_Blocked;

	if (!ent->locals.speed) {
		ent->locals.speed = 200.0;
	}

	if (!ent->locals.accel) {
		ent->locals.accel = ent->locals.speed * 2.0;
	}

	if (!ent->locals.decel) {
		ent->locals.decel = ent->locals.accel;
	}

	if (!ent->locals.damage) {
		ent->locals.damage = 2;
	}

	if (!ent->locals.lip) {
		ent->locals.lip = 8.0;
	}

	// pos1 is the top position, pos2 is the bottom
	ent->locals.pos1 = ent->s.origin;
	ent->locals.pos2 = ent->s.origin;

	const cm_entity_t *height = gi.EntityValue(ent->def, "height");
	if (height->parsed & ENTITY_INTEGER) { // use the specified height
		ent->locals.pos2.z -= height->integer;
	} else { // or derive it from the model height
		ent->locals.pos2.z -= Box3_Size(ent->bounds).z - ent->locals.lip;
	}

	ent->locals.Use = G_func_plat_Use;

	G_func_plat_CreateTrigger(ent); // the "start moving" trigger

	if (ent->locals.target_name) {
		ent->locals.move_info.state = MOVE_STATE_GOING_UP;
	} else {
		ent->s.origin = ent->locals.pos2;
		ent->locals.move_info.state = MOVE_STATE_BOTTOM;
	}

	gi.LinkEntity(ent);

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;

	ent->locals.move_info.start_origin = ent->locals.pos1;
	ent->locals.move_info.start_angles = ent->s.angles;
	ent->locals.move_info.end_origin = ent->locals.pos2;
	ent->locals.move_info.end_angles = ent->s.angles;

	const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
	if (s != -1) {
		ent->locals.move_info.sound_start = gi.SoundIndex(va("world/plat_start_%d", s + 1));
		ent->locals.move_info.sound_middle = gi.SoundIndex(va("world/plat_middle_%d", s + 1));
		ent->locals.move_info.sound_end = gi.SoundIndex(va("world/plat_end_%d", s + 1));
	}
}

#define ROTATE_START_ON		0x1
#define ROTATE_REVERSE		0x2
#define ROTATE_AXIS_X		0x4
#define ROTATE_AXIS_Y		0x8
#define ROTATE_TOUCH_PAIN	0x10
#define ROTATE_TOUCH_STOP	0x20

/**
 * @brief
 */
static void G_func_rotating_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (self->locals.damage) {
		if (!Vec3_Equal(self->locals.avelocity, Vec3_Zero())) {
			G_Damage(other, self, NULL, Vec3_Zero(), other->s.origin, Vec3_Zero(), self->locals.damage, 1, 0, MOD_CRUSH);
		}
	}
}

/**
 * @brief
 */
static void G_func_rotating_Use(g_entity_t *self, g_entity_t *other,
                                g_entity_t *activator) {

	if (!Vec3_Equal(self->locals.avelocity, Vec3_Zero())) {
		self->s.sound = 0;
		self->locals.avelocity = Vec3_Zero();
		self->locals.Touch = NULL;
	} else {
		self->s.sound = self->locals.move_info.sound_middle;
		self->locals.avelocity = Vec3_Scale(self->locals.move_dir, self->locals.speed);
		if (self->locals.spawn_flags & ROTATE_TOUCH_PAIN) {
			self->locals.Touch = G_func_rotating_Touch;
		}
	}
}

/*QUAKED func_rotating (0 .5 .8) ? start_on reverse x_axis y_axis touch_pain stop
 Solid entity that rotates continuously. Rotates on the Z axis by default and requires an origin brush. It will always start on in the game and is not targetable.

 -------- Keys --------
 speed : The speed at which the entity rotates (default 100).
 dmg : The damage to inflict to players who touch or block the entity (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : The entity will spawn rotating.
 reverse : Will cause the entity to rotate in the opposite direction.
 x_axis : Rotate on the X axis.
 y_axis : Rotate on the Y axis.
 touch_pain : If set, any interaction with the entity will inflict damage to the player.
 stop : If set and the entity is blocked, the entity will stop rotating.
 */
void G_func_rotating(g_entity_t *ent) {

	ent->solid = SOLID_BSP;

	if (ent->locals.spawn_flags & ROTATE_TOUCH_STOP) {
		ent->locals.move_type = MOVE_TYPE_STOP;
	} else {
		ent->locals.move_type = MOVE_TYPE_PUSH;
	}

	// set the axis of rotation
	ent->locals.move_dir = Vec3_Zero();
	if (ent->locals.spawn_flags & ROTATE_AXIS_X) {
		ent->locals.move_dir.z = 1.0;
	} else if (ent->locals.spawn_flags & ROTATE_AXIS_Y) {
		ent->locals.move_dir.x = 1.0;
	} else {
		ent->locals.move_dir.y = 1.0;
	}

	// check for reverse rotation
	if (ent->locals.spawn_flags & ROTATE_REVERSE) {
		ent->locals.move_dir = Vec3_Negate(ent->locals.move_dir);
	}

	if (!ent->locals.speed) {
		ent->locals.speed = 100.0;
	}

	if (!ent->locals.damage) {
		ent->locals.damage = 2;
	}

	ent->locals.Use = G_func_rotating_Use;
	ent->locals.Blocked = G_MoveType_Push_Blocked;

	if (ent->locals.spawn_flags & ROTATE_START_ON) {
		ent->locals.Use(ent, NULL, NULL);
	}

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/**
 * @brief
 */
static void G_func_button_Done(g_entity_t *self) {
	self->locals.move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief
 */
static void G_func_button_Reset(g_entity_t *self) {
	g_move_info_t *move = &self->locals.move_info;

	move->state = MOVE_STATE_GOING_DOWN;

	G_MoveInfo_Linear_Init(self, move->start_origin, G_func_button_Done);

	if (self->locals.health) {
		self->locals.take_damage = true;
	}
}

/**
 * @brief
 */
static void G_func_button_Wait(g_entity_t *self) {
	g_move_info_t *move = &self->locals.move_info;

	move->state = MOVE_STATE_TOP;

	G_UseTargets(self, self->locals.activator);

	if (move->wait >= 0) {
		self->locals.next_think = g_level.time + move->wait * 1000;
		self->locals.Think = G_func_button_Reset;
	}
}

/**
 * @brief
 */
static void G_func_button_Activate(g_entity_t *self) {
	g_move_info_t *move = &self->locals.move_info;

	if (move->state == MOVE_STATE_GOING_UP || move->state == MOVE_STATE_TOP) {
		return;
	}

	move->state = MOVE_STATE_GOING_UP;

	if (move->sound_start && !(self->locals.flags & FL_TEAM_SLAVE)) {
		G_MulticastSound(&(const g_play_sound_t) {
			.index = move->sound_start,
			.entity = self,
			.atten = SOUND_ATTEN_SQUARE
		}, MULTICAST_PHS, NULL);
	}

	G_MoveInfo_Linear_Init(self, move->end_origin, G_func_button_Wait);
}

/**
 * @brief
 */
static void G_func_button_Use(g_entity_t *self, g_entity_t *other,
                              g_entity_t *activator) {

	self->locals.activator = activator;
	G_func_button_Activate(self);
}

/**
 * @brief
 */
static void G_func_button_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (!other->client) {
		return;
	}

	if (other->locals.health <= 0) {
		return;
	}

	self->locals.activator = other;
	G_func_button_Activate(self);
}

/**
 * @brief
 */
static void G_func_button_Die(g_entity_t *self, g_entity_t *attacker,
                              uint32_t mod) {

	self->locals.health = self->locals.max_health;
	self->locals.take_damage = false;

	self->locals.activator = attacker;
	G_func_button_Activate(self);
}

/*QUAKED func_button (0 .5 .8) ?
 When a button is touched by a player, it moves in the direction set by the "angle" key, triggers all its targets, stays pressed by the amount of time set by the "wait" key, then returns to its original position where it can be operated again.

 -------- Keys --------
 angle : Determines the direction in which the button will move (up = -1, down = -2).
 target : All entities with a matching target name will be triggered.
 speed : Speed of the button's displacement (default 40).
 wait : Number of seconds the button stays pressed (default 1, -1 = indefinitely).
 lip : Lip remaining at end of move (default 4 units).
 sounds : The sound set for the button (0 default, -1 silent).
 health : If set, the button must be killed instead of touched to use.
 targetname : The target name of this entity if it is to be triggered.
 */
void G_func_button(g_entity_t *ent) {
	vec3_t abs_move_dir;
	float dist;

	G_SetMoveDir(ent);
	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	gi.LinkEntity(ent);

	if (gi.EntityValue(ent->def, "sounds")->integer != -1) {
		ent->locals.move_info.sound_start = gi.SoundIndex("world/switch");
	}

	if (!ent->locals.speed) {
		ent->locals.speed = 40.0;
	}

	if (!ent->locals.wait) {
		ent->locals.wait = 3.0;
	}

	if (!ent->locals.lip) {
		ent->locals.lip = 4.0;
	}

	ent->locals.pos1 = ent->s.origin;
	abs_move_dir.x = fabsf(ent->locals.move_dir.x);
	abs_move_dir.y = fabsf(ent->locals.move_dir.y);
	abs_move_dir.z = fabsf(ent->locals.move_dir.z);
	dist = abs_move_dir.x * ent->size.x + abs_move_dir.y * ent->size.y
	       + abs_move_dir.z * ent->size.z - ent->locals.lip;
	ent->locals.pos2 = Vec3_Fmaf(ent->locals.pos1, dist, ent->locals.move_dir);

	ent->locals.Use = G_func_button_Use;

	if (ent->locals.health) {
		ent->locals.max_health = ent->locals.health;
		ent->locals.Die = G_func_button_Die;
		ent->locals.take_damage = true;
	} else if (!ent->locals.target_name) {
		ent->locals.Touch = G_func_button_Touch;
	}

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.wait = ent->locals.wait;
	ent->locals.move_info.start_origin = ent->locals.pos1;
	ent->locals.move_info.start_angles = ent->s.angles;
	ent->locals.move_info.end_origin = ent->locals.pos2;
	ent->locals.move_info.end_angles = ent->s.angles;
}

#define DOOR_START_OPEN        0x1
#define DOOR_TOGGLE            0x2
#define DOOR_ROTATING_REVERSE  0x4
#define DOOR_ROTATING_X_AXIS   0x8
#define DOOR_ROTATING_Y_AXIS   0x10

static void G_func_door_GoingDown(g_entity_t *self);

/**
 * @brief
 */
static void G_func_door_Top(g_entity_t *self) {

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_end) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_end,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = 0;
	}

	self->locals.move_info.state = MOVE_STATE_TOP;

	if (self->locals.spawn_flags & DOOR_TOGGLE) {
		return;
	}

	if (self->locals.move_info.wait >= 0) {
		self->locals.Think = G_func_door_GoingDown;
		self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
	}
}

/**
 * @brief
 */
static void G_func_door_Bottom(g_entity_t *self) {

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_end) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_end,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = 0;
	}

	self->locals.move_info.state = MOVE_STATE_BOTTOM;
}

/**
 * @brief
 */
static void G_func_door_GoingDown(g_entity_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_start,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}
		self->s.sound = self->locals.move_info.sound_middle;
	}
	if (self->locals.max_health) {
		self->locals.take_damage = true;
		self->locals.health = self->locals.max_health;
	}

	self->locals.move_info.state = MOVE_STATE_GOING_DOWN;
	if (g_strcmp0(self->class_name, "func_door_rotating")) {
		G_MoveInfo_Linear_Init(self, self->locals.move_info.start_origin, G_func_door_Bottom);
	} else { // rotating
		G_MoveInfo_Angular_Init(self, G_func_door_Bottom);
	}
}

/**
 * @brief
 */
static void G_func_door_GoingUp(g_entity_t *self, g_entity_t *activator) {

	if (self->locals.move_info.state == MOVE_STATE_GOING_UP) {
		return; // already going up
	}

	if (self->locals.move_info.state == MOVE_STATE_TOP) { // reset top wait time
		if (self->locals.move_info.wait >= 0) {
			self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
		}
		return;
	}

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_start,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}
		self->s.sound = self->locals.move_info.sound_middle;
	}
	self->locals.move_info.state = MOVE_STATE_GOING_UP;
	if (g_strcmp0(self->class_name, "func_door_rotating")) {
		G_MoveInfo_Linear_Init(self, self->locals.move_info.end_origin, G_func_door_Top);
	} else { // rotating
		G_MoveInfo_Angular_Init(self, G_func_door_Top);
	}

	G_UseTargets(self, activator);
}

/**
 * @brief
 */
static void G_func_door_Use(g_entity_t *self, g_entity_t *other, g_entity_t *activator) {
	g_entity_t *ent;

	if (self->locals.flags & FL_TEAM_SLAVE) {
		return;
	}

	if (self->locals.spawn_flags & DOOR_TOGGLE) {
		if (self->locals.move_info.state == MOVE_STATE_GOING_UP
		        || self->locals.move_info.state == MOVE_STATE_TOP) {
			// trigger all paired doors
			for (ent = self; ent; ent = ent->locals.team_next) {
				ent->locals.message = NULL;
				ent->locals.Touch = NULL;
				G_func_door_GoingDown(ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for (ent = self; ent; ent = ent->locals.team_next) {
		ent->locals.message = NULL;
		ent->locals.Touch = NULL;
		G_func_door_GoingUp(ent, activator);
	}
}

/**
 * @brief
 */
static void G_func_door_TouchTrigger(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (other->locals.health <= 0) {
		return;
	}

	if (!other->client) {
		return;
	}

	if (g_level.time < self->locals.touch_time) {
		return;
	}

	self->locals.touch_time = g_level.time + 1000;

	G_func_door_Use(self->owner, other, other);
}

/**
 * @brief
 */
static void G_func_door_CalculateMove(g_entity_t *self) {
	g_entity_t *ent;

	if (self->locals.flags & FL_TEAM_SLAVE) {
		return; // only the team master does this
	}

	// find the smallest distance any member of the team will be moving
	float min = fabsf(self->locals.move_info.distance);
	for (ent = self->locals.team_next; ent; ent = ent->locals.team_next) {
		float dist = fabsf(ent->locals.move_info.distance);
		if (dist < min) {
			min = dist;
		}
	}

	const float time = min / self->locals.move_info.speed;

	// adjust speeds so they will all complete at the same time
	for (ent = self; ent; ent = ent->locals.team_next) {
		const float new_speed = fabsf(ent->locals.move_info.distance) / time;
		const float ratio = new_speed / ent->locals.move_info.speed;
		if (ent->locals.move_info.accel == ent->locals.move_info.speed) {
			ent->locals.move_info.accel = new_speed;
		} else {
			ent->locals.move_info.accel *= ratio;
		}
		if (ent->locals.move_info.decel == ent->locals.move_info.speed) {
			ent->locals.move_info.decel = new_speed;
		} else {
			ent->locals.move_info.decel *= ratio;
		}
		ent->locals.move_info.speed = new_speed;
	}
}

/**
 * @brief
 */
static void G_func_door_CreateTrigger(g_entity_t *ent) {
	g_entity_t *trigger;

	if (ent->locals.flags & FL_TEAM_SLAVE) {
		return; // only the team leader spawns a trigger
	}

	box3_t bounds = ent->abs_bounds;

	for (trigger = ent->locals.team_next; trigger; trigger = trigger->locals.team_next) {
		bounds = Box3_Union(bounds, trigger->abs_bounds);
	}

	// expand
	bounds = Box3_Expand3(bounds, Vec3(60.f, 60.f, 0.f));

	trigger = G_AllocEntity();
	trigger->bounds = bounds;
	trigger->owner = ent;
	trigger->solid = SOLID_TRIGGER;
	trigger->locals.move_type = MOVE_TYPE_NONE;
	trigger->locals.Touch = G_func_door_TouchTrigger;
	gi.LinkEntity(trigger);

	G_func_door_CalculateMove(ent);
}

/**
 * @brief
 */
static void G_func_door_Blocked(g_entity_t *self, g_entity_t *other) {

	G_MoveType_Push_Blocked(self, other);

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (self->locals.move_info.wait >= 0) {
		g_entity_t *ent;
		if (self->locals.move_info.state == MOVE_STATE_GOING_DOWN) {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_next) {
				G_func_door_GoingUp(ent, ent->locals.activator);
			}
		} else {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_next) {
				G_func_door_GoingDown(ent);
			}
		}
	}
}

/**
 * @brief
 */
static void G_func_door_Die(g_entity_t *self, g_entity_t *attacker, uint32_t mod) {

	g_entity_t *ent;

	for (ent = self->locals.team_master; ent; ent = ent->locals.team_next) {
		ent->locals.health = ent->locals.max_health;
		ent->locals.take_damage = false;
	}

	G_func_door_Use(self->locals.team_master, attacker, attacker);
}

/**
 * @brief
 */
static void G_func_door_Touch(g_entity_t *self, g_entity_t *other, const cm_trace_t *trace) {

	if (!other->client) {
		return;
	}

	if (g_level.time < self->locals.touch_time) {
		return;
	}

	self->locals.touch_time = g_level.time + 3000;

	if (self->locals.message && strlen(self->locals.message)) {
		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(self->locals.message);
		gi.Unicast(other, true);
	}

	G_UnicastSound(&(const g_play_sound_t) {
		.index = g_media.sounds.chat,
	}, other, true);
}

/*QUAKED func_door (0 .5 .8) ? start_open x x x toggle
 A sliding door. By default, doors open when a player walks close to them.

 -------- Keys --------
 message : An optional string printed when the door is first touched.
 angle : Determines the opening direction of the door (up = -1, down = -2).
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_open : The door to moves to its destination when spawned, and operates in reverse.
 toggle : The door will wait in both the start and end states for a trigger event.
 */
void G_func_door(g_entity_t *ent) {
	vec3_t abs_move_dir;

	G_SetMoveDir(ent);
	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_door_Blocked;
	ent->locals.Use = G_func_door_Use;

	if (!ent->locals.speed) {
		ent->locals.speed = 200.0;
	}

	if (!ent->locals.accel) {
		ent->locals.accel = ent->locals.speed * 2.0;
	}

	if (!ent->locals.decel) {
		ent->locals.decel = ent->locals.accel;
	}

	if (!ent->locals.wait) {
		ent->locals.wait = 3.0;
	}

	if (!ent->locals.lip) {
		ent->locals.lip = 8.0;
	}

	if (!ent->locals.damage) {
		ent->locals.damage = 2;
	}

	// calculate second position
	ent->locals.pos1 = ent->s.origin;
	abs_move_dir = Vec3_Fabsf(ent->locals.move_dir);
	ent->locals.move_info.distance = abs_move_dir.x * ent->size.x +
	                                 abs_move_dir.y * ent->size.y +
	                                 abs_move_dir.z * ent->size.z - ent->locals.lip;

	ent->locals.pos2 = Vec3_Fmaf(ent->locals.pos1, ent->locals.move_info.distance, ent->locals.move_dir);

	// if it starts open, switch the positions
	if (ent->locals.spawn_flags & DOOR_START_OPEN) {
		ent->s.origin = ent->locals.pos2;
		ent->locals.pos2 = ent->locals.pos1;
		ent->locals.pos1 = ent->s.origin;
	}

	gi.LinkEntity(ent);

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_Die;
		ent->locals.max_health = ent->locals.health;
	} else if (ent->locals.target_name && ent->locals.message) {
		ent->locals.Touch = G_func_door_Touch;
	}

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;

	ent->locals.move_info.start_origin = ent->locals.pos1;
	ent->locals.move_info.start_angles = ent->s.angles;
	ent->locals.move_info.end_origin = ent->locals.pos2;
	ent->locals.move_info.end_angles = ent->s.angles;

	const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
	if (s != -1) {
		ent->locals.move_info.sound_start = gi.SoundIndex(va("world/door_start_%d", s + 1));
		ent->locals.move_info.sound_middle = gi.SoundIndex(va("world/door_middle_%d", s + 1));
		ent->locals.move_info.sound_end = gi.SoundIndex(va("world/door_end_%d", s + 1));
	}

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->locals.team) {
		ent->locals.team_master = ent;
	}

	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	if (ent->locals.health || ent->locals.target_name) {
		ent->locals.Think = G_func_door_CalculateMove;
	} else {
		ent->locals.Think = G_func_door_CreateTrigger;
	}
}

/*QUAKED func_door_rotating (0 .5 .8) ? start_open reverse toggle x_axis y_axis
 A door which rotates about an origin on its Z axis. By default, doors open when a player walks close to them.

 -------- Keys --------
 message : An optional string printed when the door is first touched.
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 rotation : The rotation the door will open, in degrees (default 90).
 wait : wait before returning (3 default, -1 = never return).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_open : The door to moves to its destination when spawned, and operates in reverse.
 toggle : The door will wait in both the start and end states for a trigger event.
 reverse : The door will rotate in the opposite (negative) direction along its axis.
 x_axis : The door will rotate along its X axis.
 y_axis : The door will rotate along its Y axis.
 */
void G_func_door_rotating(g_entity_t *ent) {
	ent->s.angles = Vec3_Zero();

	// set the axis of rotation
	ent->locals.move_dir = Vec3_Zero();
	if (ent->locals.spawn_flags & DOOR_ROTATING_X_AXIS) {
		ent->locals.move_dir.z = 1.0;
	} else if (ent->locals.spawn_flags & DOOR_ROTATING_Y_AXIS) {
		ent->locals.move_dir.x = 1.0;
	} else {
		ent->locals.move_dir.y = 1.0;
	}

	// check for reverse rotation
	if (ent->locals.spawn_flags & DOOR_ROTATING_REVERSE) {
		ent->locals.move_dir = Vec3_Negate(ent->locals.move_dir);
	}

	const float rotation = gi.EntityValue(ent->def, "rotation")->value ?: 90.f;

	ent->locals.pos1 = ent->s.angles;
	ent->locals.pos2 = Vec3_Fmaf(ent->s.angles, rotation, ent->locals.move_dir);
	ent->locals.move_info.distance = rotation;

	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_door_Blocked;
	ent->locals.Use = G_func_door_Use;

	if (!ent->locals.speed) {
		ent->locals.speed = 100.0;
	}
	if (!ent->locals.accel) {
		ent->locals.accel = ent->locals.speed;
	}
	if (!ent->locals.decel) {
		ent->locals.decel = ent->locals.speed;
	}

	if (!ent->locals.wait) {
		ent->locals.wait = 3.0;
	}
	if (!ent->locals.damage) {
		ent->locals.damage = 2;
	}

	// if it starts open, switch the positions
	if (ent->locals.spawn_flags & DOOR_START_OPEN) {
		ent->s.angles = ent->locals.pos2;
		ent->locals.pos2 = ent->locals.pos1;
		ent->locals.pos1 = ent->s.angles;
		ent->locals.move_dir = Vec3_Negate(ent->locals.move_dir);
	}

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_Die;
		ent->locals.max_health = ent->locals.health;
	}

	if (ent->locals.target_name && ent->locals.message) {
		ent->locals.Touch = G_func_door_Touch;
	}

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;
	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;

	ent->locals.move_info.start_origin = ent->s.origin;
	ent->locals.move_info.start_angles = ent->locals.pos1;
	ent->locals.move_info.end_origin = ent->s.origin;
	ent->locals.move_info.end_angles = ent->locals.pos2;

	const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
	if (s != -1) {
		ent->locals.move_info.sound_middle = gi.SoundIndex(va("world/door_middle_%d", s + 1));
	}

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->locals.team) {
		ent->locals.team_master = ent;
	}

	gi.LinkEntity(ent);

	ent->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
	if (ent->locals.health || ent->locals.target_name) {
		ent->locals.Think = G_func_door_CalculateMove;
	} else {
		ent->locals.Think = G_func_door_CreateTrigger;
	}
}

#define SECRET_ALWAYS_SHOOT		1
#define SECRET_FIRST_LEFT		2
#define SECRET_FIRST_DOWN		4

static void G_func_door_secret_Move1(g_entity_t *self);
static void G_func_door_secret_Move2(g_entity_t *self);
static void G_func_door_secret_Move3(g_entity_t *self);
static void G_func_door_secret_Move4(g_entity_t *self);
static void G_func_door_secret_Move5(g_entity_t *self);
static void G_func_door_secret_Move6(g_entity_t *self);
static void G_func_door_secret_Done(g_entity_t *self);

/**
 * @brief
 */
static void G_func_door_secret_Use(g_entity_t *self, g_entity_t *other,
                                   g_entity_t *activator) {

	// make sure we're not already moving
	if (!Vec3_Equal(self->s.origin, Vec3_Zero())) {
		return;
	}

	G_MoveInfo_Linear_Init(self, self->locals.pos1, G_func_door_secret_Move1);

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_start,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = self->locals.move_info.sound_middle;
	}
}

/**
 * @brief
 */
static void G_func_door_secret_Move1(g_entity_t *self) {

	self->locals.next_think = g_level.time + 1000;
	self->locals.Think = G_func_door_secret_Move2;
}

/**
 * @brief
 */
static void G_func_door_secret_Move2(g_entity_t *self) {

	G_MoveInfo_Linear_Init(self, self->locals.pos2, G_func_door_secret_Move3);
}

/**
 * @brief
 */
static void G_func_door_secret_Move3(g_entity_t *self) {

	if (self->locals.wait == -1.0) {
		return;
	}

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_end) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_end,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = 0;
	}

	self->locals.next_think = g_level.time + self->locals.wait * 1000;
	self->locals.Think = G_func_door_secret_Move4;
}

/**
 * @brief
 */
static void G_func_door_secret_Move4(g_entity_t *self) {

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_start,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = self->locals.move_info.sound_middle;
	}

	G_MoveInfo_Linear_Init(self, self->locals.pos1, G_func_door_secret_Move5);
}

/**
 * @brief
 */
static void G_func_door_secret_Move5(g_entity_t *self) {

	self->locals.next_think = g_level.time + 1000;
	self->locals.Think = G_func_door_secret_Move6;
}

/**
 * @brief
 */
static void G_func_door_secret_Move6(g_entity_t *self) {

	G_MoveInfo_Linear_Init(self, Vec3_Zero(), G_func_door_secret_Done);
}

/**
 * @brief
 */
static void G_func_door_secret_Done(g_entity_t *self) {

	if (!(self->locals.target_name) || (self->locals.spawn_flags & SECRET_ALWAYS_SHOOT)) {
		self->locals.dead = true;
		self->locals.take_damage = true;
	}

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {

		if (self->locals.move_info.sound_end) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_end,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}

		self->s.sound = 0;
	}
}

/**
 * @brief
 */
static void G_func_door_secret_Blocked(g_entity_t *self, g_entity_t *other) {

	if (!other->client) {
		return;
	}

	if (g_level.time < self->locals.touch_time) {
		return;
	}

	self->locals.touch_time = g_level.time + 500;

	G_Damage(other, self, self, Vec3_Zero(), other->s.origin, Vec3_Zero(), self->locals.damage, 1, 0, MOD_CRUSH);

	self->locals.next_think = g_level.time + 1;
}

/**
 * @brief
 */
static void G_func_door_secret_Die(g_entity_t *self, g_entity_t *attacker, uint32_t mod) {

	self->locals.take_damage = false;
	G_func_door_secret_Use(self, attacker, attacker);
}

/*QUAKED func_door_secret (0 .5 .8) ? always_shoot 1st_left 1st_down
 A secret door which opens when shot, or when targeted. The door first slides
 back, and then to the side.

 -------- Keys --------
 angle : The angle at which the door opens.
 message : An optional string printed when the door is first touched.
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 sounds : The sound set for the door (0 default, 1 stone, -1 silent).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
always_shoot : The door will open when shot, even if it is targeted.
first_left : The door will first slide to the left.
first_down : The door will first slide down.
*/
void G_func_door_secret(g_entity_t *ent) {
	vec3_t forward, right, up;

	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_door_secret_Blocked;
	ent->locals.Use = G_func_door_secret_Use;

	if (!(ent->locals.target_name) || (ent->locals.spawn_flags & SECRET_ALWAYS_SHOOT)) {
		ent->locals.dead = true;
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_secret_Die;
	}

	if (!ent->locals.damage) {
		ent->locals.damage = 2;
	}

	if (!ent->locals.wait) {
		ent->locals.wait = 5.0;
	}

	if (!ent->locals.speed) {
		ent->locals.speed = 50.0;
	}

	ent->locals.move_info.speed = ent->locals.speed;

	const int32_t s = gi.EntityValue(ent->def, "sounds")->integer;
	if (s != -1) {
		ent->locals.move_info.sound_start = gi.SoundIndex(va("world/door_start_%d", s + 1));
		ent->locals.move_info.sound_middle = gi.SoundIndex(va("world/door_middle_%d", s + 1));
		ent->locals.move_info.sound_end = gi.SoundIndex(va("world/door_end_%d", s + 1));
	}

	// calculate positions
	Vec3_Vectors(ent->s.angles, &forward, &right, &up);
	ent->s.angles = Vec3_Zero();

	const float side = 1.0 - (ent->locals.spawn_flags & SECRET_FIRST_LEFT);

	const float length = fabsf(Vec3_Dot(forward, ent->size));

	float width;
	if (ent->locals.spawn_flags & SECRET_FIRST_DOWN) {
		width = fabsf(Vec3_Dot(up, ent->size));
	} else {
		width = fabsf(Vec3_Dot(right, ent->size));
	}

	if (ent->locals.spawn_flags & SECRET_FIRST_DOWN) {
		ent->locals.pos1 = Vec3_Fmaf(ent->s.origin, -1.0 * width, up);
	} else {
		ent->locals.pos1 = Vec3_Fmaf(ent->s.origin, side * width, right);
	}

	ent->locals.pos2 = Vec3_Fmaf(ent->locals.pos1, length, forward);

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_Die;
		ent->locals.max_health = ent->locals.health;
	} else if (ent->locals.target_name && ent->locals.message) {
		gi.SoundIndex("misc/chat");
		ent->locals.Touch = G_func_door_Touch;
	}

	gi.LinkEntity(ent);
}

/**
 * @brief
 */
static void G_func_wall_Use(g_entity_t *self, g_entity_t *other,
                            g_entity_t *activator) {

	if (self->solid == SOLID_NOT) {
		self->solid = SOLID_BSP;
		self->sv_flags &= ~SVF_NO_CLIENT;
		G_KillBox(self);
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}
	gi.LinkEntity(self);

	if (!(self->locals.spawn_flags & 2)) {
		self->locals.Use = NULL;
	}
}

#define WALL_TRIGGER   0x1
#define WALL_TOGGLE    0x2
#define WALL_START_ON  0x4

#define WALL_SPAWN_FLAGS (WALL_TRIGGER | WALL_TOGGLE | WALL_START_ON)

/*QUAKED func_wall (0 .5 .8) ? triggered toggle start_on
 A solid that may spawn into existence via trigger.

 -------- Keys --------
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 triggered : The wall is inhibited until triggered, at which point it appears and kills everything in its way.
 toggle : The wall may be triggered off and on.
 start_on : The wall will initially be present, but can be toggled off.
 */
void G_func_wall(g_entity_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;
	gi.SetModel(self, self->model);

	if ((self->locals.spawn_flags & WALL_SPAWN_FLAGS) == 0) {
		self->solid = SOLID_BSP;
		gi.LinkEntity(self);
		return;
	}

	// it must be triggered to use start_on or toggle
	self->locals.spawn_flags |= WALL_TRIGGER;

	// and if it's start_on, it must be toggled
	if (self->locals.spawn_flags & WALL_START_ON) {
		self->locals.spawn_flags |= WALL_TOGGLE;
	}

	self->locals.Use = G_func_wall_Use;

	if (self->locals.spawn_flags & WALL_START_ON) {
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}

	gi.LinkEntity(self);
}

/*QUAKED func_water(0 .5 .8) ? start_open
 A movable water brush, which must be targeted to operate.

 -------- Keys --------
 angle : Determines the opening direction (up = -1, down = -2)
 speed : The speed at which the water moves (default 25).
 wait : Delay in seconds before returning to the initial position (default -1).
 lip : Lip remaining at end of move (default 0 units).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags ---------
 start_open : If set, causes the water to move to its destination when spawned and operate in reverse.
 */
void G_func_water(g_entity_t *self) {
	vec3_t abs_move_dir;

	G_SetMoveDir(self);
	self->locals.move_type = MOVE_TYPE_PUSH;
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	// calculate second position
	self->locals.pos1 = self->s.origin;
	abs_move_dir = Vec3_Fabsf(self->locals.move_dir);
	self->locals.move_info.distance = abs_move_dir.x * self->size.x +
									  abs_move_dir.y * self->size.y +
									  abs_move_dir.z * self->size.z - self->locals.lip;

	self->locals.pos2 = Vec3_Fmaf(self->locals.pos1, self->locals.move_info.distance, self->locals.move_dir);

	// if it starts open, switch the positions
	if (self->locals.spawn_flags & DOOR_START_OPEN) {
		self->s.origin = self->locals.pos2;
		self->locals.pos2 = self->locals.pos1;
		self->locals.pos1 = self->s.origin;
	}

	self->locals.move_info.start_origin = self->locals.pos1;
	self->locals.move_info.start_angles = self->s.angles;
	self->locals.move_info.end_origin = self->locals.pos2;
	self->locals.move_info.end_angles = self->s.angles;

	self->locals.move_info.state = MOVE_STATE_BOTTOM;

	if (!self->locals.speed) {
		self->locals.speed = 25.0;
	}

	self->locals.move_info.speed = self->locals.speed;

	if (!self->locals.wait) {
		self->locals.wait = -1;
	}

	self->locals.move_info.wait = self->locals.wait;

	self->locals.Use = G_func_door_Use;

	if (self->locals.wait == -1) {
		self->locals.spawn_flags |= DOOR_TOGGLE;
	}

	gi.LinkEntity(self);
}

#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

static void G_func_train_Next(g_entity_t *self);

/**
 * @brief
 */
static void G_func_train_Wait(g_entity_t *self) {

	if (self->locals.target_ent->locals.path_target) {
		g_entity_t *ent = self->locals.target_ent;
		const char *target = ent->locals.target;
		ent->locals.target = ent->locals.path_target;
		G_UseTargets(ent, self->locals.activator);
		ent->locals.target = target;

		// make sure we didn't get killed by a killtarget
		if (!self->in_use) {
			return;
		}
	}

	if (self->locals.move_info.wait) {
		if (self->locals.move_info.wait > 0) {
			self->locals.next_think = g_level.time + (self->locals.move_info.wait * 1000);
			self->locals.Think = G_func_train_Next;
		} else if (self->locals.spawn_flags & TRAIN_TOGGLE) {
			G_func_train_Next(self);
			self->locals.spawn_flags &= ~TRAIN_START_ON;
			self->locals.velocity = Vec3_Zero();
			self->locals.next_think = 0;
		}

		if (!(self->locals.flags & FL_TEAM_SLAVE)) {
			if (self->locals.move_info.sound_end) {
				G_MulticastSound(&(const g_play_sound_t) {
					.index = self->locals.move_info.sound_end,
					.entity = self,
					.atten = SOUND_ATTEN_SQUARE
				}, MULTICAST_PHS, NULL);
			}
			self->s.sound = 0;
		}
	} else {
		G_func_train_Next(self);
	}
}

/**
 * @brief
 */
static void G_func_train_Next(g_entity_t *self) {
	g_entity_t *ent;
	vec3_t dest;
	_Bool first;

	first = true;
again:
	if (!self->locals.target) {
		return;
	}

	ent = G_PickTarget(self->locals.target);
	if (!ent) {
		G_Debug("%s has invalid target %s\n", etos(self), self->locals.target);
		return;
	}

	self->locals.target = ent->locals.target;

	// check for a teleport path_corner
	if (ent->locals.spawn_flags & 1) {
		if (!first) {
			G_Debug("%s has teleport path_corner %s\n", etos(self), etos(ent));
			return;
		}
		first = false;
		self->s.origin = Vec3_Subtract(ent->s.origin, self->bounds.mins);
		self->s.event = EV_CLIENT_TELEPORT;
		gi.LinkEntity(self);
		goto again;
	}

	self->locals.move_info.wait = ent->locals.wait;
	self->locals.target_ent = ent;

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start) {
			G_MulticastSound(&(const g_play_sound_t) {
				.index = self->locals.move_info.sound_start,
				.entity = self,
				.atten = SOUND_ATTEN_SQUARE
			}, MULTICAST_PHS, NULL);
		}
		self->s.sound = self->locals.move_info.sound_middle;
	}

	dest = Vec3_Subtract(ent->s.origin, self->bounds.mins);
	self->locals.move_info.state = MOVE_STATE_TOP;
	self->locals.move_info.start_origin = self->s.origin;
	self->locals.move_info.end_origin = dest;
	G_MoveInfo_Linear_Init(self, dest, G_func_train_Wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/**
 * @brief
 */
static void G_func_train_Resume(g_entity_t *self) {
	g_entity_t *ent;
	vec3_t dest;

	ent = self->locals.target_ent;

	dest = Vec3_Subtract(ent->s.origin, self->bounds.mins);
	self->locals.move_info.state = MOVE_STATE_TOP;
	self->locals.move_info.start_origin = self->s.origin;
	self->locals.move_info.end_origin = dest;
	G_MoveInfo_Linear_Init(self, dest, G_func_train_Wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/**
 * @brief
 */
static void G_func_train_Find(g_entity_t *self) {
	g_entity_t *ent;

	if (!self->locals.target) {
		G_Debug("No target specified\n");
		return;
	}
	ent = G_PickTarget(self->locals.target);
	if (!ent) {
		G_Debug("Target \"%s\" not found\n", self->locals.target);
		return;
	}
	self->locals.target = ent->locals.target;

	self->s.origin = Vec3_Subtract(ent->s.origin, self->bounds.mins);
	gi.LinkEntity(self);

	// if not triggered, start immediately
	if (!self->locals.target_name) {
		self->locals.spawn_flags |= TRAIN_START_ON;
	}

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
		self->locals.Think = G_func_train_Next;
		self->locals.activator = self;
	}
}

/**
 * @brief
 */
static void G_func_train_Use(g_entity_t *self, g_entity_t *other,
                             g_entity_t *activator) {
	self->locals.activator = activator;

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		if (!(self->locals.spawn_flags & TRAIN_TOGGLE)) {
			return;
		}
		self->locals.spawn_flags &= ~TRAIN_START_ON;
		self->locals.velocity = Vec3_Zero();
		self->locals.next_think = 0;
	} else {
		if (self->locals.target_ent) {
			G_func_train_Resume(self);
		} else {
			G_func_train_Next(self);
		}
	}
}

/*QUAKED func_train (0 .5 .8) ? start_on toggle block_stops
 Trains are moving solids that players can ride along a series of path_corners. The origin of
 each corner specifies the lower bounding point of the train at that corner. If the train is
 the target of a button or trigger, it will not begin moving until activated.

 -------- Keys --------
 speed : The speed with which the train moves (default 100).
 dmg : The damage inflicted on players who block the train (default 2).
 sound : The looping sound to play while the train is in motion.
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : If set, the train will begin moving once spawned.
 toggle : If set, the train will start or stop each time it is activated.
 block_stops : When blocked, stop moving and inflict no damage.
 */
void G_func_train(g_entity_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;

	self->s.angles = Vec3_Zero();

	if (self->locals.spawn_flags & TRAIN_BLOCK_STOPS) {
		self->locals.damage = 0;
	} else {
		if (!self->locals.damage) {
			self->locals.damage = 100;
		}
	}
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	const char *sound = gi.EntityValue(self->def, "sound")->string;
	if (*sound) {
		self->locals.move_info.sound_middle = gi.SoundIndex(sound);
	}

	if (!self->locals.speed) {
		self->locals.speed = 100.0;
	}

	self->locals.move_info.speed = self->locals.speed;

	self->locals.Use = G_func_train_Use;
	self->locals.Blocked = G_MoveType_Push_Blocked;

	gi.LinkEntity(self);

	if (self->locals.target) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
		self->locals.Think = G_func_train_Find;
	} else {
		G_Debug("No target: %s\n", vtos(self->s.origin));
	}
}

/**
 * @brief
 */
static void G_func_timer_Think(g_entity_t *self) {

	G_UseTargets(self, self->locals.activator);

	const uint32_t wait = self->locals.wait * 1000;
	const uint32_t rand = self->locals.random * 1000 * RandomRangef(-1.f, 1.f);

	self->locals.next_think = g_level.time + wait + rand;
}

/**
 * @brief
 */
static void G_func_timer_Use(g_entity_t *self, g_entity_t *other,
                             g_entity_t *activator) {
	self->locals.activator = activator;

	// if on, turn it off
	if (self->locals.next_think) {
		self->locals.next_think = 0;
		return;
	}

	// turn it on
	if (self->locals.delay) {
		self->locals.next_think = g_level.time + self->locals.delay * 1000;
	} else {
		G_func_timer_Think(self);
	}
}

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) start_on
 Time delay trigger that will continuously fire its targets after a preset time delay. The time delay can also be randomized. When triggered, the timer will toggle on/off.

 -------- Keys --------
 wait : Delay in seconds between each triggering of all targets (default 1).
 random : Random time variance in seconds added to "wait" delay (default 0).
 delay :  Additional delay before the first firing when start_on (default 0).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : If set, the timer will begin firing once spawned.
 */
void G_func_timer(g_entity_t *self) {

	if (!self->locals.wait) {
		self->locals.wait = 1.0;
	}

	self->locals.Use = G_func_timer_Use;
	self->locals.Think = G_func_timer_Think;

	if (self->locals.random >= self->locals.wait) {
		self->locals.random = self->locals.wait - QUETOO_TICK_SECONDS;
		G_Debug("random >= wait: %s\n", vtos(self->s.origin));
	}

	if (self->locals.spawn_flags & 1) {

		const uint32_t delay = self->locals.delay * 1000;
		const uint32_t wait = self->locals.wait * 1000;
		const uint32_t rand = self->locals.random * 1000 * RandomRangef(-1.f, 1.f);

		self->locals.next_think = g_level.time + delay + wait + rand;
		self->locals.activator = self;
	}

	self->sv_flags = SVF_NO_CLIENT;
}

/**
 * @brief
 */
static void G_func_conveyor_Use(g_entity_t *self, g_entity_t *other,
                                g_entity_t *activator) {
	if (self->locals.spawn_flags & 1) {
		self->locals.speed = 0;
		self->locals.spawn_flags &= ~1;
	} else {
		self->locals.speed = self->locals.count;
		self->locals.spawn_flags |= 1;
	}

	if (!(self->locals.spawn_flags & 2)) {
		self->locals.count = 0;
	}
}

/*QUAKED func_conveyor (0 .5 .8) ? start_on toggle
 Conveyors are stationary brushes that move what's on them. The brush should be have a surface with at least one current content enabled.

 -------- Keys --------
 speed : The speed at which objects on the conveyor are moved (default 100).
 targetname : The target name of this entity if it is to be triggered.

 -------- Spawn flags --------
 start_on : The conveyor will be active immediately.
 toggle : The conveyor is toggled each time it is used.
 */
void G_func_conveyor(g_entity_t *self) {
	if (!self->locals.speed) {
		self->locals.speed = 100;
	}

	if (!(self->locals.spawn_flags & 1)) {
		self->locals.count = self->locals.speed;
		self->locals.speed = 0;
	}

	self->locals.Use = G_func_conveyor_Use;

	gi.SetModel(self, self->model);
	self->solid = SOLID_BSP;
	gi.LinkEntity(self);
}
