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

	if (delta_frame && delta_frame->valid) {
		Net_ReadDeltaPlayerState(&net_message, &delta_frame->ps, &frame->ps);
	} else {
		Net_ReadDeltaPlayerState(&net_message, &null_state, &frame->ps);
	}

	if (cl.demo_server) { // if playing a demo, force freeze
		frame->ps.pm_state.type = PM_FREEZE;
	}
}

/**
 * @return True if the delta is valid and the entity should be interpolated, false
 * if the delta is invalid and the entity should be snapped to `to`.
 */
static _Bool Cl_ValidDeltaEntity(const cl_frame_t *frame, const cl_entity_t *ent,
                                 const entity_state_t *from, const entity_state_t *to) {

	if (frame->delta_frame_num == -1) {
		return false;
	}

	if (cl.previous_frame) {
		if (ent->frame_num != cl.previous_frame->frame_num) {
			return false;
		}
	}

	if (ent->current.model1 != to->model1) {
		return false;
	}

	if (Vec3_Distance(ent->current.origin, to->origin) > MAX_DELTA_ORIGIN) {
		return false;
	}

	return true;
}

/**
 * @brief Resets all trails to initial values
 * @param ent Entity to reset trails for
 */
static void Cl_ResetTrails(cl_entity_t *ent) {

	for (cl_entity_trail_t *trail = ent->trails; trail < ent->trails + lengthof(ent->trails); trail++) {
		trail->last_origin = ent->previous_origin;
		trail->trail_updated = false;
	}
}

/**
 * @brief Reads deltas from the given base and adds the resulting entity to the
 * current frame.
 */
static void Cl_ReadDeltaEntity(cl_frame_t *frame, const entity_state_t *from, uint16_t number, uint16_t bits) {

	cl_entity_t *ent = &cl.entities[number];

	entity_state_t *to = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;

	frame->num_entities++;

	Net_ReadDeltaEntity(&net_message, from, to, number, bits);

	// check to see if the delta was successful and valid
	if (!Cl_ValidDeltaEntity(frame, ent, from, to)) {
		ent->prev = *to; // copy the current state to the previous
		ent->animation1.time = ent->animation2.time = 0;
		ent->animation1.frame = ent->animation2.frame = -1;
		ent->previous_origin = to->origin;
		Cl_ResetTrails(ent);
		ent->legs_current_yaw = to->angles.y;
	} else { // shuffle the last state to previous
		ent->prev = ent->current;
	}

	// set the current frame number and entity state
	ent->frame_num = cl.frame.frame_num;
	ent->current = *to;
}

/**
 * @brief An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *delta_frame, cl_frame_t *frame) {

	frame->entity_state = cl.entity_state;
	frame->num_entities = 0;

	entity_state_t *from = NULL;
	uint16_t from_number;

	if (delta_frame == NULL || delta_frame->num_entities == 0) {
		from_number = UINT16_MAX;
	} else {
		from = &cl.entity_states[delta_frame->entity_state & ENTITY_STATE_MASK];
		from_number = from->number;
	}

	int32_t index = 0;

	while (true) {
		const uint16_t number = Net_ReadShort(&net_message);

		if (number >= MAX_ENTITIES) {
			Com_Error(ERROR_DROP, "Bad number: %i\n", number);
		}

		if (net_message.read > net_message.size) {
			Com_Error(ERROR_DROP, "End of message\n");
		}

		if (!number) { // done
			break;
		}

		// before dealing with new entities, copy unchanged entities into the frame
		while (from_number < number) {

			if (cl_draw_net_messages->integer == 3) {
				Com_Print("   unchanged: %i\n", from_number);
			}

			Cl_ReadDeltaEntity(frame, from, from_number, 0);

			index++;

			if (index >= delta_frame->num_entities) {
				from_number = UINT16_MAX;
			} else {
				from = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				from_number = from->number;
			}
		}

		// now deal with the new entity
		const uint16_t bits = Net_ReadShort(&net_message);

		if (bits & U_REMOVE) { // remove it, no delta

			if (cl_draw_net_messages->integer == 3) {
				Com_Print("   remove: %i\n", number);
			}

			if (from_number != number) {
				Com_Debug(DEBUG_CLIENT, "U_REMOVE: %u != %u\n", from_number, number);
			}

			index++;

			if (index >= delta_frame->num_entities) {
				from_number = UINT16_MAX;
			} else {
				from = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				from_number = from->number;
			}

			continue;
		}

		if (from_number == number) { // delta from previous state

			if (cl_draw_net_messages->integer == 3) {
				Com_Print("   delta: %i\n", number);
			}

			Cl_ReadDeltaEntity(frame, from, number, bits);

			index++;

			if (index >= delta_frame->num_entities) {
				from_number = UINT16_MAX;
			} else {
				from = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
				from_number = from->number;
			}

			continue;
		}

		if (from_number > number) { // delta from baseline

			if (cl_draw_net_messages->integer == 3) {
				Com_Print("   baseline: %i\n", number);
			}

			Cl_ReadDeltaEntity(frame, &cl.entities[number].baseline, number, bits);

			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while (from_number != UINT16_MAX) { // one or more entities from the old packet are unchanged

		if (cl_draw_net_messages->integer == 3) {
			Com_Print("   unchanged: %i\n", from_number);
		}

		Cl_ReadDeltaEntity(frame, from, from_number, 0);

		index++;

		if (index >= delta_frame->num_entities) {
			from_number = UINT16_MAX;
		} else {
			from = &cl.entity_states[(delta_frame->entity_state + index) & ENTITY_STATE_MASK];
			from_number = from->number;
		}
	}
}

/**
 * @brief Parses a new server frame, ensuring that the previously received frame is interpolated
 * before proceeding. This ensure that all server frames are processed, even if their simulation
 * result doesn't make it to the screen.
 */
void Cl_ParseFrame(void) {

	if (cl.frame.valid && !cl.frame.interpolated) {
		Cl_Interpolate();
	}

	memset(&cl.frame, 0, sizeof(cl.frame));

	cl.frame.frame_num = Net_ReadLong(&net_message);
	cl.frame.delta_frame_num = Net_ReadLong(&net_message);

	cl.suppress_count += Net_ReadByte(&net_message);

	if (cl_draw_net_messages->integer == 3) {
		Com_Print("   frame:%i  delta:%i\n", cl.frame.frame_num, cl.frame.delta_frame_num);
	}

	if (cl.frame.delta_frame_num <= 0) { // uncompressed frame
		cl.delta_frame = cl.previous_frame = NULL;
	} else { // delta compressed frame
		cl.delta_frame = &cl.frames[cl.frame.delta_frame_num & PACKET_MASK];

		if (!cl.delta_frame->valid) {
			Com_Error(ERROR_DROP, "Delta from invalid frame\n");
		} else if (cl.delta_frame->frame_num != cl.frame.delta_frame_num) {
			Com_Error(ERROR_DROP, "Delta frame too old\n");
		} else if (cl.entity_state - cl.delta_frame->entity_state > ENTITY_STATE_BACKUP - PACKET_BACKUP) {
			Com_Error(ERROR_DROP, "Delta entity state too old\n");
		}

		cl.previous_frame = &cl.frames[(cl.frame.frame_num - 1) & PACKET_MASK];

		if (cl.previous_frame->frame_num != (cl.frame.frame_num - 1)) {
			Com_Debug(DEBUG_CLIENT, "Previous frame too old\n");
			cl.previous_frame = NULL;
		} else if (!cl.previous_frame->valid) {
			Com_Debug(DEBUG_CLIENT, "Previous frame invalid\n");
			cl.previous_frame = NULL;
		}
	}

	cl.frame.valid = true;

	Cl_ParsePlayerState(cl.delta_frame, &cl.frame);

	Cl_ParseEntities(cl.delta_frame, &cl.frame);

	// set the simulation time for the frame
	cl.frame.time = cl.frame.frame_num * QUETOO_TICK_MILLIS;

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.frame_num & PACKET_MASK] = cl.frame;

	if (cl.frame.valid) {

		// getting a valid frame message ends the connection process
		if (cls.state == CL_LOADING) {
			cls.state = CL_ACTIVE;
		}

		Cl_CheckPredictionError();
	}
}

/**
 * @brief Updates the interpolation fraction for the current client frame.
 * Because the client often runs at a higher framerate than the server, we
 * use linear interpolation between the last 2 server frames. We aim to reach
 * the current server time just as a new packet arrives.
 *
 * @remarks The client advances its simulation time each frame, by the elapsed
 * millisecond delta. Here, we clamp the simulation time to be within the
 * range of the current frame. Even under ideal conditions, it's likely that
 * clamping will occur due to e.g. network jitter.
 */
static void Cl_UpdateLerp(void) {

	_Bool no_lerp = cl.delta_frame == NULL || cl_no_lerp->value || time_demo->value;

	if (no_lerp == false && cl.previous_frame) {
		vec3_t delta;

		delta = Vec3_Subtract(cl.frame.ps.pm_state.origin, cl.previous_frame->ps.pm_state.origin);
		if (Vec3_Length(delta) > MAX_DELTA_ORIGIN) {
			Com_Debug(DEBUG_CLIENT, "%d No lerp\n", cl.frame.frame_num);
			no_lerp = true;
		}
	}

	if (no_lerp) {
		cl.time = cl.frame.time;
		cl.lerp = 1.0;
	} else {
		if (cl.time > cl.frame.time) {
			Com_Debug(DEBUG_CLIENT, "High clamp: %dms\n", cl.time - cl.frame.time);
			cl.time = cl.frame.time;
			cl.lerp = 1.0;
		} else if (cl.time < cl.frame.time - QUETOO_TICK_MILLIS) {
			Com_Debug(DEBUG_CLIENT, "Low clamp: %dms\n", (cl.frame.time - QUETOO_TICK_MILLIS) - cl.time);
			cl.time = cl.frame.time - QUETOO_TICK_MILLIS;
			cl.lerp = 0.0;
		} else {
			cl.lerp = 1.0 - (cl.frame.time - cl.time) / (float) QUETOO_TICK_MILLIS;
		}
	}
}

/**
 * @brief Interpolates the simulation for the most recently parsed server frame.
 * @remarks This can be called multiple times per frame, in the event that the client has received
 * multiple server updates at once. This happens somewhat frequently, especially at higher server
 * tick rates, or with significant network jitter.
 */
void Cl_Interpolate(void) {

	if (cls.state != CL_ACTIVE) {
		return;
	}

	if (!cl.frame.valid) {
		return;
	}

	Cl_UpdateLerp();

	for (int32_t i = 0; i < cl.frame.num_entities; i++) {

		const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		cl_entity_t *ent = &cl.entities[cl.entity_states[snum].number];

		if (!Vec3_Equal(ent->prev.origin, ent->current.origin)) {
			ent->previous_origin = ent->origin;
			ent->origin = Vec3_Mix(ent->prev.origin, ent->current.origin, cl.lerp);
		} else {
			ent->origin = ent->current.origin;
		}

		if (!Vec3_Equal(ent->prev.termination, ent->current.termination)) {
			ent->termination = Vec3_Mix(ent->prev.termination, ent->current.termination, cl.lerp);
		} else {
			ent->termination = ent->current.termination;
		}

		if (!Vec3_Equal(ent->prev.angles, ent->current.angles)) {
			ent->angles = Vec3_MixEuler(ent->prev.angles, ent->current.angles, cl.lerp);
		} else {
			ent->angles = ent->current.angles;
		}

		if (ent->current.animation1 != ent->prev.animation1 || !ent->animation1.time) {
			ent->animation1.animation = ent->current.animation1 & ANIM_MASK_VALUE;
			ent->animation1.time = cl.unclamped_time;
			ent->animation1.reverse = ent->current.animation1 & ANIM_REVERSE_BIT;
		}

		if (ent->current.animation2 != ent->prev.animation2 || !ent->animation2.time) {
			ent->animation2.animation = ent->current.animation2 & ANIM_MASK_VALUE;
			ent->animation2.time = cl.unclamped_time;
			ent->animation2.reverse = ent->current.animation2 & ANIM_REVERSE_BIT;
		}

		if (ent->current.sound) {
			S_AddSample(&(const s_play_sample_t) {
				.sample = cl.sound_precache[ent->current.sound],
				.entity = ent->current.number,
				.attenuation = ATTEN_IDLE,
				.flags = S_PLAY_ENTITY | S_PLAY_LOOP | S_PLAY_FRAME
			});
			ent->current.sound = 0;
		}

		vec3_t angles;
		if (ent->current.solid == SOLID_BSP) {
			angles = ent->angles;

			const r_model_t *mod = cl.model_precache[ent->current.model1];

			assert(mod);
			assert(mod->bsp_inline);

			ent->mins = mod->bsp_inline->mins;
			ent->maxs = mod->bsp_inline->maxs;
		} else {
			angles = Vec3_Zero();

			ent->mins = ent->current.mins;
			ent->maxs = ent->current.maxs;
		}

		// FIXME
		//
		// Currently, the entity's collision state is snapped to the most recently received
		// server frame, rather than its interpolated state. This works well with the prediction
		// code, but has certain side effects, such as shadows flickering while riding platforms
		// etc. Ideally, we would create the clipping matrix from ent->origin and ent->angles,
		// but this introduces prediction error issues. Need to understand why the prediction
		// code doesn't play well with the more accurate collision simulation.

		Cm_EntityBounds(ent->current.solid, ent->current.origin,
						angles,
						ent->mins,
						ent->maxs,
						&ent->abs_mins,
						&ent->abs_maxs);

		Matrix4x4_CreateFromEntity(&ent->matrix, ent->current.origin, angles, 1.f);
		Matrix4x4_Invert_Simple(&ent->inverse_matrix, &ent->matrix);
	}

	cls.cgame->Interpolate(&cl.frame);

	cl.frame.interpolated = true;
}
