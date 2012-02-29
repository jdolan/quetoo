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

#include "cl_local.h"

/*
 * Cl_DeltaEntity
 *
 * Parses deltas from the given base and adds the resulting entity
 * to the current frame.
 */
static void Cl_DeltaEntity(cl_frame_t *frame, entity_state_t *from,
		unsigned short number, unsigned short bits) {

	cl_entity_t *ent;
	entity_state_t *to;

	ent = &cl.entities[number];

	to = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;
	frame->num_entities++;

	Msg_ReadDeltaEntity(from, to, &net_message, number, bits);

	// some data changes will force no interpolation
	if (to->event == EV_TELEPORT) {
		ent->server_frame = -99999;
	}

	if (ent->server_frame != cl.frame.server_frame - 1) {
		// wasn't in last update, so initialize some things
		// duplicate the current state so interpolation works
		ent->prev = *to;
		VectorCopy(to->old_origin, ent->prev.origin);
		ent->animation1.time = ent->animation2.time = 0;
	} else { // shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->server_frame = cl.frame.server_frame;
	ent->current = *to;
}

/*
 * Cl_ParseEntities
 *
 * An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *old_frame, cl_frame_t *new_frame) {
	entity_state_t *old_state = NULL;
	unsigned int old_index;
	unsigned short old_number;

	new_frame->entity_state = cl.entity_state;
	new_frame->num_entities = 0;

	// delta from the entities present in old_frame
	old_index = 0;

	if (!old_frame)
		old_number = 0xffff;
	else {
		if (old_index >= old_frame->num_entities)
			old_number = 0xffff;
		else {
			old_state = &cl.entity_states[(old_frame->entity_state + old_index)
					& ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}

	while (true) {
		const unsigned short number = Msg_ReadShort(&net_message);
		const unsigned short bits = Msg_ReadShort(&net_message);

		if (number >= MAX_EDICTS)
			Com_Error(ERR_DROP, "Cl_ParseEntities: bad number: %i.\n", number);

		if (net_message.read > net_message.size)
			Com_Error(ERR_DROP, "Cl_ParseEntities: end of message.\n");

		if (!number)
			break;

		while (old_number < number) { // one or more entities from old_frame are unchanged

			if (cl_show_net_messages->integer == 3)
				Com_Print("   unchanged: %i\n", old_number);

			Cl_DeltaEntity(new_frame, old_state, old_number, 0);

			old_index++;

			if (old_index >= old_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state
						+ old_index) & ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
		}

		if (bits & U_REMOVE) { // the entity present in the old_frame, and is not in the current frame

			if (cl_show_net_messages->integer == 3)
				Com_Print("   remove: %i\n", number);

			if (old_number != number)
				Com_Warn("Cl_ParseEntities: U_REMOVE: %u != %u.\n", old_number,
						number);

			old_index++;

			if (old_index >= old_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state
						+ old_index) & ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
			continue;
		}

		if (old_number == number) { // delta from previous state

			if (cl_show_net_messages->integer == 3)
				Com_Print("   delta: %i\n", number);

			Cl_DeltaEntity(new_frame, old_state, number, bits);

			old_index++;

			if (old_index >= old_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state
						+ old_index) & ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
			continue;
		}

		if (old_number > number) { // delta from baseline

			if (cl_show_net_messages->integer == 3)
				Com_Print("   baseline: %i\n", number);

			Cl_DeltaEntity(new_frame, &cl.entities[number].baseline, number,
					bits);
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while (old_number != 0xffff) { // one or more entities from the old packet are unchanged

		if (cl_show_net_messages->integer == 3)
			Com_Print("   unchanged: %i\n", old_number);

		Cl_DeltaEntity(new_frame, old_state, old_number, 0);

		old_index++;

		if (old_index >= old_frame->num_entities)
			old_number = 0xffff;
		else {
			old_state = &cl.entity_states[(old_frame->entity_state + old_index)
					& ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}
}

/*
 * Cl_ParsePlayerstate
 */
static void Cl_ParsePlayerstate(const cl_frame_t *old_frame,
		cl_frame_t *new_frame) {
	player_state_t *ps;
	byte bits;
	int i;
	int statbits;

	ps = &new_frame->ps;

	// copy old value before delta parsing
	if (old_frame)
		*ps = old_frame->ps;
	else
		// or start clean
		memset(ps, 0, sizeof(*ps));

	bits = Msg_ReadByte(&net_message);

	// parse the pmove_state_t

	if (bits & PS_M_TYPE)
		ps->pmove.pm_type = Msg_ReadByte(&net_message);

	if (cl.demo_server)
		ps->pmove.pm_type = PM_FREEZE;

	if (bits & PS_M_ORIGIN) {
		ps->pmove.origin[0] = Msg_ReadShort(&net_message);
		ps->pmove.origin[1] = Msg_ReadShort(&net_message);
		ps->pmove.origin[2] = Msg_ReadShort(&net_message);
	}

	if (bits & PS_M_VELOCITY) {
		ps->pmove.velocity[0] = Msg_ReadShort(&net_message);
		ps->pmove.velocity[1] = Msg_ReadShort(&net_message);
		ps->pmove.velocity[2] = Msg_ReadShort(&net_message);
	}

	if (bits & PS_M_TIME)
		ps->pmove.pm_time = Msg_ReadByte(&net_message);

	if (bits & PS_M_FLAGS)
		ps->pmove.pm_flags = Msg_ReadShort(&net_message);

	if (bits & PS_M_VIEW_OFFSET) {
		ps->pmove.view_offset[0] = Msg_ReadShort(&net_message);
		ps->pmove.view_offset[1] = Msg_ReadShort(&net_message);
		ps->pmove.view_offset[2] = Msg_ReadShort(&net_message);
	}

	if (bits & PS_M_DELTA_ANGLES) {
		ps->pmove.delta_angles[0] = Msg_ReadShort(&net_message);
		ps->pmove.delta_angles[1] = Msg_ReadShort(&net_message);
		ps->pmove.delta_angles[2] = Msg_ReadShort(&net_message);
	}

	if (bits & PS_VIEW_ANGLES) { // demo, chasecam, recording
		ps->angles[0] = Msg_ReadAngle16(&net_message);
		ps->angles[1] = Msg_ReadAngle16(&net_message);
		ps->angles[2] = Msg_ReadAngle16(&net_message);
	}

	// parse stats
	statbits = Msg_ReadLong(&net_message);

	for (i = 0; i < MAX_STATS; i++) {
		if (statbits & (1 << i))
			ps->stats[i] = Msg_ReadShort(&net_message);
	}
}

/*
 * Cl_ParseFrame
 */
void Cl_ParseFrame(void) {
	size_t len;
	cl_frame_t *old_frame;

	cl.frame.server_frame = Msg_ReadLong(&net_message);
	cl.frame.server_time = cl.frame.server_frame * 1000 / cl.server_frame_rate;

	cl.frame.delta_frame = Msg_ReadLong(&net_message);

	cl.surpress_count = Msg_ReadByte(&net_message);

	if (cl_show_net_messages->integer == 3)
		Com_Print("   frame:%i  delta:%i\n", cl.frame.server_frame,
				cl.frame.delta_frame);

	if (cl.frame.delta_frame <= 0) { // uncompressed frame
		old_frame = NULL;
		cls.demo_waiting = false;
		cl.frame.valid = true;
	} else { // delta compressed frame
		old_frame = &cl.frames[cl.frame.delta_frame & UPDATE_MASK];

		if (!old_frame->valid)
			Com_Error(ERR_DROP, "Cl_ParseFrame: Delta from invalid frame.\n");

		if (old_frame->server_frame != (unsigned int) cl.frame.delta_frame)
			Com_Error(ERR_DROP, "Cl_ParseFrame: Delta frame too old.\n");

		else if (cl.entity_state - old_frame->entity_state
				> ENTITY_STATE_BACKUP - UPDATE_BACKUP)
			Com_Error(ERR_DROP,
					"Cl_ParseFrame: Delta parse_entities too old.\n");

		cl.frame.valid = true;
	}

	len = Msg_ReadByte(&net_message); // read area_bits
	Msg_ReadData(&net_message, &cl.frame.area_bits, len);

	Cl_ParsePlayerstate(old_frame, &cl.frame);

	Cl_ParseEntities(old_frame, &cl.frame);

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.server_frame & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid) {
		// getting a valid frame message ends the connection process
		if (cls.state != CL_ACTIVE) {
			cls.state = CL_ACTIVE;

			VectorCopy(cl.frame.ps.pmove.origin, cl.predicted_origin);
			VectorScale(cl.predicted_origin, (1.0 / 8.0), cl.predicted_origin);

			VectorCopy(cl.frame.ps.angles, cl.predicted_angles);
		}

		Cl_CheckPredictionError();
	}
}

/*
 * Cl_UpdateEntities
 *
 * Invalidate lighting caches on media load.
 */
void Cl_UpdateEntities(void) {
	unsigned int i;

	if (!r_view.update)
		return;

	for (i = 0; i < MAX_EDICTS; i++) {
		cl.entities[i].lighting.state = LIGHTING_INIT;
	}
}
