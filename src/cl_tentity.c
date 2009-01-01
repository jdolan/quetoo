/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "client.h"

sfx_t *cl_sfx_bfg_hit;
sfx_t *cl_sfx_hyperblaster_hit;
sfx_t *cl_sfx_lightning_discharge;
sfx_t *cl_sfx_explosion;
sfx_t *cl_sfx_machinegun_hit[3];
sfx_t *cl_sfx_spark[3];
sfx_t *cl_sfx_footsteps[4];

/*
 * Cl_LoadTempEntitySounds
 */
void Cl_LoadTempEntitySounds(void){
	int i;
	char name[MAX_QPATH];

	cl_sfx_bfg_hit = S_LoadSound("weapons/bfg/hit.wav");
	cl_sfx_hyperblaster_hit = S_LoadSound("weapons/hyperblaster/hit.wav");
	cl_sfx_lightning_discharge = S_LoadSound("weapons/lightning/discharge.wav");
	cl_sfx_explosion = S_LoadSound("weapons/common/explosion.wav");

	for(i = 0; i < 3; i++){
		snprintf(name, sizeof(name), "weapons/machinegun/hit_%i.wav", i + 1);
		cl_sfx_machinegun_hit[i] = S_LoadSound(name);
	}

	for(i = 0; i < 3; i++){
		snprintf(name, sizeof(name), "world/spark_%i.wav", i + 1);
		cl_sfx_spark[i] = S_LoadSound(name);
	}

	for(i = 0; i < 4; i++){
		snprintf(name, sizeof(name), "#players/common/step_%i.wav", i + 1);
		cl_sfx_footsteps[i] = S_LoadSound(name);
	}
}


vec3_t burn_light = {
	0.4, 0.7, 1.0
};
vec3_t sparks_light = {
	0.7, 0.5, 0.5
};
vec3_t lightning_det_light = {
	0.6, 0.6, 1.0
};
vec3_t rail_light = {
	0.0, 0.0, 0.0
};
vec3_t explosion_light = {
	1.0, 0.5, 0.3
};

/*
 * Cl_ParseTempEntity
 */
void Cl_ParseTempEntity(void){
	static int last_ric_time;
	int i, type;
	vec3_t pos, pos2, dir;

	type = Msg_ReadByte(&net_message);

	switch(type){

		case TE_TRACER:
			Msg_ReadPos(&net_message, pos);
			Msg_ReadPos(&net_message, pos2);
			Cl_BulletTrail(pos, pos2);
			break;

		case TE_BULLET:  // bullet hitting wall
			Msg_ReadPos(&net_message, pos);
			Msg_ReadDir(&net_message, dir);
			Cl_BulletEffect(pos, dir);
			if(cl.time - last_ric_time > 300){
				S_StartSound(pos, 0, 0, cl_sfx_machinegun_hit[rand() % 3], 1, ATTN_NORM, 0);
				last_ric_time = cl.time;
			}
			break;

		case TE_BURN:   // burn mark on wall
			Msg_ReadPos(&net_message, pos);
			Msg_ReadDir(&net_message, dir);
			i = Msg_ReadByte(&net_message);
			Cl_BurnEffect(pos, dir, i);
			break;

		case TE_BLOOD:  // projectile hitting flesh
			Msg_ReadPos(&net_message, pos);
			Msg_ReadDir(&net_message, dir);
			Cl_BloodEffect(pos, dir, 12);
			break;

		case TE_SPARKS:  // colored sparks
			Msg_ReadPos(&net_message, pos);
			Msg_ReadDir(&net_message, dir);
			Cl_SparksEffect(pos, dir, 12);
			S_StartSound(pos, 0, 0, cl_sfx_spark[rand() % 3], 1, ATTN_STATIC, 0);
			R_AddSustainedLight(pos, 1.0, sparks_light, 0.65);
			break;

		case TE_HYPERBLASTER:  // hyperblaster hitting wall
			Msg_ReadPos(&net_message, pos);
			S_StartSound(pos, 0, 0, cl_sfx_hyperblaster_hit, 1, ATTN_NORM, 0);
			R_AddSustainedLight(pos, 1.0, burn_light, 0.25);
			break;

		case TE_LIGHTNING:  // lightning detonation in water
			Msg_ReadPos(&net_message, pos);
			Cl_LightningEffect(pos);
			S_StartSound(pos, 0, 0, cl_sfx_lightning_discharge, 1, ATTN_NORM, 0);
			R_AddSustainedLight(pos, 1.25, lightning_det_light, 0.75);
			break;

		case TE_RAILTRAIL:  // railgun effect
			Msg_ReadPos(&net_message, pos);
			Msg_ReadPos(&net_message, pos2);
			i = Msg_ReadByte(&net_message);
			Cl_RailTrail(pos, pos2, i);
			Img_ColorFromPalette(i, rail_light);
			R_AddSustainedLight(pos, 1.25, rail_light, 0.75);
			VectorSubtract(pos2, pos, dir);
			VectorNormalize(dir);
			VectorMA(pos2, -12.0, dir, pos2);
			R_AddSustainedLight(pos2, 1.0, rail_light, 0.75);
			break;

		case TE_EXPLOSION:  // rocket and grenade explosions
			Msg_ReadPos(&net_message, pos);
			Cl_ExplosionEffect(pos);
			S_StartSound(pos, 0, 0, cl_sfx_explosion, 1, ATTN_NORM, 0);
			R_AddSustainedLight(pos, 2.5, explosion_light, 1.0);
			break;

		case TE_BFG:  // bfg explosion
			Msg_ReadPos(&net_message, pos);
			Cl_BFGEffect(pos);
			S_StartSound(pos, 0, 0, cl_sfx_bfg_hit, 0.5, ATTN_NORM, 0);
			R_AddSustainedLight(pos, 2.5, burn_light, 1.0);
			break;

		case TE_BUBBLETRAIL:  // bubbles chasing projectiles in water
			Msg_ReadPos(&net_message, pos);
			Msg_ReadPos(&net_message, pos2);
			Cl_BubbleTrail(pos, pos2, 1.0);
			break;

		default:
			Com_Warn("Cl_ParseTempEntity: Unknown type: %d\n.", type);
			break;
	}
}
