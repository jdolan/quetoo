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
#include "bg_pmove.h"

/**
 * @brief Inspect all damage received this frame and play a pain sound if appropriate.
 */
static void G_ClientDamage(g_entity_t *ent) {
	g_client_t *client = ent->client;

	if (client->locals.damage_health || client->locals.damage_armor) {
		// play an appropriate pain sound
		if (g_level.time > client->locals.pain_time) {
			int32_t l;

			client->locals.pain_time = g_level.time + 700;

			if (ent->locals.health < 25) {
				l = 25;
			} else if (ent->locals.health < 50) {
				l = 50;
			} else if (ent->locals.health < 75) {
				l = 75;
			} else {
				l = 100;
			}

			const vec3_t org = Vec3_Add(client->ps.pm_state.origin, client->ps.pm_state.view_offset);

			gi.PositionedSound(org, ent, gi.SoundIndex(va("*pain%i_1", l)), ATTEN_NORM, 0);
		}
	}

	// clear totals
	client->locals.damage_health = 0;
	client->locals.damage_armor = 0;
	client->locals.damage_inflicted = 0;
}

/**
 * @brief Handles water entry and exit
 */
static void G_ClientWaterInteraction(g_entity_t *ent) {
	g_client_t *client = ent->client;

	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		client->locals.drown_time = g_level.time + 12000; // don't need air
		return;
	}

	const pm_water_level_t water_level = ent->locals.water_level;
	const pm_water_level_t old_water_level = client->locals.old_water_level;

	// if just entered a water volume, play a sound
	if (old_water_level <= WATER_NONE && water_level >= WATER_FEET) {
		gi.Sound(ent, g_media.sounds.water_in, ATTEN_NORM, 0);
	}

	// completely exited the water
	if (old_water_level >= WATER_FEET && water_level == WATER_NONE) {
		gi.Sound(ent, g_media.sounds.water_out, ATTEN_NORM, 0);
	}

	// same water level, head out of water
	if ((old_water_level == water_level) && (water_level > WATER_FEET && water_level < WATER_UNDER)) {
		if (Vec3_Length(ent->locals.velocity) > 10.0) {
			G_Ripple(ent, Vec3_Zero(), Vec3_Zero(), 0.0, false);
		}
	}

	if (ent->locals.dead == false) { // if we're alive, we can drown

		// head just coming out of water, play a gasp if we were down for a while
		if (old_water_level == WATER_UNDER && water_level != WATER_UNDER && (client->locals.drown_time - g_level.time) < 8000) {

			const vec3_t org = Vec3_Add(client->ps.pm_state.origin, client->ps.pm_state.view_offset);

			gi.PositionedSound(org, ent, gi.SoundIndex("*gasp_1"), ATTEN_NORM, 0);
		}

		// check for drowning
		if (water_level != WATER_UNDER) { // take some air, push out drown time
			client->locals.drown_time = g_level.time + 12000;
			ent->locals.damage = 0;
		} else { // we're under water
			if (client->locals.drown_time < g_level.time && ent->locals.health > 0) {
				client->locals.drown_time = g_level.time + 1000;

				// take more damage the longer under water
				ent->locals.damage += 2;

				if (ent->locals.damage > 12) {
					ent->locals.damage = 12;
				}

				// play a gurp sound instead of a normal pain sound
				if (ent->locals.health <= ent->locals.damage) {
					ent->s.event = EV_CLIENT_DROWN;
				} else {
					ent->s.event = EV_CLIENT_GURP;
				}

				// suppress normal pain sound
				client->locals.pain_time = g_level.time;

				// and apply the damage
				G_Damage(ent, NULL, NULL, Vec3_Zero(), Vec3_Zero(), Vec3_Zero(), ent->locals.damage, 0, DMG_NO_ARMOR, MOD_WATER);
			}
		}
	}

	// check for sizzle damage
	if (water_level && (ent->locals.water_type & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		if (client->locals.sizzle_time <= g_level.time) {
			client->locals.sizzle_time = g_level.time + 100;

			if (ent->locals.dead == false && (ent->locals.water_type & CONTENTS_LAVA) && client->locals.pain_time <= g_level.time) {

				// play a sizzle sound instead of a normal pain sound
				ent->s.event = EV_CLIENT_SIZZLE;

				// suppress normal pain sound
				client->locals.pain_time = g_level.time + 1000;
			}

			if (ent->locals.water_type & CONTENTS_LAVA) {
				G_Damage(ent, NULL, NULL, Vec3_Zero(), Vec3_Zero(), Vec3_Zero(), 2 * water_level, 0, DMG_NO_ARMOR, MOD_LAVA);
			}

			if (ent->locals.water_type & CONTENTS_SLIME) {
				G_Damage(ent, NULL, NULL, Vec3_Zero(), Vec3_Zero(), Vec3_Zero(), water_level, 0, 0, MOD_SLIME);
			}
		}
	}

	client->locals.old_water_level = water_level;
}

/**
 * @brief Set the angles of the client's world model, after clamping them to sane
 * values.
 */
static void G_ClientWorldAngles(g_entity_t *ent) {

	if (ent->locals.dead) { // just lay there like a lump
		ent->s.angles.x = 0.0;
		return;
	}

	ent->s.angles.x = ent->client->locals.angles.x / 1.5;
	ent->s.angles.y = ent->client->locals.angles.y;

	// set roll based on lateral velocity and ground entity
	const float dot = Vec3_Dot(ent->locals.velocity, ent->client->locals.right);

	ent->s.angles.z = ent->locals.ground_entity ? dot * 0.015 : dot * 0.005;

	// check for footsteps
	if (ent->locals.ground_entity && ent->locals.move_type == MOVE_TYPE_WALK && !ent->s.event) {

		if (ent->client->locals.speed > 250.0 && ent->client->locals.footstep_time < g_level.time) {
			ent->client->locals.footstep_time = g_level.time + 275;
			ent->s.event = EV_CLIENT_FOOTSTEP;
		}
	}
}

/**
 * @brief Adds view kick in the specified direction to the specified client.
 */
void G_ClientDamageKick(g_entity_t *ent, const vec3_t dir, const float kick) {
	vec3_t ndir;

	ndir = Vec3_Normalize(dir);

	const float pitch = Vec3_Dot(ndir, ent->client->locals.forward) * kick;
	ent->client->locals.kick_angles.x += pitch;

	const float roll = Vec3_Dot(ndir, ent->client->locals.right) * kick;
	ent->client->locals.kick_angles.z += roll;
}

/**
 * @brief Adds view angle kick based on entity events (falling, landing, etc).
 */
static void G_ClientFallKick(g_entity_t *ent, const float kick) {
	ent->client->locals.kick_angles.x += kick;
}

/**
 * @brief Sends the kick angles accumulated this frame to the client.
 */
static void G_ClientKickAngles(g_entity_t *ent) {

	switch (ent->s.event) {
		case EV_CLIENT_LAND:
			G_ClientFallKick(ent, 2.0);
			break;
		case EV_CLIENT_FALL:
			G_ClientFallKick(ent, 3.0);
			break;
		case EV_CLIENT_FALL_FAR:
			G_ClientFallKick(ent, 4.0);
			break;
		default:
			break;
	}

	if (!Vec3_Equal(ent->client->locals.kick_angles, Vec3_Zero())) {
		gi.WriteByte(SV_CMD_VIEW_KICK);
		gi.WriteAngle(ent->client->locals.kick_angles.x);
		gi.WriteAngle(ent->client->locals.kick_angles.z);
		gi.Unicast(ent, false);
	}

	ent->client->locals.kick_angles = Vec3_Zero();
}

/**
 * @brief Sets the animation sequences for the specified entity. This is called
 * towards the end of each frame, after our ground entity and water level have
 * been resolved.
 */
static void G_ClientAnimation(g_entity_t *ent) {

	if (ent->s.model1 != MODEL_CLIENT) {
		return;
	}

	// corpses animate to their final resting place

	if (ent->solid == SOLID_DEAD) {
		if (g_level.time >= ent->client->locals.respawn_time) {
			switch (ent->s.animation1) {
				case ANIM_BOTH_DEATH1:
				case ANIM_BOTH_DEATH2:
				case ANIM_BOTH_DEATH3:
					G_SetAnimation(ent, ent->s.animation1 + 1, false);
					break;
				default:
					break;
			}
		}
		return;
	}

	// no-clippers do not animate

	if (ent->locals.move_type == MOVE_TYPE_NO_CLIP) {
		G_SetAnimation(ent, ANIM_TORSO_STAND1, false);
		G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);
		return;
	}

	// check for falling

	g_client_locals_t *cl = &ent->client->locals;
	if (!ent->locals.ground_entity) { // not on the ground

		if (g_level.time - cl->jump_time > 400) {
			if (ent->locals.water_level == WATER_UNDER && cl->speed > 10.0) { // swimming
				G_SetAnimation(ent, ANIM_LEGS_SWIM, false);
				return;
			}
			if (ent->client->ps.pm_state.flags & PMF_DUCKED) { // ducking
				G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
				return;
			}
		}

		_Bool jumping = G_IsAnimation(ent, ANIM_LEGS_JUMP1);
		jumping |= G_IsAnimation(ent, ANIM_LEGS_JUMP2);

		if (!jumping) {
			G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);
		}

		return;
	}

	// duck, walk or run after landing

	if (g_level.time - 400 > cl->land_time && g_level.time - 50 > cl->ground_time) {

		vec3_t forward;
		
		const vec3_t euler = Vec3(0.0, ent->s.angles.y, 0.0);
		Vec3_Vectors(euler, &forward, NULL, NULL);

		const _Bool backwards = Vec3_Dot(ent->locals.velocity, forward) < -0.1;

		if (ent->client->ps.pm_state.flags & PMF_DUCKED) { // ducked
			if (cl->speed < 1.0) {
				G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
			} else if (backwards) {
				G_SetAnimation(ent, ANIM_LEGS_WALKCR | ANIM_REVERSE_BIT, false);
			} else {
				G_SetAnimation(ent, ANIM_LEGS_WALKCR, false);
			}

			return;
		}

		if (cl->speed < 1.0 && !cl->cmd.forward && !cl->cmd.right) {
			G_SetAnimation(ent, ANIM_LEGS_IDLE, false);
			return;
		}

		entity_animation_t anim = ANIM_LEGS_RUN;

		if (cl->speed < 200.0) {
			anim = ANIM_LEGS_WALK;

			if (backwards) {
				anim |= ANIM_REVERSE_BIT;
			}
		} else {
			if (backwards) {
				anim = ANIM_LEGS_BACK;
			}
		}

		G_SetAnimation(ent, anim, false);
		return;
	}
}

/**
 * @brief Called for each client at the end of the server frame.
 */
void G_ClientEndFrame(g_entity_t *ent) {

	g_client_t *client = ent->client;

	// If the origin or velocity have changed since G_ClientThink(),
	// update the pm_state_t values. This will happen when the client
	// is pushed by another entity or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	client->ps.pm_state.origin = ent->s.origin;
	client->ps.pm_state.velocity = ent->locals.velocity;

	// If in intermission, just set stats and scores and return
	if (g_level.intermission_time) {
		G_ClientStats(ent);
		G_ClientScores(ent);
		return;
	}

	// check for water entry / exit, burn from lava, slime, etc
	G_ClientWaterInteraction(ent);

	// set the stats for this client
	if (ent->client->locals.persistent.spectator) {
		G_ClientSpectatorStats(ent);
	} else {
		G_ClientStats(ent);
	}

	// apply all the damage taken this frame
	G_ClientDamage(ent);

	// send the kick angles
	G_ClientKickAngles(ent);

	// and the angles on the world model
	G_ClientWorldAngles(ent);

	// update the player's animations
	G_ClientAnimation(ent);

	// if the scoreboard is up, update it
	if (ent->client->locals.show_scores) {
		G_ClientScores(ent);
	}
}

/**
 * @brief
 */
void G_EndClientFrames(void) {

	// finalize the player_state_t for this frame
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = g_game.entities + 1 + i;

		if (!ent->in_use || !ent->client) {
			continue;
		}

		G_ClientEndFrame(ent);
	}

	// now loop through again, and for chase camera users, copy the final player state
	for (int32_t i = 0; i < sv_max_clients->integer; i++) {

		g_entity_t *ent = g_game.entities + 1 + i;

		if (!ent->in_use || !ent->client) {
			continue;
		}

		if (ent->client->locals.chase_target) {
			G_ClientChaseThink(ent);
			G_ClientSpectatorStats(ent);
		}
	}
}
