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
static void G_func_areaportal_use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	ent->locals.count ^= 1; // toggle state
	gi.SetAreaPortalState(ent->locals.area_portal, ent->locals.count);
}

/*QUAKED func_areaportal (0 0 0) ?

 This is a non-visible object that divides the world into
 areas that are separated when this portal is not activated.
 Usually enclosed in the middle of a door.
 */
void G_func_areaportal(g_edict_t *ent) {
	ent->locals.use = G_func_areaportal_use;
	ent->locals.count = 0; // always start closed;
}

/*
 * @brief
 */
static void G_MoveInfo_Done(g_edict_t *ent) {
	VectorClear(ent->locals.velocity);
	ent->locals.move_info.done(ent);
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

	ent->locals.think = G_MoveInfo_Done;
	ent->locals.next_think = g_level.time + gi.frame_millis;
}

/*
 * @brief Starts a move with constant velocity. The entity will think again when it
 * has reached its destination.
 */
static void G_MoveInfo_Constant(g_edict_t *ent) {
	float frames;

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
	ent->locals.think = G_MoveInfo_End;
}

#define AccelerationDistance(target, rate) (target * ((target / rate) + 1) / 2)

/*
 * @brief Updates the acceleration parameters for the specified move. This determines
 * whether we should accelerate or decelerate based on the distance remaining.
 */
static void G_MoveInfo_UpdateAcceleration(g_move_info_t *move_info) {
	float accel_dist;
	float decel_dist;

	move_info->move_speed = move_info->speed;

	if (move_info->remaining_distance < move_info->accel) {
		move_info->current_speed = move_info->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance(move_info->speed, move_info->accel);
	decel_dist = AccelerationDistance(move_info->speed, move_info->decel);

	if ((move_info->remaining_distance - accel_dist - decel_dist) < 0) {
		float f;

		f = (move_info->accel + move_info->decel) / (move_info->accel * move_info->decel);
		move_info->move_speed = (-2 + sqrt(4 - 4 * f * (-2 * move_info->remaining_distance))) / (2
				* f);
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
			float p1_distance;
			float p2_distance;
			float distance;

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
		float old_speed;
		float p1_distance;
		float p1_speed;
		float p2_distance;
		float distance;

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
	ent->locals.think = G_MoveInfo_Accelerative;
}

/*
 * @brief Sets up movement for the specified entity. Both constant and accelerative
 * movements are initiated through this function.
 */
static void G_MoveInfo_Init(g_edict_t *ent, vec3_t dest, void(*done)(g_edict_t*)) {

	VectorClear(ent->locals.velocity);

	VectorSubtract(dest, ent->s.origin, ent->locals.move_info.dir);
	ent->locals.move_info.remaining_distance = VectorNormalize(ent->locals.move_info.dir);

	ent->locals.move_info.done = done;

	if (ent->locals.move_info.speed == ent->locals.move_info.accel && ent->locals.move_info.speed
			== ent->locals.move_info.decel) { // constant
		if (g_level.current_entity == ((ent->locals.flags & FL_TEAM_SLAVE)
				? ent->locals.team_master : ent)) {
			G_MoveInfo_Constant(ent);
		} else {
			ent->locals.next_think = g_level.time + gi.frame_millis;
			ent->locals.think = G_MoveInfo_Constant;
		}
	} else { // accelerative
		ent->locals.move_info.current_speed = 0;
		ent->locals.think = G_MoveInfo_Accelerative;
		ent->locals.next_think = g_level.time + gi.frame_millis;
	}
}

#define PLAT_LOW_TRIGGER	1

static void G_func_plat_go_down(g_edict_t *ent);

/*
 * @brief
 */
static void G_func_plat_up(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_end)
			gi.Sound(ent, ent->locals.move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->locals.move_info.state = STATE_TOP;

	ent->locals.think = G_func_plat_go_down;
	ent->locals.next_think = g_level.time + 3000;
}

/*
 * @brief
 */
static void G_func_plat_down(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_end)
			gi.Sound(ent, ent->locals.move_info.sound_end, ATTN_IDLE);
		ent->s.sound = 0;
	}
	ent->locals.move_info.state = STATE_BOTTOM;
}

/*
 * @brief
 */
static void G_func_plat_go_down(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_start)
			gi.Sound(ent, ent->locals.move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->locals.move_info.sound_middle;
	}
	ent->locals.move_info.state = STATE_DOWN;
	G_MoveInfo_Init(ent, ent->locals.move_info.end_origin, G_func_plat_down);
}

/*
 * @brief
 */
static void G_func_plat_go_up(g_edict_t *ent) {
	if (!(ent->locals.flags & FL_TEAM_SLAVE)) {
		if (ent->locals.move_info.sound_start)
			gi.Sound(ent, ent->locals.move_info.sound_start, ATTN_IDLE);
		ent->s.sound = ent->locals.move_info.sound_middle;
	}
	ent->locals.move_info.state = STATE_UP;
	G_MoveInfo_Init(ent, ent->locals.move_info.start_origin, G_func_plat_up);
}

/*
 * @brief
 */
static void G_func_plat_blocked(g_edict_t *self, g_edict_t *other) {

	if (!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);

	if (self->locals.move_info.state == STATE_UP)
		G_func_plat_go_down(self);
	else if (self->locals.move_info.state == STATE_DOWN)
		G_func_plat_go_up(self);
}

/*
 * @brief
 */
static void G_func_plat_use(g_edict_t *ent, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (ent->locals.think)
		return; // already down

	G_func_plat_go_down(ent);
}

/*
 * @brief
 */
static void G_func_plat_touch(g_edict_t *ent, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (other->locals.health <= 0)
		return;

	ent = ent->locals.enemy; // now point at the plat, not the trigger

	if (ent->locals.move_info.state == STATE_BOTTOM)
		G_func_plat_go_up(ent);
	else if (ent->locals.move_info.state == STATE_TOP)
		ent->locals.next_think = g_level.time + 1000; // the player is still on the plat, so delay going down
}

/*
 * @brief
 */
static void G_func_plat_create_trigger(g_edict_t *ent) {
	g_edict_t *trigger;
	vec3_t tmin, tmax;

	// middle trigger
	trigger = G_Spawn();
	trigger->locals.touch = G_func_plat_touch;
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

	gi.LinkEntity(trigger);
}

/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER

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
void G_func_plat(g_edict_t *ent) {
	float f;

	VectorClear(ent->s.angles);
	ent->solid = SOLID_BSP;
	ent->locals.move_type = MOVE_TYPE_PUSH;

	gi.SetModel(ent, ent->model);

	ent->locals.blocked = G_func_plat_blocked;

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
	f = 100.0 / (gi.frame_rate * gi.frame_rate);
	ent->locals.speed *= f;
	ent->locals.accel *= f;
	ent->locals.decel *= f;

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

	ent->locals.use = G_func_plat_use;

	G_func_plat_create_trigger(ent); // the "start moving" trigger

	if (ent->locals.target_name) {
		ent->locals.move_info.state = STATE_UP;
	} else {
		VectorCopy(ent->locals.pos2, ent->s.origin);
		gi.LinkEntity(ent);
		ent->locals.move_info.state = STATE_BOTTOM;
	}

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait * 1000; //this appears to be compltely optimized out by GCC
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
static void G_func_rotating_blocked(g_edict_t *self, g_edict_t *other) {
	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);
}

/*
 * @brief
 */
static void G_func_rotating_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!VectorCompare(self->locals.avelocity, vec3_origin)) {
		G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1,
				0, MOD_CRUSH);
	}
}

/*
 * @brief
 */
static void G_func_rotating_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {

	if (!VectorCompare(self->locals.avelocity, vec3_origin)) {
		self->s.sound = 0;
		VectorClear(self->locals.avelocity);
		self->locals.touch = NULL;
	} else {
		self->s.sound = self->locals.move_info.sound_middle;
		VectorScale(self->locals.move_dir, self->locals.speed, self->locals.avelocity);
		if (self->locals.spawn_flags & 16)
			self->locals.touch = G_func_rotating_touch;
	}
}

/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP
 You need to have an origin brush as part of this entity. The center of that brush will be
 the point around which it is rotated. It will rotate around the Z axis by default. You can
 check either the X_AXIS or Y_AXIS box to change that.

 "speed" determines how fast it moves; default value is 100.
 "dmg"	damage to inflict when blocked(2 default)

 REVERSE will cause the it to rotate in the opposite direction.
 STOP mean it will stop moving instead of pushing entities
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

	ent->locals.use = G_func_rotating_use;
	if (ent->locals.dmg)
		ent->locals.blocked = G_func_rotating_blocked;

	if (ent->locals.spawn_flags & 1)
		ent->locals.use(ent, NULL, NULL);

	gi.SetModel(ent, ent->model);
	gi.LinkEntity(ent);
}

/*
 * @brief
 */
static void G_func_button_done(g_edict_t *self) {
	self->locals.move_info.state = STATE_BOTTOM;
}

/*
 * @brief
 */
static void G_func_button_reset(g_edict_t *self) {

	self->locals.move_info.state = STATE_DOWN;

	G_MoveInfo_Init(self, self->locals.move_info.start_origin, G_func_button_done);

	if (self->locals.health)
		self->locals.take_damage = true;
}

/*
 * @brief
 */
static void G_func_button_wait(g_edict_t *self) {

	self->locals.move_info.state = STATE_TOP;

	G_UseTargets(self, self->locals.activator);

	if (self->locals.move_info.wait >= 0) {
		self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
		self->locals.think = G_func_button_reset;
	}
}

/*
 * @brief
 */
static void G_func_button_activate(g_edict_t *self) {

	if (self->locals.move_info.state == STATE_UP || self->locals.move_info.state == STATE_TOP)
		return;

	self->locals.move_info.state = STATE_UP;

	if (self->locals.move_info.sound_start && !(self->locals.flags & FL_TEAM_SLAVE))
		gi.Sound(self, self->locals.move_info.sound_start, ATTN_IDLE);

	G_MoveInfo_Init(self, self->locals.move_info.end_origin, G_func_button_wait);
}

/*
 * @brief
 */
static void G_func_button_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;
	G_func_button_activate(self);
}

/*
 * @brief
 */
static void G_func_button_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {

	if (!other->client)
		return;

	if (other->locals.health <= 0)
		return;

	self->locals.activator = other;
	G_func_button_activate(self);
}

static void G_func_button_die(g_edict_t *self, g_edict_t *inflictor __attribute__((unused)), g_edict_t *attacker,
		int32_t damage __attribute__((unused)), vec3_t point __attribute__((unused))) {
	self->locals.activator = attacker;
	self->locals.health = self->locals.max_health;
	self->locals.take_damage = false;
	G_func_button_activate(self);
}

/*QUAKED func_button (0 .5 .8) ?
 When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

 "angle"		determines the opening direction
 "target"	all entities with a matching targetname will be used
 "speed"		override the default 40 speed
 "wait"		override the default 1 second wait (-1 = never return)
 "lip"		override the default 4 pixel lip remaining at end of move
 "health"	if set, the button must be killed instead of touched
 */
void G_func_button(g_edict_t *ent) {
	vec3_t abs_move_dir;
	float dist;

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

	ent->locals.use = G_func_button_use;

	if (ent->locals.health) {
		ent->locals.max_health = ent->locals.health;
		ent->locals.die = G_func_button_die;
		ent->locals.take_damage = true;
	} else if (!ent->locals.target_name)
		ent->locals.touch = G_func_button_touch;

	ent->locals.move_info.state = STATE_BOTTOM;

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait * 1000;
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.start_angles);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.end_angles);

	gi.LinkEntity(ent);
}

#define DOOR_START_OPEN		1
#define DOOR_TOGGLE			2

/*
 * @brief
 */
static void G_func_door_use_areaportals(g_edict_t *self, _Bool open) {
	g_edict_t *t = NULL;

	if (!self->locals.target)
		return;

	while ((t = G_Find(t, LOFS(target_name), self->locals.target))) {
		if (strcasecmp(t->class_name, "func_areaportal") == 0) {
			gi.SetAreaPortalState(t->locals.area_portal, open);
		}
	}
}

static void G_func_door_go_down(g_edict_t *self);

/*
 * @brief
 */
static void G_func_door_up(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_end)
			gi.Sound(self, self->locals.move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->locals.move_info.state = STATE_TOP;

	if (self->locals.spawn_flags & DOOR_TOGGLE)
		return;

	if (self->locals.move_info.wait >= 0) {
		self->locals.think = G_func_door_go_down;
		self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
	}
}

/*
 * @brief
 */
static void G_func_door_down(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_end)
			gi.Sound(self, self->locals.move_info.sound_end, ATTN_IDLE);
		self->s.sound = 0;
	}
	self->locals.move_info.state = STATE_BOTTOM;
	G_func_door_use_areaportals(self, false);
}

/*
 * @brief
 */
static void G_func_door_go_down(g_edict_t *self) {
	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}
	if (self->locals.max_health) {
		self->locals.take_damage = true;
		self->locals.health = self->locals.max_health;
	}

	self->locals.move_info.state = STATE_DOWN;
	G_MoveInfo_Init(self, self->locals.move_info.start_origin, G_func_door_down);
}

/*
 * @brief
 */
static void G_func_door_go_up(g_edict_t *self, g_edict_t *activator) {
	if (self->locals.move_info.state == STATE_UP)
		return; // already going up

	if (self->locals.move_info.state == STATE_TOP) { // reset top wait time
		if (self->locals.move_info.wait >= 0)
			self->locals.next_think = g_level.time + self->locals.move_info.wait * 1000;
		return;
	}

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}
	self->locals.move_info.state = STATE_UP;
	G_MoveInfo_Init(self, self->locals.move_info.end_origin, G_func_door_up);

	G_UseTargets(self, activator);
	G_func_door_use_areaportals(self, true);
}

/*
 * @brief
 */
static void G_func_door_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	g_edict_t *ent;

	if (self->locals.flags & FL_TEAM_SLAVE)
		return;

	if (self->locals.spawn_flags & DOOR_TOGGLE) {
		if (self->locals.move_info.state == STATE_UP || self->locals.move_info.state == STATE_TOP) {
			// trigger all paired doors
			for (ent = self; ent; ent = ent->locals.team_chain) {
				ent->locals.message = NULL;
				ent->locals.touch = NULL;
				G_func_door_go_down(ent);
			}
			return;
		}
	}

	// trigger all paired doors
	for (ent = self; ent; ent = ent->locals.team_chain) {
		ent->locals.message = NULL;
		ent->locals.touch = NULL;
		G_func_door_go_up(ent, activator);
	}
}

/*
 * @brief
 */
static void G_func_door_touch_trigger(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
		c_bsp_surface_t *surf __attribute__((unused))) {
	if (other->locals.health <= 0)
		return;

	if (!other->client)
		return;

	if (g_level.time < self->locals.touch_time)
		return;

	self->locals.touch_time = g_level.time + 1000;

	G_func_door_use(self->owner, other, other);
}

/*
 * @brief
 */
static void G_func_door_calc_move(g_edict_t *self) {
	g_edict_t *ent;
	float min;
	float time;
	float new_speed;
	float ratio;
	float dist;

	if (self->locals.flags & FL_TEAM_SLAVE)
		return; // only the team master does this

	// find the smallest distance any member of the team will be moving
	min = fabsf(self->locals.move_info.distance);
	for (ent = self->locals.team_chain; ent; ent = ent->locals.team_chain) {
		dist = fabsf(ent->locals.move_info.distance);
		if (dist < min)
			min = dist;
	}

	time = min / self->locals.move_info.speed;

	// adjust speeds so they will all complete at the same time
	for (ent = self; ent; ent = ent->locals.team_chain) {
		new_speed = fabsf(ent->locals.move_info.distance) / time;
		ratio = new_speed / ent->locals.move_info.speed;
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
static void G_func_door_create_trigger(g_edict_t *ent) {
	g_edict_t *other;
	vec3_t mins, maxs;

	if (ent->locals.flags & FL_TEAM_SLAVE)
		return; // only the team leader spawns a trigger

	VectorCopy(ent->abs_mins, mins);
	VectorCopy(ent->abs_maxs, maxs);

	for (other = ent->locals.team_chain; other; other = other->locals.team_chain) {
		AddPointToBounds(other->abs_mins, mins, maxs);
		AddPointToBounds(other->abs_maxs, mins, maxs);
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
	other->locals.move_type = MOVE_TYPE_NONE;
	other->locals.touch = G_func_door_touch_trigger;
	gi.LinkEntity(other);

	if (ent->locals.spawn_flags & DOOR_START_OPEN)
		G_func_door_use_areaportals(ent, true);

	G_func_door_calc_move(ent);
}

/*
 * @brief
 */
static void G_func_door_blocked(g_edict_t *self, g_edict_t *other) {
	g_edict_t *ent;

	if (!other->client)
		return;

	G_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->locals.dmg, 1, 0,
			MOD_CRUSH);

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (self->locals.move_info.wait >= 0) {
		if (self->locals.move_info.state == STATE_DOWN) {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain)
				G_func_door_go_up(ent, ent->locals.activator);
		} else {
			for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain)
				G_func_door_go_down(ent);
		}
	}
}

/*
 * @brief
 */
static void G_func_door_die(g_edict_t *self, g_edict_t *inflictor __attribute__((unused)), g_edict_t *attacker,
		int32_t damage __attribute__((unused)), vec3_t point __attribute__((unused))) {
	g_edict_t *ent;

	for (ent = self->locals.team_master; ent; ent = ent->locals.team_chain) {
		ent->locals.health = ent->locals.max_health;
		ent->locals.take_damage = false;
	}

	G_func_door_use(self->locals.team_master, attacker, attacker);
}

/*
 * @brief
 */
static void G_func_door_touch(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane __attribute__((unused)),
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

	gi.Sound(other, gi.SoundIndex("misc/chat"), ATTN_NORM);
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN TOGGLE
 START_OPEN	the door to moves to its destination when spawned, and operate in reverse. It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
 TOGGLE		wait in both the start and end states for a trigger event.

 "message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
 "angle"		determines the opening direction
 "targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
 "health"	if set, door must be shot open
 "speed"		movement speed(100 default)
 "wait"		wait before returning(3 default, -1 = never return)
 "lip"		lip remaining at end of move(8 default)
 "dmg"		damage to inflict when blocked(2 default)
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

	ent->locals.blocked = G_func_door_blocked;
	ent->locals.use = G_func_door_use;

	if (!ent->locals.speed)
		ent->locals.speed = 100.0;

	ent->locals.speed *= 2.0;

	if (!ent->locals.accel)
		ent->locals.accel = ent->locals.speed;
	if (!ent->locals.decel)
		ent->locals.decel = ent->locals.speed;

	if (!ent->locals.wait)
		ent->locals.wait = 3;
	if (!g_game.spawn.lip)
		g_game.spawn.lip = 8;
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

	ent->locals.move_info.state = STATE_BOTTOM;

	if (ent->locals.health) {
		ent->locals.take_damage = true;
		ent->locals.die = G_func_door_die;
		ent->locals.max_health = ent->locals.health;
	} else if (ent->locals.target_name && ent->locals.message) {
		gi.SoundIndex("misc/chat");
		ent->locals.touch = G_func_door_touch;
	}

	ent->locals.move_info.speed = ent->locals.speed;
	ent->locals.move_info.accel = ent->locals.accel;
	ent->locals.move_info.decel = ent->locals.decel;
	ent->locals.move_info.wait = ent->locals.wait * 1000;
	VectorCopy(ent->locals.pos1, ent->locals.move_info.start_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.start_angles);
	VectorCopy(ent->locals.pos2, ent->locals.move_info.end_origin);
	VectorCopy(ent->s.angles, ent->locals.move_info.end_angles);

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->locals.team)
		ent->locals.team_master = ent;

	gi.LinkEntity(ent);

	ent->locals.next_think = g_level.time + gi.frame_millis;
	if (ent->locals.health || ent->locals.target_name)
		ent->locals.think = G_func_door_calc_move;
	else
		ent->locals.think = G_func_door_create_trigger;
}

/*
 * @brief
 */
static void G_func_wall_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	if (self->solid == SOLID_NOT) {
		self->solid = SOLID_BSP;
		self->sv_flags &= ~SVF_NO_CLIENT;
		G_KillBox(self);
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}
	gi.LinkEntity(self);

	if (!(self->locals.spawn_flags & 2))
		self->locals.use = NULL;
}

/*QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON
 This is just a solid wall if not inhibited

 TRIGGER_SPAWN	the wall will not be present until triggered
 it will then blink in to existence; it will
 kill anything that was in it's way

 TOGGLE			only valid for TRIGGER_SPAWN walls
 this allows the wall to be turned on and off

 START_ON		only valid for TRIGGER_SPAWN walls
 the wall will initially be present
 */
void G_func_wall(g_edict_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;
	gi.SetModel(self, self->model);

	// just a wall
	if ((self->locals.spawn_flags & 7) == 0) {
		self->solid = SOLID_BSP;
		gi.LinkEntity(self);
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

	self->locals.use = G_func_wall_use;

	if (self->locals.spawn_flags & 4) {
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->sv_flags |= SVF_NO_CLIENT;
	}

	gi.LinkEntity(self);
}

/*QUAKED func_water(0 .5 .8) ? START_OPEN
 func_water is a moveable water brush. It must be targeted to operate. Use a non-water texture at your own risk.

 START_OPEN causes the water to move to its destination when spawned and operate in reverse.

 "angle"		determines the opening direction(up or down only)
 "speed"		movement speed(25 default)
 "wait"		wait before returning(-1 default, -1 = TOGGLE)
 "lip"		lip remaining at end of move(0 default)
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

	self->locals.move_info.state = STATE_BOTTOM;

	if (!self->locals.speed)
		self->locals.speed = 25;
	self->locals.move_info.accel = self->locals.move_info.decel = self->locals.move_info.speed
			= self->locals.speed;

	if (!self->locals.wait)
		self->locals.wait = -1;
	self->locals.move_info.wait = self->locals.wait * 1000;

	self->locals.use = G_func_door_use;

	if (self->locals.wait == -1)
		self->locals.spawn_flags |= DOOR_TOGGLE;

	self->class_name = "func_door";

	gi.LinkEntity(self);
}

#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

static void G_func_train_next(g_edict_t *self);

/*
 * @brief
 */
static void G_func_train_blocked(g_edict_t *self, g_edict_t *other) {
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
static void G_func_train_wait(g_edict_t *self) {
	if (self->locals.target_ent->locals.path_target) {
		char *savetarget;
		g_edict_t *ent;

		ent = self->locals.target_ent;
		savetarget = ent->locals.target;
		ent->locals.target = ent->locals.path_target;
		G_UseTargets(ent, self->locals.activator);
		ent->locals.target = savetarget;

		// make sure we didn't get killed by a killtarget
		if (!self->in_use)
			return;
	}

	if (self->locals.move_info.wait) {
		if (self->locals.move_info.wait > 0) {
			self->locals.next_think = g_level.time + self->locals.move_info.wait;
			self->locals.think = G_func_train_next;
		} else if (self->locals.spawn_flags & TRAIN_TOGGLE) // && wait < 0
		{
			G_func_train_next(self);
			self->locals.spawn_flags &= ~TRAIN_START_ON;
			VectorClear(self->locals.velocity);
			self->locals.next_think = 0;
		}

		if (!(self->locals.flags & FL_TEAM_SLAVE)) {
			if (self->locals.move_info.sound_end)
				gi.Sound(self, self->locals.move_info.sound_end, ATTN_IDLE);
			self->s.sound = 0;
		}
	} else {
		G_func_train_next(self);
	}
}

/*
 * @brief
 */
static void G_func_train_next(g_edict_t *self) {
	g_edict_t *ent;
	vec3_t dest;
	_Bool first;

	first = true;
	again: if (!self->locals.target)
		return;

	ent = G_PickTarget(self->locals.target);
	if (!ent) {
		gi.Debug("bad target %s\n", self->locals.target);
		return;
	}

	self->locals.target = ent->locals.target;

	// check for a teleport path_corner
	if (ent->locals.spawn_flags & 1) {
		if (!first) {
			gi.Debug("Connected teleport path_corners, see %s at %s\n", ent->class_name,
					vtos(ent->s.origin));
			return;
		}
		first = false;
		VectorSubtract(ent->s.origin, self->mins, self->s.origin);
		VectorCopy(self->s.origin, self->s.old_origin);
		self->s.event = EV_CLIENT_TELEPORT;
		gi.LinkEntity(self);
		goto again;
	}

	self->locals.move_info.wait = ent->locals.wait * 1000;
	self->locals.target_ent = ent;

	if (!(self->locals.flags & FL_TEAM_SLAVE)) {
		if (self->locals.move_info.sound_start)
			gi.Sound(self, self->locals.move_info.sound_start, ATTN_IDLE);
		self->s.sound = self->locals.move_info.sound_middle;
	}

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->locals.move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->locals.move_info.start_origin);
	VectorCopy(dest, self->locals.move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/*
 * @brief
 */
static void G_func_train_resume(g_edict_t *self) {
	g_edict_t *ent;
	vec3_t dest;

	ent = self->locals.target_ent;

	VectorSubtract(ent->s.origin, self->mins, dest);
	self->locals.move_info.state = STATE_TOP;
	VectorCopy(self->s.origin, self->locals.move_info.start_origin);
	VectorCopy(dest, self->locals.move_info.end_origin);
	G_MoveInfo_Init(self, dest, G_func_train_wait);
	self->locals.spawn_flags |= TRAIN_START_ON;
}

/*
 * @brief
 */
static void G_func_train_find(g_edict_t *self) {
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
	gi.LinkEntity(self);

	// if not triggered, start immediately
	if (!self->locals.target_name)
		self->locals.spawn_flags |= TRAIN_START_ON;

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		self->locals.next_think = g_level.time + gi.frame_millis;
		self->locals.think = G_func_train_next;
		self->locals.activator = self;
	}
}

/*
 * @brief
 */
static void G_func_train_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
	self->locals.activator = activator;

	if (self->locals.spawn_flags & TRAIN_START_ON) {
		if (!(self->locals.spawn_flags & TRAIN_TOGGLE))
			return;
		self->locals.spawn_flags &= ~TRAIN_START_ON;
		VectorClear(self->locals.velocity);
		self->locals.next_think = 0;
	} else {
		if (self->locals.target_ent)
			G_func_train_resume(self);
		else
			G_func_train_next(self);
	}
}

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
 Trains are moving platforms that players can ride.
 The targets origin specifies the min point of the train at each corner.
 The train spawns at the first target it is pointing at.
 If the train is the target of a button or trigger, it will not begin moving until activated.
 speed	default 100
 dmg		default	2
 noise	looping sound to play when the train is in motion

 */
void G_func_train(g_edict_t *self) {
	self->locals.move_type = MOVE_TYPE_PUSH;

	VectorClear(self->s.angles);
	self->locals.blocked = G_func_train_blocked;
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

	self->locals.use = G_func_train_use;

	gi.LinkEntity(self);

	if (self->locals.target) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->locals.next_think = g_level.time + gi.frame_millis;
		self->locals.think = G_func_train_find;
	} else {
		gi.Debug("No target: %s\n", vtos(self->abs_mins));
	}
}

/*
 * @brief
 */
static void G_func_timer_think(g_edict_t *self) {
	G_UseTargets(self, self->locals.activator);
	self->locals.next_think = g_level.time + self->locals.wait + Randomc() * self->locals.random;
}

/*
 * @brief
 */
static void G_func_timer_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator) {
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
		G_func_timer_think(self);
}

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
 "wait"			base time between triggering all targets, default is 1
 "random"		wait variance, default is 0

 so, the basic time between firing is a random time between
 (wait - random) and(wait + random)

 "delay"			delay before first firing when turned on, default is 0

 "pausetime"		additional delay used only the very first time
 and only if spawned with START_ON

 These can used but not touched.
 */
void G_func_timer(g_edict_t *self) {
	if (!self->locals.wait)
		self->locals.wait = 1.0;

	self->locals.use = G_func_timer_use;
	self->locals.think = G_func_timer_think;

	if (self->locals.random >= self->locals.wait) {
		self->locals.random = self->locals.wait - gi.frame_seconds;
		gi.Debug("Random >= wait: %s\n", vtos(self->s.origin));
	}

	if (self->locals.spawn_flags & 1) {
		self->locals.next_think = g_level.time + 1000 + g_game.spawn.pause_time
				+ self->locals.delay * 1000 + self->locals.wait * 1000 + Randomc()
				* (self->locals.random * 1000);
		self->locals.activator = self;
	}

	self->sv_flags = SVF_NO_CLIENT;
}

/*
 * @brief
 */
static void G_func_conveyor_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
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
 Conveyors are stationary brushes that move what's on them.
 The brush should be have a surface with at least one current content enabled.
 speed	default 100
 */
void G_func_conveyor(g_edict_t *self) {
	if (!self->locals.speed)
		self->locals.speed = 100;

	if (!(self->locals.spawn_flags & 1)) {
		self->locals.count = self->locals.speed;
		self->locals.speed = 0;
	}

	self->locals.use = G_func_conveyor_use;

	gi.SetModel(self, self->model);
	self->solid = SOLID_BSP;
	gi.LinkEntity(self);
}

/*
 * @brief
 */
static void G_func_killbox_use(g_edict_t *self, g_edict_t *other __attribute__((unused)), g_edict_t *activator __attribute__((unused))) {
	G_KillBox(self);
}

/*QUAKED func_killbox (1 0 0) ?
 Kills everything inside when fired, regardless of protection.
 */
void G_func_killbox(g_edict_t *ent) {
	gi.SetModel(ent, ent->model);
	ent->locals.use = G_func_killbox_use;
	ent->sv_flags = SVF_NO_CLIENT;
}
