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

		cm_trace_t trace = gi.Trace(ent->s.origin, pos, ent->bounds, ent, ent->locals.clip_mask ? : CONTENTS_MASK_SOLID);

		if (trace.ent && trace.plane.normal.z >= PM_STEP_NORMAL) {
			if (ent->locals.ground_entity == NULL) {
				G_Debug("%s meeting ground %s\n", etos(ent), etos(trace.ent));
			}
			ent->locals.ground_entity = trace.ent;
			ent->locals.ground_plane = trace.plane;
			ent->locals.ground_texinfo = trace.texinfo;
			ent->locals.ground_contents = trace.contents;
		} else {
			if (ent->locals.ground_entity) {
				G_Debug("%s leaving ground %s\n", etos(ent), etos(ent->locals.ground_entity));
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
	vec3_t pos;

	if (ent->locals.move_type == MOVE_TYPE_WALK) {
		return;
	}

	if (ent->solid == SOLID_NOT) {
		return;
	}

	// check for water interaction
	const pm_water_level_t old_water_level = ent->locals.water_level;
	const int32_t old_water_type = ent->locals.water_type;
	box3_t bounds;

	if (ent->solid == SOLID_BSP) {
		pos = Box3_Center(ent->abs_bounds);
		bounds = Box3(
			Vec3_Subtract(pos, ent->abs_bounds.mins),
			Vec3_Subtract(ent->abs_bounds.maxs, pos)
		);
	} else {
		pos = ent->s.origin;
		bounds = ent->bounds;
	}

	const cm_trace_t tr = gi.Trace(pos, pos, bounds, ent, CONTENTS_MASK_LIQUID);

	ent->locals.water_type = tr.contents;
	ent->locals.water_level = ent->locals.water_type ? WATER_UNDER : WATER_NONE;

	if (old_water_level == WATER_NONE && ent->locals.water_level == WATER_UNDER) {

		if (ent->locals.move_type == MOVE_TYPE_BOUNCE) {
			ent->locals.velocity = Vec3_Scale(ent->locals.velocity, 0.66);
		}

		if (!(ent->sv_flags & SVF_NO_CLIENT)) {
			if (ent->locals.water_type & (CONTENTS_LAVA | CONTENTS_SLIME)) {
				gi.PositionedSound(pos, ent, g_media.sounds.water_in, SOUND_ATTEN_CUBIC, -32);
			} else {
				gi.PositionedSound(pos, ent, g_media.sounds.water_in, SOUND_ATTEN_CUBIC,  0);
			}

			if (ent->locals.move_type != MOVE_TYPE_NO_CLIP) {
				G_Ripple(ent, Vec3_Zero(), Vec3_Zero(), 0.0, true);
			}
		}

	} else if (old_water_level == WATER_UNDER && ent->locals.water_level == WATER_NONE) {

		if (!(ent->sv_flags & SVF_NO_CLIENT)) {
			if (old_water_type & (CONTENTS_LAVA | CONTENTS_SLIME)) {
				gi.PositionedSound(pos, ent, g_media.sounds.water_out, SOUND_ATTEN_CUBIC, -32);
			} else {
				gi.PositionedSound(pos, ent, g_media.sounds.water_out, SOUND_ATTEN_CUBIC,  0);
			}

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
 * @return True if the entity is in a valid position, false otherwise.
 */
static _Bool G_GoodPosition(const g_entity_t *ent) {

	const int32_t mask = ent->locals.clip_mask ? : CONTENTS_MASK_SOLID;

	const cm_trace_t tr = gi.Trace(ent->s.origin, ent->s.origin, ent->bounds, ent, mask);

	return tr.start_solid == false && tr.all_solid == false;
}

/**
 * @return True if the entity is in, or could be moved to, a valid position, false otherwise.
 */
static _Bool G_CorrectPosition(g_entity_t *ent) {

	const int32_t offsets[] = { 0, 1, -1 };

	vec3_t pos = ent->s.origin;

	for (size_t i = 0; i < lengthof(offsets); i++) {
		for (size_t j = 0; j < lengthof(offsets); j++) {
			for (size_t k = 0; k < lengthof(offsets); k++) {

				ent->s.origin = Vec3_Add(pos, Vec3(offsets[i], offsets[j], offsets[k]));
				gi.LinkEntity(ent);

				if (G_GoodPosition(ent)) {
					return true;
				}
			}
		}
	}

	ent->s.origin = pos;
	gi.LinkEntity(ent);
	G_Debug("still solid, reverting %s\n", etos(ent));

	return false;
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
		const cm_bsp_texinfo_t *texinfo = ent->locals.ground_texinfo;
		if (texinfo && (texinfo->flags & SURF_SLICK)) {
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

	ent->locals.velocity = Vec3_Fmaf(ent->locals.velocity, accel_speed, dir);
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

	const size_t len = gi.BoxEntities(ent->abs_bounds, ents, lengthof(ents), BOX_OCCUPY);
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
				.normal = { { 0.0, 0.0, 1.0 } },
				.dist = 0.0,
				.type = PLANE_Z
			};
			G_Debug("%s occupying %s\n", etos(ent), etos(occupied));
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

	ent->s.angles = Vec3_Fmaf(ent->s.angles, QUETOO_TICK_SECONDS, ent->locals.avelocity);
	ent->s.origin = Vec3_Fmaf(ent->s.origin, QUETOO_TICK_SECONDS, ent->locals.velocity);

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
static void G_Physics_Push_Rotate_Entity(g_entity_t *self, g_entity_t *ent, float yaw) {

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
static g_entity_t *G_Physics_Push_Translate(g_entity_t *self, const vec3_t move) {
	g_entity_t *ents[MAX_ENTITIES];

	G_Physics_Push_Impact(self);

	// calculate bounds for the entire move
	box3_t total_bounds = self->abs_bounds;

	// unlink the pusher so we don't get it in the entity list
	gi.UnlinkEntity(self);

	// store original position
	const vec3_t original_position = self->s.origin;

	// move the pusher to it's intended position
	const vec3_t final_position = Vec3_Add(original_position, move);

	self->s.origin = final_position;

	gi.LinkEntity(self);

	total_bounds = Box3_Union(total_bounds, self->abs_bounds);

	const size_t len = gi.BoxEntities(total_bounds, ents, lengthof(ents), BOX_ALL);

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

			G_Physics_Push_Impact(ent);

			if (ent->locals.ground_entity == self) {
				// we can only ride a bmodel if we're in a good position
				// on top of it already; to make things simpler, we assume
				// that we're not going to self-intersect with the pusher.

				// move us by the full translation, clipping to the rest of the world,
				// and clip us to where we end up.
				gi.UnlinkEntity(self);

				const cm_trace_t tr = gi.Trace(ent->s.origin, Vec3_Add(ent->s.origin, move), ent->bounds, ent, ent->locals.clip_mask);

				gi.LinkEntity(self);

				ent->s.origin = tr.end;

				// if we're good here, we can stop
				if (G_CorrectPosition(ent)) {
					continue;
				}

				// we intersected with the mover. we may have been pushed off of us by the world,
				// so try it's original position, which may now be valid.
				G_Physics_Push_Revert(--g_push_p);

				if (G_CorrectPosition(ent)) {
					continue;
				}
				
				// no good, we're blocking!
			} else {
				// we're not riding the bmodel, so we must be being pushed by it.
				// trace backwards to find our TOI with the pusher.
				gi.UnlinkEntity(ent);

				// restore original position to calculate our hit with it
				self->s.origin = original_position;

				gi.LinkEntity(self);

				cm_trace_t tr = gi.Clip(ent->s.origin, Vec3_Subtract(ent->s.origin, move), ent->bounds, self, ent->locals.clip_mask);

				// move back to final position
				self->s.origin = final_position;

				gi.LinkEntity(self);

				// did we even collide with it?
				if (tr.fraction >= 1.0) {
					G_Debug("%s false positive clip\n", etos(self));
					continue; // was a false positive?
				}

				// didn't hit the mover??
				if (tr.ent == self) {
					// we did; clip us against the world with the full movement that
					// we need to do
					const float remaining_dist = 1.0f - tr.fraction;

					const vec3_t new_position = Vec3_Fmaf(ent->s.origin, remaining_dist, Vec3_Multiply(move, Vec3_Fabsf(tr.plane.normal)));

					tr = gi.Trace(ent->s.origin, new_position, ent->s.bounds, self, ent->locals.clip_mask);
				
					ent->s.origin = tr.end;

					// in theory this should never be a bad spot, but who knows
					if (G_CorrectPosition(ent)) {
						G_Debug("%s: pushed %s into correct position\n", etos(self), etos(ent));
						continue;
					}
					
					G_Debug("%s: pushed %s into bad position\n", etos(self), etos(ent));
				} else {
					G_Debug("%s -> %s didn't clip?\n", etos(self), etos(ent));
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

		G_Debug("%s blocked by %s\n", etos(self), etos(ent));

		// if we've reached this point, we were G_MOVE_TYPE_STOP, or we were
		// blocked: revert any moves we may have made and return our obstacle

		while (g_push_p > g_pushes) {
			G_Physics_Push_Revert(--g_push_p);
		}

		return ent;
	}

	// set us in the new position
	self->s.origin = final_position;

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

static cm_trace_t G_Physics_Push_Rotate_And_Trace(g_entity_t *ent, g_entity_t *mover, const vec3_t angles) {
	mover->s.angles = angles;
	
	gi.LinkEntity(mover);

	return gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, mover, ent->locals.clip_mask);
}

/**
 * @brief The smallest fraction we care about in rotational
 * TOI precision checking
 */
#define TOI_MIN_FRACTION	0.06f

/**
 * @return 
 */
static float G_Physics_Push_Calculate_Rotational_TOI(g_entity_t *ent, g_entity_t *mover, const vec3_t original_angles, const vec3_t final_angles, const float left, const float right) {
	const cm_trace_t left_tr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(original_angles, final_angles, left));
	
	const float half = Mixf(left, right, 0.5f);

	const cm_trace_t half_tr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(original_angles, final_angles, half));

	if (left_tr.fraction == 1.f && half_tr.fraction < 1.f) {

		if (half - left < TOI_MIN_FRACTION) {
			return left;
		}

		return G_Physics_Push_Calculate_Rotational_TOI(ent, mover, original_angles, final_angles, left, half);
	}

	const cm_trace_t right_tr = G_Physics_Push_Rotate_And_Trace(ent, mover, Vec3_Mix(original_angles, final_angles, right));

	if (half_tr.fraction == 1.f && right_tr.fraction < 1.f) {

		if (half - left < TOI_MIN_FRACTION) {
			return half;
		}

		return G_Physics_Push_Calculate_Rotational_TOI(ent, mover, original_angles, final_angles, half, right);
	}

	// this is an edge case where both positions are occupied by the mover.
	// should never be possible on any iteration other than the first.
	return 0.f;
}

/**
 * @brief
 */
static g_entity_t *G_Physics_Push_Rotate(g_entity_t *self, const vec3_t amove) {
	g_entity_t *ents[MAX_ENTITIES];

	G_Physics_Push_Impact(self);

	// calculate bounds for the entire move
	box3_t total_bounds = self->abs_bounds;

	// unlink the pusher so we don't get it in the entity list
	gi.UnlinkEntity(self);

	const vec3_t original_angles = self->s.angles;

	const vec3_t final_angles = Vec3_Add(original_angles, amove);

	// move the pusher to it's intended position
	self->s.angles = final_angles;

	gi.LinkEntity(self);

	total_bounds = Box3_Union(total_bounds, self->abs_bounds);

	const size_t len = gi.BoxEntities(total_bounds, ents, lengthof(ents), BOX_ALL);

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

			G_Physics_Push_Impact(ent);

			// are we going to collide with the mover?
			// put us to final angles and check for intersection.
			// FIXME: this won't work for larger rotations of thin objects
			// that rotate beyond the bounds of an object.
			self->s.angles = final_angles;

			gi.LinkEntity(self);

			cm_trace_t tr = gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, self, ent->locals.clip_mask);
			float remaining_move = 1.0f;

			if (tr.fraction < 1.f) {
				// we intersect with the final position, so we're gonna be
				// pushed by the rotator. calculate approximate TOI
				remaining_move = 1.0f - G_Physics_Push_Calculate_Rotational_TOI(ent, self, original_angles, final_angles, 0.f, 1.f);

				// put us back to final position
				self->s.angles = final_angles;

				gi.LinkEntity(self);
			}

			// calculate the rotational matrix for the rotation around the origin
			vec3_t original_ent_position = ent->s.origin;

			// try a few movements, taking the one that doesn't clip with the mover.
			const int32_t total_movements = 55;
			int32_t i;

			for (i = 0; i < total_movements; i++) {
				int32_t offset = (int32_t) ceilf(i * 0.5f);
				if (i & 1) {
					offset = -offset;
				}
				mat4_t m = Mat4_FromTranslation(self->s.origin);
				m = Mat4_ConcatRotation3(m, Vec3_Scale(Vec3(amove.z, amove.x, amove.y), remaining_move + (remaining_move * offset * 0.5f)));
				m = Mat4_ConcatTranslation(m, Vec3_Negate(self->s.origin));

				ent->s.origin = Mat4_Transform(m, original_ent_position);

				if (gi.Clip(ent->s.origin, ent->s.origin, ent->bounds, self, ent->locals.clip_mask).fraction == 1.0f) {
					G_Debug("%s rotated %s @ %i, good position\n", etos(self), etos(ent), i);
					break;
				}
			}

			if (i == total_movements) {
				G_Debug("%s rotated %s, but couldn't fit; %f was remaining\n", etos(self), etos(ent), remaining_move);
			}

			// clip rest of the movement.
			tr = gi.Trace(original_ent_position, ent->s.origin, ent->bounds, ent, ent->locals.clip_mask);

			ent->s.origin = tr.end;

			// if the move has separated us, finish up by rotating the entity
			if (G_CorrectPosition(ent)) {
				G_Physics_Push_Rotate_Entity(self, ent, amove.y);
				break;
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

		G_Debug("%s blocked by %s\n", etos(self), etos(ent));

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
	for (g_entity_t *part = ent; part; part = part->locals.team_next) {
		if (!Vec3_Equal(part->locals.velocity, Vec3_Zero())) { // object is translating
			const vec3_t move = Vec3_Scale(part->locals.velocity, QUETOO_TICK_SECONDS);

			if ((obstacle = G_Physics_Push_Translate(part, move))) {
				break; // move was blocked
			}
		}

		if (!Vec3_Equal(part->locals.avelocity, Vec3_Zero())) { // object is rotating
			const vec3_t amove = Vec3_Scale(part->locals.avelocity, QUETOO_TICK_SECONDS);

			if ((obstacle = G_Physics_Push_Rotate(part, amove))) {
				break; // move was blocked
			}
		}
	}

	if (!obstacle) { // the move succeeded, so call all think functions
		for (g_entity_t *part = ent; part; part = part->locals.team_next) {
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
		G_Debug("%s touching %s\n", etos(ent), etos(trace->ent));
		ent->locals.Touch(ent, trace->ent, &trace->plane, trace->texinfo);
	}

	if (ent->in_use && trace->ent->in_use) {

		if (trace->ent->locals.Touch) {
			G_Debug("%s touching %s\n", etos(trace->ent), etos(ent));
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

	const int32_t mask = ent->locals.clip_mask ? : CONTENTS_MASK_SOLID;

	float time_remaining = QUETOO_TICK_SECONDS;
	int32_t num_planes = 0;

	for (int32_t bump = 0; bump < MAX_CLIP_PLANES; bump++) {
		vec3_t pos;

		if (time_remaining <= 0.0) {
			break;
		}

		// project desired destination
		pos = Vec3_Fmaf(ent->s.origin, time_remaining, ent->locals.velocity);

		// trace to it
		const cm_trace_t trace = gi.Trace(ent->s.origin, pos, ent->bounds, ent, mask);

		// if the entity is trapped in a solid, don't build up Z
		if (trace.all_solid) {
			ent->locals.velocity.z = 0;
			return true;
		}

		const float time = trace.fraction * time_remaining;

		ent->s.origin = Vec3_Fmaf(ent->s.origin, time, ent->locals.velocity);
		ent->s.angles = Vec3_Fmaf(ent->s.angles, time, ent->locals.avelocity);

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
		G_Debug("reverting %s\n", etos(ent));

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
