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

s_sample_t *cl_sample_bfg_hit;
s_sample_t *cl_sample_hyperblaster_hit;
s_sample_t *cl_sample_lightning_discharge;
s_sample_t *cl_sample_explosion;
s_sample_t *cl_sample_sparks;
s_sample_t *cl_sample_machinegun_hit[3];

/*
 * Cl_LoadTempEntitySamples
 */
void Cl_LoadTempEntitySamples(void){
	int i;
	char name[MAX_QPATH];

	cl_sample_bfg_hit = S_LoadSample("weapons/bfg/hit");
	cl_sample_hyperblaster_hit = S_LoadSample("weapons/hyperblaster/hit");
	cl_sample_lightning_discharge = S_LoadSample("weapons/lightning/discharge");
	cl_sample_explosion = S_LoadSample("weapons/common/explosion");
	cl_sample_sparks = S_LoadSample("world/sparks");

	for(i = 0; i < 3; i++){
		snprintf(name, sizeof(name), "weapons/machinegun/hit_%i", i + 1);
		cl_sample_machinegun_hit[i] = S_LoadSample(name);
	}
}


static const vec3_t sparks_light = {
	0.7, 0.5, 0.5
};
static const vec3_t explosion_light = {
	1.0, 0.5, 0.3
};
static const vec3_t hyperblaster_hit_light = {
	0.4, 0.7, 1.0
};
static const vec3_t lightning_det_light = {
	0.6, 0.6, 1.0
};
static const vec3_t bfg_hit_light = {
	0.5, 1.0, 0.5
};

/*
 * Cl_ParseTempEntity
 */
void Cl_ParseTempEntity(void){
	static int last_ric_time;
	int i, type;
	vec3_t pos, pos2, dir, light;

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
				S_PlaySample(pos, -1, cl_sample_machinegun_hit[rand() % 3], ATTN_NORM);
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

		case TE_GIB:  // player death
			Msg_ReadPos(&net_message, pos);
			Cl_GibEffect(pos, 12);
			break;

		case TE_SPARKS:  // colored sparks
			Msg_ReadPos(&net_message, pos);
			Msg_ReadDir(&net_message, dir);
			Cl_SparksEffect(pos, dir, 12);
			S_PlaySample(pos, -1, cl_sample_sparks, ATTN_STATIC);
			R_AddSustainedLight(pos, 1.0, sparks_light, 0.65);
			break;

		case TE_HYPERBLASTER:  // hyperblaster hitting wall
			Msg_ReadPos(&net_message, pos);
			S_PlaySample(pos, -1, cl_sample_hyperblaster_hit, ATTN_NORM);
			R_AddSustainedLight(pos, 1.0, hyperblaster_hit_light, 0.25);
			break;

		case TE_LIGHTNING:  // lightning detonation in water
			Msg_ReadPos(&net_message, pos);
			Cl_LightningEffect(pos);
			S_PlaySample(pos, -1, cl_sample_lightning_discharge, ATTN_NORM);
			R_AddSustainedLight(pos, 1.25, lightning_det_light, 0.75);
			break;

		case TE_RAILTRAIL:  // railgun effect
			Msg_ReadPos(&net_message, pos);
			Msg_ReadPos(&net_message, pos2);
			i = Msg_ReadByte(&net_message);
			Cl_RailTrail(pos, pos2, i);
			Img_ColorFromPalette(i, light);
			R_AddSustainedLight(pos, 1.25, light, 0.75);
			VectorSubtract(pos2, pos, dir);
			VectorNormalize(dir);
			VectorMA(pos2, -12.0, dir, pos2);
			R_AddSustainedLight(pos2, 1.0, light, 0.75);
			break;

		case TE_EXPLOSION:  // rocket and grenade explosions
			Msg_ReadPos(&net_message, pos);
			Cl_ExplosionEffect(pos);
			S_PlaySample(pos, -1, cl_sample_explosion, ATTN_NORM);
			R_AddSustainedLight(pos, 2.5, explosion_light, 1.0);
			break;

		case TE_BFG:  // bfg explosion
			Msg_ReadPos(&net_message, pos);
			Cl_BFGEffect(pos);
			S_PlaySample(pos, -1, cl_sample_bfg_hit, ATTN_NORM);
			R_AddSustainedLight(pos, 2.5, bfg_hit_light, 1.0);
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
