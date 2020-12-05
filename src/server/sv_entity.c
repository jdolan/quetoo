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

#include "sv_local.h"

/**
 * @brief Writes a delta update of an entity_state_t list to the message.
 */
static void Sv_WriteEntities(sv_frame_t *from, sv_frame_t *to, mem_buf_t *msg) {
	entity_state_t *old_state = NULL, *new_state = NULL;
	uint32_t old_index, new_index;
	uint16_t old_num, new_num;
	uint16_t from_num_entities;

	if (!from) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->num_entities;
	}

	new_index = 0;
	old_index = 0;
	while (new_index < to->num_entities || old_index < from_num_entities) {
		if (new_index >= to->num_entities) {
			new_num = 0xffff;
		} else {
			new_state = &svs.entity_states[(to->entity_state + new_index) % svs.num_entity_states];
			new_num = new_state->number;
		}

		if (old_index >= from_num_entities) {
			old_num = 0xffff;
		} else {
			old_state
			    = &svs.entity_states[(from->entity_state + old_index) % svs.num_entity_states];
			old_num = old_state->number;
		}

		if (new_num == old_num) { // delta update from old position
			Net_WriteDeltaEntity(msg, old_state, new_state, false);
			old_index++;
			new_index++;
			continue;
		}

		if (new_num < old_num) { // this is a new entity, send it from the baseline
			Net_WriteDeltaEntity(msg, &sv.baselines[new_num], new_state, true);
			new_index++;
			continue;
		}

		if (new_num > old_num) { // the old entity isn't present in the new message
			const int16_t bits = U_REMOVE;

			Net_WriteShort(msg, old_num);
			Net_WriteShort(msg, bits);

			old_index++;
			continue;
		}
	}

	Net_WriteShort(msg, 0); // end of entities
}

/**
 * @brief
 */
static void Sv_WritePlayerState(sv_frame_t *from, sv_frame_t *to, mem_buf_t *msg) {
	static player_state_t null_state;

	if (from) {
		Net_WriteDeltaPlayerState(msg, &from->ps, &to->ps);
	} else {
		Net_WriteDeltaPlayerState(msg, &null_state, &to->ps);
	}
}

/**
 * @brief
 */
void Sv_WriteClientFrame(sv_client_t *client, mem_buf_t *msg) {
	sv_frame_t *frame, *delta_frame;
	int32_t delta_frame_num;

	// this is the frame we are creating
	frame = &client->frames[sv.frame_num & PACKET_MASK];

	if (client->last_frame < 0) {
		// client is asking for a retransmit
		delta_frame = NULL;
		delta_frame_num = -1;
	} else if (sv.frame_num - client->last_frame >= (PACKET_BACKUP - 3)) {
		// client hasn't gotten a good message through in a long time
		delta_frame = NULL;
		delta_frame_num = -1;
	} else {
		// we have a valid message to delta from
		delta_frame = &client->frames[client->last_frame & PACKET_MASK];
		delta_frame_num = client->last_frame;
	}

	Net_WriteByte(msg, SV_CMD_FRAME);
	Net_WriteLong(msg, sv.frame_num);
	Net_WriteLong(msg, delta_frame_num); // what we are delta'ing from
	Net_WriteByte(msg, client->suppress_count); // rate dropped packets
	client->suppress_count = 0;

	// delta encode the player state
	Sv_WritePlayerState(delta_frame, frame, msg);

	// delta encode the entities
	Sv_WriteEntities(delta_frame, frame, msg);
}

/**
 * @brief Resolve the visibility data for the bounding box around the client. The
 * bounding box provides some leniency because the client's actual view origin
 * is likely slightly different than what we think it is.
 */
static void Sv_ClientVisibility(const vec3_t org, byte *pvs, byte *phs) {

	// spread the bounds to account for view offset
	const vec3_t mins = Vec3_Add(org, Vec3(-16.f, -16.f, -16.f));
	const vec3_t maxs = Vec3_Add(org, Vec3( 16.f,  16.f,  16.f));

	Cm_BoxPVS(mins, maxs, pvs);
	Cm_BoxPHS(mins, maxs, phs);
}

/**
 * @brief Decides which entities are going to be visible to the client, and
 * copies off the player state.
 */
void Sv_BuildClientFrame(sv_client_t *client) {

	g_entity_t *cent = client->entity;
	if (!cent->client) {
		return;    // not in game yet
	}

	// this is the frame we are creating
	sv_frame_t *frame = &client->frames[sv.frame_num & PACKET_MASK];
	frame->sent_time = quetoo.ticks; // timestamp for ping calculation

	// grab the current player_state_t
	frame->ps = cent->client->ps;

	// find the client's PVS
	const pm_state_t *pm = &cent->client->ps.pm_state;

	const vec3_t org = Vec3_Add(pm->origin, pm->view_offset);

	// resolve the visibility data
	byte pvs[MAX_BSP_LEAFS >> 3], phs[MAX_BSP_LEAFS >> 3];
	Sv_ClientVisibility(org, pvs, phs);

	// build up the list of relevant entities
	frame->num_entities = 0;
	frame->entity_state = svs.next_entity_state;

	for (int32_t e = 1; e < svs.game->num_entities; e++) {
		g_entity_t *ent = ENTITY_FOR_NUM(e);

		// ignore entities that are local to the server
		if (ent->sv_flags & SVF_NO_CLIENT) {
			continue;
		}

		// ignore entities without visible presence unless they have an effect
		if (!ent->s.event && !ent->s.effects && !ent->s.trail && !ent->s.model1 && !ent->s.sound) {
			continue;
		}

		// ignore entities not in PVS / PHS
		if (ent != cent) {
			const sv_entity_t *sent = &sv.entities[e];

			const byte *vis = ent->s.sound || ent->s.event ? phs : pvs;

			if (sent->num_clusters < 1) { // use top_node
				if (!Cm_HeadnodeVisible(sent->top_node, vis)) {
					continue;
				}
			} else { // or check individual leafs
				int32_t i;
				for (i = 0; i < sent->num_clusters; i++) {
					const int32_t c = sent->clusters[i];
					if (vis[c >> 3] & (1 << (c & 7))) {
						break;
					}
				}
				if (i == sent->num_clusters) {
					continue; // not visible
				}
			}
		}

		// copy it to the circular entity_state_t array
		entity_state_t *s = &svs.entity_states[svs.next_entity_state % svs.num_entity_states];
		if (ent->s.number != e) {
			Com_Warn("Fixing entity number: %d -> %d\n", ent->s.number, e);
			ent->s.number = e;
		}
		*s = ent->s;

		// don't mark our own missiles as solid for prediction
		if (ent->owner == client->entity) {
			s->solid = SOLID_NOT;
		}

		svs.next_entity_state++;
		frame->num_entities++;
	}
}
