/*
 * Copyright(c) 1997-2001 id Software, Inc.
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
static void Cl_ParseDeltaEntity(cl_frame_t *frame, entity_state_t *from, uint16_t number,
		uint16_t bits) {

	cl_entity_t *ent = &cl.entities[number];

	entity_state_t *to = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;

	frame->num_entities++;

	Net_ReadDeltaEntity(&net_message, from, to, number, bits);

	// some changes will force no interpolation
	if (from->model1 != to->model1
			|| from->model2 != to->model2
			|| from->model3 != to->model3
			|| from->model4 != to->model4
			|| fabs(from->origin[0] - to->origin[0]) > 256.0
			|| fabs(from->origin[1] - to->origin[1]) > 256.0
			|| fabs(from->origin[2] - to->origin[2]) > 256.0) {
		ent->frame_num = -1;
	}

	if (ent->frame_num != cl.frame.frame_num - 1) {
		// wasn't in last update, so initialize some things
		// duplicate the current state to avoid interpolation
		ent->prev = *to;
		VectorCopy(to->old_origin, ent->prev.origin);
		ent->animation1.time = ent->animation2.time = 0;
	} else { // shuffle the last state to previous
		ent->prev = ent->current;
	}

	// finally set the current frame number and entity state
	ent->frame_num = cl.frame.frame_num;
	ent->current = *to;
}

/*
 * @brief An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *delta_frame, cl_frame_t *frame) {
	entity_state_t *old_state = NULL;
	uint32_t old_index;
	uint16_t old_number;

	frame->entity_state = cl.entity_state;
	frame->num_entities = 0;

	// delta from the entities present in old_frame
	old_index = 0;

	if (!delta_frame)
		old_number = 0xffff;
	else {
		if (old_index >= delta_frame->num_entities)
			old_number = 0xffff;
		else {
			old_state = &cl.entity_states[(delta_frame->entity_state + old_index)
					& ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}

	while (true) {
		const uint16_t number = Net_ReadShort(&net_message);

		if (number >= MAX_EDICTS)
			Com_Error(ERR_DROP, "Bad number: %i\n", number);

		if (net_message.read > net_message.size)
			Com_Error(ERR_DROP, "End of message\n");

		if (!number)
			break;

		const uint16_t bits = Net_ReadShort(&net_message);

		while (old_number < number) { // one or more entities from old_frame are unchanged

			if (cl_show_net_messages->integer == 3)
				Com_Print("   unchanged: %i\n", old_number);

			Cl_ParseDeltaEntity(frame, old_state, old_number, 0);

			old_index++;

			if (old_index >= delta_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(delta_frame->entity_state + old_index)
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

			if (old_index >= delta_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(delta_frame->entity_state + old_index)
						& ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
			continue;
		}

		if (old_number == number) { // delta from previous state

			if (cl_show_net_messages->integer == 3)
				Com_Print("   delta: %i\n", number);

			Cl_ParseDeltaEntity(frame, old_state, number, bits);

			old_index++;

			if (old_index >= delta_frame->num_entities)
				old_number = 0xffff;
			else {
				old_state = &cl.entity_states[(delta_frame->entity_state + old_index)
						& ENTITY_STATE_MASK];
				old_number = old_state->number;
			}
			continue;
		}

		if (old_number > number) { // delta from baseline

			if (cl_show_net_messages->integer == 3)
				Com_Print("   baseline: %i\n", number);

			Cl_ParseDeltaEntity(frame, &cl.entities[number].baseline, number, bits);
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while (old_number != 0xffff) { // one or more entities from the old packet are unchanged

		if (cl_show_net_messages->integer == 3)
			Com_Print("   unchanged: %i\n", old_number);

		Cl_ParseDeltaEntity(frame, old_state, old_number, 0);

		old_index++;

		if (old_index >= delta_frame->num_entities)
			old_number = 0xffff;
		else {
			old_state = &cl.entity_states[(delta_frame->entity_state + old_index)
					& ENTITY_STATE_MASK];
			old_number = old_state->number;
		}
	}
}

/*
 * @brief Parse the player_state_t for the current frame from the server, using delta
 * compression for all fields where possible.
 */
static void Cl_ParsePlayerstate(const cl_frame_t *delta_frame, cl_frame_t *frame) {
	player_state_t *ps;
	uint16_t pm_state_bits;
	int32_t i;
	uint32_t stat_bits;

	ps = &frame->ps;

	// copy old value before delta parsing
	if (delta_frame)
		*ps = delta_frame->ps;
	else
		// or start clean
		memset(ps, 0, sizeof(*ps));

	pm_state_bits = Net_ReadShort(&net_message);

	// parse the pm_state_t

	if (pm_state_bits & PS_PM_TYPE)
		ps->pm_state.type = Net_ReadByte(&net_message);

	if (cl.demo_server)
		ps->pm_state.type = PM_FREEZE;

	if (pm_state_bits & PS_PM_ORIGIN) {
		ps->pm_state.origin[0] = Net_ReadShort(&net_message);
		ps->pm_state.origin[1] = Net_ReadShort(&net_message);
		ps->pm_state.origin[2] = Net_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_PM_VELOCITY) {
		ps->pm_state.velocity[0] = Net_ReadShort(&net_message);
		ps->pm_state.velocity[1] = Net_ReadShort(&net_message);
		ps->pm_state.velocity[2] = Net_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_PM_FLAGS)
		ps->pm_state.flags = Net_ReadShort(&net_message);

	if (pm_state_bits & PS_PM_TIME)
		ps->pm_state.time = Net_ReadShort(&net_message);

	if (pm_state_bits & PS_PM_GRAVITY)
		ps->pm_state.gravity = Net_ReadShort(&net_message);

	if (pm_state_bits & PS_PM_VIEW_OFFSET) {
		ps->pm_state.view_offset[0] = Net_ReadShort(&net_message);
		ps->pm_state.view_offset[1] = Net_ReadShort(&net_message);
		ps->pm_state.view_offset[2] = Net_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_PM_VIEW_ANGLES) {
		ps->pm_state.view_angles[0] = Net_ReadShort(&net_message);
		ps->pm_state.view_angles[1] = Net_ReadShort(&net_message);
		ps->pm_state.view_angles[2] = Net_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_PM_KICK_ANGLES) {
		ps->pm_state.kick_angles[0] = Net_ReadShort(&net_message);
		ps->pm_state.kick_angles[1] = Net_ReadShort(&net_message);
		ps->pm_state.kick_angles[2] = Net_ReadShort(&net_message);
	}

	if (pm_state_bits & PS_PM_DELTA_ANGLES) {
		ps->pm_state.delta_angles[0] = Net_ReadShort(&net_message);
		ps->pm_state.delta_angles[1] = Net_ReadShort(&net_message);
		ps->pm_state.delta_angles[2] = Net_ReadShort(&net_message);
	}

	// parse stats

	stat_bits = Net_ReadLong(&net_message);

	for (i = 0; i < MAX_STATS; i++) {
		if (stat_bits & (1 << i))
			ps->stats[i] = Net_ReadShort(&net_message);
	}
}

/*
 * @brief
 */
void Cl_ParseFrame(void) {
	cl_frame_t *delta_frame;

	cl.frame.frame_num = Net_ReadLong(&net_message);

	cl.frame.delta_frame_num = Net_ReadLong(&net_message);

	cl.surpress_count = Net_ReadByte(&net_message);

	if (cl_show_net_messages->integer == 3)
		Com_Print("   frame:%i  delta:%i\n", cl.frame.frame_num, cl.frame.delta_frame_num);

	if (cl.frame.delta_frame_num <= 0) { // uncompressed frame
		delta_frame = NULL;
		cl.frame.valid = true;
	} else { // delta compressed frame
		delta_frame = &cl.frames[cl.frame.delta_frame_num & PACKET_MASK];

		if (!delta_frame->valid)
			Com_Error(ERR_DROP, "Delta from invalid frame\n");

		if (delta_frame->frame_num != cl.frame.delta_frame_num)
			Com_Error(ERR_DROP, "Delta frame too old\n");

		else if (cl.entity_state - delta_frame->entity_state > ENTITY_STATE_BACKUP - PACKET_BACKUP)
			Com_Error(ERR_DROP, "Delta parse_entities too old\n");

		cl.frame.valid = true;
	}

	const size_t len = Net_ReadByte(&net_message); // read area_bits
	Net_ReadData(&net_message, &cl.frame.area_bits, len);

	Cl_ParsePlayerstate(delta_frame, &cl.frame);

	Cl_ParseEntities(delta_frame, &cl.frame);

	// set the simulation time for the frame
	cl.frame.time = cl.frame.frame_num * 1000 / cl.server_hz;

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.frame_num & PACKET_MASK] = cl.frame;

	if (cl.frame.valid) {
		// getting a valid frame message ends the connection process
		if (cls.state != CL_ACTIVE) {
			cls.state = CL_ACTIVE;

			UnpackVector(cl.frame.ps.pm_state.origin, cl.predicted_state.origin);

			UnpackVector(cl.frame.ps.pm_state.view_offset, cl.predicted_state.view_offset);
			UnpackAngles(cl.frame.ps.pm_state.view_angles, cl.predicted_state.view_angles);
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
