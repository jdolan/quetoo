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
 * @brief Writes a delta update of an entity_state_t list to the message.
 */
static void Sv_EmitEntities(sv_frame_t *from, sv_frame_t *to, size_buf_t *msg) {
	entity_state_t *old_ent = NULL, *new_ent = NULL;
	uint32_t old_index, new_index;
	uint16_t old_num, new_num;
	uint16_t from_num_entities;

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
			new_ent = &svs.entity_states[(to->first_entity + new_index) % svs.num_entity_states];
			new_num = new_ent->number;
		}

		if (old_index >= from_num_entities)
			old_num = 0xffff;
		else {
			old_ent = &svs.entity_states[(from->first_entity + old_index) % svs.num_entity_states];
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
			Msg_WriteDeltaEntity(&sv.baselines[new_num], new_ent, msg, true, true);
			new_index++;
			continue;
		}

		if (new_num > old_num) { // the old entity isn't present in the new message
			const int16_t bits = U_REMOVE;

			Msg_WriteShort(msg, old_num);
			Msg_WriteShort(msg, bits);

			old_index++;
			continue;
		}
	}

	Msg_WriteLong(msg, 0); // end of entities
}

/*
 * @brief
 */
static void Sv_WritePlayerstate(sv_frame_t *from, sv_frame_t *to, size_buf_t *msg) {
	uint16_t pm_state_bits;
	uint32_t stat_bits;
	player_state_t *ps, *ops;
	player_state_t dummy;
	int32_t i;

	ps = &to->ps;

	if (!from) {
		memset(&dummy, 0, sizeof(dummy));
		ops = &dummy;
	} else {
		ops = &from->ps;
	}

	// determine what needs to be sent
	pm_state_bits = 0;

	if (ps->pm_state.pm_type != ops->pm_state.pm_type)
		pm_state_bits |= PS_M_TYPE;

	if (ps->pm_state.origin[0] != ops->pm_state.origin[0] || ps->pm_state.origin[1]
			!= ops->pm_state.origin[1] || ps->pm_state.origin[2] != ops->pm_state.origin[2])
		pm_state_bits |= PS_M_ORIGIN;

	if (ps->pm_state.velocity[0] != ops->pm_state.velocity[0] || ps->pm_state.velocity[1]
			!= ops->pm_state.velocity[1] || ps->pm_state.velocity[2] != ops->pm_state.velocity[2])
		pm_state_bits |= PS_M_VELOCITY;

	if (ps->pm_state.pm_flags != ops->pm_state.pm_flags)
		pm_state_bits |= PS_M_FLAGS;

	if (ps->pm_state.pm_time != ops->pm_state.pm_time)
		pm_state_bits |= PS_M_TIME;

	if (ps->pm_state.gravity != ops->pm_state.gravity)
		pm_state_bits |= PS_M_GRAVITY;

	if (ps->pm_state.view_offset[0] != ops->pm_state.view_offset[0] || ps->pm_state.view_offset[1]
			!= ops->pm_state.view_offset[1] || ps->pm_state.view_offset[2]
			!= ops->pm_state.view_offset[2])
		pm_state_bits |= PS_M_VIEW_OFFSET;

	if (ps->pm_state.view_angles[0] != ops->pm_state.view_angles[0] || ps->pm_state.view_angles[1]
			!= ops->pm_state.view_angles[1] || ps->pm_state.view_angles[2]
			!= ops->pm_state.view_angles[2])
		pm_state_bits |= PS_M_VIEW_ANGLES;

	if (ps->pm_state.kick_angles[0] != ops->pm_state.kick_angles[0] || ps->pm_state.kick_angles[1]
			!= ops->pm_state.kick_angles[1] || ps->pm_state.kick_angles[2]
			!= ops->pm_state.kick_angles[2])
		pm_state_bits |= PS_M_KICK_ANGLES;

	if (ps->pm_state.delta_angles[0] != ops->pm_state.delta_angles[0]
			|| ps->pm_state.delta_angles[1] != ops->pm_state.delta_angles[1]
			|| ps->pm_state.delta_angles[2] != ops->pm_state.delta_angles[2])
		pm_state_bits |= PS_M_DELTA_ANGLES;

	// write it
	Msg_WriteShort(msg, pm_state_bits);

	// write the pmove_state_t
	if (pm_state_bits & PS_M_TYPE)
		Msg_WriteByte(msg, ps->pm_state.pm_type);

	if (pm_state_bits & PS_M_ORIGIN) {
		Msg_WriteShort(msg, ps->pm_state.origin[0]);
		Msg_WriteShort(msg, ps->pm_state.origin[1]);
		Msg_WriteShort(msg, ps->pm_state.origin[2]);
	}

	if (pm_state_bits & PS_M_VELOCITY) {
		Msg_WriteShort(msg, ps->pm_state.velocity[0]);
		Msg_WriteShort(msg, ps->pm_state.velocity[1]);
		Msg_WriteShort(msg, ps->pm_state.velocity[2]);
	}

	if (pm_state_bits & PS_M_FLAGS)
		Msg_WriteShort(msg, ps->pm_state.pm_flags);

	if (pm_state_bits & PS_M_TIME)
		Msg_WriteByte(msg, ps->pm_state.pm_time);

	if (pm_state_bits & PS_M_GRAVITY)
		Msg_WriteShort(msg, ps->pm_state.gravity);

	if (pm_state_bits & PS_M_VIEW_OFFSET) {
		Msg_WriteShort(msg, ps->pm_state.view_offset[0]);
		Msg_WriteShort(msg, ps->pm_state.view_offset[1]);
		Msg_WriteShort(msg, ps->pm_state.view_offset[2]);
	}

	if (pm_state_bits & PS_M_VIEW_ANGLES) {
		Msg_WriteShort(msg, ps->pm_state.view_angles[0]);
		Msg_WriteShort(msg, ps->pm_state.view_angles[1]);
		Msg_WriteShort(msg, ps->pm_state.view_angles[2]);
	}

	if (pm_state_bits & PS_M_KICK_ANGLES) {
		Msg_WriteShort(msg, ps->pm_state.kick_angles[0]);
		Msg_WriteShort(msg, ps->pm_state.kick_angles[1]);
		Msg_WriteShort(msg, ps->pm_state.kick_angles[2]);
	}

	if (pm_state_bits & PS_M_DELTA_ANGLES) {
		Msg_WriteShort(msg, ps->pm_state.delta_angles[0]);
		Msg_WriteShort(msg, ps->pm_state.delta_angles[1]);
		Msg_WriteShort(msg, ps->pm_state.delta_angles[2]);
	}

	// send stats
	stat_bits = 0;

	for (i = 0; i < MAX_STATS; i++) {
		if (ps->stats[i] != ops->stats[i]) {
			stat_bits |= 1 << i;
		}
	}

	Msg_WriteLong(msg, stat_bits);

	for (i = 0; i < MAX_STATS; i++) {
		if (stat_bits & (1 << i)) {
			Msg_WriteShort(msg, ps->stats[i]);
		}
	}
}

/*
 * @brief
 */
void Sv_WriteFrame(sv_client_t *client, size_buf_t *msg) {
	sv_frame_t *frame, *old_frame;
	int32_t last_frame;

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
	Sv_WritePlayerstate(old_frame, frame, msg);

	// delta encode the entities
	Sv_EmitEntities(old_frame, frame, msg);
}

/*
 * @brief Resolve the visibility data for the bounding box around the client. The
 * bounding box provides some leniency because the client's actual view origin
 * is likely slightly different than what we think it is.
 */
static byte *Sv_ClientPVS(const vec3_t org) {
	static byte pvs[MAX_BSP_LEAFS >> 3];
	int32_t leafs[64];
	int32_t i, j, count;
	int32_t longs;
	byte *src;
	vec3_t mins, maxs;

	for (i = 0; i < 3; i++) {
		mins[i] = org[i] - 16.0;
		maxs[i] = org[i] + 16.0;
	}

	count = Cm_BoxLeafnums(mins, maxs, leafs, 64, NULL);
	if (count < 1) {
		Com_Error(ERR_DROP, "Bad leaf count\n");
	}

	longs = (Cm_NumClusters() + 31) >> 5;

	// convert leafs to clusters
	for (i = 0; i < count; i++)
		leafs[i] = Cm_LeafCluster(leafs[i]);

	memcpy(pvs, Cm_ClusterPVS(leafs[0]), longs << 2);

	// or in all the other leaf bits
	for (i = 1; i < count; i++) {
		for (j = 0; j < i; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue; // already have the cluster we want
		src = Cm_ClusterPVS(leafs[i]);
		for (j = 0; j < longs; j++)
			((long *) pvs)[j] |= ((long *) src)[j];
	}

	return pvs;
}

/*
 * @brief Decides which entities are going to be visible to the client, and
 * copies off the playerstat and area_bits.
 */
void Sv_BuildClientFrame(sv_client_t *client) {
	uint32_t e;
	vec3_t org;
	g_edict_t *ent;
	g_edict_t *cent;
	pm_state_t *pm;
	sv_frame_t *frame;
	entity_state_t *state;
	int32_t i;
	int32_t area, cluster;
	int32_t leaf;
	byte *phs;
	byte *vis;

	cent = client->edict;
	if (!cent->client)
		return; // not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.frame_num & UPDATE_MASK];

	frame->sent_time = svs.real_time; // save it for ping calc later

	// find the client's PVS
	pm = &cent->client->ps.pm_state;

	VectorScale(pm->origin, 0.125, org);
	for (i = 0; i < 3; i++)
		org[i] += pm->view_offset[i] * 0.125;

	leaf = Cm_PointLeafnum(org);
	area = Cm_LeafArea(leaf);
	cluster = Cm_LeafCluster(leaf);

	// calculate the visible areas
	frame->area_bytes = Cm_WriteAreaBits(frame->area_bits, area);

	// grab the current player_state_t
	frame->ps = cent->client->ps;

	// resolve the visibility data
	vis = Sv_ClientPVS(org);
	phs = Cm_ClusterPHS(cluster);

	// build up the list of relevant entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_entity_state;

	for (e = 1; e < svs.game->num_edicts; e++) {
		ent = EDICT_FOR_NUM(e);

		// ignore ents that are local to the server
		if (ent->sv_flags & SVF_NO_CLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.model1 && !ent->s.effects && !ent->s.sound && !ent->s.event)
			continue;

		// ignore if not touching a PVS leaf
		if (ent != cent) {
			// check area
			if (!Cm_AreasConnected(area, ent->area_num)) { // doors can occupy two areas, so
				// we may need to check another one
				if (!ent->area_num2 || !Cm_AreasConnected(area, ent->area_num2))
					continue; // blocked by a door
			}

			const byte *vis_data = ent->s.sound || ent->s.event ? phs : vis;

			if (ent->num_clusters == -1) { // too many leafs for individual check, go by head_node
				if (!Cm_HeadnodeVisible(ent->head_node, vis_data))
					continue;
			} else { // check individual leafs
				for (i = 0; i < ent->num_clusters; i++) {
					const int32_t c = ent->cluster_nums[i];
					if (vis_data[c >> 3] & (1 << (c & 7)))
						break;
				}
				if (i == ent->num_clusters)
					continue; // not visible
			}
		}

		// add it to the circular entity_state_t array
		state = &svs.entity_states[svs.next_entity_state % svs.num_entity_states];
		if (ent->s.number != e) {
			Com_Warn("Fixing entity number\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// don't mark our own missiles as solid for prediction
		if (ent->owner == client->edict)
			state->solid = 0;

		svs.next_entity_state++;
		frame->num_entities++;
	}
}
