/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "server.h"

/*
 * 
 * Encode a client frame onto the network channel
 * 
 */

/*
 * Sv_EmitEntities
 * 
 * Writes a delta update of an entity_state_t list to the message.
 */
static void Sv_EmitEntities(client_frame_t *from, client_frame_t *to, sizebuf_t *msg){
	entity_state_t *oldent = NULL, *newent = NULL;
	int oldindex, newindex;
	int oldnum, newnum;
	int from_num_entities;
	int bits;

	if(!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while(newindex < to->num_entities || oldindex < from_num_entities){
		if(newindex >= to->num_entities)
			newnum = 9999;
		else {
			newent = &svs.client_entities[(to->first_entity + newindex) % svs.num_client_entities];
			newnum = newent->number;
		}

		if(oldindex >= from_num_entities)
			oldnum = 9999;
		else {
			oldent = &svs.client_entities[(from->first_entity + oldindex) % svs.num_client_entities];
			oldnum = oldent->number;
		}

		if(newnum == oldnum){  // delta update from old position
			Msg_WriteDeltaEntity(oldent, newent, msg, false, newent->number <= sv_maxclients->value);
			oldindex++;
			newindex++;
			continue;
		}

		if(newnum < oldnum){  // this is a new entity, send it from the baseline
			Msg_WriteDeltaEntity(&sv.baselines[newnum], newent, msg, true, true);
			newindex++;
			continue;
		}

		if(newnum > oldnum){  // the old entity isn't present in the new message
			bits = U_REMOVE;
			if(oldnum >= 256)
				bits |= U_NUMBER16 | U_MOREBITS1;

			Msg_WriteByte(msg, bits & 255);
			if(bits & 0x0000ff00)
				Msg_WriteByte(msg, (bits >> 8) & 255);

			if(bits & U_NUMBER16)
				Msg_WriteShort(msg, oldnum);
			else
				Msg_WriteByte(msg, oldnum);

			oldindex++;
			continue;
		}
	}

	Msg_WriteShort(msg, 0);  // end of packetentities
}


/*
 * Sv_WritePlayerstateToClient
 */
static void Sv_WritePlayerstateToClient(client_t *client, client_frame_t *from, client_frame_t *to, sizebuf_t *msg){
	int i;
	int pflags;
	player_state_t *ps, *ops;
	player_state_t dummy;
	int statbits;

	ps = &to->ps;
	if(!from){
		memset(&dummy, 0, sizeof(dummy));
		ops = &dummy;
	} else
		ops = &from->ps;

	// determine what needs to be sent
	pflags = 0;

	if(ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	if(ps->pmove.origin[0] != ops->pmove.origin[0]
			|| ps->pmove.origin[1] != ops->pmove.origin[1]
			|| ps->pmove.origin[2] != ops->pmove.origin[2])
		pflags |= PS_M_ORIGIN;

	if(ps->pmove.velocity[0] != ops->pmove.velocity[0]
			|| ps->pmove.velocity[1] != ops->pmove.velocity[1]
			|| ps->pmove.velocity[2] != ops->pmove.velocity[2])
		pflags |= PS_M_VELOCITY;

	if(ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= PS_M_TIME;

	if(ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= PS_M_FLAGS;

	if(ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0]
			|| ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1]
			|| ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2])
		pflags |= PS_M_DELTA_ANGLES;

	if(ps->pmove.pm_type == PM_FREEZE &&  // send for chasecammers
			(ps->angles[0] != ops->angles[0]
			|| ps->angles[1] != ops->angles[1]
			|| ps->angles[2] != ops->angles[2]))
		pflags |= PS_VIEWANGLES;

	if(client->recording)  // or anyone recording a demo
		pflags |= PS_VIEWANGLES;

	// write it
	Msg_WriteByte(msg, pflags);

	// write the pmove_state_t
	if(pflags & PS_M_TYPE)
		Msg_WriteByte(msg, ps->pmove.pm_type);

	if(pflags & PS_M_ORIGIN){
		Msg_WriteShort(msg, ps->pmove.origin[0]);
		Msg_WriteShort(msg, ps->pmove.origin[1]);
		Msg_WriteShort(msg, ps->pmove.origin[2]);
	}

	if(pflags & PS_M_VELOCITY){
		Msg_WriteShort(msg, ps->pmove.velocity[0]);
		Msg_WriteShort(msg, ps->pmove.velocity[1]);
		Msg_WriteShort(msg, ps->pmove.velocity[2]);
	}

	if(pflags & PS_M_TIME)
		Msg_WriteByte(msg, ps->pmove.pm_time);

	if(pflags & PS_M_FLAGS)
		Msg_WriteByte(msg, ps->pmove.pm_flags);

	if(pflags & PS_M_DELTA_ANGLES){
		Msg_WriteShort(msg, ps->pmove.delta_angles[0]);
		Msg_WriteShort(msg, ps->pmove.delta_angles[1]);
		Msg_WriteShort(msg, ps->pmove.delta_angles[2]);
	}

	if(pflags & PS_VIEWANGLES){
		Msg_WriteAngle16(msg, ps->angles[0]);
		Msg_WriteAngle16(msg, ps->angles[1]);
		Msg_WriteAngle16(msg, ps->angles[2]);
	}

	// send stats
	statbits = 0;
	for(i = 0; i < MAX_STATS; i++)
		if(ps->stats[i] != ops->stats[i])
			statbits |= 1 << i;
	Msg_WriteLong(msg, statbits);
	for(i = 0; i < MAX_STATS; i++)
		if(statbits & (1 << i))
			Msg_WriteShort(msg, ps->stats[i]);
}


/*
 * Sv_WriteFrameToClient
 */
void Sv_WriteFrameToClient(client_t *client, sizebuf_t *msg){
	client_frame_t *frame, *oldframe;
	int lastframe;

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	if(client->lastframe < 0){
		// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	} else if(sv.framenum - client->lastframe >= (UPDATE_BACKUP - 3)){
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
		lastframe = -1;
	} else {
		// we have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	Msg_WriteByte(msg, svc_frame);
	Msg_WriteLong(msg, sv.framenum);
	Msg_WriteLong(msg, lastframe);  // what we are delta'ing from
	Msg_WriteByte(msg, client->surpress_count);  // rate dropped packets
	client->surpress_count = 0;

	// send over the areabits
	Msg_WriteByte(msg, frame->areabytes);
	Sb_Write(msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	Sv_WritePlayerstateToClient(client, oldframe, frame, msg);

	// delta encode the entities
	Sv_EmitEntities(oldframe, frame, msg);
}


/*
 * 
 * Build a client frame structure
 * 
 */

byte fatpvs[65536 / 8];  // 32767 is MAX_BSP_LEAFS

/*
 * Sv_FatPVS
 * 
 * The client will interpolate the view position, so we can't use a single PVS point.
 */
static void Sv_FatPVS(const vec3_t org){
	int leafs[64];
	int i, j, count;
	int longs;
	byte *src;
	vec3_t mins, maxs;

	for(i = 0; i < 3; i++){
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = Cm_BoxLeafnums(mins, maxs, leafs, 64, NULL);
	if(count < 1){
		Com_Error(ERR_FATAL, "Sv_FatPVS: count < 1.");
	}

	longs = (Cm_NumClusters() + 31) >> 5;

	// convert leafs to clusters
	for(i = 0; i < count; i++)
		leafs[i] = Cm_LeafCluster(leafs[i]);

	memcpy(fatpvs, Cm_ClusterPVS(leafs[0]), longs << 2);
	// or in all the other leaf bits
	for(i = 1; i < count; i++){
		for(j = 0; j < i; j++)
			if(leafs[i] == leafs[j])
				break;
		if(j != i)
			continue;  // already have the cluster we want
		src = Cm_ClusterPVS(leafs[i]);
		for(j = 0; j < longs; j++)
			((long *)fatpvs)[j] |=((long *)src)[j];
	}
}


/*
 * Sv_BuildClientFrame
 * 
 * Decides which entities are going to be visible to the client, and
 * copies off the playerstat and areabits.
 */
void Sv_BuildClientFrame(client_t *client){
	int e, i;
	vec3_t org;
	edict_t *ent;
	edict_t *clent;
	client_frame_t *frame;
	entity_state_t *state;
	int l;
	int clientarea, clientcluster;
	int leafnum;
	int c_fullsend;
	byte *clientphs;
	byte *bitvector;

	clent = client->edict;
	if(!clent->client)
		return;  // not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	frame->senttime = svs.realtime; // save it for ping calc later

	// find the client's PVS
	for(i = 0; i < 3; i++)
		org[i] = clent->client->ps.pmove.origin[i] * 0.125;

	leafnum = Cm_PointLeafnum(org);
	clientarea = Cm_LeafArea(leafnum);
	clientcluster = Cm_LeafCluster(leafnum);

	// calculate the visible areas
	frame->areabytes = Cm_WriteAreaBits(frame->areabits, clientarea);

	// grab the current player_state_t
	frame->ps = clent->client->ps;

	Sv_FatPVS(org);
	clientphs = Cm_ClusterPHS(clientcluster);

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	c_fullsend = 0;

	for(e = 1; e < ge->num_edicts; e++){
		ent = EDICT_NUM(e);

		// ignore ents without visible models
		if(ent->svflags & SVF_NOCLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if(!ent->s.modelindex && !ent->s.effects && !ent->s.sound && !ent->s.event)
			continue;

		// ignore if not touching a PV leaf
		if(ent != clent){
			// check area
			if(!Cm_AreasConnected(clientarea, ent->areanum)){  // doors can legally straddle two areas, so
				// we may need to check another one
				if(!ent->areanum2 || !Cm_AreasConnected(clientarea, ent->areanum2))
					continue;  // blocked by a door
			}

			// FIXME: if an ent has a model and a sound, but isn't
			// in the PVS, only the PHS, clear the model
			if(ent->s.sound){
				bitvector = fatpvs;  // clientphs;
			} else
				bitvector = fatpvs;

			if(ent->num_clusters == -1){  // too many leafs for individual check, go by headnode
				if(!Cm_HeadnodeVisible(ent->headnode, bitvector))
					continue;
				c_fullsend++;
			} else {  // check individual leafs
				for(i = 0; i < ent->num_clusters; i++){
					l = ent->clusternums[i];
					if(bitvector[l >> 3] & (1 << (l & 7)))
						break;
				}
				if(i == ent->num_clusters)
					continue;  // not visible
			}

			if(!ent->s.modelindex && !ent->solid && !ent->s.effects){
				// don't send sounds if they will be attenuated away
				vec3_t delta;
				float len;

				VectorSubtract(org, ent->s.origin, delta);
				len = VectorLength(delta);
				if(len > 600.0)
					continue;
			}
		}

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		if(ent->s.number != e){
			Com_Warn("Sv_BuildClientFrame: Fixing ent->s.number.\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// don't mark players missiles as solid
		if(ent->owner == client->edict)
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}
