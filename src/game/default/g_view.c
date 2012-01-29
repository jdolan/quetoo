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
 * G_ClientDamage
 *
 * Inspect all damage received this frame and play a pain sound if appropriate.
 */
static void G_ClientDamage(g_edict_t *ent) {
	g_client_t *client;
	int l;

	client = ent->client;

	if (client->damage_blood + client->damage_armor == 0)
		return; // didn't take any damage

	// play an appropriate pain sound
	if (g_level.time > client->pain_time) {

		client->pain_time = g_level.time + 0.7;

		if (ent->health < 25)
			l = 25;
		else if (ent->health < 50)
			l = 50;
		else if (ent->health < 75)
			l = 75;
		else
			l = 100;

		gi.Sound(ent, gi.SoundIndex(va("*pain%i_1", l)), ATTN_NORM);
	}

	// clear totals
	client->damage_blood = 0;
	client->damage_armor = 0;
}

/*
 * G_ClientFall
 */
static void G_ClientFall(g_edict_t *ent) {

	if (!ent->ground_entity)
		return;

	if (ent->health < 1 || ent->water_level == 3)
		return;

	if (ent->client->fall_time > g_level.time)
		return;

	float fall = -ent->client->old_velocity[2];

	if (fall < 220.0)
		return;

	entity_event_t event = EV_CLIENT_LAND;

	if (fall > 550.0) { // player will take damage
		int damage = ((int) (fall * 0.01)) >> ent->water_level;
		vec3_t dir;

		if (fall > 750.0)
			event = EV_CLIENT_FALL_FAR;
		else
			event = EV_CLIENT_FALL;

		ent->client->pain_time = g_level.time; // suppress pain sound

		VectorSet(dir, 0.0, 0.0, 1.0);

		G_Damage(ent, NULL, NULL, dir, ent->s.origin, vec3_origin, damage, 0,
				DAMAGE_NO_ARMOR, MOD_FALLING);
	}

	if (ent->s.event != EV_TELEPORT) { // don't override teleport events
		ent->client->fall_time = g_level.time + 0.2;
		ent->s.event = event;
	}
}

/*
 * G_ClientWaterLevel
 */
static void G_ClientWaterLevel(g_edict_t *ent) {
	g_client_t *client = ent->client;

	if (ent->move_type == MOVE_TYPE_NO_CLIP) {
		client->drown_time = g_level.time + 12; // don't need air
		return;
	}

	const int water_level = ent->water_level;
	const int old_water_level = ent->old_water_level;

	ent->old_water_level = water_level;

	// if just entered a water volume, play a sound
	if (!old_water_level && water_level)
		gi.Sound(ent, gi.SoundIndex("world/water_in"), ATTN_NORM);

	// completely exited the water
	if (old_water_level && !water_level)
		gi.Sound(ent, gi.SoundIndex("world/water_out"), ATTN_NORM);

	// head just coming out of water
	if (old_water_level == 3 && water_level != 3 && g_level.time
			- client->gasp_time > 2.0) {

		gi.Sound(ent, gi.SoundIndex("*gasp_1"), ATTN_NORM);
		client->gasp_time = g_level.time;
	}

	// check for drowning
	if (water_level != 3) { // take some air, push out drown time
		client->drown_time = g_level.time + 12.0;
		ent->dmg = 0;
	} else { // we're under water
		if (client->drown_time < g_level.time && ent->health > 0) {
			client->drown_time = g_level.time + 1.0;

			// take more damage the longer under water
			ent->dmg += 2;

			if (ent->dmg > 15)
				ent->dmg = 15;

			// play a gurp sound instead of a normal pain sound
			if (ent->health <= ent->dmg)
				gi.Sound(ent, gi.SoundIndex("*drown_1"), ATTN_NORM);
			else
				gi.Sound(ent, gi.SoundIndex("*gurp_1"), ATTN_NORM);

			client->pain_time = g_level.time;

			G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin, vec3_origin,
					ent->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
		}
	}

	// check for sizzle damage
	if (water_level && (ent->water_type & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		if (client->sizzle_time <= g_level.time && ent->health > 0) {

			client->sizzle_time = g_level.time + 0.1;

			if (ent->water_type & CONTENTS_LAVA) {
				G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin,
						vec3_origin, 2 * water_level, 0, DAMAGE_NO_ARMOR,
						MOD_LAVA);
			}

			if (ent->water_type & CONTENTS_SLIME) {
				G_Damage(ent, NULL, NULL, vec3_origin, ent->s.origin,
						vec3_origin, 1 * water_level, 0, DAMAGE_NO_ARMOR,
						MOD_SLIME);
			}
		}
	}
}

/*
 * G_ClientAnimation
 *
 * Sets the animation sequences for the specified entity.  This is called
 * towards the end of each frame, after our ground entity and water level have
 * been resolved.
 */
static void G_ClientAnimation(g_edict_t *ent) {

	if (ent->sv_flags & SVF_NO_CLIENT)
		return;

	// check for falling

	if (!ent->ground_entity) { // not on the ground
		vec3_t point;
		c_trace_t trace;

		if (ent->water_level == 3) { // swimming
			G_SetAnimation(ent, ANIM_LEGS_SWIM, false);
			return;
		}

		VectorCopy(ent->s.origin, point);
		point[2] -= 8.0;

		trace = gi.Trace(ent->s.origin, ent->mins, ent->maxs, point, ent,
				MASK_PLAYER_SOLID);
		if (trace.fraction == 1.0) {

			boolean_t jumping = G_IsAnimation(ent, ANIM_LEGS_JUMP1);
			jumping |= G_IsAnimation(ent, ANIM_LEGS_JUMP2);

			if (!jumping)
				G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);
		}

		return;
	}

	// check for landing

	const entity_event_t e = ent->s.event;

	if (e >= EV_CLIENT_LAND && e <= EV_CLIENT_FALL_FAR) {

		if (G_IsAnimation(ent, ANIM_LEGS_JUMP2))
			G_SetAnimation(ent, ANIM_LEGS_LAND2, false);
		else
			G_SetAnimation(ent, ANIM_LEGS_LAND1, false);

		return;
	}

	// duck, walk or run

	if (ent->client->fall_time <= g_level.time) {
		vec3_t velocity;
		float speed;

		VectorCopy(ent->velocity, velocity);
		velocity[2] = 0.0;

		speed = VectorNormalize(velocity);

		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) { // ducked
			if (speed < 1.0)
				G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
			else
				G_SetAnimation(ent, ANIM_LEGS_WALKCR, false);

			return;
		}

		if (speed < 1.0) {
			G_SetAnimation(ent, ANIM_LEGS_IDLE, false);
			return;
		}

		vec3_t angles, forward;

		VectorSet(angles, 0.0, ent->s.angles[YAW], 0.0);
		AngleVectors(angles, forward, NULL, NULL);

		if (DotProduct(velocity, forward) < -0.1)
			G_SetAnimation(ent, ANIM_LEGS_BACK, false);
		else if (speed < 200.0)
			G_SetAnimation(ent, ANIM_LEGS_WALK, false);
		else
			G_SetAnimation(ent, ANIM_LEGS_RUN, false);

		return;
	}
}

/*
 * G_ClientEndFrame
 *
 * Called for each client at the end of the server frame.
 */
void G_ClientEndFrame(g_edict_t *ent) {
	vec3_t forward, right, up;
	float dot, xy_speed;
	int i;

	g_client_t *client = ent->client;

	// If the origin or velocity have changed since G_Think(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	for (i = 0; i < 3; i++) {
		client->ps.pmove.origin[i] = ent->s.origin[i] * 8.0;
		client->ps.pmove.velocity[i] = ent->velocity[i] * 8.0;
	}

	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	if (g_level.intermission_time) {
		G_ClientStats(ent);
		G_ClientScores(ent);
		return;
	}

	// check for water entry / exit, burn from lava, slime, etc
	G_ClientWaterLevel(ent);

	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	if (ent->client->angles[PITCH] > 180.0)
		ent->s.angles[PITCH] = (-360.0 + ent->client->angles[PITCH]) / 3.0;
	else
		ent->s.angles[PITCH] = ent->client->angles[PITCH] / 3.0;
	ent->s.angles[YAW] = ent->client->angles[YAW];

	// set roll based on lateral velocity
	AngleVectors(ent->client->angles, forward, right, up);
	dot = DotProduct(ent->velocity, right);
	ent->s.angles[ROLL] = dot * 0.025;

	// less roll when in the air
	if (!ent->ground_entity)
		ent->s.angles[ROLL] *= 0.25;

	// detect hitting the floor
	G_ClientFall(ent);

	// check for footsteps
	if (ent->ground_entity && !ent->s.event) {

		xy_speed = sqrt(
				ent->velocity[0] * ent->velocity[0] + ent->velocity[1]
						* ent->velocity[1]);

		if (xy_speed > 265.0 && ent->client->footstep_time < g_level.time) {
			ent->client->footstep_time = g_level.time + 0.3;
			ent->s.event = EV_CLIENT_FOOTSTEP;
		}
	}

	// apply all the damage taken this frame
	G_ClientDamage(ent);

	// update the player's animations
	G_ClientAnimation(ent);

	// set the stats for this client
	if (ent->client->persistent.spectator)
		G_ClientSpectatorStats(ent);
	else
		G_ClientStats(ent);

	VectorCopy(ent->velocity, ent->client->old_velocity);
	VectorCopy(ent->client->ps.angles, ent->client->old_angles);

	// if the scoreboard is up, update it
	if (ent->client->show_scores)
		G_ClientScores(ent);
}

/*
 * G_EndClientFrames
 */
void G_EndClientFrames(void) {
	int i;
	g_edict_t *ent;

	// finalize the player_state_t for this frame
	for (i = 0; i < sv_max_clients->integer; i++) {

		ent = g_game.edicts + 1 + i;

		if (!ent->in_use || !ent->client)
			continue;

		G_ClientEndFrame(ent);
	}
}
