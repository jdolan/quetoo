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

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @see Pm_CheckGround
 */
static void G_CheckGround(g_entity_t *ent) {
	vec3_t pos;

	if (ent->locals.move_type == MOVE_TYPE_WALK) {
		return;
	}

	// check for ground interaction
	if (ent->locals.move_type == MOVE_TYPE_BOUNCE) {

		pos = ent->s.origin;
		pos.z -= PM_GROUND_DIST;

		cm_trace_t trace = gi.Trace(ent->s.origin, pos, ent->mins, ent->maxs, ent, ent->locals.clip_mask ? : MASK_SOLID);

		if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {
			if (ent->locals.ground_entity == NULL) {
				gi.Debug("%s meeting ground %s\n", etos(ent), etos(trace.ent));
			}
			ent->locals.ground_entity = trace.ent;
			ent->locals.ground_plane = trace.plane;
			ent->locals.ground_surface = trace.surface;
			ent->locals.ground_contents = trace.contents;
		} else {
			if (ent->locals.ground_entity) {
				gi.Debug("%s leaving ground %s\n", etos(ent), etos(ent->locals.ground_entity));
			}
			ent->locals.ground_entity = NULL;
		}
	} else {
		ent->locals.ground_entity = NULL;
	}
}

/**
 * @brief
 */
static void G_CheckWater(g_entity_t *ent) {
	vec3_t pos, mins, maxs;

	if (ent->locals.move_type == MOVE_TYPE_WALK) {
		return;
	}

	if (ent->solid == SOLID_NOT) {
		return;
	}

	// check for water interaction
	const uint8_t old_water_level = ent->locals.water_level;

	if (ent->solid == SOLID_BSP) {
		pos = Vec3_Mix(ent->abs_mins, ent->abs_maxs, 0.5);
		mins = Vec3_Subtract(pos, ent->abs_mins);
		maxs = Vec3_Subtract(ent->abs_maxs, pos);
	} else {
		pos = ent->s.origin;
		mins = ent->mins;
		maxs = ent->maxs;
	}

	const cm_trace_t tr = gi.Trace(pos, pos, mins, maxs, ent, MASK_LIQUID);

	ent->locals.water_type = tr.contents;
	ent->locals.water_level = ent->locals.water_type ? WATER_UNDER : WATER_NONE;

	if (old_water_level == WATER_NONE && ent->locals.water_level == WATER_UNDER) {

		if (ent->locals.move_type == MOVE_TYPE_BOUNCE) {
			ent->locals.velocity = Vec3_Scale(ent->locals.velocity, 0.66);
		}

		if (!(ent->sv_flags & SVF_NO_CLIENT)) {
			gi.PositionedSound(pos, ent, g_media.sounds.water_in, ATTEN_IDLE, 0);

			if (ent->locals.move_type != MOVE_TYPE_NO_CLIP) {
				G_Ripple(ent, Vec3_Zero(), Vec3_Zero(), 0.0, true);
			}
		}

	} else if (old_water_level == WATER_UNDER && ent->locals.water_level == WATER_NONE) {

		if (!(ent->sv_flags & SVF_NO_CLIENT)) {
			gi.PositionedSound(pos, ent, g_media.sounds.water_out, ATTEN_IDLE, 0);

			if (ent->locals.move_type != MOVE_TYPE_NO_CLIP) {
				G_Ripple(ent, Vec3_Zero(), Vec3_Zero(), 0.0, false);
			}
		}
	}
}

/**
 * @brief Runs thinking code for this frame if necessary
 */
void G_RunThink(g_entity_t *ent) {

	if (ent->locals.next_think == 0) {
		return;
	}

	if (ent->locals.next_think > g_level.time + 1) {
		return;
	}

	ent->locals.next_think = 0;

	if (!ent->locals.Think) {
		gi.Error("%s has no Think function\n", etos(ent));
	}

	ent->locals.Think(ent);
}

/**
 * @return True if the entitiy is in a valid position, false otherwise.
 */
static _Bool G_GoodPosition(const g_entity_t *ent) {

	const int32_t mask = ent->locals.clip_mask ? : MASK_SOLID;

	const cm_trace_t tr = gi.Trace(ent->s.origin, ent->s.origin, ent->mins, ent->maxs, ent, mask);

	return tr.start_solid == false;
}

/**
 * @return True if the entitiy is in, or could be moved to, a valid position, false otherwise.
 */
static _Bool G_CorrectPosition(g_entity_t *ent) {

	if (!G_GoodPosition(ent)) {

		vec3_t pos;
		pos = ent->s.origin;

		for (int32_t i = -1; i <= 1; i++) {
			for (int32_t j = -1; j <= 1; j++) {
				for (int32_t k = -1; k <= 1; k++) {
					ent->s.origin = pos;

					ent->s.origin.x += i * PM_NUDGE_DIST;
					ent->s.origin.y += j * PM_NUDGE_DIST;
					ent->s.origin.z += k * PM_NUDGE_DIST;

					if (G_GoodPosition(ent)) {
						return true;
					}
				}
			}
		}

		ent->s.origin = pos;
		gi.Debug("still solid, reverting %s\n", etos(ent));

		return false;
	}

	return true;
}

#define MAX_SPEED 2400.0
#define STOP_EPSILON PM_STOP_EPSILON

/**
 * @brief
 */
static void G_ClampVelocity(g_entity_t *ent) {

	const float speed = Vec3_Length(ent->locals.velocity);

	if (speed > MAX_SPEED) {
		ent->locals.velocity = Vec3_Scale(ent->locals.velocity, MAX_SPEED / speed);
	} else if (speed < STOP_EPSILON) {
		ent->locals.velocity = Vec3_Zero();
	}
}

/**
 * @brief Slide off of the impacted plane.
 */
static vec3_t G_ClipVelocity(const vec3_t in, const vec3_t normal, float bounce) {

	float backoff = Vec3_Dot(in, normal);

	if (backoff < 0.0) {
		backoff *= bounce;
	} else {
		backoff /= bounce;
	}

	return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

#define SPEED_STOP 150.0

/**
 * @see Pm_Friction
 */
static void G_Friction(g_entity_t *ent) {

	vec3_t vel = ent->locals.velocity;

	if (ent->locals.ground_entity) {
		vel.z = 0.0;
	}

	const float speed = Vec3_Length(vel);

	if (speed < 1.0) {
		ent->locals.velocity = Vec3_Zero();
		return;
	}

	const float control = Maxf(SPEED_STOP, speed);

	float friction = 0.0;

	if (ent->locals.ground_entity) {
		const cm_bsp_texinfo_t *surf = ent->locals.ground_surface;
		if (surf && (surf->flags & SURF_SLICK)) {
			friction = PM_FRICT_GROUND_SLICK;
		} else {
			friction = PM_FRICT_GROUND;
		}
	} else {
		friction = PM_FRICT_AIR;
	}

	if (ent->locals.water_type) {
		friction += PM_FRICT_WATER;
	}

	float scale = Maxf(0.0, speed - (friction * control * QUETOO_TICK_SECONDS)) / speed;

	ent->locals.velocity = Vec3_Scale(ent->locals.velocity, scale);
	ent->locals.avelocity = Vec3_Scale(ent->locals.avelocity, scale);
}

/**
 * @see Pm_Accelerate
 */
static void G_Accelerate(g_entity_t *ent, const vec3_t dir, float speed, float accel) {

	const float current_speed = Vec3_Dot(ent->locals.velocity, dir);
	const float add_speed = speed - current_speed;

	if (add_speed <= 0.0) {
		return;
	}

	float accel_speed = accel * QUETOO_TICK_SECONDS * speed;

	if (accel_speed > add_speed) {
		accel_speed = add_speed;
	}

	ent->locals.velocity = Vec3_Add(ent->locals.velocity, Vec3_Scale(dir, accel_speed));
}

/**
 * @see Pm_Gravity
 */
static void G_Gravity(g_entity_t *ent) {

	if (ent->locals.ground_entity == NULL) {
		float gravity = g_level.gravity;

		if (ent->locals.water_level) {
			gravity *= PM_GRAVITY_WATER;
		}

		ent->locals.velocity.z -= gravity * QUETOO_TICK_SECONDS;
	}
}

/**
 * @see Pm_Currents
 */
static void G_Currents(g_entity_t *ent) {
	vec3_t current;

	current = Vec3_Zero();

	if (ent->locals.water_level) {

		if (ent->locals.water_type & CONTENTS_CURRENT_0) {
			current.x += 1.0;
		}
		if (ent->locals.water_type & CONTENTS_CURRENT_90) {
			current.y += 1.0;
		}
		if (ent->locals.water_type & CONTENTS_CURRENT_180) {
			current.x -= 1.0;
		}
		if (ent->locals.water_type & CONTENTS_CURRENT_270) {
			current.y -= 1.0;
		}
		if (ent->locals.water_type & CONTENTS_CURRENT_UP) {
			current.z += 1.0;
		}
		if (ent->locals.water_type & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.0;
		}
	}

	if (ent->locals.ground_entity) {

		if (ent->locals.ground_contents & CONTENTS_CURRENT_0) {
			current.x += 1.0;
		}
		if (ent->locals.ground_contents & CONTENTS_CURRENT_90) {
			current.y += 1.0;
		}
		if (ent->locals.ground_contents & CONTENTS_CURRENT_180) {
			current.x -= 1.0;
		}
		if (ent->locals.ground_contents & CONTENTS_CURRENT_270) {
			current.y -= 1.0;
		}
		if (ent->locals.ground_contents & CONTENTS_CURRENT_UP) {
			current.z += 1.0;
		}
		if (ent->locals.ground_contents & CONTENTS_CURRENT_DOWN) {
			current.z -= 1.0;
		}
	}

	current = Vec3_Scale(current, PM_SPEED_CURRENT);

	const float speed = Vec3_Length(current);

	if (speed == 0.0) {
		return;
	}

	current = Vec3_Normalize(current);

	G_Accelerate(ent, current, speed, PM_ACCEL_GROUND);
}

/**
 * @brief Interact with `BOX_OCCUPY` objects after moving.
 */
void G_TouchOccupy(g_entity_t *ent) {
	g_entity_t *ents[MAX_ENTITIES];

	switch (ent->solid) {
		case SOLID_PROJECTILE:
		case SOLID_DEAD:
		case SOLID_BOX:
			break;
		default:
			return;
	}

	const size_t len = gi.BoxEntities(ent->abs_mins, ent->abs_maxs, ents, lengthof(ents), BOX_OCCUPY);
	for (size_t i = 0; i < len; i++) {

		g_entity_t *occupied = ents[i];

		if (occupied == ent) {
			continue;
		}

		if (occupied->solid == SOLID_PROJECTILE && occupied->owner == ent) {
			continue;
		}

		if (occupied->locals.Touch) {
			static cm_bsp_plane_t plane = {
				.normal = { 0.0, 0.0, 1.0 },
				.dist = 0.0,
				.type = PLANE_Z
			};
			gi.Debug("%s occupying %s\n", etos(ent), etos(occupied));
			occupied->locals.Touch(occupied, ent, &plane, NULL);
		}

		if (!ent->in_use) {
			break;
		}
	}
}

/**
 * @brief A moving object that doesn't obey physics
 */
static void G_Physics_NoClip(g_entity_t *ent) {

	ent->s.angles = Vec3_Add(ent->s.angles, Vec3_Scale(ent->locals.avelocity, QUETOO_TICK_SECONDS));
	ent->s.origin = Vec3_Add(ent->s.origin, Vec3_Scale(ent->locals.velocity, QUETOO_TICK_SECONDS));

	gi.LinkEntity(ent);
}

/**
 * @brief Maintain a record of all pushed entities for each move, in case that
 * move needs to be reverted.
 */
typedef struct {
	g_entity_t *ent;
	vec3_t origin;
	vec3_t angles;
	int16_t delta_yaw;
} g_push_t;

static g_push_t g_pushes[MAX_ENTITIES], *g_push_p;

/**
 * @brief
 */
static void G_Physics_Push_Impact(g_entity_t *ent) {

	if (g_push_p - g_pushes == MAX_ENTITIES) {
		gi.Error("MAX_ENTITIES\n");
	}

	g_push_p->ent = ent;

	g_push_p->origin = ent->s.origin;
	g_push_p->angles = ent->s.angles;

	if (ent->client) {
		g_push_p->delta_yaw = ent->client->ps.pm_state.delta_angles.y;
	} else {
		g_push_p->delta_yaw = 0;
	}

	g_push_p++;
}

/**
 * @brief
 */
static void G_Physics_Push_Revert(const g_push_t *p) {

	p->ent->s.origin = p->origin;
	p->ent->s.angles = p->angles;

	if (p->ent->client) {
		p->ent->client->ps.pm_state.delta_angles.y = p->delta_yaw;
	}

	gi.LinkEntity(p->ent);
}

/**
 * @brief When items ride pushers, they rotate along with them. For clients,
 * this requires incrementing their delta angles.
 */
static void G_Physics_Push_Rotate(g_entity_t *self, g_entity_t *ent, float yaw) {

	if (ent->locals.ground_entity == self) {
		if (ent->client) {
			ent->client->ps.pm_state.delta_angles.y += yaw;
		} else {
			ent->s.angles.y += yaw;
		}
	}
}

/**
 * @brief
 */
static g_entity_t *G_Physics_Push_Move(g_entity_t *self, const vec3_t move, const vec3_t amove) {
	vec3_t inverse_amove, forward, right, up;
	g_entity_t *ents[MAX_ENTITIES];

	G_Physics_Push_Impact(self);

	// calculate bounds for the entire move
	vec3_t total_mins, total_maxs;

	if (!Vec3_Equal(self->s.angles, Vec3_Zero()) || !Vec3_Equal(amove, Vec3_Zero())) {
		const float radius = Vec3_Distance(self->mins, self->maxs) * 0.5;

		for (int32_t i = 0 ; i < 3 ; i++ ) {
			total_mins.xyz[i] = self->s.origin.xyz[i] - radius;
			total_maxs.xyz[i] = self->s.origin.xyz[i] + radius;
		}
	} else {

		total_mins = self->abs_mins;
		total_maxs = self->abs_maxs;

		for (int32_t i = 0; i < 3; i++) {
			if (move.xyz[i] > 0) {
				total_maxs.xyz[i] += move.xyz[i];
			} else {
				total_mins.xyz[i] += move.xyz[i];
			}
		}
	}

	// unlink the pusher so we don't get it in the entity list
	gi.UnlinkEntity(self);

	const size_t len = gi.BoxEntities(total_mins, total_maxs, ents, lengthof(ents), BOX_ALL);

	// move the pusher to it's intended position
	self->s.origin = Vec3_Add(self->s.origin, move);
	self->s.angles = Vec3_Add(self->s.angles, amove);

	gi.LinkEntity(self);

	// calculate the angle vectors for rotational movement
	inverse_amove = Vec3_Negate(amove);
	Vec3_Vectors(inverse_amove, &forward, &right, &up);

	// see if any solid entities are inside the final position
	for (size_t i = 0; i < len; i++) {

		g_entity_t *ent = ents[i];

		if (ent->solid == SOLID_BSP) {
			continue;
		}

		if (ent->locals.move_type < MOVE_TYPE_WALK) {
			continue;
		}

		// if the entity is in a good position and not riding us, we can skip them
		if (G_GoodPosition(ent) && ent->locals.ground_entity != self) {
			continue;
		}

		// if we are a pusher, or someone is riding us, try to move them
		if ((self->locals.move_type == MOVE_TYPE_PUSH) || (ent->locals.ground_entity == self)) {
			vec3_t translate, rotate, delta;

			G_Physics_Push_Impact(ent);

			// translate the pushed entity
			ent->s.origin = Vec3_Add(ent->s.origin, move);

			// then rotate the movement to comply with the pusher's rotation
			translate = Vec3_Subtract(ent->s.origin, self->s.origin);

			rotate.x = Vec3_Dot(translate, forward);
			rotate.y = -Vec3_Dot(translate, right);
			rotate.z = Vec3_Dot(translate, up);

			delta = Vec3_Subtract(rotate, translate);

			ent->s.origin = Vec3_Add(ent->s.origin, delta);

			// if the move has separated us, finish up by rotating the entity
			if (G_CorrectPosition(ent)) {
				G_Physics_Push_Rotate(self, ent, amove.y);
				continue;
			}

			if (ent->locals.ground_entity == self) {

				// an entity riding us may have been pushed off of us by the world, so try
				// it's original position, which may now be valid

				G_Physics_Push_Revert(--g_push_p);

				// but in this case, don't rotate
				if (G_CorrectPosition(ent)) {
					continue;
				}
			}
		}

		// try to destroy the impeding entity by calling our Blocked function

		if (self->locals.Blocked) {
			self->locals.Blocked(self, ent);
			if (!ent->in_use || ent->locals.dead) {
				continue;
			}
		}

		gi.Debug("%s blocked by %s\n", etos(self), etos(ent));

		// if we've reached this point, we were G_MOVE_TYPE_STOP, or we were
		// blocked: revert any moves we may have made and return our obstacle

		while (g_push_p > g_pushes) {
			G_Physics_Push_Revert(--g_push_p);
		}

		return ent;
	}

	// the move was successful, so re-link all pushed entities
	for (g_push_t *p = g_push_p - 1; p >= g_pushes; p--) {
		if (p->ent->in_use) {

			gi.LinkEntity(p->ent);

			G_CheckGround(p->ent);

			G_CheckWater(p->ent);

			G_TouchOccupy(p->ent);
		}
	}

	return NULL;
}

/**
 * @brief For G_MOVE_TYPE_PUSH, push all box entities intersected while moving.
 * Generally speaking, only inline BSP models are pushers.
 */
static void G_Physics_Push(g_entity_t *ent) {
	g_entity_t *obstacle = NULL;

	// for teamed entities, the master must initiate all moves
	if (ent->locals.flags & FL_TEAM_SLAVE) {
		return;
	}

	// reset the pushed array
	g_push_p = g_pushes;

	// make sure all team slaves can move before committing any moves
	for (g_entity_t *part = ent; part; part = part->locals.team_chain) {
		if (!Vec3_Equal(part->locals.velocity, Vec3_Zero()) ||
				!Vec3_Equal(part->locals.avelocity, Vec3_Zero())) { // object is moving
			vec3_t move, amove;

			move = Vec3_Scale(part->locals.velocity, QUETOO_TICK_SECONDS);
			amove = Vec3_Scale(part->locals.avelocity, QUETOO_TICK_SECONDS);

			if ((obstacle = G_Physics_Push_Move(part, move, amove))) {
				break; // move was blocked
			}
		}
	}

	if (obstacle) { // blocked, let's try again next frame
		for (g_entity_t *part = ent; part; part = part->locals.team_chain) {
			if (part->locals.next_think) {
				part->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
			}
		}
	} else { // the move succeeded, so call all think functions
		for (g_entity_t *part = ent; part; part = part->locals.team_chain) {
			G_RunThink(part);
		}
	}
}

#define MAX_CLIP_PLANES 4

typedef struct {
	g_entity_t *entities[MAX_CLIP_PLANES];
	int32_t num_entities;
} g_touch_t;

static g_touch_t g_touch;

/**
 * @brief Runs the `Touch` functions of each object.
 */
static void G_TouchEntity(g_entity_t *ent, const cm_trace_t *trace) {

	// ensure that we only impact an entity once per frame

	for (int32_t i = 0; i < g_touch.num_entities; i++) {
		if (g_touch.entities[i] == trace->ent) {
			return;
		}
	}

	g_touch.entities[g_touch.num_entities++] = trace->ent;

	// run the interaction

	if (ent->locals.Touch) {
		gi.Debug("%s touching %s\n", etos(ent), etos(trace->ent));
		ent->locals.Touch(ent, trace->ent, &trace->plane, trace->surface);
	}

	if (ent->in_use && trace->ent->in_use) {

		if (trace->ent->locals.Touch) {
			gi.Debug("%s touching %s\n", etos(trace->ent), etos(ent));
			trace->ent->locals.Touch(trace->ent, ent, NULL, NULL);
		}
	}
}

/**
 * @see Pm_SlideMove
 */
static _Bool G_Physics_Fly_Move(g_entity_t *ent, const float bounce) {
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t origin, angles;

	memset(&g_touch, 0, sizeof(g_touch));

	origin = ent->s.origin;
	angles = ent->s.angles;

	const int32_t mask = ent->locals.clip_mask ? : MASK_SOLID;

	float time_remaining = QUETOO_TICK_SECONDS;
	int32_t num_planes = 0;

	for (int32_t bump = 0; bump < MAX_CLIP_PLANES; bump++) {
		vec3_t pos;

		if (time_remaining <= 0.0) {
			break;
		}

		// project desired destination
		pos = Vec3_Add(ent->s.origin, Vec3_Scale(ent->locals.velocity, time_remaining));

		// trace to it
		const cm_trace_t trace = gi.Trace(ent->s.origin, pos, ent->mins, ent->maxs, ent, mask);

		// if the entity is trapped in a solid, don't build up Z
		if (trace.all_solid) {
			ent->locals.velocity.z = 0;
			return true;
		}

		const float time = trace.fraction * time_remaining;

		ent->s.origin = Vec3_Add(ent->s.origin, Vec3_Scale(ent->locals.velocity, time));
		ent->s.angles = Vec3_Add(ent->s.angles, Vec3_Scale(ent->locals.avelocity, time));

		time_remaining -= time;

		if (trace.ent && trace.ent->solid > SOLID_TRIGGER) {

			G_TouchEntity(ent, &trace);

			if (!ent->in_use) {
				return true;
			}

			if (!trace.ent->in_use) {
				continue;
			}

			// if both entities remain, clip this entity to the trace entity

			planes[num_planes] = trace.plane.normal;
			num_planes++;

			for (int32_t i = 0; i < num_planes; i++) {

				if (Vec3_Dot(ent->locals.velocity, planes[i]) >= 0.0) {
					continue;
				}

				// slide along the plane
				vec3_t vel = G_ClipVelocity(ent->locals.velocity, planes[i], bounce);

				// see if there is a second plane that the new move enters
				for (int32_t j = 0; j < num_planes; j++) {
					vec3_t cross;

					if (j == i) {
						continue;
					}

					if (Vec3_Dot(vel, planes[j]) >= 0.0) {
						continue;
					}

					// try clipping the move to the plane
					vel = G_ClipVelocity(vel, planes[j], PM_CLIP_BOUNCE);

					// see if it goes back into the first clip plane
					if (Vec3_Dot(vel, planes[i]) >= 0.0) {
						continue;
					}

					// slide the original velocity along the crease
					cross = Vec3_Cross(planes[i], planes[j]);
					cross = Vec3_Normalize(cross);

					const float scale = Vec3_Dot(cross, ent->locals.velocity);
					vel = Vec3_Scale(cross, scale);

					// see if there is a third plane the the new move enters
					for (int32_t k = 0; k < num_planes; k++) {

						if (k == i || k == j) {
							continue;
						}

						if (Vec3_Dot(vel, planes[k]) >= 0.0) {
							continue;
						}

						// stop dead at a triple plane interaction
						ent->locals.velocity = Vec3_Zero();
						return true;
					}
				}

				// if we have fixed all interactions, try another move
				ent->locals.velocity = vel;
				break;
			}
		}
	}

	if (!G_CorrectPosition(ent)) {
		gi.Debug("reverting %s\n", etos(ent));

		ent->s.origin = origin;
		ent->s.angles = angles;

		ent->locals.velocity = Vec3_Zero();
		ent->locals.avelocity = Vec3_Zero();
	}

	gi.LinkEntity(ent);

	return num_planes == 0;
}

/**
 * @brief Fly through the world, interacting with other solids.
 */
static void G_Physics_Fly(g_entity_t *ent) {

	G_Physics_Fly_Move(ent, 1.0);

	G_CheckGround(ent);

	G_CheckWater(ent);

	G_TouchOccupy(ent);
}

/**
 * @brief Bounce movement. When on ground, do nothing.
 */
static void G_Physics_Bounce(g_entity_t *ent) {

	if (ent->locals.ground_entity == NULL || Vec3_Equal(ent->locals.velocity, Vec3_Zero()) == false) {

		G_Friction(ent);

		G_Gravity(ent);

		G_Currents(ent);

		G_Physics_Fly_Move(ent, 1.33);
	}

	G_CheckGround(ent);

	G_CheckWater(ent);

	G_TouchOccupy(ent);
}

/**
 * @brief Dispatches thinking and physics routines for the specified entity.
 */
void G_RunEntity(g_entity_t *ent) {

	G_ClampVelocity(ent);

	G_RunThink(ent);

	switch (ent->locals.move_type) {
		case MOVE_TYPE_NONE:
			break;
		case MOVE_TYPE_NO_CLIP:
			G_Physics_NoClip(ent);
			break;
		case MOVE_TYPE_PUSH:
		case MOVE_TYPE_STOP:
			G_Physics_Push(ent);
			break;
		case MOVE_TYPE_FLY:
			G_Physics_Fly(ent);
			break;
		case MOVE_TYPE_BOUNCE:
			G_Physics_Bounce(ent);
			break;
		default:
			gi.Error("Bad move type %i\n", ent->locals.move_type);
	}

	// update BSP sub-model animations based on move state
	if (ent->solid == SOLID_BSP) {
		ent->s.animation1 = ent->locals.move_info.state;
	}
}
