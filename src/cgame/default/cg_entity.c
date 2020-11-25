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
 * @brief The min velocity we should apply leg rotation on.
 */
#define CLIENT_LEGS_SPEED_EPSILON		.5f

/**
 * @brief The max yaw that we'll rotate the legs by when moving left/right.
 */
#define CLIENT_LEGS_YAW_MAX				65.f

/**
 * @brief Clamp angle
 */
#define CLIENT_LEGS_CLAMP				(CLIENT_LEGS_YAW_MAX * 1.5f)

/**
 * @brief The speed that the legs will catch up to the current leg yaw.
 */
#define CLIENT_LEGS_YAW_LERP_SPEED		240.f

static inline float AngleMod(float a) {
	a = fmodf(a, 360.f);// (360.0 / 65536) * ((int32_t) (a * (65536 / 360.0)) & 65535);

	if (a < 0) {
		return a + (((int32_t)(a / 360.f) + 1) * 360.f);
	}

	return a;
}

static inline float SmallestAngleBetween(const float x, const float y) {
	return min(360.f - fabsf(x - y), fabsf(x - y));
}

static inline float Cg_CalculateAngle(const float speed, float current, float ideal) {
	current = AngleMod(current);
	ideal = AngleMod(ideal);

	if (current == ideal) {
		return current;
	}

	float move = ideal - current;

	if (ideal > current) {
		if (move >= 180.0) {
			move = move - 360.0;
		}
	} else {
		if (move <= -180.0) {
			move = move + 360.0;
		}
	}

	if (move > 0) {
		if (move > speed) {
			move = speed;
		}
	} else {
		if (move < -speed) {
			move = -speed;
		}
	}

	return AngleMod(current + move);
}

/**
 * @brief Adds the numerous render entities which comprise a given client (player)
 * entity: head, torso, legs, weapon, flags, etc.
 */
static void Cg_AddClientEntity(cl_entity_t *ent, r_entity_t *e) {
	const entity_state_t *s = &ent->current;

	const cl_client_info_t *ci = &cgi.client->client_info[s->client];
	if (!ci->head || !ci->torso || !ci->legs) {
		Cg_Debug("Invalid client info: %d\n", s->client);
		return;
	}

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
		e->tints[0] = Color_Vec4(ci->shirt);
	}

	if (ci->pants.a) {
		e->tints[1] = Color_Vec4(ci->pants);
	}

	if (ci->helmet.a) {
		e->tints[2] = Color_Vec4(ci->helmet);
	}

	r_entity_t head, torso, legs;

	// copy the specified entity to all body segments
	head = torso = legs = *e;

	if ((ent->current.effects & EF_CORPSE) == 0) {
		vec3_t right;
		Vec3_Vectors(legs.angles, NULL, &right, NULL);

		vec3_t move_dir;
		move_dir = Vec3_Subtract(ent->prev.origin, ent->current.origin);
		move_dir.z = 0.f; // don't care about z, just x/y

		if (ent->animation2.animation < ANIM_LEGS_SWIM) {
			if (Vec3_Length(move_dir) > CLIENT_LEGS_SPEED_EPSILON) {
				move_dir = Vec3_Normalize(move_dir);
				float legs_yaw = Vec3_Dot(move_dir, right) * CLIENT_LEGS_YAW_MAX;

				if (ent->animation2.animation == ANIM_LEGS_BACK ||
					ent->animation2.reverse) {
					legs_yaw = -legs_yaw;
				}

				ent->legs_yaw = ent->angles.y + legs_yaw;
			} else {
				
				ent->legs_yaw = ent->angles.y;
			}
		} else {

			const float angle_diff = SmallestAngleBetween(ent->legs_yaw, ent->angles.y);

			if (ent->animation2.animation == ANIM_LEGS_TURN ||
				fabsf(angle_diff) > CLIENT_LEGS_YAW_MAX) {

				ent->legs_yaw = ent->angles.y;

				// change animation as well
				if (ent->animation2.animation == ANIM_LEGS_IDLE) {
					ent->animation2.time = cgi.client->unclamped_time;
					ent->animation2.frame = ent->animation2.old_frame = -1;
					ent->animation2.lerp = ent->animation2.fraction = 0;
				}
			}
		}

		ent->legs_current_yaw = Cg_CalculateAngle(CLIENT_LEGS_YAW_LERP_SPEED * MILLIS_TO_SECONDS(cgi.client->frame_msec), ent->legs_current_yaw, ent->legs_yaw);

		const float angle_delta = AngleMod(ent->legs_current_yaw - ent->legs_yaw + 180.0f) - 180.0f;

		ent->legs_current_yaw = AngleMod(ent->legs_yaw + clamp(angle_delta, -CLIENT_LEGS_CLAMP, CLIENT_LEGS_CLAMP));

		if (fabsf(SmallestAngleBetween(ent->legs_yaw, ent->legs_current_yaw)) > 1) {
			if (ent->animation2.animation == ANIM_LEGS_IDLE) {
				ent->animation2.time = cgi.client->unclamped_time;
				ent->animation2.animation = ANIM_LEGS_TURN;
			}
		} else {
			if (ent->animation2.animation == ANIM_LEGS_TURN) {
				ent->animation2.time = cgi.client->unclamped_time;
				ent->animation2.animation = ANIM_LEGS_IDLE;
			}
		}
	}

	legs.model = ci->legs;
	legs.angles.y = ent->legs_current_yaw;
	legs.angles.x = legs.angles.z = 0.0; // legs only use yaw
	memcpy(legs.skins, ci->legs_skins, sizeof(legs.skins));

	torso.model = ci->torso;
	torso.origin = Vec3_Zero();
	torso.angles.y = ent->angles.y - legs.angles.y; // legs twisted already, we just need to pitch/roll
	memcpy(torso.skins, ci->torso_skins, sizeof(torso.skins));

	head.model = ci->head;
	head.origin = Vec3_Zero();
	head.angles.y = 0.0;
	memcpy(head.skins, ci->head_skins, sizeof(head.skins));

	Cg_AnimateClientEntity(ent, &torso, &legs);

	r_entity_t *r_legs = cgi.AddEntity(&legs);

	torso.parent = r_legs;
	torso.tag = "tag_torso";

	r_entity_t *r_torso = cgi.AddEntity(&torso);

	head.parent = r_torso;
	head.tag = "tag_head";

	cgi.AddEntity(&head);

	if (s->model2) {
		cgi.AddEntity(&(const r_entity_t) {
			.parent = r_torso,
			.tag = "tag_weapon",
			.scale = e->scale,
			.model = cgi.client->model_precache[s->model2],
			.effects = e->effects,
			.color = e->color,
			.shell = e->shell,
		});
	}

	if (s->model3) {
		cgi.AddEntity(&(const r_entity_t) {
			.parent = r_torso,
			.tag = "tag_head",
			.scale = e->scale,
			.model = cgi.client->model_precache[s->model3],
			.effects = e->effects,
			.color = e->color,
			.shell = e->shell,
		});
	}

	if (s->model4) {
		cgi.Warn("Unsupported model_index4\n");
	}
}

/**
 * @brief Adds weapon bob due to running, walking, crouching, etc.
 */
static void Cg_WeaponBob(const player_state_t *ps, vec3_t *offset, vec3_t *angles) {
	const vec3_t bob = Vec3(0.2f, 0.4f, 0.2f);

	*offset = Vec3_Add(*offset, Vec3_Scale(bob, cg_view.bob));

	*angles = Vec3_Add(*angles, Vec3(0.f, 1.5f * cg_view.bob, 0.f));
}

/**
 * @brief Calculates a kick offset and angles based on our player's animation state.
 */
static void Cg_WeaponOffset(cl_entity_t *ent, vec3_t *offset, vec3_t *angles) {

	const vec3_t drop_raise_offset = Vec3(-4.f, -4.f, -4.f);
	const vec3_t drop_raise_angles = Vec3(25.f, -35.f, 2.f);

	const vec3_t kick_offset = Vec3(-6.f, 0.f, 0.f);
	const vec3_t kick_angles = Vec3(-2.f, 0.f, 0.f);

	*offset = Vec3_Zero();
	*angles = Vec3_Zero();

	if (ent->animation1.animation == ANIM_TORSO_DROP) {
		*offset = Vec3_Add(*offset, Vec3_Scale(drop_raise_offset, ent->animation1.fraction));
		*angles = Vec3_Scale(drop_raise_angles, ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_RAISE) {
		*offset = Vec3_Add(*offset, Vec3_Scale(drop_raise_offset, 1.f - ent->animation1.fraction));
		*angles = Vec3_Scale(drop_raise_angles, 1.f - ent->animation1.fraction);
	} else if (ent->animation1.animation == ANIM_TORSO_ATTACK1) {
		*offset = Vec3_Add(*offset, Vec3_Scale(kick_offset, 1.f - ent->animation1.fraction));
		*angles = Vec3_Scale(kick_angles, 1.f - ent->animation1.fraction);
	}

	*offset = Vec3_Scale(*offset, cg_bob->value);
	*angles = Vec3_Scale(*angles, cg_bob->value);

	*offset = Vec3_Add(*offset, Vec3(cg_draw_weapon_x->value, cg_draw_weapon_y->value, cg_draw_weapon_z->value));
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

		old_speed = Vec3_Zero();
		new_speed = Vec3_Zero();
	}

	vec3_t speed;

	const uint32_t delta = cgi.client->unclamped_time - time;
	if (delta < 100) {
		const float lerp = delta / 100.f;

		speed.x = old_speed.x + lerp * (new_speed.x - old_speed.x);
		speed.y = old_speed.y + lerp * (new_speed.y - old_speed.y);
		speed.z = old_speed.z + lerp * (new_speed.z - old_speed.z);
	} else {
		old_speed = new_speed;

		new_speed.x = -Clampf(ps->pm_state.velocity.x / 200.f, -1.f, 1.f);
		new_speed.y = -Clampf(ps->pm_state.velocity.y / 200.f, -1.f, 1.f);
		new_speed.z = -Clampf(ps->pm_state.velocity.z / 200.f, -.3f, 1.f);

		speed = old_speed;

		time = cgi.client->unclamped_time;
	}

	if (cg_draw_weapon_bob->modified) {
		cg_draw_weapon_bob->value = Clampf(cg_draw_weapon_bob->value, 0.0, 2.0);
		cg_draw_weapon_bob->modified = false;
	}

	*offset = Vec3_Scale(speed, cg_draw_weapon_bob->value);
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

	w.origin = Vec3_Add(w.origin, velocity);

	// Hand

	switch (cg_hand->integer) {
		case HAND_LEFT:
			offset.y -= 5.f;
			break;
		case HAND_RIGHT:
			offset.y += 5.f;
			break;
		default:
			break;
	}

	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->up, offset.z));
	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->right, offset.y));
	w.origin = Vec3_Add(w.origin, Vec3_Scale(cgi.view->forward, offset.x));

	w.angles = Vec3_Add(cgi.view->angles, angles);

	// Copy state over to render entity

	w.effects = EF_WEAPON | EF_NO_SHADOW;

	w.color = Vec4(1.0, 1.0, 1.0, 1.0);

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

	// set the origin and angles so that we know where to add effects
	r_entity_t e = {
		.origin = ent->origin,
		.termination = ent->termination,
		.angles = ent->angles,
		.scale = 1.f,
		.mins = ent->current.mins,
		.maxs = ent->current.maxs
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
