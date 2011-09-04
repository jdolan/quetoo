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

static edict_t *current_player;
static g_client_t *current_client;


/*
 * G_ClientDamage
 *
 * Inspect all damage received this frame and play a pain sound if appropriate.
 */
static void G_ClientDamage(edict_t *player){
	g_client_t *client;
	int l;

	client = player->client;

	if(client->damage_blood + client->damage_armor == 0)
		return;  // didn't take any damage

	// play an appropriate pain sound
	if(g_level.time > player->pain_time){
		player->pain_time = g_level.time + 0.7;
		if(player->health < 25)
			l = 25;
		else if(player->health < 50)
			l = 50;
		else if(player->health < 75)
			l = 75;
		else
			l = 100;
		gi.Sound(player, gi.SoundIndex(va("*pain%i_1", l)), ATTN_NORM);
	}

	// clear totals
	client->damage_blood = 0;
	client->damage_armor = 0;
}


/*
 * G_ClientFall
 */
static void G_ClientFall(edict_t *ent){

	if(!ent->ground_entity)
		return;

	if(ent->health < 1 || ent->water_level == 3)
		return;

	if(ent->client->fall_time > g_level.time)
		return;

	float fall = -ent->client->old_velocity[2];

	if(fall < 220.0)
		return;

	entity_event_t event = EV_CLIENT_LAND;

	if(fall > 400.0){  // player will take damage
		int damage = ((int)(fall * 0.01)) >> ent->water_level;
		vec3_t dir;

		if(fall > 450.0)
			event = EV_CLIENT_FALL_FAR;
		else
			event = EV_CLIENT_FALL;

		ent->pain_time = g_level.time;  // suppress pain sound

		VectorSet(dir, 0.0, 0.0, 1.0);

		G_Damage(ent, NULL, NULL, dir, ent->s.origin,
				vec3_origin, damage, 0, 0, MOD_FALLING);
	}

	if(ent->s.event != EV_TELEPORT){  // don't override teleport events
		ent->client->fall_time = g_level.time + 0.3;
		ent->s.event = event;
	}
}


/*
 * G_ClientWaterLevel
 */
static void G_ClientWaterLevel(void){
	int water_level, old_water_level;

	if(current_player->move_type == MOVE_TYPE_NO_CLIP){
		current_player->drown_time = g_level.time + 12;  // don't need air
		return;
	}

	water_level = current_player->water_level;
	old_water_level = current_client->old_water_level;
	current_client->old_water_level = water_level;

	// if just entered a water volume, play a sound
	if(!old_water_level && water_level)
		gi.Sound(current_player, gi.SoundIndex("world/water_in"), ATTN_NORM);

	// completely exited the water
	if(old_water_level && !water_level)
		gi.Sound(current_player, gi.SoundIndex("world/water_out"), ATTN_NORM);

	// head just coming out of water
	if(old_water_level == 3 && water_level != 3 &&
			g_level.time - current_player->gasp_time > 2.0){

		gi.Sound(current_player, gi.SoundIndex("*gasp_1"), ATTN_NORM);
		current_player->gasp_time = g_level.time;
	}

	// check for drowning
	if(water_level != 3){  // take some air, push out drown time
		current_player->drown_time = g_level.time + 12.0;
	}
	else {  // we're under water
		if(current_player->drown_time < g_level.time){  // drown
			if(current_client->drown_time < g_level.time && current_player->health > 0){
				current_client->drown_time = g_level.time + 1.0;

				// take more damage the longer under water
				current_player->dmg += 2;
				if(current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if(current_player->health <= current_player->dmg)
					gi.Sound(current_player, gi.SoundIndex("*drown_1"), ATTN_NORM);
				else
					gi.Sound(current_player, gi.SoundIndex("*gurp_1"), ATTN_NORM);

				current_player->pain_time = g_level.time;

				G_Damage(current_player, NULL, NULL, vec3_origin, current_player->s.origin,
						vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	}

	// check for sizzle damage
	if(water_level && (current_player->water_type & (CONTENTS_LAVA | CONTENTS_SLIME))){
		if(current_client->sizzle_time <= g_level.time && current_player->health > 0){

			current_client->sizzle_time = g_level.time + 0.1;

			if(current_player->water_type & CONTENTS_LAVA){
				G_Damage(current_player, NULL, NULL, vec3_origin, current_player->s.origin,
						vec3_origin, 2 * water_level, 0, 0, MOD_LAVA);
			}

			if(current_player->water_type & CONTENTS_SLIME){
				G_Damage(current_player, NULL, NULL, vec3_origin, current_player->s.origin,
						vec3_origin, 1 * water_level, 0, 0, MOD_SLIME);
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
static void G_ClientAnimation(edict_t *ent){

	if(ent->ground_entity){  // on the ground

		if(ent->water_level < 2){  // not swimming
			vec3_t velocity;
			float speed;

			VectorCopy(ent->velocity, velocity);
			velocity[2] = 0.0;

			speed = VectorNormalize(velocity);

			if(ent->client->ps.pmove.pm_flags & PMF_DUCKED){  // ducked
				if(speed < 1.0)
					G_SetAnimation(ent, ANIM_LEGS_IDLECR, false);
				else
					G_SetAnimation(ent, ANIM_LEGS_WALKCR, false);
			}
			else {  // landing / standing / walking / running
				const entity_event_t e = ent->s.event;

				if(e >= EV_CLIENT_LAND && e <= EV_CLIENT_FALL_FAR){  // just landed
					G_SetAnimation(ent, ANIM_LEGS_LAND1, false);
				}
				else if(speed < 1.0){
					G_SetAnimation(ent, ANIM_LEGS_IDLE, false);
				}
				else {
					vec3_t angles, forward;

					VectorSet(angles, 0.0, ent->s.angles[YAW], 0.0);
					AngleVectors(angles, forward, NULL, NULL);

					if(DotProduct(velocity, forward) < 0.0)
						G_SetAnimation(ent, ANIM_LEGS_BACK, false);
					else if(speed < 150.0)
						G_SetAnimation(ent, ANIM_LEGS_WALK, false);
					else
						G_SetAnimation(ent, ANIM_LEGS_RUN, false);
				}
			}
		}
		else {
			G_SetAnimation(ent, ANIM_LEGS_SWIM, false);
		}
	}
	else {
		G_SetAnimation(ent, ANIM_LEGS_JUMP1, false);
	}
}


/*
 * G_ClientEndFrame
 *
 * Called for each client at the end of the server frame.
 */
void G_ClientEndFrame(edict_t *ent){
	vec3_t forward, right, up;
	float dot, xy_speed;
	int i;

	current_player = ent;
	current_client = ent->client;

	// If the origin or velocity have changed since G_Think(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	for(i = 0; i < 3; i++){
		current_client->ps.pmove.origin[i] = ent->s.origin[i] * 8.0;
		current_client->ps.pmove.velocity[i] = ent->velocity[i] * 8.0;
	}

	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	if(g_level.intermission_time){
		G_ClientStats(ent);
		return;
	}

	// check for water entry / exit, burn from lava, slime, etc
	G_ClientWaterLevel();

	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	if(ent->client->angles[PITCH] > 180.0)
		ent->s.angles[PITCH] = (-360.0 + ent->client->angles[PITCH]) / 3.0;
	else
		ent->s.angles[PITCH] = ent->client->angles[PITCH] / 3.0;
	ent->s.angles[YAW] = ent->client->angles[YAW];

	// set roll based on lateral velocity
	AngleVectors(ent->client->angles, forward, right, up);
	dot = DotProduct(ent->velocity, right);
	ent->s.angles[ROLL] = dot * 0.025;

	// less roll when in the air
	if(!ent->ground_entity)
		ent->s.angles[ROLL] *= 0.25;

	// detect hitting the floor
	G_ClientFall(ent);

	// check for footsteps
	if(ent->ground_entity && !ent->s.event){

		xy_speed = sqrt(ent->velocity[0] * ent->velocity[0] +
				ent->velocity[1] * ent->velocity[1]);

		if(xy_speed > 265.0 && ent->client->footstep_time < g_level.time){
			ent->client->footstep_time = g_level.time + 0.3;
			ent->s.event = EV_CLIENT_FOOTSTEP;
		}
	}

	// apply all the damage taken this frame
	G_ClientDamage(ent);

	// update the player's animations
	G_ClientAnimation(ent);

	// set the stats for this client
	if(ent->client->locals.spectator)
		G_ClientSpectatorStats(ent);
	else
		G_ClientStats(ent);

	VectorCopy(ent->velocity, ent->client->old_velocity);
	VectorCopy(ent->client->ps.angles, ent->client->old_angles);

	// if the scoreboard is up, update it
	if(ent->client->show_scores && !(g_level.frame_num % gi.frame_rate)){
		if(g_level.teams || g_level.ctf)
			G_ClientTeamsScoreboard(ent);
		else
			G_ClientScoreboard(ent);
		gi.Unicast(ent, false);
	}
}


/*
 * G_EndClientFrames
 */
void G_EndClientFrames(void){
	int i;
	edict_t *ent;

	// finalize the player_state_t for this frame
	for(i = 0; i < sv_max_clients->integer; i++){

		ent = g_game.edicts + 1 + i;

		if(!ent->in_use || !ent->client)
			continue;

		G_ClientEndFrame(ent);
	}
}
