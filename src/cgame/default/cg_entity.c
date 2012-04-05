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

#include "cg_local.h"

/**
 * Cg_UpdateLighting
 *
 * Establishes the write-through lighting cache for the specified entity,
 * marking it as dirty if necessary.
 */
static void Cg_UpdateLighting(cl_entity_t *e, r_entity_t *ent) {

	// setup the write-through lighting cache
	ent->lighting = &e->lighting;

	if (e->current.effects & EF_NO_LIGHTING) {
		// some entities are never lit
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

/**
 * Cg_AddClientEntity
 *
 * Adds the numerous render entities which comprise a given client (player)
 * entity: head, upper, lower, weapon, flags, etc.
 */
static void Cg_AddClientEntity(cl_entity_t *e, r_entity_t *ent) {
	const entity_state_t *s = &e->current;
	const cl_client_info_t *ci = &cgi.client->client_info[s->client];
	r_entity_t head, upper, lower;

	if (!ci->head || !ci->upper || !ci->lower) {
		cgi.Debug("Cg_AddClientEntity: Invalid client info: %d\n", s->client);
		return;
	}

	int effects = s->effects;

	ent->origin[2] -= 6.0; // small hack for PM_SCALE

	// copy the specified entity to all body segments
	head = upper = lower = *ent;

	head.model = ci->head;
	memcpy(head.skins, ci->head_skins, sizeof(head.skins));

	upper.model = ci->upper;
	memcpy(upper.skins, ci->upper_skins, sizeof(upper.skins));

	lower.model = ci->lower;
	memcpy(lower.skins, ci->lower_skins, sizeof(lower.skins));

	Cg_AnimateClientEntity(e, &upper, &lower);

	// don't draw ourselves unless third person is set
	if (s->number == cgi.client->player_num + 1) {

		if (!cg_third_person->value) {
			effects |= EF_NO_DRAW;

			// keep our shadow underneath us using the predicted origin
			lower.origin[0] = cgi.view->origin[0];
			lower.origin[1] = cgi.view->origin[1];
		}
	}

	head.effects = upper.effects = lower.effects = effects;

	upper.parent = cgi.AddEntity(&lower);
	upper.tag_name = "tag_torso";

	head.parent = cgi.AddEntity(&upper);
	head.tag_name = "tag_head";

	cgi.AddEntity(&head);

	if (s->model2) {
		r_model_t *model = cgi.client->model_draw[s->model2];
		cgi.AddLinkedEntity(head.parent, model, "tag_weapon");
	}

	if (s->model3) {
		r_model_t *model = cgi.client->model_draw[s->model3];
		cgi.AddLinkedEntity(head.parent, model, "tag_head");
	}

	if (s->model4)
		cgi.Warn("Cg_AddClientEntity: Unsupported model_index4\n");
}

/**
 * Cg_WeaponKick
 *
 * Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponKick(cl_entity_t *e, vec3_t offset, vec3_t angles) {
	const vec3_t drop_raise_offset = { -4.0, -4.0, -4.0 };
	const vec3_t drop_raise_angles = { 25.0, -35.0, 2.0 };

	const vec3_t kick_offset = { -4.0, 0.0, 1.0 };
	const vec3_t kick_angles = { -8.0, 0.0, 0.0 };

	VectorSet(offset, cg_draw_weapon_x->value, cg_draw_weapon_y->value, cg_draw_weapon_z->value);
	VectorClear(angles);

	if (e->animation1.animation == ANIM_TORSO_DROP) {
		VectorMA(offset, e->animation1.fraction, drop_raise_offset, offset);
		VectorScale(drop_raise_angles, e->animation1.fraction, angles);
	} else if (e->animation1.animation == ANIM_TORSO_RAISE) {
		VectorMA(offset, 1.0 - e->animation1.fraction, drop_raise_offset, offset);
		VectorScale(drop_raise_angles, 1.0 - e->animation1.fraction, angles);
	} else if (e->animation1.animation == ANIM_TORSO_ATTACK1) {
		VectorMA(offset, 1.0 - e->animation1.fraction, kick_offset, offset);
		VectorScale(kick_angles, 1.0 - e->animation1.fraction, angles);
	}

	VectorScale(offset, cg_bob->value, offset);
	VectorScale(angles, cg_bob->value, angles);
}

/**
 * Cg_AddWeapon
 *
 * Adds the first-person weapon model to the view.
 */
static void Cg_AddWeapon(cl_entity_t *e, r_entity_t *self) {
	static r_entity_t ent;
	static r_lighting_t lighting;
	vec3_t offset, angles;

	const player_state_t *ps = &cgi.client->frame.ps;

	if (!cg_draw_weapon->value)
		return;

	if (cg_third_person->value)
		return;

	if (!ps->stats[STAT_HEALTH])
		return; // deadz0r

	if (ps->stats[STAT_SPECTATOR])
		return; // spectating

	if (ps->stats[STAT_CHASE])
		return; // chase camera

	if (!ps->stats[STAT_WEAPON])
		return; // no weapon, e.g. level intermission

	memset(&ent, 0, sizeof(ent));

	Cg_WeaponKick(e, offset, angles);

	VectorCopy(cgi.view->origin, ent.origin);

	VectorMA(ent.origin, offset[2], cgi.view->up, ent.origin);
	VectorMA(ent.origin, offset[1], cgi.view->right, ent.origin);
	VectorMA(ent.origin, offset[0], cgi.view->forward, ent.origin);

	VectorAdd(cgi.view->angles, angles, ent.angles);

	ent.effects = EF_WEAPON | EF_NO_SHADOW;

	VectorCopy(self->shell, ent.shell);

	ent.model = cgi.client->model_draw[ps->stats[STAT_WEAPON]];

	ent.lerp = ent.scale = 1.0;

	memset(&lighting, 0, sizeof(lighting));
	ent.lighting = &lighting;

	cgi.AddEntity(&ent);
}

/**
 * Cg_AddEntity
 *
 * Adds the specified client entity to the view.
 */
static void Cg_AddEntity(cl_entity_t *e) {
	r_entity_t ent;

	memset(&ent, 0, sizeof(ent));
	ent.scale = 1.0;

	// update the static lighting cache
	Cg_UpdateLighting(e, &ent);

	// resolve the origin so that we know where to add effects
	VectorLerp(e->prev.origin, e->current.origin, cgi.client->lerp, ent.origin);

	// add events
	Cg_EntityEvent(e);

	// add effects, augmenting the renderer entity
	Cg_EntityEffects(e, &ent);

	// if there's no model associated with the entity, we're done
	if (!e->current.model1)
		return;

	if (e->current.model1 == 0xff) { // add a player entity

		Cg_AddClientEntity(e, &ent);

		if (IS_SELF(e))
			Cg_AddWeapon(e, &ent);

		return;
	}

	// assign the model
	ent.model = cgi.client->model_draw[e->current.model1];

	// add to view list
	cgi.AddEntity(&ent);
}

/**
 * Cg_AddEntities
 *
 * Iterate all entities in the current frame, adding models, particles,
 * lights, and anything else associated with them.
 *
 * The correlation of client entities to renderer entities is not 1:1; some
 * client entities have no visible model, and others (e.g. players) require
 * several.
 */
void Cg_AddEntities(void) {
	int i;

	if (!cg_add_entities->value)
		return;

	const cl_frame_t *frame = &cgi.client->frame;

	// resolve any models, animations, interpolations, rotations, bobbing, etc..
	for (i = 0; i < frame->num_entities; i++) {

		const int snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *cent = &cgi.client->entities[s->number];

		Cg_AddEntity(cent);
	}
}
