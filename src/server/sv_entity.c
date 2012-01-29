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

#include "sv_local.h"

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
static void Sv_EmitEntities(sv_frame_t *from, sv_frame_t *to, size_buf_t *msg) {
	entity_state_t *old_ent = NULL, *new_ent = NULL;
	unsigned int old_index, new_index;
	unsigned short old_num, new_num;
	unsigned short from_num_entities;

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	new_index = 0;
	old_index = 0;
	while (new_index < to->num_entities || old_index < from_num_entities) {
		if (new_index >= to->num_entities)
			new_num = 0xffff;
		else {
			new_ent = &svs.entity_states[(to->first_entity + new_index)
					% svs.num_entity_states];
			new_num = new_ent->number;
		}

		if (old_index >= from_num_entities)
			old_num = 0xffff;
		else {
			old_ent = &svs.entity_states[(from->first_entity + old_index)
					% svs.num_entity_states];
			old_num = old_ent->number;
		}

		if (new_num == old_num) { // delta update from old position
			Msg_WriteDeltaEntity(old_ent, new_ent, msg, false,
					new_ent->number <= sv_max_clients->integer);
			old_index++;
			new_index++;
			continue;
		}

		if (new_num < old_num) { // this is a new entity, send it from the baseline
			Msg_WriteDeltaEntity(&sv.baselines[new_num], new_ent, msg, true,
					true);
			new_index++;
			continue;
		}

		if (new_num > old_num) { // the old entity isn't present in the new message
			const short bits = U_REMOVE;

			Msg_WriteShort(msg, old_num);
			Msg_WriteShort(msg, bits);

			old_index++;
			continue;
		}
	}

	Msg_WriteLong(msg, 0); // end of entities
}

/*
 * Sv_WritePlayerstateToClient
 */
static void Sv_WritePlayerstateToClient(sv_client_t *client, sv_frame_t *from,
		sv_frame_t *to, size_buf_t *msg) {
	int i;
	byte bits;
	player_state_t *ps, *ops;
	player_state_t dummy;
	int statbits;

	ps = &to->ps;
	if (!from) {
		memset(&dummy, 0, sizeof(dummy));
		ops = &dummy;
	} else
		ops = &from->ps;

	// determine what needs to be sent
	bits = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		bits |= PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0] || ps->pmove.origin[1]
			!= ops->pmove.origin[1] || ps->pmove.origin[2]
			!= ops->pmove.origin[2])
		bits |= PS_M_ORIGIN;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0]
			|| ps->pmove.velocity[1] != ops->pmove.velocity[1]
			|| ps->pmove.velocity[2] != ops->pmove.velocity[2])
		bits |= PS_M_VELOCITY;

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		bits |= PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		bits |= PS_M_FLAGS;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0]
			|| ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1]
			|| ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2])
		bits |= PS_M_DELTA_ANGLES;

	if (ps->pmove.pm_type == PM_FREEZE && // send for chasecammers
			(ps->angles[0] != ops->angles[0] || ps->angles[1] != ops->angles[1]
					|| ps->angles[2] != ops->angles[2]))
		bits |= PS_VIEW_ANGLES;

	if (client->recording) // or anyone recording a demo
		bits |= PS_VIEW_ANGLES;

	// write it
	Msg_WriteByte(msg, bits);

	// write the pmove_state_t
	if (bits & PS_M_TYPE)
		Msg_WriteByte(msg, ps->pmove.pm_type);

	if (bits & PS_M_ORIGIN) {
		Msg_WriteShort(msg, ps->pmove.origin[0]);
		Msg_WriteShort(msg, ps->pmove.origin[1]);
		Msg_WriteShort(msg, ps->pmove.origin[2]);
	}

	if (bits & PS_M_VELOCITY) {
		Msg_WriteShort(msg, ps->pmove.velocity[0]);
		Msg_WriteShort(msg, ps->pmove.velocity[1]);
		Msg_WriteShort(msg, ps->pmove.velocity[2]);
	}

	if (bits & PS_M_TIME)
		Msg_WriteByte(msg, ps->pmove.pm_time);

	if (bits & PS_M_FLAGS)
		Msg_WriteShort(msg, ps->pmove.pm_flags);

	if (bits & PS_M_DELTA_ANGLES) {
		Msg_WriteShort(msg, ps->pmove.delta_angles[0]);
		Msg_WriteShort(msg, ps->pmove.delta_angles[1]);
		Msg_WriteShort(msg, ps->pmove.delta_angles[2]);
	}

	if (bits & PS_VIEW_ANGLES) {
		Msg_WriteAngle16(msg, ps->angles[0]);
		Msg_WriteAngle16(msg, ps->angles[1]);
		Msg_WriteAngle16(msg, ps->angles[2]);
	}

	// send stats
	statbits = 0;
	for (i = 0; i < MAX_STATS; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1 << i;
	Msg_WriteLong(msg, statbits);
	for (i = 0; i < MAX_STATS; i++)
		if (statbits & (1 << i))
			Msg_WriteShort(msg, ps->stats[i]);
}

/*
 * Sv_WriteFrameToClient
 */
void Sv_WriteFrameToClient(sv_client_t *client, size_buf_t *msg) {
	sv_frame_t *frame, *old_frame;
	int last_frame;

	// this is the frame we are creating
	frame = &client->frames[sv.frame_num & UPDATE_MASK];

	if (client->last_frame < 0) {
		// client is asking for a retransmit
		old_frame = NULL;
		last_frame = -1;
	} else if (sv.frame_num - client->last_frame >= (UPDATE_BACKUP - 3)) {
		// client hasn't gotten a good message through in a long time
		old_frame = NULL;
		last_frame = -1;
	} else {
		// we have a valid message to delta from
		old_frame = &client->frames[client->last_frame & UPDATE_MASK];
		last_frame = client->last_frame;
	}

	Msg_WriteByte(msg, SV_CMD_FRAME);
	Msg_WriteLong(msg, sv.frame_num);
	Msg_WriteLong(msg, last_frame); // what we are delta'ing from
	Msg_WriteByte(msg, client->surpress_count); // rate dropped packets
	client->surpress_count = 0;

	// send over the area_bits
	Msg_WriteByte(msg, frame->area_bytes);
	Msg_WriteData(msg, frame->area_bits, frame->area_bytes);

	// delta encode the playerstate
	Sv_WritePlayerstateToClient(client, old_frame, frame, msg);

	// delta encode the entities
	Sv_EmitEntities(old_frame, frame, msg);
}

/*
 *
 * Build a client frame structure
 *
 */

byte fatpvs[65536 / 8]; // 32767 is MAX_BSP_LEAFS

/*
 * Sv_FatPVS
 *
 * The client will interpolate the view position, so we can't use a single PVS point.
 */
static void Sv_FatPVS(const vec3_t org) {
	int leafs[64];
	int i, j, count;
	int longs;
	byte *src;
	vec3_t mins, maxs;

	for (i = 0; i < 3; i++) {
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = Cm_BoxLeafnums(mins, maxs, leafs, 64, NULL);
	if (count < 1) {
		Com_Error(ERR_FATAL, "Sv_FatPVS: count < 1.\n");
	}

	longs = (Cm_NumClusters() + 31) >> 5;

	// convert leafs to clusters
	for (i = 0; i < count; i++)
		leafs[i] = Cm_LeafCluster(leafs[i]);

	memcpy(fatpvs, Cm_ClusterPVS(leafs[0]), longs << 2);
	// or in all the other leaf bits
	for (i = 1; i < count; i++) {
		for (j = 0; j < i; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue; // already have the cluster we want
		src = Cm_ClusterPVS(leafs[i]);
		for (j = 0; j < longs; j++)
			((long *) fatpvs)[j] |= ((long *) src)[j];
	}
}

/*
 * Sv_BuildClientFrame
 *
 * Decides which entities are going to be visible to the client, and
 * copies off the playerstat and area_bits.
 */
void Sv_BuildClientFrame(sv_client_t *client) {
	unsigned int e;
	vec3_t org;
	g_edict_t *ent;
	g_edict_t *clent;
	sv_frame_t *frame;
	entity_state_t *state;
	int i, l;
	int clientarea, clientcluster;
	int leaf_num;
	int c_fullsend;
	byte *clientphs;
	byte *bitvector;

	clent = client->edict;
	if (!clent->client)
		return; // not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.frame_num & UPDATE_MASK];

	frame->sent_time = svs.real_time; // save it for ping calc later

	// find the client's PVS
	for (i = 0; i < 3; i++)
		org[i] = clent->client->ps.pmove.origin[i] * 0.125;

	leaf_num = Cm_PointLeafnum(org);
	clientarea = Cm_LeafArea(leaf_num);
	clientcluster = Cm_LeafCluster(leaf_num);

	// calculate the visible areas
	frame->area_bytes = Cm_WriteAreaBits(frame->area_bits, clientarea);

	// grab the current player_state_t
	frame->ps = clent->client->ps;

	Sv_FatPVS(org);
	clientphs = Cm_ClusterPHS(clientcluster);

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_entity_state;

	c_fullsend = 0;

	for (e = 1; e < svs.game->num_edicts; e++) {
		ent = EDICT_FOR_NUM(e);

		// ignore ents without visible models
		if (ent->sv_flags & SVF_NO_CLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.model1 && !ent->s.effects && !ent->s.sound && !ent->s.event)
			continue;

		// ignore if not touching a PVS leaf
		if (ent != clent) {
			// check area
			if (!Cm_AreasConnected(clientarea, ent->area_num)) { // doors can legally straddle two areas, so
				// we may need to check another one
				if (!ent->area_num2 || !Cm_AreasConnected(clientarea,
						ent->area_num2))
					continue; // blocked by a door
			}

			// FIXME: if an ent has a model and a sound, but isn't
			// in the PVS, only the PHS, clear the model
			if (ent->s.sound) {
				bitvector = fatpvs; // clientphs;
			} else {
				bitvector = fatpvs;
			}

			if (ent->num_clusters == -1) { // too many leafs for individual check, go by head_node
				if (!Cm_HeadnodeVisible(ent->head_node, bitvector))
					continue;
				c_fullsend++;
			} else { // check individual leafs
				for (i = 0; i < ent->num_clusters; i++) {
					l = ent->cluster_nums[i];
					if (bitvector[l >> 3] & (1 << (l & 7)))
						break;
				}
				if (i == ent->num_clusters)
					continue; // not visible
			}

			if (!ent->s.model1 && !ent->solid && !ent->s.effects) {
				// don't send sounds if they will be attenuated away
				vec3_t delta;
				float len;

				VectorSubtract(org, ent->s.origin, delta);
				len = VectorLength(delta);
				if (len > 600.0)
					continue;
			}
		}

		// add it to the circular entity_state_t array
		state = &svs.entity_states[svs.next_entity_state
				% svs.num_entity_states];
		if (ent->s.number != e) {
			Com_Warn("Sv_BuildClientFrame: Fixing ent->s.number.\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// don't mark players missiles as solid
		if (ent->owner == client->edict)
			state->solid = 0;

		svs.next_entity_state++;
		frame->num_entities++;
	}
}
