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
 * @brief Parses deltas from the given base and adds the resulting entity
 * to the current frame.
 */
static void Cl_DeltaEntity(cl_frame_t *frame, entity_state_t *from, uint16_t number, uint16_t bits) {

	cl_entity_t *ent;
	entity_state_t *to;

	ent = &cl.entities[number];

	to = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;

	frame->num_entities++;

	Msg_ReadDeltaEntity(from, to, &net_message, number, bits);

	// some data changes will force no interpolation
	if (to->event == EV_CLIENT_TELEPORT) {
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
 * @brief An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *old_frame, cl_frame_t *new_frame) {
	entity_state_t *old_state = NULL;
	uint32_t old_index;
	uint16_t old_number;

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
			old_state
					= &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}

	while (true) {
		const uint16_t number = Msg_ReadShort(&net_message);

		if (number >= MAX_EDICTS)
			Com_Error(ERR_DROP, "Bad number: %i\n", number);

		if (net_message.read > net_message.size)
			Com_Error(ERR_DROP, "End of message\n");

		if (!number)
			break;

		const uint16_t bits = Msg_ReadShort(&net_message);

		while (old_number < number) { // one or more entities from old_frame are unchanged

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

		if (bits & U_REMOVE) { // the entity present in the old_frame, and is not in the current frame

			if (cl_show_net_messages->integer == 3)
				Com_Print("   remove: %i\n", number);

			if (old_number != number)
				Com_Warn("U_REMOVE: %u != %u\n", old_number, number);

			old_index++;

			if (old_index >= old_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state + old_index)
						& ENTITY_STATE_MASK];
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
				old_state = &cl.entity_states[(old_frame->entity_state + old_index)
						& ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
			continue;
		}

		if (old_number > number) { // delta from baseline

			if (cl_show_net_messages->integer == 3)
				Com_Print("   baseline: %i\n", number);

			Cl_DeltaEntity(new_frame, &cl.entities[number].baseline, number, bits);
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
			old_state
					= &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}
}

/*
 * @brief Parse the player_state_t for the current frame from the server, using delta
 * compression for all fields where possible.
 */
static void Cl_ParsePlayerstate(const cl_frame_t *old_frame, cl_frame_t *new_frame) {
	player_state_t *ps;
	uint16_t pm_state_bits;
	int32_t i;
	uint32_t stat_bits;

	ps = &new_frame->ps;

	// copy old value before delta parsing
	if (old_frame)
		*ps = old_frame->ps;
	else
		// or start clean
		memset(ps, 0, sizeof(*ps));

	pm_state_bits = Msg_ReadShort(&net_message);

	// parse the pm_state_t

	if (pm_state_bits & PS_M_TYPE)
		ps->pm_state.pm_type = Msg_ReadByte(&net_message);

	if (cl.demo_server)
		ps->pm_state.pm_type = PM_FREEZE;

	if (pm_state_bits & PS_M_ORIGIN) {
		ps->pm_state.origin[0] = Msg_ReadShort(&net_message);
		ps->pm_state.origin[1] = Msg_ReadShort(&net_message);
		ps->pm_state.origin[2] = Msg_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_M_VELOCITY) {
		ps->pm_state.velocity[0] = Msg_ReadShort(&net_message);
		ps->pm_state.velocity[1] = Msg_ReadShort(&net_message);
		ps->pm_state.velocity[2] = Msg_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_M_FLAGS)
		ps->pm_state.pm_flags = Msg_ReadShort(&net_message);

	if (pm_state_bits & PS_M_TIME)
		ps->pm_state.pm_time = Msg_ReadShort(&net_message);

	if (pm_state_bits & PS_M_GRAVITY)
		ps->pm_state.gravity = Msg_ReadShort(&net_message);

	if (pm_state_bits & PS_M_VIEW_OFFSET) {
		ps->pm_state.view_offset[0] = Msg_ReadShort(&net_message);
		ps->pm_state.view_offset[1] = Msg_ReadShort(&net_message);
		ps->pm_state.view_offset[2] = Msg_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_M_VIEW_ANGLES) {
		ps->pm_state.view_angles[0] = Msg_ReadShort(&net_message);
		ps->pm_state.view_angles[1] = Msg_ReadShort(&net_message);
		ps->pm_state.view_angles[2] = Msg_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_M_KICK_ANGLES) {
		ps->pm_state.kick_angles[0] = Msg_ReadShort(&net_message);
		ps->pm_state.kick_angles[1] = Msg_ReadShort(&net_message);
		ps->pm_state.kick_angles[2] = Msg_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_M_DELTA_ANGLES) {
		ps->pm_state.delta_angles[0] = Msg_ReadShort(&net_message);
		ps->pm_state.delta_angles[1] = Msg_ReadShort(&net_message);
		ps->pm_state.delta_angles[2] = Msg_ReadShort(&net_message);
	}

	// parse stats

	stat_bits = Msg_ReadLong(&net_message);

	for (i = 0; i < MAX_STATS; i++) {
		if (stat_bits & (1 << i))
			ps->stats[i] = Msg_ReadShort(&net_message);
	}
}

/*
 * @brief
 */
void Cl_ParseFrame(void) {
	size_t len;
	cl_frame_t *old_frame;

	cl.frame.server_frame = Msg_ReadLong(&net_message);
	cl.frame.server_time = cl.frame.server_frame * 1000 / cl.server_hz;

	cl.frame.delta_frame = Msg_ReadLong(&net_message);

	cl.surpress_count = Msg_ReadByte(&net_message);

	if (cl_show_net_messages->integer == 3)
		Com_Print("   frame:%i  delta:%i\n", cl.frame.server_frame, cl.frame.delta_frame);

	if (cl.frame.delta_frame <= 0) { // uncompressed frame
		old_frame = NULL;
		cl.frame.valid = true;
	} else { // delta compressed frame
		old_frame = &cl.frames[cl.frame.delta_frame & UPDATE_MASK];

		if (!old_frame->valid)
			Com_Error(ERR_DROP, "Delta from invalid frame\n");

		if (old_frame->server_frame != (uint32_t) cl.frame.delta_frame)
			Com_Error(ERR_DROP, "Delta frame too old\n");

		else if (cl.entity_state - old_frame->entity_state > ENTITY_STATE_BACKUP - UPDATE_BACKUP)
			Com_Error(ERR_DROP, "Delta parse_entities too old\n");

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

			UnpackPosition(cl.frame.ps.pm_state.origin, cl.predicted_origin);
			UnpackAngles(cl.frame.ps.pm_state.view_angles, cl.predicted_angles);
		}

		Cl_CheckPredictionError();
	}
}

/*
 * @brief Invalidate lighting caches on media load.
 */
void Cl_UpdateEntities(void) {

	if (r_view.update) {
		uint16_t i;

		for (i = 0; i < MAX_EDICTS; i++) {
			cl.entities[i].lighting.state = LIGHTING_INIT;
			cl.entities[i].lighting.number = i;
		}
	}
}
