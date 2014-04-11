/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include "g_local.h"
#include "bg_pmove.h"

#define MAX_SPEED 2400.0

/*
 * @brief
 */
static void G_ClampVelocity(g_edict_t *ent) {

	const vec_t speed = VectorLength(ent->locals.velocity);

	if (speed > MAX_SPEED) {
		VectorScale(ent->locals.velocity, MAX_SPEED / speed, ent->locals.velocity);
	}
}

/*
 * @brief Runs thinking code for this frame if necessary
 */
static void G_RunThink(g_edict_t *ent) {

	if (ent->locals.next_think == 0)
		return;

	if (ent->locals.next_think > g_level.time + 1)
		return;

	ent->locals.next_think = 0;

	if (!ent->locals.Think)
		gi.Error("%s has no Think function\n", etos(ent));

	ent->locals.Think(ent);
}

/*
 * @brief Two entities have touched, so run their touch functions
 */
static void G_RunTouch(g_edict_t *ent, cm_trace_t *trace) {

	g_edict_t *other = trace->ent;

	if (ent->locals.Touch && other->solid != SOLID_NOT)
		ent->locals.Touch(ent, other, &trace->plane, trace->surface);

	if (other->locals.Touch && ent->solid != SOLID_NOT)
		other->locals.Touch(other, ent, NULL, NULL);
}

#define STOP_EPSILON PM_STOP_EPSILON

/*
 * @brief Slide off of the impacted plane.
 */
static void G_ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, vec_t bounce) {
	int32_t i;

	vec_t backoff = DotProduct(in, normal);

	if (backoff < 0.0)
		backoff *= bounce;
	else
		backoff /= bounce;

	for (i = 0; i < 3; i++) {

		const vec_t change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] < STOP_EPSILON && out[i] > -STOP_EPSILON)
			out[i] = 0.0;
	}
}

/*
 * @see Pm_GoodPosition.
 */
static _Bool G_GoodPosition(const g_edict_t *ent) {

	const int32_t mask = ent->locals.clip_mask ? ent->locals.clip_mask : MASK_SOLID;

	return !gi.Trace(ent->s.origin, ent->s.origin, ent->mins, ent->maxs, ent, mask).start_solid;
}

/*
 * @see Pm_SnapPosition.
 */
static _Bool G_SnapPosition(g_edict_t *ent) {
	const int16_t jitter_bits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };
	int16_t i, sign[3], org[3];
	vec3_t old_origin;
	size_t j;

	VectorCopy(ent->s.origin, old_origin);

	// snap the origin, but be prepared to try nearby locations
	for (i = 0; i < 3; i++) {
		sign[i] = ent->s.origin[i] >= 0.0 ? 1 : -1;
	}

	// try all combinations, bumping the position away from the origin
	for (j = 0; j < lengthof(jitter_bits); j++) {
		const int16_t bit = jitter_bits[j];

		PackVector(ent->s.origin, org);

		for (i = 0; i < 3; i++) {
			if (bit & (1 << i))
				org[i] += sign[i];
		}

		UnpackVector(org, ent->s.origin);

		if (G_GoodPosition(ent)) {
			return true;
		}
	}

	VectorCopy(old_origin, ent->s.origin);
	return false;
}

/*
 * @brief Applies gravity, which is dependent on water level.
 */
static void G_Gravity(g_edict_t *ent) {
	vec_t gravity = g_level.gravity;

	if (ent->locals.water_level) {
		gravity *= PM_GRAVITY_WATER;
	}

	ent->locals.velocity[2] -= gravity * gi.frame_seconds;
}

/*
 * @brief Handles friction against entity momentum, and based on contents.
 */
static void G_Friction(g_edict_t *ent) {

	const vec_t speed = VectorLength(ent->locals.velocity);

	if (speed < 1.0) {
		VectorClear(ent->locals.velocity);
		return;
	}

	vec_t friction = 0.0;

	if (ent->locals.ground_entity) {
		if (ent->locals.ground_surface && (ent->locals.ground_surface->flags & SURF_SLICK)) {
			friction = PM_FRICT_GROUND_SLICK;
		} else {
			friction = PM_FRICT_GROUND;
		}
	} else {
		friction = PM_FRICT_AIR;
	}

	friction += PM_FRICT_WATER * ent->locals.water_level;

	vec_t scale = MAX(0.0, speed - (friction * friction * gi.frame_seconds)) / speed;

	VectorScale(ent->locals.velocity, scale, ent->locals.velocity);
}

/*
 * @brief A moving object that doesn't obey physics
 */
static void G_Physics_NoClip(g_edict_t *ent) {

	VectorMA(ent->s.angles, gi.frame_seconds, ent->locals.avelocity, ent->s.angles);
	VectorMA(ent->s.origin, gi.frame_seconds, ent->locals.velocity, ent->s.origin);

	gi.LinkEdict(ent);
}

/*
 * @brief Maintain a record of all pushed entities for each move, in case that
 * move needs to be reverted.
 */
typedef struct {
	g_edict_t *ent;
	vec3_t origin;
	vec3_t angles;
	int16_t delta_yaw;
} g_push_t;

static g_push_t g_pushes[MAX_EDICTS], *g_push_p;

/*
 * @brief
 */
static void G_Physics_Push_Impact(g_edict_t *ent) {

	if (g_push_p - g_pushes == MAX_EDICTS)
		gi.Error("MAX_EDICTS\n");

	g_push_p->ent = ent;

	VectorCopy(ent->s.origin, g_push_p->origin);
	VectorCopy(ent->s.angles, g_push_p->angles);

	if (ent->client) {
		g_push_p->delta_yaw = ent->client->ps.pm_state.delta_angles[YAW];
		ent->client->ps.pm_state.flags |= PMF_PUSHED;
	} else {
		g_push_p->delta_yaw = 0;
	}

	g_push_p++;
}

/*
 * @brief When items ride pushers, they rotate along with them. For clients,
 * this requires incrementing their delta angles.
 */
static void G_Physics_Push_Rotate(g_edict_t *self, g_edict_t *ent, vec_t yaw) {

	if (ent->locals.ground_entity == self) {
		if (ent->client) {
			g_client_t *cl = ent->client;

			yaw += UnpackAngle(cl->ps.pm_state.delta_angles[YAW]);

			cl->ps.pm_state.delta_angles[YAW] = PackAngle(yaw);
		} else {
			ent->s.angles[YAW] += yaw;
		}
	}
}

/*
 * @brief
 */
static g_edict_t *G_Physics_Push_Move(g_edict_t *self, vec3_t move, vec3_t amove) {
	vec3_t inverse_amove, forward, right, up;
	int16_t tmp[3];
	int32_t i;

	G_Physics_Push_Impact(self);

	PackVector(move, tmp);
	UnpackVector(tmp, move);

	PackVector(amove, tmp);
	UnpackVector(tmp, amove);

	// move the pusher to it's intended position
	VectorAdd(self->s.origin, move, self->s.origin);
	VectorAdd(self->s.angles, amove, self->s.angles);

	gi.LinkEdict(self);

	// calculate the angle vectors for rotational movement
	VectorNegate(amove, inverse_amove);
	AngleVectors(inverse_amove, forward, right, up);

	// see if any solid entities are inside the final position
	g_edict_t *ent;
	for (ent = g_game.edicts + 1, i = 1; i < ge.num_edicts; i++, ent++) {

		if (!ent->in_use)
			continue;

		if (ent->solid != SOLID_BOX)
			continue;

		if (ent->locals.move_type < MOVE_TYPE_WALK)
			continue;

		// determine we're touching the entity at all
		if (ent->locals.ground_entity != self) {

			if (!BoxIntersect(ent->abs_mins, ent->abs_maxs, self->abs_mins, self->abs_maxs))
				continue;

			if (G_GoodPosition(ent))
				continue;
		}

		// if we are a pusher, or someone is riding us, try to move them
		if ((self->locals.move_type == MOVE_TYPE_PUSH) || (ent->locals.ground_entity == self)) {
			vec3_t translate, rotate, delta;

			G_Physics_Push_Impact(ent);

			// translate the pushed entity
			VectorAdd(ent->s.origin, move, ent->s.origin);

			// then rotate the movement to comply with the pusher's rotation
			VectorSubtract(ent->s.origin, self->s.origin, translate);

			rotate[0] = DotProduct(translate, forward);
			rotate[1] = -DotProduct(translate, right);
			rotate[2] = DotProduct(translate, up);

			VectorSubtract(rotate, translate, delta);

			VectorAdd(ent->s.origin, delta, ent->s.origin);

			// if the move has separated us, finish up by rotating the entity
			if (G_SnapPosition(ent)) {
				G_Physics_Push_Rotate(self, ent, amove[YAW]);
				gi.LinkEdict(ent);
				continue;
			}

			// perhaps the rider has been pushed off by the world, that's okay
			if (ent->locals.ground_entity == self) {
				VectorCopy(g_push_p->origin, ent->s.origin);
				VectorCopy(g_push_p->angles, ent->s.angles);

				// but in this case, we don't rotate
				if (G_SnapPosition(ent)) {
					gi.LinkEdict(ent);
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

		g_push_t *p;
		for (p = g_push_p - 1; p >= g_pushes; p--) {

			VectorCopy(p->origin, p->ent->s.origin);
			VectorCopy(p->angles, p->ent->s.angles);

			if (p->ent->client) {
				p->ent->client->ps.pm_state.delta_angles[YAW] = p->delta_yaw;
				p->ent->client->ps.pm_state.flags &= ~PMF_PUSHED;
			}

			gi.LinkEdict(p->ent);
		}

		return ent;
	}

	// the move was successful, so touch triggers and water
	g_push_t *p;
	for (p = g_push_p - 1; p >= g_pushes; p--) {
		if (p->ent->in_use) {
			if (p->ent->solid == SOLID_BOX || p->ent->solid == SOLID_MISSILE)
				G_TouchTriggers(p->ent);
			G_TouchWater(p->ent);
		}
	}

	return NULL;
}

/*
 * @brief For G_MOVE_TYPE_PUSH, push all box entities intersected while moving.
 * Generally speaking, only inline BSP models are pushers.
 */
static void G_Physics_Push(g_edict_t *ent) {
	vec3_t move, amove;
	g_edict_t *part, *obstacle = NULL;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->locals.flags & FL_TEAM_SLAVE)
		return;

	// reset the pushed array
	g_push_p = g_pushes;

	// make sure all team slaves can move before committing any moves
	for (part = ent; part; part = part->locals.team_chain) {
		if (!VectorCompare(part->locals.velocity, vec3_origin) || !VectorCompare(
				part->locals.avelocity, vec3_origin)) { // object is moving

			VectorScale(part->locals.velocity, gi.frame_seconds, move);
			VectorScale(part->locals.avelocity, gi.frame_seconds, amove);

			if ((obstacle = G_Physics_Push_Move(part, move, amove)))
				break; // move was blocked
		}
	}

	if (obstacle) { // blocked, let's try again next frame
		for (part = ent; part; part = part->locals.team_chain) {
			if (part->locals.next_think)
				part->locals.next_think += gi.frame_millis;
		}
	} else { // the move succeeded, so call all think functions
		for (part = ent; part; part = part->locals.team_chain) {
			G_RunThink(part);
		}
	}
}

/*
 * @brief Attempt to move through the world, interacting with triggers and
 * solids along the way. It's possible to be freed through impacting other
 * objects.
 */
static cm_trace_t G_Physics_Fly_Move(g_edict_t *ent) {
	vec3_t start, end;
	cm_trace_t trace;

	VectorCopy(ent->s.origin, start);

	VectorMA(ent->s.origin, gi.frame_seconds, ent->locals.velocity, end);

	VectorMA(ent->s.angles, gi.frame_seconds, ent->locals.avelocity, ent->s.angles);

	const int32_t mask = ent->locals.clip_mask ? ent->locals.clip_mask : MASK_SOLID;

	retry: trace = gi.Trace(start, end, ent->mins, ent->maxs, ent, mask);

	VectorCopy(trace.end, ent->s.origin);
	gi.LinkEdict(ent);

	if (trace.ent && trace.plane.num != ent->locals.ground_plane.num) {
		G_RunTouch(ent, &trace);

		// if we kill the other entity, retry as if it were never there
		if (ent->in_use && !trace.ent->in_use) {
			goto retry;
		}
	}

	// the move was successful, so touch triggers and water
	if (ent->in_use) {
		if (ent->solid == SOLID_BOX || ent->solid == SOLID_MISSILE)
			G_TouchTriggers(ent);
		G_TouchWater(ent);
	}

	return trace;
}

/*
 * @brief Fly through the world, interacting with other solids.
 */
static void G_Physics_Fly(g_edict_t *ent) {

	cm_trace_t trace = G_Physics_Fly_Move(ent);

	if (!ent->in_use)
		return;

	if (trace.fraction < 1.0)
		G_ClipVelocity(ent->locals.velocity, trace.plane.normal, ent->locals.velocity, 1.0);
}

/*
 * @brief Toss, bounce, and fly movement. When on ground, do nothing.
 */
static void G_Physics_Toss(g_edict_t *ent) {

	G_Gravity(ent);

	G_Friction(ent);

	cm_trace_t trace = G_Physics_Fly_Move(ent);

	if (!ent->in_use)
		return;

	if (trace.fraction < 1.0) {
		G_ClipVelocity(ent->locals.velocity, trace.plane.normal, ent->locals.velocity, 1.3);

		// if it was a floor, bounce or come to rest
		if (trace.plane.normal[2] >= PM_STEP_NORMAL) {

			const vec_t stop = g_level.gravity * gi.frame_seconds;
			if (ent->locals.velocity[2] <= stop) { // come to a stop and set ground

				if (ent->locals.ground_entity != trace.ent) {
					// gi.Debug("%s to rest on %s\n", etos(ent), etos(trace.ent));

					ent->locals.ground_entity = trace.ent;
					ent->locals.ground_plane = trace.plane;
					ent->locals.ground_surface = trace.surface;
				}

				VectorClear(ent->locals.velocity);
				VectorClear(ent->locals.avelocity);
			}
		} else {
			ent->locals.ground_entity = NULL;
		}
	}
}

/*
 * @brief Dispatches thinking and physics routine for the specified entity.
 */
void G_RunEntity(g_edict_t *ent) {

	// only team masters are run directly
	if (ent->locals.flags & FL_TEAM_SLAVE)
		return;

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
		case MOVE_TYPE_TOSS:
			G_Physics_Toss(ent);
			break;
		default:
			gi.Error("Bad move type %i\n", ent->locals.move_type);
			break;
	}

	// move all team members to the new origin
	g_edict_t *e = ent->locals.team_chain;
	while (e) {
		VectorCopy(ent->s.origin, e->s.origin);
		gi.LinkEdict(e);
		e = e->locals.team_chain;
	}

	// update BSP sub-model animations based on move state
	if (ent->solid == SOLID_BSP) {
		ent->s.animation1 = ent->locals.move_info.state;
	}
}
