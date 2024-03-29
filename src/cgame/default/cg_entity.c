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

GArray *cg_entities = NULL;

/**
 * @brief The `Cg_EntityPredicate` type for `Cg_FindEntity`.
 */
typedef bool (*Cg_EntityPredicate)(const cm_entity_t *e, void *data);

/**
 * @return The first entity after `from` for which `predicate` returns `true`, or `NULL`.
 */
static const cm_entity_t *Cg_FindEntity(const cm_entity_t *from, const Cg_EntityPredicate predicate, void *data) {

	const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;

	int32_t start = 0;
	if (from) {
		while (start < bsp->num_entities) {
			if (bsp->entities[start] == from) {
				break;
			}
			start++;
		}
		assert(start < bsp->num_entities);
	}

	for (int32_t i = start; i < bsp->num_entities; i++) {
		const cm_entity_t *e = bsp->entities[i];
		if (predicate(e, data)) {
			return e;
		}
	}

	return NULL;
}

/**
 * @brief
 */
static bool Cg_EntityTarget_Predicate(const cm_entity_t *e, void *data) {
	return !g_strcmp0(cgi.EntityValue(e, "targetname")->nullable_string, data);
}

/**
 * @brief
 */
static bool Cg_EntityTeam_Predicate(const cm_entity_t *e, void *data) {
	return !g_strcmp0(cgi.EntityValue(e, "team")->nullable_string, data);
}

/**
 * @return The `cg_entity_t *` for the specified `cm_entity_t *`, if any.
 */
cg_entity_t *Cg_EntityForDefinition(const cm_entity_t *e) {

	if (e) {
		for (guint i = 0; i < cg_entities->len; i++) {
			cg_entity_t *ent = &g_array_index(cg_entities, cg_entity_t, i);
			if (ent->def == e) {
				return ent;
			}
		}
	}

	return NULL;
}

/**
 * @brief Loads entities from the current level.
 * @remarks This should be called once per map load, after the precache routine.
 */
void Cg_LoadEntities(void) {
	 const cg_entity_class_t *classes[] = {
		&cg_misc_dust,
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
	for (int32_t i = 0; i < bsp->num_entities; i++) {

		const cm_entity_t *def = bsp->entities[i];
		const char *class_name = cgi.EntityValue(def, "classname")->string;

		const cg_entity_class_t **clazz = classes;
		for (size_t j = 0; j < lengthof(classes); j++, clazz++) {

			if (!g_strcmp0(class_name, (*clazz)->class_name)) {

				cg_entity_t e = {
					.id = MAX_ENTITIES + cg_entities->len,
					.clazz = *clazz,
					.def = def
				};

				e.origin = cgi.EntityValue(def, "origin")->vec3;
				e.bounds = Box3_FromCenter(e.origin);

				if (cgi.EntityValue(def, "target")->parsed & ENTITY_STRING) {
					const char *target_name = cgi.EntityValue(def, "target")->string;
					e.target = Cg_FindEntity(NULL, Cg_EntityTarget_Predicate, (void *) target_name);
					if (!e.target) {
						const char *class_name = cgi.EntityValue(def, "classname")->string;
						cgi.Warn("Target not found for %s @ %s\n", class_name, vtos(e.origin));
					}
				}

				if (cgi.EntityValue(def, "team")->parsed & ENTITY_STRING) {
					const char *team_name = cgi.EntityValue(def, "team")->string;
					e.team = Cg_FindEntity(def, Cg_EntityTeam_Predicate, (void *) team_name);
				}

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
bool Cg_IsSelf(const cl_entity_t *ent) {

	if (ent == cgi.client->entity) {
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
bool Cg_IsDucking(const cl_entity_t *ent) {

	const float standing_height = Box3_Size(PM_BOUNDS).z * PM_SCALE;
	const float height = Box3_Size(ent->current.bounds).z;

	return standing_height - height > PM_STOP_EPSILON;
}

/**
 * @brief Adds the entity's current sound, if any, to the sound stage.
 */
static void Cg_EntitySound(cl_entity_t *ent) {

	entity_state_t *s = &ent->current;

	if (s->sound) {
		Cg_AddSample(cgi.stage, (const s_play_sample_t *) &(s_play_sample_t) {
			.sample = cgi.client->sounds[s->sound],
			.origin = s->origin,
			.entity = s->number,
			.atten = SOUND_ATTEN_SQUARE,
			.flags = S_PLAY_LOOP | S_PLAY_FRAME
		});
	}

	s->sound = 0;
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

		Cg_EntitySound(ent);

		Cg_EntityEvent(ent);
	}
}

/**
 * @brief Adds the specified client entity to the view.
 */
static void Cg_AddEntity(cl_entity_t *ent) {

	// set the origin and angles so that we know where to add effects
	r_entity_t e = {
		.id = ent,
		.origin = ent->origin,
		.termination = ent->termination,
		.angles = ent->angles,
		.scale = 1.f,
		.bounds = ent->bounds,
		.abs_bounds = ent->abs_bounds
	};

	// add effects, augmenting the renderer entity
	Cg_EntityEffects(ent, &e);

	// if there's no model associated with the entity, we're done
	if (!ent->current.model1) {
		return;
	}

	if (ent->current.model1 == MODEL_CLIENT) {

		// add a client entity, with an animated player model
		Cg_AddClientEntity(ent, &e);

		// add our view weapon, if it's us
		if (Cg_IsSelf(ent)) {
			Cg_AddWeapon(ent, &e);
		}

		return;
	}

	// don't draw our own giblet, since the view is inside it
	if (Cg_IsSelf(ent) && !cgi.client->third_person) {
		e.effects |= EF_NO_DRAW;
	}

	// assign the model
	e.model = cgi.client->models[ent->current.model1];

	// and any frame animations (button state, etc)
	e.frame = ent->current.animation1;

	// add to view list
	cgi.AddEntity(cgi.view, &e);
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

	// and client side entities too
	cg_entity_t *e = (cg_entity_t *) cg_entities->data;
	for (guint i = 0; i < cg_entities->len; i++, e++) {

		if (e->next_think > cgi.client->unclamped_time) {
			continue;
		}

		e->clazz->Think(e);

		e->next_think += 1000.f / e->hz + 1000.f * e->drift * Randomf();
	}
}
