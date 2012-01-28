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

#include "g_local.h"

/*
 * pushmove objects do not obey gravity, and do not interact with
 * each other or trigger fields, but block normal movement and push
 * normal objects when they move.
 *
 * onground is set for toss objects when they come to a complete
 * rest.  it is set for steping or walking objects
 *
 * doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
 * bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
 * crates are SOLID_BBOX and MOVETYPE_TOSS
 *
 * solid_edge items only clip against bsp models.
 */

/*
 * G_TestEntityPosition
 */
static g_edict_t *G_TestEntityPosition(g_edict_t *ent) {
	c_trace_t trace;
	int mask;

	if (ent->clip_mask)
		mask = ent->clip_mask;
	else
		mask = MASK_SOLID;
	trace = gi.Trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent,
			mask);

	if (trace.start_solid)
		return g_game.edicts;

	return NULL;
}

#define MAX_VELOCITY 2500

/*
 * G_ClampVelocity
 */
static void G_ClampVelocity(g_edict_t *ent) {
	int i;

	// bound velocity
	for (i = 0; i < 3; i++) {
		if (ent->velocity[i] > MAX_VELOCITY)
			ent->velocity[i] = MAX_VELOCITY;
		else if (ent->velocity[i] < -MAX_VELOCITY)
			ent->velocity[i] = -MAX_VELOCITY;
	}
}

/*
 * G_RunThink
 *
 * Runs thinking code for this frame if necessary
 */
static boolean_t G_RunThink(g_edict_t *ent) {
	float think_time;

	think_time = ent->next_think;

	if (think_time <= 0)
		return true;

	if (think_time > g_level.time + 0.001)
		return true;

	ent->next_think = 0;

	if (!ent->think)
		gi.Error("G_RunThink: No think function for ent.");

	ent->think(ent);

	return false;
}

/*
 * G_Impact
 *
 * Two entities have touched, so run their touch functions
 */
static void G_Impact(g_edict_t *e1, c_trace_t *trace) {
	g_edict_t *e2;

	e2 = trace->ent;

	if (e1->touch && e1->solid != SOLID_NOT)
		e1->touch(e1, e2, &trace->plane, trace->surface);

	if (e2->touch && e2->solid != SOLID_NOT)
		e2->touch(e2, e1, NULL, NULL);
}

#define STOP_EPSILON	0.1

/*
 * G_ClipVelocity
 *
 * Slide off of the impacting object
 * returns the blocked flags (1 = floor, 2 = step / wall)
 */
static int G_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out,
		float overbounce) {
	float backoff;
	float change;
	int i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= 1; // floor
	if (!normal[2])
		blocked |= 2; // step

	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0.0;
	}

	return blocked;
}

/*
 * G_AddGravity
 */
static void G_AddGravity(g_edict_t *ent) {
	float g = g_level.gravity;

	if (ent->water_level)
		g *= 0.5;

	ent->velocity[2] -= ent->gravity * g * gi.server_frame;
}

/*
 *
 * PUSHMOVE
 *
 */

/*
 * G_PushEntity
 *
 * Does not change the entity's velocity at all
 */
c_trace_t G_PushEntity(g_edict_t *ent, vec3_t push) {
	c_trace_t trace;
	vec3_t start;
	vec3_t end;
	int mask;

	VectorCopy(ent->s.origin, start);
	VectorAdd(start, push, end);

	retry: if (ent->clip_mask)
		mask = ent->clip_mask;
	else
		mask = MASK_SOLID;

	trace = gi.Trace(start, ent->mins, ent->maxs, end, ent, mask);

	VectorCopy(trace.end, ent->s.origin);
	gi.LinkEntity(ent);

	if (trace.fraction != 1.0) {
		G_Impact(ent, &trace);

		// if the pushed entity went away and the pusher is still there
		if (!trace.ent->in_use && ent->in_use) {
			// move the pusher back and try again
			VectorCopy(start, ent->s.origin);
			gi.LinkEntity(ent);
			goto retry;
		}
	}

	if (ent->in_use && ent->client && ent->health > 0)
		G_TouchTriggers(ent);

	return trace;
}

typedef struct {
	g_edict_t *ent;
	vec3_t origin;
	vec3_t angles;
	float deltayaw;
} pushed_t;

pushed_t pushed[MAX_EDICTS], *pushed_p;

g_edict_t *obstacle;

/*
 * G_Push
 *
 * Objects need to be moved back on a failed push,
 * otherwise riders would continue to slide.
 */
static boolean_t G_Push(g_edict_t *pusher, vec3_t move, vec3_t amove) {
	int i, e;
	g_edict_t *check, *block;
	vec3_t mins, maxs;
	pushed_t *p;
	vec3_t org, org2, move2, forward, right, up;

	// clamp the move to 1/8 units, so the position will
	// be accurate for client side prediction
	for (i = 0; i < 3; i++) {
		float temp;
		temp = move[i] * 8.0;
		if (temp > 0.0)
			temp += 0.5;
		else
			temp -= 0.5;
		move[i] = 0.125 * (int) temp;
	}

	// find the bounding box
	for (i = 0; i < 3; i++) {
		mins[i] = pusher->abs_mins[i] + move[i];
		maxs[i] = pusher->abs_maxs[i] + move[i];
	}

	// we need this for pushing things later
	VectorSubtract(vec3_origin, amove, org);
	AngleVectors(org, forward, right, up);

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy(pusher->s.origin, pushed_p->origin);
	VectorCopy(pusher->s.angles, pushed_p->angles);
	if (pusher->client)
		pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
	pushed_p++;

	// move the pusher to it's final position
	VectorAdd(pusher->s.origin, move, pusher->s.origin);
	VectorAdd(pusher->s.angles, amove, pusher->s.angles);
	gi.LinkEntity(pusher);

	// see if any solid entities are inside the final position
	check = g_game.edicts + 1;
	for (e = 1; e < ge.num_edicts; e++, check++) {

		if (!check->in_use)
			continue;

		if (check->move_type == MOVE_TYPE_PUSH || check->move_type
				== MOVE_TYPE_STOP || check->move_type == MOVE_TYPE_NONE
				|| check->move_type == MOVE_TYPE_NO_CLIP)
			continue;

		if (!check->area.prev)
			continue; // not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->ground_entity != pusher) {

			// do not push entities which are beside us
			if (check->item)
				continue;

			// see if the ent needs to be tested
			if (check->abs_mins[0] >= maxs[0] || check->abs_mins[1] >= maxs[1]
					|| check->abs_mins[2] >= maxs[2] || check->abs_maxs[0]
					<= mins[0] || check->abs_maxs[1] <= mins[1]
					|| check->abs_maxs[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!G_TestEntityPosition(check))
				continue;
		}

		if ((pusher->move_type == MOVE_TYPE_PUSH) || (check->ground_entity
				== pusher)) {
			// move this entity
			pushed_p->ent = check;
			VectorCopy(check->s.origin, pushed_p->origin);
			VectorCopy(check->s.angles, pushed_p->angles);
			pushed_p++;

			// try moving the contacted entity
			VectorAdd(check->s.origin, move, check->s.origin);
			if (check->client) { // disable stair prediction
				check->client->ps.pmove.pm_flags |= PMF_PUSHED;
				check->client->ps.pmove.delta_angles[YAW] += amove[YAW];
			}

			// figure movement due to the pusher's move
			VectorSubtract(check->s.origin, pusher->s.origin, org);
			org2[0] = DotProduct(org, forward);
			org2[1] = -DotProduct(org, right);
			org2[2] = DotProduct(org, up);
			VectorSubtract(org2, org, move2);
			VectorAdd(check->s.origin, move2, check->s.origin);

			// may have pushed them off an edge
			if (check->ground_entity != pusher)
				check->ground_entity = NULL;

			block = G_TestEntityPosition(check);
			if (!block) { // pushed okay
				gi.LinkEntity(check);
				continue;
			}

			// if it is okay to leave in the old position, do it
			// this is only relevant for riding entities, not pushed
			// FIXME: this doesn't account for rotation
			VectorSubtract(check->s.origin, move, check->s.origin);
			block = G_TestEntityPosition(check);
			if (!block) {
				pushed_p--;
				continue;
			}
		}

		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p = pushed_p - 1; p >= pushed; p--) {
			VectorCopy(p->origin, p->ent->s.origin);
			VectorCopy(p->angles, p->ent->s.angles);
			if (p->ent->client) {
				p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
			}
			gi.LinkEntity(p->ent);
		}
		return false;
	}

	// FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for (p = pushed_p - 1; p >= pushed; p--) {
		if (p->ent->in_use && p->ent->client && p->ent->health > 0)
			G_TouchTriggers(p->ent);
	}

	return true;
}

/*
 * G_Physics_Pusher
 *
 * Bmodel objects don't interact with each other, but push all box objects
 */
static void G_Physics_Pusher(g_edict_t *ent) {
	vec3_t move, amove;
	g_edict_t *part, *mv;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAM_SLAVE)
		return;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	// retry:
	pushed_p = pushed;
	for (part = ent; part; part = part->team_chain) {
		if (part->velocity[0] || part->velocity[1] || part->velocity[2]
				|| part->avelocity[0] || part->avelocity[1]
				|| part->avelocity[2]) { // object is moving
			VectorScale(part->velocity, gi.server_frame, move);
			VectorScale(part->avelocity, gi.server_frame, amove);

			if (!G_Push(part, move, amove))
				break; // move was blocked
		}
	}

	if (pushed_p > &pushed[MAX_EDICTS])
		gi.Error("G_Physics_Pusher: MAX_EDICTS exceeded.");

	if (part) {
		// the move failed, bump all next_think times and back out moves
		for (mv = ent; mv; mv = mv->team_chain) {
			if (mv->next_think > 0)
				mv->next_think += gi.server_frame;
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (part->blocked)
			part->blocked(part, obstacle);

	} else {
		// the move succeeded, so call all think functions
		for (part = ent; part; part = part->team_chain) {
			G_RunThink(part);
		}
	}
}

/*
 * G_Physics_None
 *
 * Non moving objects can only think
 */
static void G_Physics_None(g_edict_t *ent) {
	// regular thinking
	G_RunThink(ent);
}

/*
 * G_Physics_Noclip
 *
 * A moving object that doesn't obey physics
 */
static void G_Physics_Noclip(g_edict_t *ent) {

	if (!G_RunThink(ent))
		return;

	VectorMA(ent->s.angles, gi.server_frame, ent->avelocity, ent->s.angles);
	VectorMA(ent->s.origin, gi.server_frame, ent->velocity, ent->s.origin);

	gi.LinkEntity(ent);
}

/*
 * G_Physics_Toss
 *
 * Toss, bounce, and fly movement.  When on ground, do nothing.
 */
static void G_Physics_Toss(g_edict_t *ent) {
	c_trace_t trace;
	vec3_t org, move;
	g_edict_t *slave;
	boolean_t was_in_water;
	boolean_t is_in_water;

	// regular thinking
	G_RunThink(ent);

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAM_SLAVE)
		return;

	// check for the ground entity going away
	if (ent->ground_entity) {
		if (!ent->ground_entity->in_use)
			ent->ground_entity = NULL;
		else if (ent->velocity[2] > ent->ground_entity->velocity[2] + 0.1)
			ent->ground_entity = NULL;
		else
			return;
	}

	// if on ground, or intentionally floating, return without moving
	if (ent->ground_entity || (ent->item && (ent->spawn_flags & 4)))
		return;

	// enforce max velocity values
	G_ClampVelocity(ent);

	// add gravity
	if (ent->move_type != MOVE_TYPE_FLY)
		G_AddGravity(ent);

	// move angles
	VectorMA(ent->s.angles, gi.server_frame, ent->avelocity, ent->s.angles);

	// move origin
	VectorCopy(ent->s.origin, org);
	VectorScale(ent->velocity, gi.server_frame, move);

	// push through the world, interacting with triggers and other ents
	trace = G_PushEntity(ent, move);

	if (!ent->in_use)
		return;

	if (trace.fraction < 1.0) { // move was blocked

		// if it was a floor, we might bounce or come to rest
		if (G_ClipVelocity(ent->velocity, trace.plane.normal, ent->velocity,
				1.3) == 1) {

			VectorSubtract(ent->s.origin, org, move);

			// if we're approaching a stop, clear our velocity and set ground
			if (VectorLength(move) < STOP_EPSILON) {

				VectorClear(ent->velocity);

				ent->ground_entity = trace.ent;
				ent->ground_entity_link_count = trace.ent->link_count;
			} else {
				// bounce and slide along the floor
				float bounce, speed = VectorLength(ent->velocity);
				bounce = sqrt(speed);

				if (ent->velocity[2] < bounce)
					ent->velocity[2] = bounce;
			}
		}

		// all impacts reduce velocity and angular velocity
		VectorScale(ent->velocity, 0.9, ent->velocity);
		VectorScale(ent->avelocity, 0.9, ent->avelocity);
	}

	// check for water transition
	was_in_water = (ent->water_type & MASK_WATER);
	ent->water_type = gi.PointContents(ent->s.origin);
	is_in_water = ent->water_type & MASK_WATER;

	if (is_in_water)
		ent->water_level = 1;
	else
		ent->water_level = 0;

	if (!was_in_water && is_in_water) {
		gi.PositionedSound(ent->s.origin, g_game.edicts,
				gi.SoundIndex("world/water_in"), ATTN_NORM);
		VectorScale(ent->velocity, 0.5, ent->velocity);
	} else if (was_in_water && !is_in_water)
		gi.PositionedSound(ent->s.origin, g_game.edicts,
				gi.SoundIndex("world/water_out"), ATTN_NORM);

	// move teamslaves
	for (slave = ent->team_chain; slave; slave = slave->team_chain) {
		VectorCopy(ent->s.origin, slave->s.origin);
		gi.LinkEntity(slave);
	}
}

/*
 * G_RunEntity
 */
void G_RunEntity(g_edict_t *ent) {

	if (ent->pre_think)
		ent->pre_think(ent);

	switch ((int) ent->move_type) {
	case MOVE_TYPE_PUSH:
	case MOVE_TYPE_STOP:
		G_Physics_Pusher(ent);
		break;
	case MOVE_TYPE_NONE:
		G_Physics_None(ent);
		break;
	case MOVE_TYPE_NO_CLIP:
		G_Physics_Noclip(ent);
		break;
	case MOVE_TYPE_FLY:
	case MOVE_TYPE_TOSS:
		G_Physics_Toss(ent);
		break;
	default:
		gi.Error("G_RunEntity: Bad move type %i.", (int) ent->move_type);
		break;
	}
}
