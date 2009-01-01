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


/*
 * Cl_ParseEntityBits
 * 
 * Returns the entity number and the header bits
 */
unsigned int Cl_ParseEntityBits(unsigned int *bits){
	unsigned int b, total;
	unsigned int number;

	total = Msg_ReadByte(&net_message);
	if(total & U_MOREBITS1){
		b = Msg_ReadByte(&net_message);
		total |= b << 8;
	}
	if(total & U_MOREBITS2){
		b = Msg_ReadByte(&net_message);
		total |= b << 16;
	}
	if(total & U_MOREBITS3){
		b = Msg_ReadByte(&net_message);
		total |= b << 24;
	}

	if(total & U_NUMBER16)
		number = Msg_ReadShort(&net_message);
	else
		number = Msg_ReadByte(&net_message);

	*bits = total;

	return number;
}


/*
 * Cl_ParseDelta
 * 
 * Can go from either a baseline or a previous packet_entity
 */
void Cl_ParseDelta(const entity_state_t *from, entity_state_t *to, int number, int bits){

	// set everything to the state we are delta'ing from
	*to = *from;

	if(!(from->effects & EF_BEAM))
		VectorCopy(from->origin, to->old_origin);

	to->number = number;

	if(bits & U_MODEL)
		to->modelindex = Msg_ReadByte(&net_message);
	if(bits & U_MODEL2)
		to->modelindex2 = Msg_ReadByte(&net_message);
	if(bits & U_MODEL3)
		to->modelindex3 = Msg_ReadByte(&net_message);
	if(bits & U_MODEL4)
		to->modelindex4 = Msg_ReadByte(&net_message);

	if(bits & U_FRAME)
		to->frame = Msg_ReadByte(&net_message);

	if(bits & U_SKIN8)
		to->skinnum = Msg_ReadByte(&net_message);
	else if(bits & U_SKIN16)
		to->skinnum = Msg_ReadShort(&net_message);

	if(bits & U_EFFECTS8)
		to->effects = Msg_ReadByte(&net_message);
	else if(bits & U_EFFECTS16)
		to->effects = Msg_ReadShort(&net_message);

	if(bits & U_ORIGIN1)
		to->origin[0] = Msg_ReadCoord(&net_message);
	if(bits & U_ORIGIN2)
		to->origin[1] = Msg_ReadCoord(&net_message);
	if(bits & U_ORIGIN3)
		to->origin[2] = Msg_ReadCoord(&net_message);

	if(bits & U_ANGLE1)
		to->angles[0] = Msg_ReadAngle(&net_message);
	if(bits & U_ANGLE2)
		to->angles[1] = Msg_ReadAngle(&net_message);
	if(bits & U_ANGLE3)
		to->angles[2] = Msg_ReadAngle(&net_message);

	if(bits & U_OLDORIGIN)
		Msg_ReadPos(&net_message, to->old_origin);

	if(bits & U_SOUND)
		to->sound = Msg_ReadByte(&net_message);

	if(bits & U_EVENT)
		to->event = Msg_ReadByte(&net_message);
	else
		to->event = 0;

	if(bits & U_SOLID)
		to->solid = Msg_ReadShort(&net_message);
}


/*
 * Cl_DeltaEntity
 * 
 * Parses deltas from the given base and adds the resulting entity
 * to the current frame
 */
static void Cl_DeltaEntity(frame_t *frame, int newnum, entity_state_t *old, int bits){
	centity_t *ent;
	entity_state_t *state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES - 1)];
	cl.parse_entities++;
	frame->num_entities++;

	Cl_ParseDelta(old, state, newnum, bits);

	// some data changes will force no lerping
	if(state->modelindex != ent->current.modelindex
			|| state->modelindex2 != ent->current.modelindex2
			|| state->modelindex3 != ent->current.modelindex3
			|| state->modelindex4 != ent->current.modelindex4
			|| abs(state->origin[0] - ent->current.origin[0]) > 512
			|| abs(state->origin[1] - ent->current.origin[1]) > 512
			|| abs(state->origin[2] - ent->current.origin[2]) > 512
			|| state->event == EV_TELEPORT
	  ){
		ent->serverframe = -99;
	}

	if(ent->serverframe != cl.frame.serverframe - 1){
		// wasn't in last update, so initialize some things
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		ent->lighting.dirty = true;
		VectorCopy(state->old_origin, ent->prev.origin);
	} else {  // shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;

	// bump animation time for frame interpolation
	if(ent->current.frame != ent->prev.frame){
		ent->anim_time = cl.time;
		ent->anim_frame = ent->prev.frame;
	}
}


/*
 * Cl_ParseEntities
 * 
 * An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const frame_t *oldframe, frame_t *newframe){
	unsigned int bits;
	entity_state_t *oldstate = NULL;
	int oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;

	if(!oldframe)
		oldnum = 99999;
	else {
		if(oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else {
			oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}

	while(true){

		const int newnum = Cl_ParseEntityBits(&bits);

		if(newnum >= MAX_EDICTS){
			Com_Error(ERR_DROP, "Cl_ParseEntities: bad number: %i.", newnum);
		}

		if(net_message.readcount > net_message.cursize){
			Com_Error(ERR_DROP, "Cl_ParseEntities: end of message.");
		}

		if(!newnum)
			break;

		while(oldnum < newnum){  // one or more entities from oldframe are unchanged

			if(cl_shownet->value == 3)
				Com_Printf("   unchanged: %i\n", oldnum);

			Cl_DeltaEntity(newframe, oldnum, oldstate, 0);

			oldindex++;

			if(oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
		}

		if(bits & U_REMOVE){  // the entity present in the oldframe, and is not in the current frame

			if(cl_shownet->value == 3)
				Com_Printf("   remove: %i\n", newnum);

			if(oldnum != newnum)
				Com_Warn("Cl_ParseEntities: U_REMOVE: oldnum != newnum.\n");

			oldindex++;

			if(oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if(oldnum == newnum){  // delta from previous state

			if(cl_shownet->value == 3)
				Com_Printf("   delta: %i\n", newnum);

			Cl_DeltaEntity(newframe, newnum, oldstate, bits);

			oldindex++;

			if(oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if(oldnum > newnum){  // delta from baseline

			if(cl_shownet->value == 3)
				Com_Printf("   baseline: %i\n", newnum);

			Cl_DeltaEntity(newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while(oldnum != 99999){  // one or more entities from the old packet are unchanged

		if(cl_shownet->value == 3)
			Com_Printf("   unchanged: %i\n", oldnum);

		Cl_DeltaEntity(newframe, oldnum, oldstate, 0);

		oldindex++;

		if(oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else {
			oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}
}


/*
 * Cl_ParsePlayerstate
 */
static void Cl_ParsePlayerstate(const frame_t *oldframe, frame_t *newframe){
	player_state_t *state;
	byte flags;
	int i;
	int statbits;

	state = &newframe->playerstate;

	// copy old value before delta parsing
	if(oldframe)
		*state = oldframe->playerstate;
	else  // or start clean
		memset(state, 0, sizeof(*state));

	flags = Msg_ReadByte(&net_message);

	// parse the pmove_state_t

	if(flags & PS_M_TYPE)
		state->pmove.pm_type = Msg_ReadByte(&net_message);

	if(cl.demoserver)
		state->pmove.pm_type = PM_FREEZE;

	if(flags & PS_M_ORIGIN){
		state->pmove.origin[0] = Msg_ReadShort(&net_message);
		state->pmove.origin[1] = Msg_ReadShort(&net_message);
		state->pmove.origin[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_M_VELOCITY){
		state->pmove.velocity[0] = Msg_ReadShort(&net_message);
		state->pmove.velocity[1] = Msg_ReadShort(&net_message);
		state->pmove.velocity[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_M_TIME)
		state->pmove.pm_time = Msg_ReadByte(&net_message);

	if(flags & PS_M_FLAGS)
		state->pmove.pm_flags = Msg_ReadByte(&net_message);

	if(flags & PS_M_DELTA_ANGLES){
		state->pmove.delta_angles[0] = Msg_ReadShort(&net_message);
		state->pmove.delta_angles[1] = Msg_ReadShort(&net_message);
		state->pmove.delta_angles[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_VIEWANGLES){  // demo, chasecam, recording
		state->angles[0] = Msg_ReadAngle16(&net_message);
		state->angles[1] = Msg_ReadAngle16(&net_message);
		state->angles[2] = Msg_ReadAngle16(&net_message);
	}

	// parse stats
	statbits = Msg_ReadLong(&net_message);
	for(i = 0; i < MAX_STATS; i++)
		if(statbits & (1 << i))
			state->stats[i] = Msg_ReadShort(&net_message);
}


/*
 * Cl_FireEntityEvents
 */
static void Cl_EntityEvents(frame_t *frame){
	int pnum;

	for(pnum = 0; pnum < frame->num_entities; pnum++){
		const int num = (frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1);
		Cl_EntityEvent(&cl_parse_entities[num]);
	}
}


/*
 * Cl_ParseFrame
 */
void Cl_ParseFrame(void){
	size_t len;
	frame_t *old;

	if(!cl.serverrate){  // avoid unstable reconnects
		Cl_Reconnect_f();
		return;
	}

	cl.frame.serverframe = Msg_ReadLong(&net_message);
	cl.frame.servertime = cl.frame.serverframe * 1000 / cl.serverrate;

	cl.frame.deltaframe = Msg_ReadLong(&net_message);

	cl.surpress_count = Msg_ReadByte(&net_message);

	if(cl_shownet->value == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverframe, cl.frame.deltaframe);

	if(cl.frame.deltaframe <= 0){  // uncompressed frame
		cls.demowaiting = false;
		cl.frame.valid = true;
		old = NULL;
	} else {  // delta compressed frame

		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if(!old->valid)
			Com_Warn("Cl_ParseFrame: Delta from invalid frame.\n");
		else if(old->serverframe != cl.frame.deltaframe)
			Com_Warn("Cl_ParseFrame: Delta frame too old.\n");
		else if(cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES - 128)
			Com_Warn("Cl_ParseFrame: Delta parse_entities too old.\n");
		else
			cl.frame.valid = true;
	}

	len = Msg_ReadByte(&net_message); // read areabits
	Msg_ReadData(&net_message, &cl.frame.areabits, len);

	Cl_ParsePlayerstate(old, &cl.frame);

	Cl_ParseEntities(old, &cl.frame);

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if(cl.frame.valid){
		// getting a valid frame message ends the connection process
		if(cls.state != ca_active){
			cls.state = ca_active;

			VectorCopy(cl.frame.playerstate.pmove.origin, cl.predicted_origin);
			VectorScale(cl.predicted_origin, 0.125, cl.predicted_origin);

			VectorCopy(cl.frame.playerstate.angles, cl.predicted_angles);
		}

		Cl_EntityEvents(&cl.frame); // fire entity events

		Cl_CheckPredictionError();
	}
}


/*
 * Cl_AddWeapon
 */
static void Cl_AddWeapon(void){
	static entity_t ent;
	static static_lighting_t lighting;
	int w;

	if(!cl_weapon->value)
		return;

	if(cl_thirdperson->value)
		return;

	if(!cl.frame.playerstate.stats[STAT_HEALTH])
		return;  // deadz0r

	if(cl.frame.playerstate.stats[STAT_SPECTATOR])
		return;  // spectating

	if(cl.frame.playerstate.stats[STAT_CHASE])
		return;  // chasecam

	w = cl.frame.playerstate.stats[STAT_WEAPON];
	if(!w)  // no weapon, e.g. level intermission
		return;

	memset(&ent, 0, sizeof(ent));

	ent.flags = EF_WEAPON;

	VectorCopy(r_view.origin, ent.origin);
	VectorCopy(r_view.angles, ent.angles);

	ent.model = cl.model_draw[w];

	if(!ent.model)  // for development
		return;

	ent.lerp = 1.0;
	VectorSet(ent.scale, 1.0, 1.0, 1.0);

	ent.lighting = &lighting;
	ent.lighting->dirty = true;

	R_AddEntity(&ent);
}


static const vec3_t rocket_light = {
	1.0, 0.4, 0.4
};
static const vec3_t grenade_light = {
	0.3, 0.2, 0.07
};
static const vec3_t ctf_blue_light = {
	0.3, 1.0, 3.0
};
static const vec3_t ctf_red_light = {
	1.0, 0.3, 0.3
};
static const vec3_t hyperblaster_light = {
	0.4, 0.7, 0.9
};
static const vec3_t lightning_light = {
	0.6, 0.6, 1.0
};
static const vec3_t bfg_light = {
	0.6, 0.8, 1.0
};

/*
 * Cl_AddEntities
 */
void Cl_AddEntities(frame_t *frame){
	entity_t ent;
	vec3_t start, end, forward;
	int pnum;

	if(!cl_addentities->value)
		return;

	VectorClear(start);
	VectorClear(end);

	AngleVectors(r_view.angles, forward, NULL, NULL);

	// resolve any animations, lerps, rotations, bobbing, etc..
	for(pnum = 0; pnum < frame->num_entities; pnum++){

		entity_state_t *state = &cl_parse_entities[(frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1)];

		centity_t *cent = &cl_entities[state->number];

		// beams have two origins, most ents have just one
		if(state->effects & EF_BEAM){

			// skinnum is overridden to specify owner of the beam
			if((state->skinnum == cl.playernum + 1) && !cl_thirdperson->value){
				// we own this beam (lightning, grapple, etc..)
				// project start position infront of view origin
				VectorCopy(r_view.origin, start);
				start[2] -= 6;
				VectorMA(start, 16, forward, start);
			}
			else  // or simply lerp the start position
				VectorLerp(cent->prev.origin, cent->current.origin, cl.lerp, start);

			VectorLerp(cent->prev.old_origin, cent->current.old_origin, cl.lerp, end);
		}
		else {  // for most ents, just lerp the origin
			VectorLerp(cent->prev.origin, cent->current.origin, cl.lerp, ent.origin);
		}

		// bob items, shift them to randomize the effect in crowded scenes
		if(state->effects & EF_BOB)
			ent.origin[2] += 4 * sin((cl.time * 0.005) + ent.origin[0] + ent.origin[1]);

		// calculate angles
		if(state->effects & EF_ROTATE){  // some bonus items rotate
			ent.angles[0] = 0;
			ent.angles[1] = cl.time / 4.0;
			ent.angles[2] = 0;
		}
		else {  // lerp angles
			AngleLerp(cent->prev.angles, cent->current.angles, cl.lerp, ent.angles);
		}

		// lerp frames
		if(state->effects & EF_ANIMATE)
			ent.frame = cl.time / 500;
		else if(state->effects & EF_ANIMATE_FAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = state->frame;

		ent.oldframe = cent->anim_frame;

		ent.lerp = (cl.time - cent->anim_time) / 100.0;

		if(ent.lerp < 0.0)  // clamp
			ent.lerp = 0.0;
		else if(ent.lerp > 1.0)
			ent.lerp = 1.0;

		ent.backlerp = 1.0 - ent.lerp;

		VectorSet(ent.scale, 1.0, 1.0, 1.0);  // scale

		// set skin
		if(state->modelindex == 255){  // use custom player skin
			const clientinfo_t *ci = &cl.clientinfo[state->skinnum & 0xff];
			ent.skinnum = 0;
			ent.skin = ci->skin;
			ent.model = ci->model;
			if(!ent.skin || !ent.model){
				ent.skin = cl.baseclientinfo.skin;
				ent.model = cl.baseclientinfo.model;
			}
		} else {
			ent.skinnum = state->skinnum;
			ent.skin = NULL;
			ent.model = cl.model_draw[state->modelindex];
		}

		if(state->effects & (EF_ROCKET | EF_GRENADE))
			Cl_SmokeTrail(cent->prev.origin, ent.origin, cent);

		if(state->effects & EF_ROCKET){
			R_AddCorona(ent.origin, 10.0, rocket_light);
			R_AddLight(ent.origin, 1.5, rocket_light);
		}

		if(state->effects & EF_GRENADE){
			R_AddCorona(ent.origin, 10.0, grenade_light);
			R_AddLight(ent.origin, 1.0, grenade_light);
		}

		if(state->effects & EF_HYPERBLASTER){
			R_AddCorona(ent.origin, 12.0, hyperblaster_light);
			R_AddLight(ent.origin, 1.25, hyperblaster_light);

			Cl_EnergyTrail(cent, 10.0);
		}

		if(state->effects & EF_LIGHTNING){
			vec3_t dir;
			Cl_LightningTrail(start, end);

			R_AddLight(start, 1.0, lightning_light);
			VectorSubtract(end, start, dir);
			VectorNormalize(dir);
			VectorMA(end, -12.0, dir, end);
			R_AddLight(end, 1.0 + (0.1 * crand()), lightning_light);
		}

		if(state->effects & EF_BFG){
			R_AddCorona(ent.origin, 24.0, bfg_light);
			R_AddLight(ent.origin, 1.5, bfg_light);

			Cl_EnergyTrail(cent, 20.0);
		}

		if(state->effects & EF_CTF_BLUE)
			R_AddLight(ent.origin, 1.5, ctf_blue_light);

		if(state->effects & EF_CTF_RED)
			R_AddLight(ent.origin, 1.5, ctf_red_light);

		if(state->effects & EF_TELEPORTER)
			Cl_TeleporterTrail(ent.origin, cent);

		// if set to invisible, skip
		if(!state->modelindex)
			continue;

		// don't draw ourselves unless third person is set
		if(state->number == cl.playernum + 1){
			if(!cl_thirdperson->value)
				continue;
		}

		ent.flags = state->effects;

		// setup the write-through lighting cache
		ent.lighting = &cent->lighting;

		if(ent.skin)  // always re-light players
			ent.lighting->dirty = true;

		if(!VectorCompare(cent->current.origin, cent->prev.origin))
			ent.lighting->dirty = true;  // or anything that moves

		// add to view list
		R_AddEntity(&ent);

		// now check for linked models
		ent.skin = NULL;
		ent.flags = 0;

		if(state->modelindex2){
			if(state->modelindex2 == 255){  // custom weapon
				const clientinfo_t *ci = &cl.clientinfo[state->skinnum & 0xff];
				int i = (state->skinnum >> 8);  // 0 is default weapon model
				if(i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
			} else
				ent.model = cl.model_draw[state->modelindex2];

			R_AddEntity(&ent);
		}
		if(state->modelindex3){
			ent.model = cl.model_draw[state->modelindex3];
			R_AddEntity(&ent);
		}
		if(state->modelindex4){
			ent.model = cl.model_draw[state->modelindex4];
			R_AddEntity(&ent);
		}
	}

	// lastly, add the view weapon
	Cl_AddWeapon();
}
