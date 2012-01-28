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
				Com_Warn("Cl_ParseEntities: U_REMOVE: %u != %u.\n", old_number, number);

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
 * Cl_EntityEvents
 */
static void Cl_EntityEvents(cl_frame_t *frame) {
	int pnum;

	for (pnum = 0; pnum < frame->num_entities; pnum++) {
		const int num = (frame->entity_state + pnum) & ENTITY_STATE_MASK;
		Cl_EntityEvent(&cl.entity_states[num]);
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

		Cl_EntityEvents(&cl.frame); // fire entity events

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

/*
 * Cl_UpdateLighting
 */
static void Cl_UpdateLighting(cl_entity_t *e, r_entity_t *ent) {

	// setup the write-through lighting cache
	ent->lighting = &e->lighting;

	if (e->current.effects & EF_NO_LIGHTING) {
		// some entities are never lit, like rockets
		ent->lighting->state = LIGHTING_READY;
	} else {
		// but most are, so update their lighting if appropriate
		if (ent->lighting->state == LIGHTING_READY) {
			if (!VectorCompare(e->current.origin, e->prev.origin)) {
				ent->lighting->state = LIGHTING_DIRTY;
			}
		}
	}
}

/*
 * Cl_AddLinkedEntity
 *
 * Adds a linked static model (weapon, CTF flag, etc..) to the renderer.
 */
static void Cl_AddLinkedEntity(const r_entity_t *parent, int model,
		const char *tag_name) {

	if (!parent) {
		Com_Warn("Cl_AddLinkedEntity: NULL parent\n");
		return;
	}

	r_entity_t ent = *parent;

	ent.parent = parent;
	ent.tag_name = tag_name;

	ent.model = cl.model_draw[model];
	ent.skin = NULL;

	ent.frame = ent.old_frame = 0;

	ent.lerp = 1.0;
	ent.back_lerp = 0.0;

	R_AddEntity(&ent);
}

/*
 * Cl_AddClientEntity
 *
 * Adds the numerous render entities which comprise a given client (player)
 * entity: head, upper, lower, weapon, flags, etc.
 */
static void Cl_AddClientEntity(cl_entity_t *e, r_entity_t *ent) {
	const entity_state_t *s = &e->current;
	const cl_client_info_t *ci = &cl.client_info[s->client];
	r_entity_t head, upper, lower;

	int effects = s->effects;

	ent->origin[2] -= 6.0; // small hack for qforcer

	// copy the specified entity to all body segments
	head = upper = lower = *ent;

	head.model = ci->head;
	head.skin = ci->head_skin;

	upper.model = ci->upper;
	upper.skin = ci->upper_skin;

	lower.model = ci->lower;
	lower.skin = ci->lower_skin;

	Cl_AnimateClientEntity(e, &upper, &lower);

	// don't draw ourselves unless third person is set
	if (s->number == cl.player_num + 1) {

		if (!cl_third_person->value) {
			effects |= EF_NO_DRAW;

			// keep our shadow underneath us using the predicted origin
			lower.origin[0] = r_view.origin[0];
			lower.origin[1] = r_view.origin[1];
		}
	}

	head.effects = upper.effects = lower.effects = effects;

	Cl_UpdateLighting(e, &lower);

	head.lighting = upper.lighting = lower.lighting;

	upper.parent = R_AddEntity(&lower);
	upper.tag_name = "tag_torso";

	head.parent = R_AddEntity(&upper);
	head.tag_name = "tag_head";

	R_AddEntity(&head);

	if (s->model2)
		Cl_AddLinkedEntity(head.parent, s->model2, "tag_weapon");

	if (s->model3)
		Cl_AddLinkedEntity(head.parent, s->model3, "tag_head");

	if (s->model4)
		Com_Warn("Cl_AddClientEntity: Unsupported model_index4\n");
}

/*
 * Cl_WeaponKick
 *
 * Calculates a kick offset and angles based on our player's animation state.
 */
static void Cl_WeaponKick(cl_entity_t *e, vec3_t offset, vec3_t angles) {
	const vec3_t drop_raise_offset = { -4.0, -4.0, -4.0 };
	const vec3_t drop_raise_angles = { 25.0, -35.0, 2.0 };

	const vec3_t kick_offset = { -3.0, 0.0, 1.0 };
	const vec3_t kick_angles = { -12.0, 0.0, 0.0 };

	VectorClear(offset);
	VectorClear(angles);

	if (e->animation1.animation == ANIM_TORSO_DROP) {
		VectorScale(drop_raise_offset, e->animation1.fraction, offset);
		VectorScale(drop_raise_angles, e->animation1.fraction, angles);
	} else if (e->animation1.animation == ANIM_TORSO_RAISE) {
		VectorScale(drop_raise_offset, 1.0 - e->animation1.fraction, offset);
		VectorScale(drop_raise_angles, 1.0 - e->animation1.fraction, angles);
	} else if (e->animation1.animation == ANIM_TORSO_ATTACK1) {
		VectorScale(kick_offset, 1.0 - e->animation1.fraction, offset);
		VectorScale(kick_angles, 1.0 - e->animation1.fraction, angles);
	}

	VectorScale(offset, cl_bob->value, offset);
	VectorScale(angles, cl_bob->value, angles);
}

/*
 * Cl_AddWeapon
 */
static void Cl_AddWeapon(cl_entity_t *e, r_entity_t *self) {
	static r_entity_t ent;
	static r_lighting_t lighting;
	vec3_t offset, angles;
	int w;

	if (!cl_weapon->value)
		return;

	if (cl_third_person->value)
		return;

	if (!cl.frame.ps.stats[STAT_HEALTH])
		return; // deadz0r

	if (cl.frame.ps.stats[STAT_SPECTATOR])
		return; // spectating

	if (cl.frame.ps.stats[STAT_CHASE])
		return; // chase camera

	w = cl.frame.ps.stats[STAT_WEAPON];
	if (!w) // no weapon, e.g. level intermission
		return;

	memset(&ent, 0, sizeof(ent));

	Cl_WeaponKick(e, offset, angles);

	VectorCopy(r_view.origin, ent.origin);

	VectorMA(ent.origin, offset[2], r_view.up, ent.origin);
	VectorMA(ent.origin, offset[1], r_view.right, ent.origin);
	VectorMA(ent.origin, offset[0], r_view.forward, ent.origin);

	VectorAdd(r_view.angles, angles, ent.angles);

	ent.effects = EF_WEAPON | EF_NO_SHADOW;

	VectorCopy(self->shell, ent.shell);

	ent.model = cl.model_draw[w];

	ent.lerp = ent.scale = 1.0;

	memset(&lighting, 0, sizeof(lighting));
	ent.lighting = &lighting;

	R_AddEntity(&ent);
}

/*
 * Cl_AddEntities
 *
 * Iterate all entities in the current frame, adding models, particles,
 * lights, and anything else associated with them.  Client-side animations
 * like bobbing and rotating are also resolved here.
 *
 * Entities with models are conditionally added to the view based on the
 * cl_add_entities bit mask.
 */
void Cl_AddEntities(cl_frame_t *frame) {
	int i;

	if (!cl_add_entities->value)
		return;

	// resolve any models, animations, interpolations, rotations, bobbing, etc..
	for (i = 0; i < frame->num_entities; i++) {
		const entity_state_t *s = &cl.entity_states[(frame->entity_state + i)
				& ENTITY_STATE_MASK];
		cl_entity_t *e = &cl.entities[s->number];
		r_entity_t ent;
		vec3_t start, end;
		int mask;

		memset(&ent, 0, sizeof(ent));
		ent.scale = 1.0;

		// beams have two origins, most entities have just one
		if (s->effects & EF_BEAM) {

			// client is overridden to specify owner of the beam
			if ((s->client == cl.player_num + 1) && !cl_third_person->value) {
				// we own this beam (lightning, grapple, etc..)
				// project start position in front of view origin
				VectorCopy(r_view.origin, start);
				VectorMA(start, 30.0, r_view.forward, start);
				VectorMA(start, 6.0, r_view.right, start);
				start[2] -= 10.0;
			} else
				// or simply interpolate the start position
				VectorLerp(e->prev.origin, e->current.origin, cl.lerp, start);

			VectorLerp(e->prev.old_origin, e->current.old_origin, cl.lerp, end);
		} else { // for most ents, just interpolate the origin
			VectorLerp(e->prev.origin, e->current.origin, cl.lerp, ent.origin);
		}

		// bob items, shift them to randomize the effect in crowded scenes
		if (s->effects & EF_BOB)
			ent.origin[2] += 4.0 * sin(
					(cl.time * 0.005) + ent.origin[0] + ent.origin[1]);

		// calculate angles
		if (s->effects & EF_ROTATE) { // some bonus items rotate
			ent.angles[0] = 0.0;
			ent.angles[1] = cl.time / 3.4;
			ent.angles[2] = 0.0;
		} else { // interpolate angles
			AngleLerp(e->prev.angles, e->current.angles, cl.lerp, ent.angles);
		}

		// parse the effects bit mask

		if (s->effects & EF_GRENADE) {
			Cl_GrenadeTrail(e->prev.origin, ent.origin, e);
		}

		if (s->effects & EF_ROCKET) {
			Cl_RocketTrail(e->prev.origin, ent.origin, e);
		}

		if (s->effects & EF_HYPERBLASTER) {
			Cl_HyperblasterTrail(e);
		}

		if (s->effects & EF_LIGHTNING) {
			Cl_LightningTrail(start, end);
		}

		if (s->effects & EF_BFG) {
			Cl_BfgTrail(e);
		}

		VectorClear(ent.shell);

		if (s->effects & EF_CTF_BLUE) {
			r_light_t ctf_blue_light = { { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.3,
					1.0 } };

			VectorCopy(ent.origin, ctf_blue_light.origin);
			R_AddLight(&ctf_blue_light);

			VectorScale(ctf_blue_light.color, 0.5, ent.shell);
		}

		if (s->effects & EF_CTF_RED) {
			r_light_t ctf_red_light = { { 0.0, 0.0, 0.0 }, 80.0, { 1.0, 0.3,
					0.3 } };

			VectorCopy(ent.origin, ctf_red_light.origin);
			R_AddLight(&ctf_red_light);

			VectorScale(ctf_red_light.color, 0.5, ent.shell);
		}

		if (s->effects & EF_QUAD) {
			r_light_t quad_light =
					{ { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.7, 0.7 } };

			VectorCopy(ent.origin, quad_light.origin);
			R_AddLight(&quad_light);

			VectorScale(quad_light.color, 0.5, ent.shell);
		}

		if (s->effects & EF_TELEPORTER)
			Cl_TeleporterTrail(ent.origin, e);

		// if there's no model associated with the entity, we're done
		if (!s->model1)
			continue;

		// assign the model, players will reference 0xff
		ent.model = cl.model_draw[s->model1];

		// filter by model type
		mask = ent.model && ent.model->type == mod_bsp_submodel ? 1 : 2;

		if (!(cl_add_entities->integer & mask))
			continue;

		// pass the effects through to the renderer
		ent.effects = s->effects;

		if (s->model1 == 0xff) { // add the client entities

			Cl_AddClientEntity(e, &ent);

			if (s->number == cl.player_num + 1)
				Cl_AddWeapon(e, &ent);

			continue;
		}

		// setup the lighting cache, flagging those which are stale
		Cl_UpdateLighting(e, &ent);

		// add to view list
		R_AddEntity(&ent);
	}
}
