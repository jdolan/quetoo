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

#include "cg_local.h"
#include "game/default/bg_pmove.h"

/**
 * @return True if the specified entity is bound to the local client.
 */
_Bool Cg_IsSelf(const cl_entity_t *ent) {

	if (ent->current.number == cgi.client->client_num + 1)
		return true;

	if ((ent->current.effects & EF_CORPSE) == 0) {
		
		if (ent->current.model1 == MODEL_CLIENT || (ent->current.effects & EF_BEAM)) {

			if (ent->current.client == cgi.client->client_num)
				return true;

			const int16_t chase = cgi.client->frame.ps.stats[STAT_CHASE] - CS_CLIENTS;

			if (ent->current.client == chase)
				return true;
		}
	}

	return false;
}

/**
 * @return True if the entity is ducking, false otherwise.
 */
_Bool Cg_IsDucking(const entity_state_t *ent) {

	vec3_t mins, maxs;
	UnpackBounds(ent->bounds, mins, maxs);

	const vec_t standing_height = (PM_MAXS[2] - PM_MINS[2]) * PM_SCALE;
	const vec_t height = maxs[2] - mins[2];

	return standing_height - height > PM_STOP_EPSILON;
}

/**
 * @brief Adds client-side player effects such as breath puffs in rain or snow
 */
static void Cg_AddBreathPuffs(cl_entity_t *ent) {

	if (cgi.view->time < ent->breath_puff_time)
		return;

	cg_particle_t *p;

	vec3_t pos;
	VectorCopy(ent->origin, pos);

	if (Cg_IsDucking(&ent->current))
		pos[2] += 18.0;
	else
		pos[2] += 30.0;

	vec3_t forward;
	AngleVectors(ent->angles, forward, NULL, NULL);

	VectorMA(pos, 8.0, forward, pos);

	if (cgi.PointContents(pos) & MASK_LIQUID) {

		if (!(p = Cg_AllocParticle(PARTICLE_BUBBLE, cg_particles_bubble)))
			return;

		cgi.ColorFromPalette(6 + (Random() & 3), p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -0.2 - Randomf() * 0.2);

		p->part.scale = 3.0;
		p->scale_vel = -0.4 - Randomf() * 0.2;

		VectorScale(forward, 2.0, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = pos[j] + Randomc() * 2.0;
			p->vel[j] += Randomc() * 5.0;
		}

		p->vel[2] += 6.0;
		p->accel[2] = 10.0;

		ent->breath_puff_time = cgi.view->time + 3000;
	} else if (cgi.view->weather & WEATHER_RAIN || cgi.view->weather & WEATHER_SNOW) {

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_steam)))
			return;
		
		cgi.ColorFromPalette(6 + (Random() & 7), p->part.color);
		p->part.color[3] = 0.7;
		
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.0 / (5.0 + Randomf() * 0.5));
		
		p->part.scale = 0.1;
		p->scale_vel = 3.0;
		
		p->part.roll = Randomc() * 20.0;
		
		VectorCopy(pos, p->part.org);

		VectorScale(forward, 5.0, p->vel);
		
		for (int32_t i = 0; i < 3; i++) {
			p->vel[i] += 2.0 * Randomc();
		}

		ent->breath_puff_time = cgi.view->time + 3000;
	}
}

/**
 * @brief Adds the numerous render entities which comprise a given client (player)
 * entity: head, torso, legs, weapon, flags, etc.
 */
static void Cg_AddClientEntity(cl_entity_t *ent, r_entity_t *e) {
	const entity_state_t *s = &ent->current;

	const cl_client_info_t *ci = &cgi.client->client_info[s->client];
	if (!ci->head || !ci->torso || !ci->legs) {
		cgi.Debug("Invalid client info: %d\n", s->client);
		return;
	}

	e->effects |= EF_CLIENT;

	// don't draw ourselves unless third person is set
	if (Cg_IsSelf(ent) && !cg_third_person->value) {

		e->effects |= EF_NO_DRAW;

		// keep our shadow underneath us using the predicted origin
		e->origin[0] = cgi.view->origin[0];
		e->origin[1] = cgi.view->origin[1];
	} else {
		// add breathing puffs in rain or snow, and bubbles while underwater
		Cg_AddBreathPuffs(ent);
	}

	r_entity_t head, torso, legs;

	// copy the specified entity to all body segments
	head = torso = legs = *e;

	head.model = ci->head;
	memcpy(head.skins, ci->head_skins, sizeof(head.skins));

	torso.model = ci->torso;
	memcpy(torso.skins, ci->torso_skins, sizeof(torso.skins));

	legs.model = ci->legs;
	memcpy(legs.skins, ci->legs_skins, sizeof(legs.skins));

	Cg_AnimateClientEntity(ent, &torso, &legs);

	torso.parent = cgi.AddEntity(&legs);
	torso.tag_name = "tag_torso";

	head.parent = cgi.AddEntity(&torso);
	head.tag_name = "tag_head";

	cgi.AddEntity(&head);

	if (s->model2) {
		r_model_t *model = cgi.client->model_precache[s->model2];
		cgi.AddLinkedEntity(head.parent, model, "tag_weapon");
	}

	if (s->model3) {
		r_model_t *model = cgi.client->model_precache[s->model3];
		cgi.AddLinkedEntity(head.parent, model, "tag_head");
	}

	if (s->model4)
		cgi.Warn("Unsupported model_index4\n");
}

/**
 * @brief Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponKick(cl_entity_t *ent, vec3_t offset, vec3_t angles) {

	const vec3_t drop_raise_offset = { -4.0, -4.0, -4.0 };
	const vec3_t drop_raise_angles = { 25.0, -35.0, 2.0 };

	const vec3_t kick_offset = { -6.0, 0.0, 0.0 };
	const vec3_t kick_angles = { -2.0, 0.0, 0.0 };

	VectorSet(offset, cg_draw_weapon_x->value, cg_draw_weapon_y->value, cg_draw_weapon_z->value);
	VectorClear(angles);

	if (ent->animation1.animation == ANIM_TORSO_DROP) {
		VectorMA(offset, ent->animation1.fraction, drop_raise_offset, offset);
		VectorScale(drop_raise_angles, ent->animation1.fraction, angles);
	} else if (ent->animation1.animation == ANIM_TORSO_RAISE) {
		VectorMA(offset, 1.0 - ent->animation1.fraction, drop_raise_offset, offset);
		VectorScale(drop_raise_angles, 1.0 - ent->animation1.fraction, angles);
	} else if (ent->animation1.animation == ANIM_TORSO_ATTACK1) {
		VectorMA(offset, 1.0 - ent->animation1.fraction, kick_offset, offset);
		VectorScale(kick_angles, 1.0 - ent->animation1.fraction, angles);
	}

	VectorScale(offset, cg_bob->value, offset);
	VectorScale(angles, cg_bob->value, angles);
}

/**
 * @brief Adds the first-person weapon model to the view.
 */
static void Cg_AddWeapon(cl_entity_t *ent, r_entity_t *self) {
	static r_entity_t w;
	static r_lighting_t lighting;
	vec3_t offset, angles;

	const player_state_t *ps = &cgi.client->frame.ps;

	if (!cg_draw_weapon->value)
		return;

	if (cg_third_person->value)
		return;

	if (ps->stats[STAT_HEALTH] <= 0)
		return; // dead

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return; // spectating

	if (!ps->stats[STAT_WEAPON])
		return; // no weapon, e.g. level intermission

	memset(&w, 0, sizeof(w));

	Cg_WeaponKick(ent, offset, angles);

	VectorCopy(cgi.view->origin, w.origin);

	switch (cg_hand->integer) {
		case HAND_LEFT:
			offset[1] -= 6.0;
			break;
		case HAND_RIGHT:
			offset[1] += 6.0;
			break;
		default:
			break;
	}

	VectorMA(w.origin, offset[2], cgi.view->up, w.origin);
	VectorMA(w.origin, offset[1], cgi.view->right, w.origin);
	VectorMA(w.origin, offset[0], cgi.view->forward, w.origin);

	VectorAdd(cgi.view->angles, angles, w.angles);

	w.effects = EF_WEAPON | EF_NO_SHADOW;

	VectorSet(w.color, 1.0, 1.0, 1.0);

	if (cg_draw_weapon_alpha->value < 1.0) {
		w.effects |= EF_BLEND;
		w.color[3] = cg_draw_weapon_alpha->value;
	}

	w.effects |= self->effects & EF_SHELL;
	VectorCopy(self->shell, w.shell);

	w.model = cgi.client->model_precache[ps->stats[STAT_WEAPON]];

	w.lerp = w.scale = 1.0;

	memset(&lighting, 0, sizeof(lighting));
	w.lighting = &lighting;

	cgi.AddEntity(&w);
}

/**
 * @brief Adds the specified client entity to the view.
 */
static void Cg_AddEntity(cl_entity_t *ent) {
	r_entity_t e;

	memset(&e, 0, sizeof(e));
	e.lighting = &ent->lighting;
	e.scale = 1.0;

	// set the origin and angles so that we know where to add effects
	VectorCopy(ent->origin, e.origin);
	VectorCopy(ent->angles, e.angles);

	// set the bounding box, according to the server, for debugging
	if (ent->current.solid != SOLID_BSP) {
		if (!Cg_IsSelf(ent) || cg_third_person->value) {
			UnpackBounds(ent->current.bounds, e.mins, e.maxs);
		}
	}

	// add events
	Cg_EntityEvent(ent);

	// add effects, augmenting the renderer entity
	Cg_EntityEffects(ent, &e);

	// and particle and light trails
	Cg_EntityTrail(ent, &e);

	// if there's no model associated with the entity, we're done
	if (!ent->current.model1)
		return;

	if (ent->current.model1 == MODEL_CLIENT) { // add a player entity

		Cg_AddClientEntity(ent, &e);

		if (Cg_IsSelf(ent))
			Cg_AddWeapon(ent, &e);

		return;
	}

	// don't draw our own giblet
	if (Cg_IsSelf(ent) && !cg_third_person->value)
		e.effects |= EF_NO_DRAW;

	// assign the model
	e.model = cgi.client->model_precache[ent->current.model1];

	// and any frame animations (button state, etc)
	e.frame = ent->current.animation1;

	// add to view list
	cgi.AddEntity(&e);
}

/**
 * @brief Iterate all entities in the current frame, adding models, particles,
 * lights, and anything else associated with them.
 *
 * The correlation of client entities to renderer entities is not 1:1; some
 * client entities have no visible model, and others (e.g. players) require
 * several.
 */
void Cg_AddEntities(const cl_frame_t *frame) {
	uint16_t i;

	if (!cg_add_entities->value)
		return;

	// resolve any models, animations, interpolations, rotations, bobbing, etc..
	for (i = 0; i < frame->num_entities; i++) {

		const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *ent = &cgi.client->entities[s->number];

		Cg_AddEntity(ent);
	}
}
