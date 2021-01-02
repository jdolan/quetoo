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

static GArray *cg_entities;

/**
 * @brief Loads entities from the current level.
 * @remarks This should be called once per map load, after the precache routine.
 */
void Cg_LoadEntities(void) {
	 const cg_entity_class_t *classes[] = {
		&cg_misc_flame,
		&cg_misc_light,
		&cg_misc_model,
		&cg_misc_sound,
		&cg_misc_sparks,
		&cg_misc_sprite,
		&cg_misc_steam
	};

	Cg_FreeEntities();

	cg_entities = g_array_new(false, false, sizeof(cg_entity_t));

	const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
	for (size_t i = 0; i < bsp->num_entities; i++) {

		const cm_entity_t *def = bsp->entities[i];
		const char *class_name = cgi.EntityValue(def, "classname")->string;

		const cg_entity_class_t **clazz = classes;
		for (size_t j = 0; j < lengthof(classes); j++, clazz++) {
			if (!g_strcmp0(class_name, (*clazz)->class_name)) {

				cg_entity_t e = {
					.clazz = *clazz,
					.def = def
				};

				e.origin = cgi.EntityValue(def, "origin")->vec3;

				e.data = cgi.Malloc(e.clazz->data_size, MEM_TAG_CGAME_LEVEL);

				e.clazz->Init(&e);

				g_array_append_val(cg_entities, e);
			}
		}
	}
}

/**
 * @brief
 */
void Cg_FreeEntities(void) {

	if (cg_entities) {
		g_array_free(cg_entities, true);
		cg_entities = NULL;
	}
}

/**
 * @return The entity bound to the client's view.
 */
cl_entity_t *Cg_Self(void) {

	int32_t index = cgi.client->client_num;

	if (cgi.client->frame.ps.stats[STAT_CHASE]) {
		index = cgi.client->frame.ps.stats[STAT_CHASE] - CS_CLIENTS;
	}

	return &cgi.client->entities[index + 1];
}

/**
 * @return True if the specified entity is bound to the local client's view.
 */
_Bool Cg_IsSelf(const cl_entity_t *ent) {

	if (ent->current.number == cgi.client->client_num + 1) {
		return true;
	}

	if ((ent->current.effects & EF_CORPSE) == 0) {

		if (ent->current.model1 == MODEL_CLIENT) {

			if (ent->current.client == cgi.client->client_num) {
				return true;
			}

			const int16_t chase = cgi.client->frame.ps.stats[STAT_CHASE] - CS_CLIENTS;

			if (ent->current.client == chase) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @return True if the entity is ducking, false otherwise.
 */
_Bool Cg_IsDucking(const cl_entity_t *ent) {

	const float standing_height = (PM_MAXS.z - PM_MINS.z) * PM_SCALE;
	const float height = ent->current.maxs.z - ent->current.mins.z;

	return standing_height - height > PM_STOP_EPSILON;
}

/**
 * @brief Setup step interpolation.
 */
void Cg_TraverseStep(cl_entity_step_t *step, uint32_t time, float height) {

	const uint32_t delta = time - step->timestamp;

	if (delta < step->interval) {
		const float lerp = (step->interval - delta) / (float) step->interval;
		step->height = step->height * (1.f - lerp) + height;
	} else {
		step->height = height;
		step->timestamp = time;
	}

	step->interval = 128.f * (fabsf(step->height) / PM_STEP_HEIGHT);
}

/**
 * @brief Interpolate the entity's step for the current frame.
 */
void Cg_InterpolateStep(cl_entity_step_t *step) {

	const uint32_t delta = cgi.client->unclamped_time - step->timestamp;

	if (delta < step->interval) {
		const float lerp = (step->interval - delta) / (float) step->interval;
		step->delta_height = lerp * step->height;
	} else {
		step->delta_height = 0.0;
	}
}

/**
 * @brief Interpolate the current frame, processing any new events and advancing the simulation.
 */
void Cg_Interpolate(const cl_frame_t *frame) {

	cgi.client->entity = Cg_Self();

	for (int32_t i = 0; i < frame->num_entities; i++) {

		const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *ent = &cgi.client->entities[s->number];

		Cg_EntityEvent(ent);

		Cg_InterpolateStep(&ent->step);

		if (ent->step.delta_height) {
			ent->origin.z = ent->current.origin.z - ent->step.delta_height;
		}
	}
}

/**
 * @brief Adds the specified client entity to the view.
 */
static void Cg_AddEntity(cl_entity_t *ent) {

	// set the origin and angles so that we know where to add effects
	r_entity_t e = {
		.origin = ent->origin,
		.termination = ent->termination,
		.angles = ent->angles,
		.scale = 1.f,
		.mins = ent->mins,
		.maxs = ent->maxs,
		.abs_mins = ent->abs_mins,
		.abs_maxs = ent->abs_maxs,
	};

	// add effects, augmenting the renderer entity
	Cg_EntityEffects(ent, &e);

	// if there's no model associated with the entity, we're done
	if (!ent->current.model1) {
		return;
	}

	if (ent->current.model1 == MODEL_CLIENT) { // add a player entity

		Cg_AddClientEntity(ent, &e);

		if (Cg_IsSelf(ent)) {
			Cg_AddWeapon(ent, &e);
		}

		return;
	}

	// don't draw our own giblet
	if (Cg_IsSelf(ent) && !cgi.client->third_person) {
		e.effects |= EF_NO_DRAW;
	}

	// assign the model
	e.model = cgi.client->models[ent->current.model1];

	// and any frame animations (button state, etc)
	e.frame = ent->current.animation1;

	// add to view list
	cgi.AddEntity(&e);
}

/**
 * @brief Iterate all entities in the current frame, adding models, sprites,
 * lights, and anything else associated with them.
 *
 * The correlation of client entities to renderer entities is not 1:1; some
 * client entities have no visible model, and others (e.g. players) require
 * several.
 */
void Cg_AddEntities(const cl_frame_t *frame) {

	if (!cg_add_entities->value) {
		return;
	}

	// add server side entities
	for (int32_t i = 0; i < frame->num_entities; i++) {

		const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *ent = &cgi.client->entities[s->number];

		Cg_EntityTrail(ent);

		Cg_AddEntity(ent);
	}

	// and client-side entities too
	cg_entity_t *e = (cg_entity_t *) cg_entities->data;
	for (guint i = 0; i < cg_entities->len; i++, e++) {

		if (e->next_think > cgi.client->unclamped_time) {
			continue;
		}

		e->clazz->Think(e);
	}
}
