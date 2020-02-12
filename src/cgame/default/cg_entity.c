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
 * @return The entity bound to the client's view.
 */
cl_entity_t *Cg_Self(void) {

	uint16_t index = cgi.client->client_num;

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
		step->height = step->height * (1.0 - lerp) + height;
	} else {
		step->height = height;
		step->timestamp = time;
	}

	step->interval = 128.0 * (fabs(step->height) / PM_STEP_HEIGHT);
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
 * @brief
 */
static void Cg_AnimateEntity(cl_entity_t *ent) {

	if (ent->current.model1 == MODEL_CLIENT) {
		Cg_AnimateClientEntity(ent, NULL, NULL);
	}
}

/**
 * @brief Interpolate the current frame, processing any new events and advancing the simulation.
 */
void Cg_Interpolate(const cl_frame_t *frame) {

	cgi.client->entity = Cg_Self();

	for (uint16_t i = 0; i < frame->num_entities; i++) {

		const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *ent = &cgi.client->entities[s->number];

		Cg_EntityEvent(ent);

		Cg_InterpolateStep(&ent->step);

		if (ent->step.delta_height) {
			ent->origin.z = ent->current.origin.z - ent->step.delta_height;
		}

		Cg_AnimateEntity(ent);
	}
}

/**
 * @brief Applies transformation and rotation for the specified linked entity. The matrix
 * component of the parent and child must be set up already. The child's matrix will be modified
 * by this function and is used instead of origin/angles/scale.
 */
void Cg_ApplyMeshTag(r_entity_t *child, const r_entity_t *parent, const char *tag_name) {

	if (!parent || !parent->model || parent->model->type != MOD_MESH) {
		cgi.Warn("Invalid parent entity\n");
		return;
	}

	if (!tag_name || !*tag_name) {
		cgi.Warn("NULL tag_name\n");
		return;
	}

	// interpolate the tag over the frames of the parent entity

	const r_mesh_tag_t *t1 = cgi.MeshTag(parent->model, tag_name, parent->old_frame);
	const r_mesh_tag_t *t2 = cgi.MeshTag(parent->model, tag_name, parent->frame);

	if (!t1 || !t2) {
		return;
	}

	mat4_t local, world;

	Matrix4x4_Interpolate(&local, &t2->matrix, &t1->matrix, parent->back_lerp);
	Matrix4x4_Normalize(&local, &local);

	// add local origins to the local offset
	Matrix4x4_Concat(&local, &local, &child->matrix);

	// move by parent matrix
	Matrix4x4_Concat(&world, &parent->matrix, &local);

	child->effects |= EF_LINKED;

	// calculate final origin/angles
	vec3_t forward;

	Matrix4x4_ToVectors(&world, forward.xyz, NULL, NULL, child->origin.xyz);

	child->angles = vec3_euler(forward);

	child->scale = Matrix4x4_ScaleFromMatrix(&world);

	// copy matrix over to child
	Matrix4x4_Copy(&child->matrix, &world);
}

/**
 * @brief Binds a linked model to its parent, and copies it into the view structure.
 */
r_entity_t *Cg_AddLinkedEntity(const r_entity_t *parent, const r_model_t *model,
                                    const char *tag_name) {

	if (!parent) {
		cgi.Warn("NULL parent\n");
		return NULL;
	}

	r_entity_t ent = *parent;

	ent.origin = vec3_zero();
	ent.angles = vec3_zero();

	ent.model = model;

	memset(ent.skins, 0, sizeof(ent.skins));
	ent.num_skins = 0;

	ent.frame = ent.old_frame = 0;

	ent.lerp = 1.0;
	ent.back_lerp = 0.0;

	r_entity_t *r_ent = cgi.AddEntity(&ent);

	Matrix4x4_CreateFromEntity(&r_ent->matrix, r_ent->origin, r_ent->angles, r_ent->scale);

	Cg_ApplyMeshTag(r_ent, parent, tag_name);

	return r_ent;
}

/**
 * @brief The min velocity we should apply leg rotation on.
 */
#define CLIENT_LEGS_SPEED_EPSILON		0.5

/**
 * @brief The max yaw that we'll rotate the legs by when moving left/right.
 */
#define CLIENT_LEGS_YAW_MAX				65.0

/**
 * @brief The speed (0 to 1) that the legs will catch up to the current leg yaw.
 */
#define CLIENT_LEGS_YAW_LERP_SPEED		0.1

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

	const uint32_t effects = e->effects;

	e->effects |= EF_CLIENT;
	e->effects &= ~EF_CTF_MASK;

	const _Bool self_no_draw = (Cg_IsSelf(ent) && !cgi.client->third_person);

	// don't draw ourselves unless third person is set
	if (self_no_draw) {

		e->effects |= EF_NO_DRAW;

		// keep our shadow underneath us using the predicted origin
		e->origin.x = cgi.view->origin.x;
		e->origin.y = cgi.view->origin.y;
	} else {
		Cg_BreathTrail(ent);
	}

	// set tints
	if (ci->shirt.a) {
		e->tints[0] = color_to_vec4(ci->shirt);
	}

	if (ci->pants.a) {
		e->tints[1] = color_to_vec4(ci->pants);
	}

	if (ci->helmet.a) {
		e->tints[2] = color_to_vec4(ci->helmet);
	}

	r_entity_t head, torso, legs;

	// copy the specified entity to all body segments
	head = torso = legs = *e;

	float leg_yaw_offset = 0.0;

	if ((ent->current.effects & EF_CORPSE) == 0) {
		vec3_t right;
		vec3_vectors(legs.angles, NULL, &right, NULL);

		vec3_t move_dir;
		move_dir = vec3_subtract(ent->prev.origin, ent->current.origin);
		move_dir.z = 0.0; // don't care about z, just x/y

		if (ent->animation2.animation < ANIM_LEGS_SWIM) {
			if (vec3_length(move_dir) > CLIENT_LEGS_SPEED_EPSILON) {
				move_dir = vec3_normalize(move_dir);
				leg_yaw_offset = vec3_dot(move_dir, right) * CLIENT_LEGS_YAW_MAX;

				if (ent->animation2.animation == ANIM_LEGS_BACK ||
					ent->animation2.reverse) {
					leg_yaw_offset = -leg_yaw_offset;
				}
			}
		}

		ent->legs_yaw = 0;//AngleLerp(ent->legs_yaw, leg_yaw_offset, CLIENT_LEGS_YAW_LERP_SPEED);
	} else {
		ent->legs_yaw = 0.0;
	}

	legs.model = ci->legs;
	legs.angles.y += ent->legs_yaw;
	legs.angles.x = legs.angles.z = 0.0; // legs only use yaw
	memcpy(legs.skins, ci->legs_skins, sizeof(legs.skins));

	torso.model = ci->torso;
	torso.origin = vec3_zero();
	torso.angles.y = -ent->legs_yaw; // legs twisted already, we just need to pitch/roll
	memcpy(torso.skins, ci->torso_skins, sizeof(torso.skins));

	head.model = ci->head;
	head.origin = vec3_zero();
	head.angles.y = 0.0; // legs twisted already, we just need to pitch/roll
	// this is applied twice so head pivots on chest as well
	memcpy(head.skins, ci->head_skins, sizeof(head.skins));

	Cg_AnimateClientEntity(ent, &torso, &legs);

	r_entity_t *r_legs = cgi.AddEntity(&legs);
	r_entity_t *r_torso = cgi.AddEntity(&torso);
	r_entity_t *r_head = cgi.AddEntity(&head);

	Matrix4x4_CreateFromEntity(&r_legs->matrix, r_legs->origin, r_legs->angles, r_legs->scale);
	Matrix4x4_CreateFromEntity(&r_torso->matrix, r_torso->origin, r_torso->angles, r_torso->scale);
	Matrix4x4_CreateFromEntity(&r_head->matrix, r_head->origin, r_head->angles, r_head->scale);

	Cg_ApplyMeshTag(r_torso, r_legs, "tag_torso");
	Cg_ApplyMeshTag(r_head, r_torso, "tag_head");

	if (s->model2) {
		r_model_t *model = cgi.client->model_precache[s->model2];
		r_entity_t *weapon = Cg_AddLinkedEntity(r_torso, model, "tag_weapon");

		if (self_no_draw) {
			weapon->effects |= EF_NO_DRAW;
		}
	}

	if (s->model3) {
		r_model_t *model = cgi.client->model_precache[s->model3];
		r_entity_t *carry = Cg_AddLinkedEntity(r_torso, model, "tag_head");
		carry->effects |= effects;

		if (self_no_draw) {
			carry->effects |= EF_NO_DRAW;
		}
	}

	if (s->model4) {
		cgi.Warn("Unsupported model_index4\n");
	}
}

/**
 * @brief Adds weapon bob due to running, walking, crouching, etc.
 */
static void Cg_WeaponBob(const player_state_t *ps, vec3_t *offset, vec3_t *angles) {
	const vec3_t bob = { 0.2, 0.4, 0.2 };

	*offset = vec3_add(*offset, vec3_scale(bob, cg_view.bob));

	*angles = vec3_add(*angles, vec3(0.f, 1.5 * cg_view.bob, 0.f));
}

/**
 * @brief Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponOffset(cl_entity_t *ent, vec3_t *offset, vec3_t *angles) {

	const vec3_t drop_raise_offset = { -4.0, -4.0, -4.0 };
	const vec3_t drop_raise_angles = { 25.0, -35.0, 2.0 };

	const vec3_t kick_offset = { -6.0, 0.0, 0.0 };
	const vec3_t kick_angles = { -2.0, 0.0, 0.0 };

	*offset = vec3_zero();
	*angles = vec3_zero();

	if (ent->animation1.animation == ANIM_TORSO_DROP) {
		*offset = vec3_add(*offset, vec3_scale(drop_raise_offset, ent->animation1.fraction));
		*angles = vec3_scale(drop_raise_angles, ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_RAISE) {
		*offset = vec3_add(*offset, vec3_scale(drop_raise_offset, 1.0 - ent->animation1.fraction));
		*angles = vec3_scale(drop_raise_angles, 1.0 - ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_ATTACK1) {
		*offset = vec3_add(*offset, vec3_scale(kick_offset, 1.0 - ent->animation1.fraction));
		*angles = vec3_scale(kick_angles, 1.0 - ent->animation1.fraction);
	}

	*offset = vec3_scale(*offset, cg_bob->value);
	*angles = vec3_scale(*angles, cg_bob->value);

	*offset = vec3_add(*offset, vec3(cg_draw_weapon_x->value, cg_draw_weapon_y->value, cg_draw_weapon_z->value));
}

/**
 * @brief Periodically calculates the player's velocity, and interpolates it
 * over a small interval to smooth out rapid changes.
 */
static void Cg_SpeedModulus(const player_state_t *ps, vec3_t *offset) {
	static vec3_t old_speed, new_speed;
	static uint32_t time;

	if (cgi.client->unclamped_time < time) {
		time = 0;

		old_speed = vec3_zero();
		new_speed = vec3_zero();
	}

	vec3_t speed;

	const uint32_t delta = cgi.client->unclamped_time - time;
	if (delta < 100) {
		const float lerp = delta / 100.0;

		speed.x = old_speed.x + lerp * (new_speed.x - old_speed.x);
		speed.y = old_speed.y + lerp * (new_speed.y - old_speed.y);
		speed.z = old_speed.z + lerp * (new_speed.z - old_speed.z);
	} else {
		old_speed = new_speed;

		new_speed.x = -clampf(ps->pm_state.velocity.x / 200.0, -1.0, 1.0);
		new_speed.y = -clampf(ps->pm_state.velocity.y / 200.0, -1.0, 1.0);
		new_speed.z = -clampf(ps->pm_state.velocity.z / 200.0, -0.3, 1.0);

		speed = old_speed;

		time = cgi.client->unclamped_time;
	}

	if (cg_draw_weapon_bob->modified) {
		cg_draw_weapon_bob->value = clampf(cg_draw_weapon_bob->value, 0.0, 2.0);
		cg_draw_weapon_bob->modified = false;
	}

	*offset = vec3_scale(speed, cg_draw_weapon_bob->value);
}

/**
 * @brief Adds the first-person weapon model to the view.
 */
static void Cg_AddWeapon(cl_entity_t *ent, r_entity_t *self) {
	static r_entity_t w;
	vec3_t offset, angles;
	vec3_t velocity;

	const player_state_t *ps = &cgi.client->frame.ps;

	if (!cg_draw_weapon->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->stats[STAT_HEALTH] <= 0) {
		return; // dead
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return; // spectating
	}

	if (!ps->stats[STAT_WEAPON]) {
		return; // no weapon, e.g. level intermission
	}

	memset(&w, 0, sizeof(w));

	// Weapon offset

	Cg_WeaponOffset(ent, &offset, &angles);

	w.origin = cgi.view->origin;

	// Weapon bob

	Cg_WeaponBob(ps, &offset, &angles);

	// Velocity swaying

	Cg_SpeedModulus(ps, &velocity);

	w.origin = vec3_add(w.origin, velocity);

	// Hand

	switch (cg_hand->integer) {
		case HAND_LEFT:
			offset.y -= 5.0;
			break;
		case HAND_RIGHT:
			offset.y += 5.0;
			break;
		default:
			break;
	}

	w.origin = vec3_add(w.origin, vec3_scale(cgi.view->up, offset.z));
	w.origin = vec3_add(w.origin, vec3_scale(cgi.view->right, offset.y));
	w.origin = vec3_add(w.origin, vec3_scale(cgi.view->forward, offset.x));

	w.angles = vec3_add(cgi.view->angles, angles);

	// Copy state over to render entity

	w.effects = EF_WEAPON | EF_NO_SHADOW;

	w.color = vec4(1.0, 1.0, 1.0, 1.0);

	if (cg_draw_weapon_alpha->value < 1.0) {
		w.effects |= EF_BLEND;
		w.color.w = cg_draw_weapon_alpha->value;
	}

	w.effects |= self->effects & EF_SHELL;
	w.shell = self->shell;

	w.model = cgi.client->model_precache[ps->stats[STAT_WEAPON]];

	w.lerp = w.scale = 1.0;

	cgi.AddEntity(&w);
}

/**
 * @brief Adds the specified client entity to the view.
 */
static void Cg_AddEntity(cl_entity_t *ent) {
	r_entity_t e;

	memset(&e, 0, sizeof(e));
	e.scale = 1.0;

	// set the origin and angles so that we know where to add effects
	e.origin = ent->origin;
	e.angles = ent->angles;

	// set the bounding box, according to the server, for debugging
	if (ent->current.solid != SOLID_BSP) {
		if (!Cg_IsSelf(ent) || cgi.client->third_person) {
			e.mins = ent->current.mins;
			e.maxs = ent->current.maxs;
		}
	}

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

	if (!cg_add_entities->value) {
		return;
	}

	// resolve any models, animations, interpolations, rotations, bobbing, etc..
	for (uint16_t i = 0; i < frame->num_entities; i++) {

		const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cgi.client->entity_states[snum];

		cl_entity_t *ent = &cgi.client->entities[s->number];

		Cg_EntityTrail(ent);

		Cg_AddEntity(ent);
	}
}
