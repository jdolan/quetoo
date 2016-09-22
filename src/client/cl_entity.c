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

#include "cl_local.h"

/**
 * @brief Parse the player_state_t for the current frame from the server, using delta
 * compression for all fields where possible.
 */
static void Cl_ParsePlayerState(const cl_frame_t *delta_frame, cl_frame_t *frame) {
	static player_state_t null_state;

	if (delta_frame && delta_frame->valid)
		Net_ReadDeltaPlayerState(&net_message, &delta_frame->ps, &frame->ps);
	else
		Net_ReadDeltaPlayerState(&net_message, &null_state, &frame->ps);

	if (cl.demo_server) // if playing a demo, force freeze
		frame->ps.pm_state.type = PM_FREEZE;
}

/**
 * @return True if the delta is valid and interpolation should be used.
 */
static _Bool Cl_ValidDeltaEntity(const entity_state_t *from, const entity_state_t *to) {
	vec3_t delta;

	if (from->model1 != to->model1)
		return false;

	VectorSubtract(from->origin, to->origin, delta);

	if (VectorLength(delta) > 256.0)
		return false;

	return true;
}

/**
 * @brief Reads deltas from the given base and adds the resulting entity to the
 * current frame.
 */
static void Cl_ReadDeltaEntity(cl_frame_t *frame, entity_state_t *from, uint16_t number, uint16_t bits) {

	cl_entity_t *ent = &cl.entities[number];

	entity_state_t *to = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;

	frame->num_entities++;

	Net_ReadDeltaEntity(&net_message, from, to, number, bits);

	// check to see if the delta was successful and valid
	if (ent->frame_num != cl.frame.frame_num - 1 || !Cl_ValidDeltaEntity(from, to)) {
		ent->prev = *to; // copy the current state to the previous
		ent->animation1.time = ent->animation2.time = 0;
		ent->animation1.frame = ent->animation2.frame = -1;
	} else { // shuffle the last state to previous
		ent->prev = ent->current;
	}

	// set the current frame number and entity state
	ent->frame_num = cl.frame.frame_num;
	ent->current = *to;

	// update clipping matrices, snapping the entity to the frame
	if (ent->current.solid) {
		if (ent->current.solid == SOLID_BSP) {
			if (bits & (U_ORIGIN | U_ANGLES)) {
				Matrix4x4_CreateFromEntity(&ent->matrix, ent->current.origin, ent->current.angles, 1.0);
				Matrix4x4_Invert_Simple(&ent->inverse_matrix, &ent->matrix);
			}
		} else { // bounding-box entities
			if (bits & U_ORIGIN) {
				Matrix4x4_CreateFromEntity(&ent->matrix, ent->current.origin, vec3_origin, 1.0);
				Matrix4x4_Invert_Simple(&ent->inverse_matrix, &ent->matrix);
			}
		}
	}

	// mark the lighting cache as dirty
	if (bits & U_ORIGIN) {
		ent->lighting.state = LIGHTING_DIRTY;
	}

	// add any new auto-looped sounds
	if (ent->current.sound) {
		S_AddSample(&(const s_play_sample_t) {
			.sample = cl.sound_precache[ent->current.sound],
			.entity = ent->current.number,
			.attenuation = ATTEN_IDLE,
			.flags = S_PLAY_ENTITY | S_PLAY_LOOP | S_PLAY_FRAME
		});
	}
}

/**
 * @brief An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *delta_frame, cl_frame_t *frame) {

	frame->entity_state = cl.entity_state;
	frame->num_entities = 0;

	entity_state_t *state = NULL;
	uint16_t delta_number;

	if (delta_frame == NULL || delta_frame->num_entities == 0) {
		delta_number = UINT16_MAX;
	} else {
		state = &cl.entity_states[delta_frame->entity_state & ENTITY_STATE_MASK];
		delta_number = state->number;
	}

	uint32_t index = 0;

	while (true) {
		const uint16_t number = Net_ReadShort(&net_message);

		if (number >= MAX_ENTITIES)
			Com_Error(ERR_DROP, "Bad number: %i\n", number);

		if (net_message.read > net_message.size)
			Com_Error(ERR_DROP, "End of message\n");

		if (!number) // done
			break;

		// before dealing with the new entity, copy unchanged entities into the frame
		while (delta_number < number) {

			if (cl_show_net_messages->integer == 3)
				Com_Print("   unchanged: %i\n", delta_number);

			Cl_ReadDeltaEntity(frame, state, delta_number, 0);

			index++;

			if (index >= delta_frame->num_entities) {
				delta_number = UINT16_MAX;
			} else {
				state = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				delta_number = state->number;
			}
		}

		// now deal with the new entity
		const uint16_t bits = Net_ReadShort(&net_message);

		if (bits & U_REMOVE) { // remove it, no delta

			if (cl_show_net_messages->integer == 3)
				Com_Print("   remove: %i\n", number);

			if (delta_number != number)
				Com_Warn("U_REMOVE: %u != %u\n", delta_number, number);

			index++;

			if (index >= delta_frame->num_entities) {
				delta_number = UINT16_MAX;
			} else {
				state = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				delta_number = state->number;
			}

			continue;
		}

		if (delta_number == number) { // delta from previous state

			if (cl_show_net_messages->integer == 3)
				Com_Print("   delta: %i\n", number);

			Cl_ReadDeltaEntity(frame, state, number, bits);

			index++;

			if (index >= delta_frame->num_entities) {
				delta_number = UINT16_MAX;
			} else {
				state = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				delta_number = state->number;
			}

			continue;
		}

		if (delta_number > number) { // delta from baseline

			if (cl_show_net_messages->integer == 3)
				Com_Print("   baseline: %i\n", number);

			Cl_ReadDeltaEntity(frame, &cl.entities[number].baseline, number, bits);

			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while (delta_number != UINT16_MAX) { // one or more entities from the old packet are unchanged

		if (cl_show_net_messages->integer == 3)
			Com_Print("   unchanged: %i\n", delta_number);

		Cl_ReadDeltaEntity(frame, state, delta_number, 0);

		index++;

		if (index >= delta_frame->num_entities) {
			delta_number = UINT16_MAX;
		} else {
			state = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
			delta_number = state->number;
		}
	}
}

/**
 * @brief
 */
void Cl_ParseFrame(void) {

	memset(&cl.frame, 0, sizeof(cl.frame));

	cl.frame.frame_num = Net_ReadLong(&net_message);

	cl.frame.delta_frame_num = Net_ReadLong(&net_message);

	cl.surpress_count = Net_ReadByte(&net_message);

	if (cl_show_net_messages->integer == 3)
		Com_Print("   frame:%i  delta:%i\n", cl.frame.frame_num, cl.frame.delta_frame_num);

	if (cl.frame.delta_frame_num <= 0) { // uncompressed frame
		cl.delta_frame = NULL;
		cl.frame.valid = true;
	} else { // delta compressed frame
		cl.delta_frame = &cl.frames[cl.frame.delta_frame_num & PACKET_MASK];

		if (!cl.delta_frame->valid)
			Com_Error(ERR_DROP, "Delta from invalid frame\n");

		if (cl.delta_frame->frame_num != cl.frame.delta_frame_num)
			Com_Error(ERR_DROP, "Delta frame too old\n");

		else if (cl.entity_state - cl.delta_frame->entity_state > ENTITY_STATE_BACKUP - PACKET_BACKUP)
			Com_Error(ERR_DROP, "Delta entity state too old\n");

		cl.frame.valid = true;
	}

	const size_t len = Net_ReadByte(&net_message); // read area_bits
	Net_ReadData(&net_message, &cl.frame.area_bits, len);

	Cl_ParsePlayerState(cl.delta_frame, &cl.frame);

	Cl_ParseEntities(cl.delta_frame, &cl.frame);

	// set the simulation time for the frame
	cl.frame.time = cl.frame.frame_num * (1000 / cl.server_hz);

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.frame_num & PACKET_MASK] = cl.frame;

	if (cl.frame.valid) {
		// getting a valid frame message ends the connection process
		if (cls.state == CL_LOADING) {
			cls.state = CL_ACTIVE;

			VectorCopy(cl.frame.ps.pm_state.origin, cl.predicted_state.origin);

			UnpackVector(cl.frame.ps.pm_state.view_offset, cl.predicted_state.view_offset);
			UnpackAngles(cl.frame.ps.pm_state.view_angles, cl.predicted_state.view_angles);
		}

		Cl_CheckPredictionError();
	}
}

/**
 * @brief Updates the interpolation fraction for the current client frame.
 * Because the client typically runs at a higher framerate than the server, we
 * use linear interpolation between the last 2 server frames. We aim to reach
 * the current server time just as a new packet arrives.
 */
static void Cl_UpdateLerp(void) {

	if (cl.delta_frame == NULL || time_demo->value) {
		cl.time = cl.frame.time;
		cl.lerp = 1.0;
		return;
	}

	const uint32_t frame_millis = 1000.0 / cl.server_hz;

	if (cl.time > cl.frame.time) {
		// Com_Debug("High clamp: %dms\n", cl.time - cl.frame.time);
		cl.time = cl.frame.time;
		cl.lerp = 1.0;
	} else if (cl.time < cl.frame.time - frame_millis) {
		// Com_Debug("Low clamp: %dms\n", (cl.frame.time - frame_millis) - cl.time);
		cl.time = cl.frame.time - frame_millis;
		cl.lerp = 0.0;
	} else {
		cl.lerp = 1.0 - (cl.frame.time - cl.time) / (vec_t) frame_millis;
	}
}

/**
 * @brief Interpolates all entities in the current frame.
 */
void Cl_Interpolate(void) {

	Cl_UpdateLerp();

	for (uint16_t i = 0; i < cl.frame.num_entities; i++) {

		const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		cl_entity_t *ent = &cl.entities[cl.entity_states[snum].number];

		if (!VectorCompare(ent->prev.origin, ent->current.origin)) {
			VectorLerp(ent->prev.origin, ent->current.origin, cl.lerp, ent->origin);
			ent->lighting.state = LIGHTING_DIRTY;
		} else {
			VectorCopy(ent->current.origin, ent->origin);
		}

		if (!VectorCompare(ent->prev.termination, ent->current.termination)) {
			VectorLerp(ent->prev.termination, ent->current.termination, cl.lerp, ent->termination);
			ent->lighting.state = LIGHTING_DIRTY;
		} else {
			VectorCopy(ent->current.termination, ent->termination);
		}

		if (!VectorCompare(ent->prev.angles, ent->current.angles)) {
			AngleLerp(ent->prev.angles, ent->current.angles, cl.lerp, ent->angles);
			ent->lighting.state = LIGHTING_DIRTY;
		} else {
			VectorCopy(ent->current.angles, ent->angles);
		}
	}
}

/**
 * @brief Invalidate lighting caches on media load.
 */
void Cl_UpdateEntities(void) {

	if (r_view.update) {

		for (size_t i = 0; i < lengthof(cl.entities); i++) {
			r_lighting_t *lighting = &cl.entities[i].lighting;

			memset(lighting, 0, sizeof(*lighting));

			lighting->state = LIGHTING_DIRTY;
			lighting->number = i;
		}
	}
}
