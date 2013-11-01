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
static void G_func_areaportal_Use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	ent->locals.count ^= 1; // toggle state
	gi.SetAreaPortalState(ent->locals.area_portal, ent->locals.count);
}

/*QUAKED func_areaportal (0 0 0) ?
 A non-visible object indicating disjoint areas in the world that should be combined when this entity is used. These are typically enclosed inside door brushes to toggle visibility of the rooms they separate.
 -------- KEYS --------
 targetname : The target name of this entity if it is to be triggered.
 */
void G_func_areaportal(g_edict_t *ent) {
	ent->locals.Use = G_func_areaportal_Use;
	ent->locals.count = 0; // always start closed;
}

/*
 * @brief
 */
static void G_MoveInfo_Done(g_edict_t *ent) {
	VectorClear(ent->locals.velocity);
	ent->locals.move_info.Done(ent);
}

/*
 * @brief
 */
static void G_MoveInfo_End(g_edict_t *ent) {

	if (ent->locals.move_info.remaining_distance == 0) {
		G_MoveInfo_Done(ent);
		return;
	}

	VectorScale(ent->locals.move_info.dir, ent->locals.move_info.remaining_distance / gi.frame_seconds, ent->locals.velocity);

	ent->locals.Think = G_MoveInfo_Done;
	ent->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief Starts a move with constant velocity. The entity will think again when it
 * has reached its destination.
 */
static void G_MoveInfo_Constant(g_edict_t *ent) {
	vec_t frames;

	if ((ent->locals.move_info.speed * gi.frame_seconds)
			>= ent->locals.move_info.remaining_distance) {
		G_MoveInfo_End(ent);
		return;
	}

	VectorScale(ent->locals.move_info.dir, ent->locals.move_info.speed, ent->locals.velocity);
	frames = floor(
			(ent->locals.move_info.remaining_distance / ent->locals.move_info.speed)
					/ gi.frame_seconds);
	ent->locals.move_info.remaining_distance -= frames * ent->locals.move_info.speed
			* gi.frame_seconds;
	ent->locals.next_think = g_level.time + (frames * gi.frame_millis);
	ent->locals.Think = G_MoveInfo_End;
}

#define AccelerationDistance(target, rate) (target * ((target / rate) + 1) / 2)

/*
 * @brief Updates the acceleration parameters for the specified move. This determines
 * whether we should accelerate or decelerate based on the distance remaining.
 */
static void G_MoveInfo_UpdateAcceleration(g_move_info_t *move_info) {
	vec_t accel_dist;
	vec_t decel_dist;

	move_info->move_speed = move_info->speed;

	if (move_info->remaining_distance < move_info->accel) {
		move_info->current_speed = move_info->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance(move_info->speed, move_info->accel);
	decel_dist = AccelerationDistance(move_info->speed, move_info->decel);

	if ((move_info->remaining_distance - accel_dist - decel_dist) < 0) {
		vec_t v;

		v = (move_info->accel + move_info->decel) / (move_info->accel * move_info->decel);
		move_info->move_speed = (-2 + sqrt(4 - 4 * v * (-2 * move_info->remaining_distance))) / (2
				* v);
		decel_dist = AccelerationDistance(move_info->move_speed, move_info->decel);
	}

	move_info->decel_distance = decel_dist;
}

/*
 * @brief Applies any acceleration / deceleration based on the distance remaining.
 */
static void G_MoveInfo_Accelerate(g_move_info_t *move_info) {
	// are we decelerating?
	if (move_info->remaining_distance <= move_info->decel_distance) {
		if (move_info->remaining_distance < move_info->decel_distance) {
			if (move_info->next_speed) {
				move_info->current_speed = move_info->next_speed;
				move_info->next_speed = 0;
				return;
			}
			if (move_info->current_speed > move_info->decel)
				move_info->current_speed -= move_info->decel;
		}
		return;
	}

	// are we at full speed and need to start decelerating during this move?
	if (move_info->current_speed == move_info->move_speed)
		if ((move_info->remaining_distance - move_info->current_speed) < move_info->decel_distance) {
			vec_t p1_distance;
			vec_t p2_distance;
			vec_t distance;

			p1_distance = move_info->remaining_distance - move_info->decel_distance;
			p2_distance = move_info->move_speed * (1.0 - (p1_distance / move_info->move_speed));
			distance = p1_distance + p2_distance;
			move_info->current_speed = move_info->move_speed;
			move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance
					/ distance);
			return;
		}

	// are we accelerating?
	if (move_info->current_speed < move_info->speed) {
		vec_t old_speed;
		vec_t p1_distance;
		vec_t p1_speed;
		vec_t p2_distance;
		vec_t distance;

		old_speed = move_info->current_speed;

		// figure simple acceleration up to move_speed
		move_info->current_speed += move_info->accel;
		if (move_info->current_speed > move_info->speed)
			move_info->current_speed = move_info->speed;

		// are we accelerating throughout this entire move?
		if ((move_info->remaining_distance - move_info->current_speed) >= move_info->decel_distance)
			return;

		// during this move we will accelerate from current_speed to move_speed
		// and cross over the decel_distance; figure the average speed for the
		// entire move
		p1_distance = move_info->remaining_distance - move_info->decel_distance;
		p1_speed = (old_speed + move_info->move_speed) / 2.0;
		p2_distance = move_info->move_speed * (1.0 - (p1_distance / p1_speed));
		distance = p1_distance + p2_distance;
		move_info->current_speed = (p1_speed * (p1_distance / distance)) + (move_info->move_speed
				* (p2_distance / distance));
		move_info->next_speed = move_info->move_speed - move_info->decel * (p2_distance / distance);
		return;
	}
}

/*
 * @brief Sets up a non-constant move, i.e. one that will accelerate near the beginning
 * and decelerate towards the end.
 */
static void G_MoveInfo_Accelerative(g_edict_t *ent) {

	ent->locals.move_info.remaining_distance -= ent->locals.move_info.current_speed;

	if (ent->locals.move_info.current_speed == 0) // starting or blocked
		G_MoveInfo_UpdateAcceleration(&ent->locals.move_info);

	G_MoveInfo_Accelerate(&ent->locals.move_info);

	// will the entire move complete on next frame?
	if (ent->locals.move_info.remaining_distance <= ent->locals.move_info.current_speed) {
		G_MoveInfo_End(ent);
		return;
	}

	VectorScale(ent->locals.move_info.dir, ent->locals.move_info.current_speed * gi.frame_rate, ent->locals.velocity);
	ent->locals.next_think = g_level.time + gi.frame_millis;
	ent->locals.Think = G_MoveInfo_Accelerative;
}

/*
 * @brief Sets up movement for the specified entity. Both constant and accelerative
 * movements are initiated through this function.
 */
static void G_MoveInfo_Init(g_edict_t *ent, vec3_t dest, void(*Done)(g_edict_t*)) {

	VectorClear(ent->locals.velocity);

	VectorSubtract(dest, ent->s.origin, ent->locals.move_info.dir);
	ent->locals.move_info.remaining_distance = VectorNormalize(ent->locals.move_info.dir);

	ent->locals.move_info.Done = Done;

	if (ent->locals.move_info.speed == ent->locals.move_info.accel && ent->locals.move_info.speed
			== ent->locals.move_info.decel) { // constant
		if (g_level.current_entity == ((ent->locals.flags & FL_TEAM_SLAVE)
				? ent->locals.team_master : ent)) {
			G_MoveInfo_Constant(ent);
		} else {
			ent->locals.next_think = g_level.time + gi.frame_millis;
			ent->locals.Think = G_MoveInfo_Constant;
		}
	} else { // accelerative
		ent->locals.move_info.current_speed = 0;
		ent->locals.Think = G_MoveInfo_Accelerative;
		ent->locals.next_think = g_level.time + gi.frame_millis;
	}
}

/*
 * @brief
 */
static void G_MoveInfo_Angular_Done(g_edict_t *ent) {
	VectorClear(ent->locals.avelocity);
	ent->locals.move_info.Done(ent);
}

/*
 * @brief
 */
static void G_MoveInfo_Angular_Final(g_edict_t *ent) {
	vec3_t move;

	if (ent->locals.move_info.state == MOVE_STATE_GOING_UP)
		VectorSubtract(ent->locals.move_info.end_angles, ent->s.angles, move);
	else
		VectorSubtract(ent->locals.move_info.start_angles, ent->s.angles, move);

	if (VectorCompare(move, vec3_origin)) {
		G_MoveInfo_Angular_Done(ent);
		return;
	}

	VectorScale(move, 1.0 / gi.frame_seconds, ent->locals.avelocity);

	ent->locals.Think = G_MoveInfo_Angular_Done;
	ent->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief
 */
static void G_MoveInfo_Angular_Begin(g_edict_t *ent) {
	vec3_t move;

	// set move to the vector needed to move
	if (ent->locals.move_info.state == MOVE_STATE_GOING_UP)
		VectorSubtract(ent->locals.move_info.end_angles, ent->s.angles, move);
	else
		VectorSubtract(ent->locals.move_info.start_angles, ent->s.angles, move);

	// calculate length of vector
	const vec_t len = VectorLength(move);

	// divide by speed to get time to reach dest
	const vec_t time = len / ent->locals.move_info.speed;

	if (time < gi.frame_seconds) {
		G_MoveInfo_Angular_Final(ent);
		return;
	}

	const vec_t frames = floor(time / gi.frame_seconds);

	// scale the move vector by the time spent traveling to get velocity
	VectorScale(move, 1.0 / time, ent->locals.avelocity);

	// set next_think to trigger a think when dest is reached
	ent->locals.next_think = g_level.time + frames * gi.frame_millis;
	ent->locals.Think = G_MoveInfo_Angular_Final;
}

/*
 * @brief
 */
static void G_MoveInfo_Angular_Init(g_edict_t *ent, void(*Done)(g_edict_t *)) {

	VectorClear(ent->locals.avelocity);

	ent->locals.move_info.Done = Done;
	if (g_level.current_entity == ((ent->locals.flags & FL_TEAM_SLAVE) ? ent->locals.team_master
			: ent)) {
		G_MoveInfo_Angular_Begin(ent);
	} else {
		ent->locals.next_think = g_level.time + gi.frame_millis;
		ent->locals.Think = G_MoveInfo_Angular_Begin;
	}
}

#define PLAT_LOW_TRIGGER	1

static void G_func_plat_GoDown(g_edict_t *ent);

/*
 * @brief
 */
static void G_func_plat_Up(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_end)
			gi.Sound(ent, ent->locals.move_info.sound_end, ATTEN_IDLE);
		ent->s.sound = 0;
	}
	ent->locals.move_info.state = MOVE_STATE_TOP;

	ent->locals.Think = G_func_plat_GoDown;
	ent->locals.next_think = g_level.time + 3000;
}

/*
 * @brief
 */
static void G_func_plat_Down(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_end)
			gi.Sound(ent, ent->locals.move_info.sound_end, ATTEN_IDLE);
		ent->s.sound = 0;
	}
	ent->locals.move_info.state = MOVE_STATE_BOTTOM;
}

/*
 * @brief
 */
static void G_func_plat_GoDown(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_start)
			gi.Sound(ent, ent->locals.move_info.sound_start, ATTEN_IDLE);
		ent->s.sound = ent->locals.move_info.sound_middle;
	}
	ent->locals.move_info.state = MOVE_STATE_GOING_DOWN;
	G_MoveInfo_Init(ent, ent->locals.move_info.end_origin, G_func_plat_Down);
}

/*
 * @brief
 */
static void G_func_plat_GoUp(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_start)
			gi.Sound(ent, ent->locals.move_info.sound_start, ATTEN_IDLE);
		ent->s.sound = ent->locals.move_info.sound_middle;
	}
	ent->locals.move_info.state = MOVE_STATE_GOING_UP;
	G_MoveInfo_Init(ent, ent->locals.move_info.start_origin, G_func_plat_Up);
}

/*
 * @brief
 */
static void G_func_plat_Blocked(g_edict_t *self, g_edict_t *other) {

	if (!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);

	if (self->locals.move_info.state == MOVE_STATE_GOING_UP)
		G_func_plat_GoDown(self);
	else if (self->locals.move_info.state == MOVE_STATE_GOING_DOWN)
		G_func_plat_GoUp(self);
}

/*
 * @brief
 */
static void G_func_plat_Use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (ent->locals.Think)
		return; // already down

	G_func_plat_GoDown(ent);
}

/*
 * @brief
 */
static void G_func_plat_Touch(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (other->locals.health <= 0)
		return;

	ent = ent->locals.enemy; // now point at the plat, not the trigger

	if (ent->locals.move_info.state == MOVE_STATE_BOTTOM)
		G_func_plat_GoUp(ent);
	else if (ent->locals.move_info.state == MOVE_STATE_TOP)
		ent->locals.next_think = g_level.time + 1000; // the player is still on the plat, so delay going down
}

/*
 * @brief
 */
static void G_func_plat_CreateTrigger(g_edict_t *ent) {
	g_edict_t *trigger;
	vec3_t tmin, tmax;

	// middle trigger
	trigger = G_Spawn(__func__);
	trigger->locals.Touch = G_func_plat_Touch;
	trigger->locals.move_type = MOVE_TYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->locals.enemy = ent;

	tmin[0] = ent->mins[0] + 25;
	tmin[1] = ent->mins[1] + 25;
	tmin[2] = ent->mins[2];

	tmax[0] = ent->maxs[0] - 25;
	tmax[1] = ent->maxs[1] - 25;
	tmax[2] = ent->maxs[2] + 8;

	tmin[2] = tmax[2] - (ent->locals.pos1[2] - ent->locals.pos2[2] + g_game.spawn.lip);

	if (ent->locals.spawn_flags & PLAT_LOW_TRIGGER)
		tmax[2] = tmin[2] + 8;

	if (tmax[0] - tmin[0] <= 0) {
		tmin[0] = (ent->mins[0] + ent->maxs[0]) * 0.5;
		tmax[0] = tmin[0] + 1;
	}
	if (tmax[1] - tmin[1] <= 0) {
		tmin[1] = (ent->mins[1] + ent->maxs[1]) * 0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy(tmin, trigger->mins);
	VectorCopy(tmax, trigger->maxs);

	gi.LinkEdict(trigger);
}

/*QUAKED func_plat (0 .5 .8) LOW_TRIGGER
 Rising platform the player can ride to reach higher places. Platforms must be placed in the raised position, so they will operate and be lit correctly, but they spawn in the lowered position. If the platform is the target of a trigger or button, it will start out disabled and in the extended position.
 -------- KEYS --------
 speed : The speed with which the platform moves (default 200).
 accel : The platform acceleration (default 500).
 lip : The lip remaining at end of move (default 8 units).
 height : If set, this will determine the extent of the platform's movement, rather than implicitly using the platform's height.
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 LOW_TRIGGER : If set, the touch field for this platform will only exist at the lower position.
 */
void G_func_plat(g_edict_t *ent) {

	VectorClear(ent->s.angles);
	ent->solid = SOLID_BSP;
	ent->locals.move_type = MOVE_TYPE_PUSH;

	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_plat_Blocked;

	if (!ent->locals.speed)
		ent->locals.speed = 200;
	ent->locals.speed *= 0.5;

	if (!ent->locals.accel)
		ent->locals.accel = 50;
	ent->locals.accel *= 0.1;

	if (!ent->locals.decel)
		ent->locals.decel = 50;
	ent->locals.decel *= 0.1;

	// normalize move_info based on server rate, stock q2 rate was 10hz
	const vec_t v = 100.0 / (gi.frame_rate * gi.frame_rate);
	ent->locals.speed *= v;
	ent->locals.accel *= v;
	ent->locals.decel *= v;

	if (!ent->locals.dmg)
		ent->locals.dmg = 2;

	if (!g_game.spawn.lip)
		g_game.spawn.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	VectorCopy(ent->s.origin, ent->locals.pos1);
	VectorCopy(ent->s.origin, ent->locals.pos2);

	if (g_game.spawn.height) // use the specified height
		ent->locals.pos2[2] -= g_game.spawn.height;
	else
		// or derive it from the model height
		ent->locals.pos2[2] -= (ent->maxs[2] - ent->mins[2]) - g_game.spawn.lip;

	ent->locals.Use = G_func_plat_Use;

	G_func_plat_CreateTrigger(ent); // the "start moving" trigger

	if (ent->locals.target_name) {
		ent->locals.move_info.state = MOVE_STATE_GOING_UP;
	} else {
		VectorCopy(ent->locals.pos2, ent->s.origin);
		gi.LinkEdict(ent);
		ent->locals.move_info.state = MOVE_STATE_BOTTOM;
	}

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.start_angles);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.end_angles);

	ent->locals.move_info.sound_start = gi.SoundIndex("world/plat_start");
	ent->locals.move_info.sound_middle = gi.SoundIndex("world/plat_mid");
	ent->locals.move_info.sound_end = gi.SoundIndex("world/plat_end");
}

/*
 * @brief
 */
static void G_func_rotating_Blocked(g_edict_t *self, g_edict_t *other) {
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);
}

/*
 * @brief
 */
static void G_func_rotating_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!VectorCompare(self->locals.avelocity, vec3_origin)) {
		G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1,
				0, MOD_CRUSH);
	}
}

/*
 * @brief
 */
static void G_func_rotating_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (!VectorCompare(self->locals.avelocity, vec3_origin)) {
		self->s.sound = 0;
		VectorClear(self->locals.avelocity);
		self->locals.Touch = NULL;
	} else {
		self->s.sound = self->locals.move_info.sound_middle;
		VectorScale(self->locals.move_dir, self->locals.speed, self->locals.avelocity);
		if (self->locals.spawn_flags & 16)
			self->locals.Touch = G_func_rotating_Touch;
	}
}

/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP
 Solid entity that rotates continuously. Rotates on the Z axis by default and requires an origin brush. It will always start on in the game and is not targetable.
 -------- KEYS --------
 speed : The speed at which the entity rotates (default 100).
 dmg : The damage to inflict to players who touch or block the entity (default 2).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_ON : The entity will spawn rotating.
 REVERSE : Will cause the entity to rotate in the opposite direction.
 X_AXIS : Rotate on the X axis.
 Y AXIS : Rotate on the Y axis.
 TOUCH_PAIN : If set, any interaction with the entity will inflict damage to the player.
 STOP : If set and the entity is blocked, the entity will stop rotating.
 */
void G_func_rotating(g_edict_t *ent) {

	ent->solid = SOLID_BSP;

	if (ent->locals.spawn_flags & 32)
		ent->locals.move_type = MOVE_TYPE_STOP;
	else
		ent->locals.move_type = MOVE_TYPE_PUSH;

	// set the axis of rotation
	VectorClear(ent->locals.move_dir);
	if (ent->locals.spawn_flags & 4)
		ent->locals.move_dir[2] = 1.0;
	else if (ent->locals.spawn_flags & 8)
		ent->locals.move_dir[0] = 1.0;
	else
		// Z_AXIS
		ent->locals.move_dir[1] = 1.0;

	// check for reverse rotation
	if (ent->locals.spawn_flags & 2)
		VectorNegate(ent->locals.move_dir, ent->locals.move_dir);

	if (!ent->locals.speed)
		ent->locals.speed = 100;

	if (!ent->locals.dmg)
		ent->locals.dmg = 2;

	ent->locals.Use = G_func_rotating_Use;
	if (ent->locals.dmg)
		ent->locals.Blocked = G_func_rotating_Blocked;

	if (ent->locals.spawn_flags & 1)
		ent->locals.Use(ent, NULL, NULL);

	gi.SetModel(ent, ent->model);
	gi.LinkEdict(ent);
}

/*
 * @brief
 */
static void G_func_button_Done(g_edict_t *self) {
	self->locals.move_info.state = MOVE_STATE_BOTTOM;
}

/*
 * @brief
 */
static void G_func_button_Reset(g_edict_t *self) {

	self->locals.move_info.state = MOVE_STATE_GOING_DOWN;

	G_MoveInfo_Init(self, self->locals.move_info.start_origin, G_func_button_Done);

	if (self->locals.health)
		self->locals.take_damage = true;
}

/*
 * @brief
 */
static void G_func_button_Wait(g_edict_t *self) {

	self->locals.move_info.state = MOVE_STATE_TOP;

	G_UseTargets(self, self->locals.activator);

	if (self->locals.move_info.wait >= 0) {
		self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
		self->locals.Think = G_func_button_Reset;
	}
}

/*
 * @brief
 */
static void G_func_button_Activate(g_edict_t *self) {

	if (self->locals.move_info.state == MOVE_STATE_GOING_UP || self->locals.move_info.state
			== MOVE_STATE_TOP)
		return;

	self->locals.move_info.state = MOVE_STATE_GOING_UP;

	if (self->locals.move_info.sound_start && !(self->locals.flags & FL_TEAM_SLAVE))
		gi.Sound(self, self->locals.move_info.sound_start, ATTEN_IDLE);

	G_MoveInfo_Init(self, self->locals.move_info.end_origin, G_func_button_Wait);
}

/*
 * @brief
 */
static void G_func_button_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;
	G_func_button_Activate(self);
}

/*
 * @brief
 */
static void G_func_button_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (other->locals.health <= 0)
		return;

	self->locals.activator = other;
	G_func_button_Activate(self);
}

static void G_func_button_Die(g_edict_t *self, g_edict_t *inflictor __attribute__((unused)), g_edict_t *attacker,
		int16_t damage __attribute__((unused)), const vec3_t pos __attribute__((unused))) {
	self->locals.activator = attacker;
	self->locals.health = self->locals.max_health;
	self->locals.take_damage = false;
	G_func_button_Activate(self);
}

/*QUAKED func_button (0 .5 .8) ?
 When a button is touched by a player, it moves in the direction set by the "angle" key, triggers all its targets, stays pressed by the amount of time set by the "wait" key, then returns to its original position where it can be operated again.
 -------- KEYS --------
 angle : Determines the direction in which the button will move (up = -1, down = -2).
 target : All entities with a matching target name will be triggered.
 speed : Speed of the button's displacement (default 40).
 wait : Number of seconds the button stays pressed (default 1, -1 = indefinitely).
 lip : Lip remaining at end of move (default 4 units).
 health : If set, the button must be killed instead of touched to use.
 targetname : The target name of this entity if it is to be triggered.
 */
void G_func_button(g_edict_t *ent) {
	vec3_t abs_move_dir;
	vec_t dist;

	G_SetMoveDir(ent->s.angles, ent->locals.move_dir);
	ent->locals.move_type = MOVE_TYPE_STOP;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	if (ent->locals.sounds != 1)
		ent->locals.move_info.sound_start = gi.SoundIndex("world/switch");

	if (!ent->locals.speed)
		ent->locals.speed = 40;
	if (!ent->locals.accel)
		ent->locals.accel = ent->locals.speed;
	if (!ent->locals.decel)
		ent->locals.decel = ent->locals.speed;

	if (!ent->locals.wait)
		ent->locals.wait = 3;
	if (!g_game.spawn.lip)
		g_game.spawn.lip = 4;

	VectorCopy(ent->s.origin, ent->locals.pos1);
	abs_move_dir[0] = fabsf(ent->locals.move_dir[0]);
	abs_move_dir[1] = fabsf(ent->locals.move_dir[1]);
	abs_move_dir[2] = fabsf(ent->locals.move_dir[2]);
	dist = abs_move_dir[0] * ent->size[0] + abs_move_dir[1] * ent->size[1] + abs_move_dir[2]
			* ent->size[2] - g_game.spawn.lip;
	VectorMA(ent->locals.pos1, dist, ent->locals.move_dir, ent->locals.pos2);

	ent->locals.Use = G_func_button_Use;

	if (ent->locals.health) {
		ent->locals.max_health = ent->locals.health;
		ent->locals.Die = G_func_button_Die;
		ent->locals.take_damage = true;
	} else if (!ent->locals.target_name)
		ent->locals.Touch = G_func_button_Touch;

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.start_angles);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.end_angles);

	gi.LinkEdict(ent);
}

#define DOOR_START_OPEN		1
#define DOOR_TOGGLE			2
#define DOOR_REVERSE		4
#define DOOR_X_AXIS			8
#define DOOR_Y_AXIS			16

/*
 * @brief
 */
static void G_func_door_UseAreaPortals(g_edict_t *self, _Bool open) {
	g_edict_t *t = NULL;

	if (!self->locals.target)
		return;

	while ((t = G_Find(t, LOFS(target_name), self->locals.target))) {
		if (g_ascii_strcasecmp(t->class_name, "func_areaportal") == 0) {
			gi.SetAreaPortalState(t->locals.area_portal, open);
		}
	}
}

static void G_func_door_GoDown(g_edict_t *self);

/*
 * @brief
 */
static void G_func_door_Up(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_end)
			gi.Sound(self, self->locals.move_info.sound_end, ATTEN_IDLE);
		self->s.sound = 0;
	}
	self->locals.move_info.state = MOVE_STATE_TOP;

	if (self->locals.spawn_flags & DOOR_TOGGLE)
		return;

	if (self->locals.move_info.wait >= 0) {
		self->locals.Think = G_func_door_GoDown;
		self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
	}
}

/*
 * @brief
 */
static void G_func_door_Down(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_end)
			gi.Sound(self, self->locals.move_info.sound_end, ATTEN_IDLE);
		self->s.sound = 0;
	}
	self->locals.move_info.state = MOVE_STATE_BOTTOM;
	G_func_door_UseAreaPortals(self, false);
}

/*
 * @brief
 */
static void G_func_door_GoDown(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTEN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}
	if (self->locals.max_health) {
		self->locals.take_damage = true;
		self->locals.health = self->locals.max_health;
	}

	self->locals.move_info.state = MOVE_STATE_GOING_DOWN;
	if (!g_strcmp0(self->class_name, "func_door")) {
		G_MoveInfo_Init(self, self->locals.move_info.start_origin, G_func_door_Down);
	} else { // rotating
		G_MoveInfo_Angular_Init(self, G_func_door_Down);
	}
}

/*
 * @brief
 */
static void G_func_door_GoUp(g_edict_t *self, g_edict_t *activator) {
	if (self->locals.move_info.state == MOVE_STATE_GOING_UP)
		return; // already going up

	if (self->locals.move_info.state == MOVE_STATE_TOP) { // reset top wait time
		if (self->locals.move_info.wait >= 0)
			self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
		return;
	}

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTEN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}
	self->locals.move_info.state = MOVE_STATE_GOING_UP;
	if (!g_strcmp0(self->class_name, "func_door")) {
		G_MoveInfo_Init(self, self->locals.move_info.end_origin, G_func_door_Up);
	} else { // rotating
		G_MoveInfo_Angular_Init(self, G_func_door_Up);
	}

	G_UseTargets(self, activator);
	G_func_door_UseAreaPortals(self, true);
}

/*
 * @brief
 */
static void G_func_door_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	g_edict_t *ent;

	if (self->locals.flags & FL_TEAM_SLAVE)
		return;

	if (self->locals.spawn_flags & DOOR_TOGGLE) {
		if (self->locals.move_info.state == MOVE_STATE_GOING_UP || self->locals.move_info.state
				== MOVE_STATE_TOP) {
			// trigger all paired doors
			for (ent = self; ent; ent = ent->locals.team_chain) {
				ent->locals.message = NULL;
				ent->locals.Touch = NULL;
				G_func_door_GoDown(ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for (ent = self; ent; ent = ent->locals.team_chain) {
		ent->locals.message = NULL;
		ent->locals.Touch = NULL;
		G_func_door_GoUp(ent, activator);
	}
}

/*
 * @brief
 */
static void G_func_door_TouchTrigger(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {
	if (other->locals.health <= 0)
		return;

	if (!other->client)
		return;

	if (g_level.time < self->locals.touch_time)
		return;

	self->locals.touch_time = g_level.time + 1000;

	G_func_door_Use(self->owner, other, other);
}

/*
 * @brief
 */
static void G_func_door_CalculateMove(g_edict_t *self) {
	g_edict_t *ent;

	if (self->locals.flags & FL_TEAM_SLAVE)
		return; // only the team master does this

	// find the smallest distance any member of the team will be moving
	vec_t min = fabsf(self->locals.move_info.distance);
	for (ent = self->locals.team_chain; ent; ent = ent->locals.team_chain) {
		vec_t dist = fabsf(ent->locals.move_info.distance);
		if (dist < min)
			min = dist;
	}

	const vec_t time = min / self->locals.move_info.speed;

	// adjust speeds so they will all complete at the same time
	for (ent = self; ent; ent = ent->locals.team_chain) {
		const vec_t new_speed = fabsf(ent->locals.move_info.distance) / time;
		const vec_t ratio = new_speed / ent->locals.move_info.speed;
		if (ent->locals.move_info.accel == ent->locals.move_info.speed)
			ent->locals.move_info.accel = new_speed;
		else
			ent->locals.move_info.accel *= ratio;
		if (ent->locals.move_info.decel == ent->locals.move_info.speed)
			ent->locals.move_info.decel = new_speed;
		else
			ent->locals.move_info.decel *= ratio;
		ent->locals.move_info.speed = new_speed;
	}
}

/*
 * @brief
 */
static void G_func_door_CreateTrigger(g_edict_t *ent) {
	g_edict_t *trigger;
	vec3_t mins, maxs;

	if (ent->locals.flags & FL_TEAM_SLAVE)
		return; // only the team leader spawns a trigger

	VectorCopy(ent->abs_mins, mins);
	VectorCopy(ent->abs_maxs, maxs);

	for (trigger = ent->locals.team_chain; trigger; trigger = trigger->locals.team_chain) {
		AddPointToBounds(trigger->abs_mins, mins, maxs);
		AddPointToBounds(trigger->abs_maxs, mins, maxs);
	}

	// expand
	mins[0] -= 60;
	mins[1] -= 60;
	maxs[0] += 60;
	maxs[1] += 60;

	trigger = G_Spawn(__func__);
	VectorCopy(mins, trigger->mins);
	VectorCopy(maxs, trigger->maxs);
	trigger->owner = ent;
	trigger->solid = SOLID_TRIGGER;
	trigger->locals.move_type = MOVE_TYPE_NONE;
	trigger->locals.Touch = G_func_door_TouchTrigger;
	gi.LinkEdict(trigger);

	if (ent->locals.spawn_flags & DOOR_START_OPEN)
		G_func_door_UseAreaPortals(ent, true);

	G_func_door_CalculateMove(ent);
}

/*
 * @brief
 */
static void G_func_door_Blocked(g_edict_t *self, g_edict_t *other) {
	g_edict_t *ent;

	if (!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (self->locals.move_info.wait >= 0) {
		if (self->locals.move_info.state == MOVE_STATE_GOING_DOWN) {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain)
				G_func_door_GoUp(ent, ent->locals.activator);
		} else {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain)
				G_func_door_GoDown(ent);
		}
	}
}

/*
 * @brief
 */
static void G_func_door_Die(g_edict_t *self, g_edict_t *inflictor __attribute__((unused)), g_edict_t *attacker,
		int16_t damage __attribute__((unused)), const vec3_t pos __attribute__((unused))) {
	g_edict_t *ent;

	for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain) {
		ent->locals.health = ent->locals.max_health;
		ent->locals.take_damage = false;
	}

	G_func_door_Use(self->locals.team_master, attacker, attacker);
}

/*
 * @brief
 */
static void G_func_door_Touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {
	if (!other->client)
		return;

	if (g_level.time < self->locals.touch_time)
		return;

	self->locals.touch_time = g_level.time + 5000;

	if (self->locals.message && strlen(self->locals.message)) {
		gi.WriteByte(SV_CMD_CENTER_PRINT);
		gi.WriteString(self->locals.message);
		gi.Unicast(other, true);
	}

	gi.Sound(other, gi.SoundIndex("misc/chat"), ATTEN_NORM);
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN TOGGLE
 A sliding door. By default, doors open when a player walks close to them.
 -------- KEYS --------
 message : An optional string printed when the door is first touched.
 angle : Determines the opening direction of the door (up = -1, down = -2).
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_OPEN : The door to moves to its destination when spawned, and operates in reverse.
 TOGGLE : The door will wait in both the start and end states for a trigger event.
 */
void G_func_door(g_edict_t *ent) {
	vec3_t abs_move_dir;

	if (ent->locals.sounds != 1) {
		ent->locals.move_info.sound_start = gi.SoundIndex("world/door_start");
		ent->locals.move_info.sound_end = gi.SoundIndex("world/door_end");
	}

	G_SetMoveDir(ent->s.angles, ent->locals.move_dir);
	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_door_Blocked;
	ent->locals.Use = G_func_door_Use;

	if (!ent->locals.speed)
		ent->locals.speed = 100.0;

	ent->locals.speed *= 2.0;

	if (!ent->locals.accel)
		ent->locals.accel = ent->locals.speed;
	if (!ent->locals.decel)
		ent->locals.decel = ent->locals.speed;

	if (!ent->locals.wait)
		ent->locals.wait = 3.0;
	if (!g_game.spawn.lip)
		g_game.spawn.lip = 8.0;
	if (!ent->locals.dmg)
		ent->locals.dmg = 2;

	// calculate second position
	VectorCopy(ent->s.origin, ent->locals.pos1);
	abs_move_dir[0] = fabsf(ent->locals.move_dir[0]);
	abs_move_dir[1] = fabsf(ent->locals.move_dir[1]);
	abs_move_dir[2] = fabsf(ent->locals.move_dir[2]);
	ent->locals.move_info.distance = abs_move_dir[0] * ent->size[0] + abs_move_dir[1]
			* ent->size[1] + abs_move_dir[2] * ent->size[2] - g_game.spawn.lip;
	VectorMA(ent->locals.pos1, ent->locals.move_info.distance, ent->locals.move_dir,
			ent->locals.pos2);

	// if it starts open, switch the positions
	if (ent->locals.spawn_flags & DOOR_START_OPEN) {
		VectorCopy(ent->locals.pos2, ent->s.origin);
		VectorCopy(ent->locals.pos1, ent->locals.pos2);
		VectorCopy(ent->s.origin, ent->locals.pos1);
	}

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_Die;
		ent->locals.max_health = ent->locals.health;
	} else if (ent->locals.target_name && ent->locals.message) {
		gi.SoundIndex("misc/chat");
		ent->locals.Touch = G_func_door_Touch;
	}

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.start_angles);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.end_angles);

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->locals.team)
		ent->locals.team_master = ent;

	gi.LinkEdict(ent);

	ent->locals.next_think = g_level.time + gi.frame_millis;
	if (ent->locals.health || ent->locals.target_name)
		ent->locals.Think = G_func_door_CalculateMove;
	else
		ent->locals.Think = G_func_door_CreateTrigger;
}

/*QUAKED func_door_rotating (0 .5 .8) START_OPEN TOGGLE REVERSE X_AXIS Y_AXIS
 A door which rotates about an origin on its Z axis. By default, doors open when a player walks close to them.
 -------- KEYS --------
 message : An optional string printed when the door is first touched.
 health : If set, door must take damage to open.
 speed : The speed with which the door opens (default 100).
 wait : wait before returning (3 default, -1 = never return).
 lip : The lip remaining at end of move (default 8 units).
 dmg : The damage inflicted on players who block the door as it closes (default 2).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_OPEN : The door to moves to its destination when spawned, and operates in reverse.
 TOGGLE : The door will wait in both the start and end states for a trigger event.
 REVERSE : The door will rotate in the opposite (negative) direction along its axis.
 X_AXIS : The door will rotate along its X axis.
 Y_AXIS : The door will rotate along its Y axis.
 */
void G_func_door_rotating(g_edict_t *ent) {
	VectorClear (ent->s.angles);

	// set the axis of rotation
	VectorClear(ent->locals.move_dir);
	if (ent->locals.spawn_flags & DOOR_X_AXIS)
		ent->locals.move_dir[2] = 1.0;
	else if (ent->locals.spawn_flags & DOOR_Y_AXIS)
		ent->locals.move_dir[0] = 1.0;
	else
		ent->locals.move_dir[1] = 1.0;

	// check for reverse rotation
	if (ent->locals.spawn_flags & DOOR_REVERSE)
		VectorNegate(ent->locals.move_dir, ent->locals.move_dir);

	if (!g_game.spawn.distance) {
		gi.Debug("%s at %s with no distance\n", ent->class_name, vtos(ent->s.origin));
		g_game.spawn.distance = 90.0;
	}

	VectorCopy(ent->s.angles, ent->locals.pos1);
	VectorMA(ent->s.angles, g_game.spawn.distance, ent->locals.move_dir, ent->locals.pos2);
	ent->locals.move_info.distance = g_game.spawn.distance;

	ent->locals.move_type = MOVE_TYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.SetModel(ent, ent->model);

	ent->locals.Blocked = G_func_door_Blocked;
	ent->locals.Use = G_func_door_Use;

	if (!ent->locals.speed)
		ent->locals.speed = 100.0;
	if (!ent->locals.accel)
		ent->locals.accel = ent->locals.speed;
	if (!ent->locals.decel)
		ent->locals.decel = ent->locals.speed;

	if (!ent->locals.wait)
		ent->locals.wait = 3.0;
	if (!ent->locals.dmg)
		ent->locals.dmg = 2;

	// if it starts open, switch the positions
	if (ent->locals.spawn_flags & DOOR_START_OPEN) {
		VectorCopy(ent->locals.pos2, ent->s.angles);
		VectorCopy(ent->locals.pos1, ent->locals.pos2);
		VectorCopy(ent->s.angles, ent->locals.pos1);
		VectorNegate(ent->locals.move_dir, ent->locals.move_dir);
	}

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.Die = G_func_door_Die;
		ent->locals.max_health = ent->locals.health;
	}

	if (ent->locals.target_name && ent->locals.message) {
		gi.SoundIndex("misc/chat.wav");
		ent->locals.Touch = G_func_door_Touch;
	}

	ent->locals.move_info.state = MOVE_STATE_BOTTOM;
	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait;

	VectorCopy(ent->s.origin, ent->locals.move_info.start_origin);
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_angles);
	VectorCopy(ent->s.origin, ent->locals.move_info.end_origin);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_angles);

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->locals.team)
		ent->locals.team_master = ent;

	gi.LinkEdict(ent);

	ent->locals.next_think = g_level.time + gi.frame_millis;
	if (ent->locals.health || ent->locals.target_name)
		ent->locals.Think = G_func_door_CalculateMove;
	else
		ent->locals.Think = G_func_door_CreateTrigger;
}

/*
 * @brief
 */
static void G_func_wall_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	if (self->solid == SOLID_NOT) {
		self->solid = SOLID_BSP;
		self->sv_flags &= ~SVF_NO_CLIENT;
		G_KillBox(self);
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}
	gi.LinkEdict(self);

	if (!(self->locals.spawn_flags & 2))
		self->locals.Use = NULL;
}

/*QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON
 A solid that may spawn into existence via trigger.
 -------- KEYS --------
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 TRIGGER_SPAWN : The wall is inhibited until triggered, at which point it appears and kills everything in its way.
 TOGGLE : The wall may be triggered off and on.
 START_ON : The wall will initially be present, but can be toggled off.
 */
void G_func_wall(g_edict_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;
	gi.SetModel(self, self->model);

	// just a wall
	if ((self->locals.spawn_flags & 7) == 0) {
		self->solid = SOLID_BSP;
		gi.LinkEdict(self);
		return;
	}

	// it must be TRIGGER_SPAWN
	if (!(self->locals.spawn_flags & 1)) {
		gi.Debug("Missing TRIGGER_SPAWN\n");
		self->locals.spawn_flags |= 1;
	}

	// yell if the spawnflags are odd
	if (self->locals.spawn_flags & 4) {
		if (!(self->locals.spawn_flags & 2)) {
			gi.Debug("START_ON without TOGGLE\n");
			self->locals.spawn_flags |= 2;
		}
	}

	self->locals.Use = G_func_wall_Use;

	if (self->locals.spawn_flags & 4) {
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}

	gi.LinkEdict(self);
}

/*QUAKED func_water(0 .5 .8) ? START_OPEN
 A movable water brush, which must be targeted to operate.
 -------- KEYS --------
 angle : Determines the opening direction (up = -1, down = -2)
 speed : The speed at which the water moves (default 25).
 wait : Delay in seconds before returning to the initial position (default -1).
 lip : Lip remaining at end of move (default 0 units).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS ---------
 START_OPEN : If set, causes the water to move to its destination when spawned and operate in reverse.
 */
void G_func_water(g_edict_t *self) {
	vec3_t abs_move_dir;

	G_SetMoveDir(self->s.angles, self->locals.move_dir);
	self->locals.move_type = MOVE_TYPE_PUSH;
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	// calculate second position
	VectorCopy(self->s.origin, self->locals.pos1);
	abs_move_dir[0] = fabsf(self->locals.move_dir[0]);
	abs_move_dir[1] = fabsf(self->locals.move_dir[1]);
	abs_move_dir[2] = fabsf(self->locals.move_dir[2]);
	self->locals.move_info.distance = abs_move_dir[0] * self->size[0] + abs_move_dir[1]
			* self->size[1] + abs_move_dir[2] * self->size[2] - g_game.spawn.lip;
	VectorMA(self->locals.pos1, self->locals.move_info.distance, self->locals.move_dir,
			self->locals.pos2);

	// if it starts open, switch the positions
	if (self->locals.spawn_flags & DOOR_START_OPEN) {
		VectorCopy(self->locals.pos2, self->s.origin);
		VectorCopy(self->locals.pos1, self->locals.pos2);
		VectorCopy(self->s.origin, self->locals.pos1);
	}

	VectorCopy(self->locals.pos1, self->locals.move_info.start_origin);
	VectorCopy(self->s.angles, self->locals.move_info.start_angles);
	VectorCopy(self->locals.pos2, self->locals.move_info.end_origin);
	VectorCopy(self->s.angles, self->locals.move_info.end_angles);

	self->locals.move_info.state = MOVE_STATE_BOTTOM;

	if (!self->locals.speed)
		self->locals.speed = 25;
	self->locals.move_info.accel = self->locals.move_info.decel = self->locals.move_info.speed
			= self->locals.speed;

	if (!self->locals.wait)
		self->locals.wait = -1;
	self->locals.move_info.wait = self->locals.wait;

	self->locals.Use = G_func_door_Use;

	if (self->locals.wait == -1)
		self->locals.spawn_flags |= DOOR_TOGGLE;

	self->class_name = "func_door";

	gi.LinkEdict(self);
}

#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

static void G_func_train_Next(g_edict_t *self);

/*
 * @brief
 */
static void G_func_train_Blocked(g_edict_t *self, g_edict_t *other) {
	if (!other->client)
		return;

	if (g_level.time < self->locals.touch_time)
		return;

	if (!self->locals.dmg)
		return;

	self->locals.touch_time = g_level.time + 500;
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);
}

/*
 * @brief
 */
static void G_func_train_Wait(g_edict_t *self) {
	if (self->locals.target_ent->locals.path_target) {
		g_edict_t *ent = self->locals.target_ent;
		char *savetarget = ent->locals.target;
		ent->locals.target = ent->locals.path_target;
		G_UseTargets(ent, self->locals.activator);
		ent->locals.target = savetarget;

		// make sure we didn't get killed by a killtarget
		if (!self->in_use)
			return;
	}

	if (self->locals.move_info.wait) {
		if (self->locals.move_info.wait > 0) {
			self->locals.next_think = g_level.time + (self->locals.move_info.wait * 1000);
			self->locals.Think = G_func_train_Next;
		} else if (self->locals.spawn_flags & TRAIN_TOGGLE) // && wait < 0
		{
			G_func_train_Next(self);
			self->locals.spawn_flags &= ~TRAIN_START_ON;
			VectorClear(self->locals.velocity);
			self->locals.next_think = 0;
		}

		if (!(self->locals.flags & FL_TEAM_SLAVE)) {
			if (self->locals.move_info.sound_end)
				gi.Sound(self, self->locals.move_info.sound_end, ATTEN_IDLE);
			self->s.sound = 0;
		}
	} else {
		G_func_train_Next(self);
	}
}

/*
 * @brief
 */
static void G_func_train_Next(g_edict_t *self) {
	g_edict_t *ent;
	vec3_t dest;
	_Bool first;

	first = true;
	again: if (!self->locals.target)
		return;

	ent = G_PickTarget(self->locals.target);
	if (!ent) {
		gi.Debug("%s has invalid target %s\n", etos(self), self->locals.target);
		return;
	}

	self->locals.target = ent->locals.target;

	// check for a teleport path_corner
	if (ent->locals.spawn_flags & 1) {
		if (!first) {
			gi.Debug("%s has teleport path_corner %s\n", etos(self), etos(ent));
			return;
		}
		first = false;
		VectorSubtract(ent->s.origin, self->mins, self->s.origin);
		VectorCopy(self->s.origin, self->s.old_origin);
		self->s.event = EV_CLIENT_TELEPORT;
		gi.LinkEdict(self);
		goto again;
	}

	self->locals.move_info.wait = ent->locals.wait;
	self->locals.target_ent = ent;

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTEN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->locals.move_info.state = MOVE_STATE_TOP;
	VectorCopy(self->s.origin, self->locals.move_info.start_origin);
	VectorCopy(dest, self->locals.move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_Wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/*
 * @brief
 */
static void G_func_train_Resume(g_edict_t *self) {
	g_edict_t *ent;
	vec3_t dest;

	ent = self->locals.target_ent;

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->locals.move_info.state = MOVE_STATE_TOP;
	VectorCopy(self->s.origin, self->locals.move_info.start_origin);
	VectorCopy(dest, self->locals.move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_Wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/*
 * @brief
 */
static void G_func_train_Find(g_edict_t *self) {
	g_edict_t *ent;

	if (!self->locals.target) {
		gi.Debug("No target specified\n");
		return;
	}
	ent = G_PickTarget(self->locals.target);
	if (!ent) {
		gi.Debug("Target \"%s\" not found\n", self->locals.target);
		return;
	}
	self->locals.target = ent->locals.target;

	VectorSubtract(ent->s.origin, self->mins, self->s.origin);
	gi.LinkEdict(self);

	// if not triggered, start immediately
	if (!self->locals.target_name)
		self->locals.spawn_flags |= TRAIN_START_ON;

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		self->locals.next_think = g_level.time + gi.frame_millis;
		self->locals.Think = G_func_train_Next;
		self->locals.activator = self;
	}
}

/*
 * @brief
 */
static void G_func_train_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		if (!(self->locals.spawn_flags & TRAIN_TOGGLE))
			return;
		self->locals.spawn_flags &= ~TRAIN_START_ON;
		VectorClear(self->locals.velocity);
		self->locals.next_think = 0;
	} else {
		if (self->locals.target_ent)
			G_func_train_Resume(self);
		else
			G_func_train_Next(self);
	}
}

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
 Trains are moving solids that players can ride along a series of path_corners. The origin of each corner specifies the lower bounding point of the train at that corner. If the train is the target of a button or trigger, it will not begin moving until activated.
 -------- KEYS --------
 speed : The speed with which the train moves (default 100).
 dmg : The damage inflicted on players who block the train (default 2).
 noise : The looping sound to play while the train is in motion.
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_ON : If set, the train will begin moving once spawned.
 TOGGLE : If set, the train will start or stop each time it is activated.
 BLOCK_STOPS : Never inflict damage on a player blocking the train.
 */
void G_func_train(g_edict_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;

	VectorClear(self->s.angles);
	self->locals.Blocked = G_func_train_Blocked;
	if (self->locals.spawn_flags & TRAIN_BLOCK_STOPS)
		self->locals.dmg = 0;
	else {
		if (!self->locals.dmg)
			self->locals.dmg = 100;
	}
	self->solid = SOLID_BSP;
	gi.SetModel(self, self->model);

	if (g_game.spawn.noise)
		self->locals.move_info.sound_middle = gi.SoundIndex(g_game.spawn.noise);

	if (!self->locals.speed)
		self->locals.speed = 100;

	self->locals.move_info.speed = self->locals.speed;
	self->locals.move_info.accel = self->locals.move_info.decel = self->locals.move_info.speed;

	self->locals.Use = G_func_train_Use;

	gi.LinkEdict(self);

	if (self->locals.target) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->locals.next_think = g_level.time + gi.frame_millis;
		self->locals.Think = G_func_train_Find;
	} else {
		gi.Debug("No target: %s\n", vtos(self->abs_mins));
	}
}

/*
 * @brief
 */
static void G_func_timer_Think(g_edict_t *self) {

	G_UseTargets(self, self->locals.activator);

	const uint32_t wait = self->locals.wait * 1000;
	const uint32_t rand = self->locals.random * 1000 * Randomc();

	self->locals.next_think = g_level.time + wait + rand;
}

/*
 * @brief
 */
static void G_func_timer_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;

	// if on, turn it off
	if (self->locals.next_think) {
		self->locals.next_think = 0;
		return;
	}

	// turn it on
	if (self->locals.delay)
		self->locals.next_think = g_level.time + self->locals.delay * 1000;
	else
		G_func_timer_Think(self);
}

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
 Time delay trigger that will continuously fire its targets after a preset time delay. The time delay can also be randomized. When triggered, the timer will toggle on/off.
 -------- KEYS --------
 wait : Delay in seconds between each triggering of all targets (default 1).
 random : Random time variance in seconds added or subtracted from "wait" delay (default 0).
 delay :  Additional delay before the first firing when START_ON (default 0).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_ON : If set, the timer will begin firing once spawned.
 */
void G_func_timer(g_edict_t *self) {

	if (!self->locals.wait) {
		self->locals.wait = 1.0;
	}

	self->locals.Use = G_func_timer_Use;
	self->locals.Think = G_func_timer_Think;

	if (self->locals.random >= self->locals.wait) {
		self->locals.random = self->locals.wait - gi.frame_seconds;
		gi.Debug("random >= wait: %s\n", vtos(self->s.origin));
	}

	if (self->locals.spawn_flags & 1) {

		const uint32_t delay = self->locals.delay * 1000;
		const uint32_t wait = self->locals.wait * 1000;
		const uint32_t rand = self->locals.random * 1000 * Randomc();

		self->locals.next_think = g_level.time + delay + wait + rand;
		self->locals.activator = self;
	}

	self->sv_flags = SVF_NO_CLIENT;
}

/*
 * @brief
 */
static void G_func_conveyor_Use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	if (self->locals.spawn_flags & 1) {
		self->locals.speed = 0;
		self->locals.spawn_flags &= ~1;
	} else {
		self->locals.speed = self->locals.count;
		self->locals.spawn_flags |= 1;
	}

	if (!(self->locals.spawn_flags & 2))
		self->locals.count = 0;
}

/*QUAKED func_conveyor (0 .5 .8) ? START_ON TOGGLE
 Conveyors are stationary brushes that move what's on them. The brush should be have a surface with at least one current content enabled.
 -------- KEYS --------
 speed : The speed at which objects on the conveyor are moved (default 100).
 targetname : The target name of this entity if it is to be triggered.
 -------- SPAWNFLAGS --------
 START_ON : The conveyor will be active immediately.
 TOGGLE : The conveyor is toggled each time it is used.
 */
void G_func_conveyor(g_edict_t *self) {
	if (!self->locals.speed)
		self->locals.speed = 100;

	if (!(self->locals.spawn_flags & 1)) {
		self->locals.count = self->locals.speed;
		self->locals.speed = 0;
	}

	self->locals.Use = G_func_conveyor_Use;

	gi.SetModel(self, self->model);
	self->solid = SOLID_BSP;
	gi.LinkEdict(self);
}
