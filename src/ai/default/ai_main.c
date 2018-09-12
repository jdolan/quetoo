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

#include "ai_local.h"
#include "game/default/bg_pmove.h"
#include "game/default/g_ai.h"

/**
 * @brief AI imports.
 */
ai_import_t aim;

/**
 * @brief AI exports.
 */
ai_export_t aix;

ai_level_t ai_level;

ai_entity_data_t ai_entity_data;
ai_client_data_t ai_client_data;

cvar_t *sv_max_clients;
cvar_t *ai_passive;

/**
 * @brief Ptr to AI locals that are hooked to bot entities.
 */
static ai_locals_t *ai_locals;

/**
 * @brief Get the locals for the specified bot entity.
 */
ai_locals_t *Ai_GetLocals(const g_entity_t *ent) {
	return ai_locals + (ent->s.number - 1);
}

/**
 * @brief
 */
static _Bool Ai_CanSee(const g_entity_t *self, const g_entity_t *other) {

	// see if we're even facing the object
	ai_locals_t *ai = Ai_GetLocals(self);

	vec3_t dir;
	VectorSubtract(other->s.origin, ai->eye_origin, dir);
	VectorNormalize(dir);

	vec_t dot = DotProduct(ai->aim_forward, dir);

	if (dot < 0.5) {
		return false;
	}

	cm_trace_t tr = aim.gi->Trace(ai->eye_origin, other->s.origin, vec3_origin, vec3_origin, self, MASK_CLIP_PROJECTILE);

	if (!BoxIntersect(tr.end, tr.end, other->abs_mins, other->abs_maxs)) {
		return false; // something was in the way of our trace
	}

	// got it!
	return true;
}

/**
 * @brief
 */
static _Bool Ai_CanTarget(const g_entity_t *self, const g_entity_t *other) {

	if (other == self || !other->in_use || other->solid == SOLID_DEAD || other->solid == SOLID_NOT ||
	        (other->sv_flags & SVF_NO_CLIENT) || (other->client && aim.OnSameTeam(self, other))) {
		return false;
	}

	return Ai_CanSee(self, other);
}

/**
 * @brief Executes a console command as if the bot typed it.
 */
static void Ai_Command(g_entity_t *self, const char *command) {

	aim.gi->TokenizeString(command);
	aim.ge->ClientCommand(self);
}

/**
 * @brief The max distance we'll try to hunt an item at.
 */
#define AI_MAX_ITEM_DISTANCE 512

/**
 * @brief
 */
typedef struct {
	const g_entity_t *entity;
	const g_item_t *item;
	vec_t weight;
} ai_item_pick_t;

/**
 * @brief
 */
static int32_t Ai_ItemPick_Compare(const void *a, const void *b) {

	const ai_item_pick_t *w0 = (const ai_item_pick_t *) a;
	const ai_item_pick_t *w1 = (const ai_item_pick_t *) b;

	return Sign(w1->weight - w0->weight);
}

#define AI_ITEM_UNREACHABLE -1.0

/**
 * @brief
 */
static vec_t Ai_ItemReachable(const g_entity_t *self, const g_entity_t *other) {

	vec3_t line;
	VectorSubtract(self->s.origin, other->s.origin, line);

	if (fabs(line[2]) >= PM_STEP_HEIGHT) {
		return AI_ITEM_UNREACHABLE;
	}

	const vec_t distance = VectorLength(line);

	if (distance >= AI_MAX_ITEM_DISTANCE) {
		return AI_ITEM_UNREACHABLE;
	}

	// if the distance is over a chasm, let's see if we can even reach it
	if (distance >= 32.0) {

		vec3_t fall_start;
		VectorAdd(self->s.origin, other->s.origin, fall_start);
		VectorScale(fall_start, 0.5, fall_start);

		vec3_t fall_end;
		VectorCopy(fall_start, fall_end);
		fall_end[2] -= PM_STEP_HEIGHT * 2.0;

		cm_trace_t tr = aim.gi->Trace(fall_start, fall_end, vec3_origin, vec3_origin, NULL, CONTENTS_SOLID);

		if (tr.start_solid || tr.all_solid || tr.fraction == 1.0) {
			return AI_ITEM_UNREACHABLE;
		}
	}

	return distance;
}

/**
 * @brief
 */
static void Ai_ResetWander(const g_entity_t *self, const vec3_t where_to) {
	ai_locals_t *ai = Ai_GetLocals(self);
	vec3_t dir;

	VectorSubtract(where_to, self->s.origin, dir);
	VectorNormalize(dir);
	VectorAngles(dir, dir);

	ai->wander_angle = dir[1];
}

/**
 * @brief Seek for items if we're not doing anything better.
 */
static uint32_t Ai_FuncGoal_FindItems(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return QUETOO_TICK_MILLIS;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	// see if we're already hunting
	if (ai->move_target.type == AI_GOAL_ENEMY) {
		return QUETOO_TICK_MILLIS * 5;
	}

	if (ai->move_target.type == AI_GOAL_ITEM) {

		// check to see if the item has gone out of our line of sight
		if (!Ai_GoalHasEntity(&ai->move_target, ai->move_target.ent) || // item picked up and changed into something else
				!Ai_CanTarget(self, ai->move_target.ent) ||
				!Ai_CanPickupItem(self, ai->move_target.ent) ||
		        Ai_ItemReachable(self, ai->move_target.ent) == AI_ITEM_UNREACHABLE) {

			Ai_ResetWander(self, ai->move_target.ent->s.origin);

			if (ai->aim_target.ent == ai->move_target.ent) {
				Ai_ClearGoal(&ai->aim_target);
			}

			Ai_ClearGoal(&ai->move_target);
		} else if (ai->aim_target.type <= AI_GOAL_GHOST) { // aim towards item so it doesn't look like we're a feckin' cheater
			Ai_CopyGoal(&ai->move_target, &ai->aim_target);
		}
	}

	// still have an item
	if (ai->move_target.type == AI_GOAL_ITEM) {
		return QUETOO_TICK_MILLIS * 3;
	}

	// if we're aiming for an active target, don't look for new items
	if (ai->aim_target.type == AI_GOAL_ENEMY) {
		return QUETOO_TICK_MILLIS * 3;
	}

	// we lost our enemy, start looking for a new one
	GArray *items_visible = g_array_new(false, false, sizeof(ai_item_pick_t));

	for (int32_t i = sv_max_clients->integer + 1; i < MAX_ENTITIES; i++) {

		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (!ent->in_use) {
			continue;
		}

		if (ent->s.solid != SOLID_TRIGGER) {
			continue;
		}

		const g_item_t *item = ENTITY_DATA(ent, item);

		if (!item) {
			continue;
		}

		// most likely an item!
		vec_t distance;

		if (!Ai_CanTarget(self, ent) ||
				!Ai_CanPickupItem(self, ent) ||
		        (distance = Ai_ItemReachable(self, ent)) == AI_ITEM_UNREACHABLE) {
			continue;
		}

		g_array_append_vals(items_visible, &(const ai_item_pick_t) {
			.entity = ent,
			 .item = item,
			  .weight = (AI_MAX_ITEM_DISTANCE - distance) * ITEM_DATA(item, priority)
		}, 1);
	}

	// found one, set it up
	if (items_visible->len) {

		if (items_visible->len > 1) {
			g_array_sort(items_visible, Ai_ItemPick_Compare);
		}

		Ai_SetEntityGoal(&ai->move_target, AI_GOAL_ITEM, 1.0, g_array_index(items_visible, ai_item_pick_t, 0).entity);

		g_array_free(items_visible, true);

		// re-run with the new target
		return Ai_FuncGoal_FindItems(self, cmd);
	}

	g_array_free(items_visible, true);

	return QUETOO_TICK_MILLIS * 5;
}

/**
 * @brief Range constants.
 */
typedef enum {
	RANGE_MELEE = 32,
	RANGE_SHORT = 128,
	RANGE_MED = 512,
	RANGE_LONG = 1024
} ai_range_t;

/**
 * @brief
 */
static ai_range_t Ai_GetRange(const vec_t distance) {
	if (distance < RANGE_MELEE) {
		return RANGE_MELEE;
	} else if (distance < RANGE_SHORT) {
		return RANGE_SHORT;
	} else if (distance < RANGE_MED) {
		return RANGE_MED;
	}

	return RANGE_LONG;
}

/**
 * @brief Picks a weapon for the AI based on its target
 */
static void Ai_PickBestWeapon(g_entity_t *self) {
	ai_locals_t *ai = Ai_GetLocals(self);

	ai->weapon_check_time = ai_level.time + 500; // don't try again for a bit

	if (ai->aim_target.type != AI_GOAL_ENEMY) {
		return;
	}

	const vec_t targ_dist = VectorDistance(self->s.origin, ai->aim_target.ent->s.origin);
	const ai_range_t targ_range = Ai_GetRange(targ_dist);

	ai_item_pick_t weapons[ai_num_weapons];
	uint16_t num_weapons = 0;

	const int16_t *inventory = &CLIENT_DATA(self->client, inventory);

	for (uint16_t i = 0; i < ai_num_items; i++) {
		const g_item_t *item = ai_items[i];

		if (ITEM_DATA(item, type) != ITEM_WEAPON) { // not weapon
			continue;
		}

		if (!inventory[ITEM_DATA(item, index)]) { // not in stock
			continue;
		}

		const g_item_t *ammo = ITEM_DATA(item, ammo);
		if (ammo) {
			if (inventory[ITEM_DATA(ammo, index)] < ITEM_DATA(item, quantity)) {
				continue;
			}
		}

		// calculate weight, start with base weapon priority
		vec_t weight = ITEM_DATA(item, priority);

		switch (targ_range) { // range bonus
			case RANGE_MELEE:
			case RANGE_SHORT:
				if (ITEM_DATA(item, flags) & WF_SHORT_RANGE) {
					weight *= 2.5;
				} else {
					weight /= 2.5;
				}
				break;
			case RANGE_MED:
				if (ITEM_DATA(item, flags) & WF_MED_RANGE) {
					weight *= 2.5;
				} else {
					weight /= 2.5;
				}
				break;
			case RANGE_LONG:
				if (ITEM_DATA(item, flags) & WF_LONG_RANGE) {
					weight *= 2.5;
				} else {
					weight /= 2.5;
				}
				break;
		}

		if ((ENTITY_DATA(ai->aim_target.ent, health) < 25) &&
		        (ITEM_DATA(item, flags) & WF_EXPLOSIVE)) { // bonus for explosive at low enemy health
			weight *= 1.5;
		}

		// additional penalty for long range + projectile unless explicitly long range
		if ((ITEM_DATA(item, flags) & WF_PROJECTILE) &&
		        !(ITEM_DATA(item, flags) & WF_LONG_RANGE)) {
			weight /= 2.0;
		}

		// penalty for explosive weapons at short range
		if ((ITEM_DATA(item, flags) & WF_EXPLOSIVE) &&
		        targ_range <= RANGE_SHORT) {
			weight /= 2.0;
		}

		// penalty for explosive weapons at low self health
		if ((ENTITY_DATA(self, health) < 25) &&
		        (ITEM_DATA(item, flags) & WF_EXPLOSIVE)) {
			weight /= 2.0;
		}

		weight *= ITEM_DATA(item, priority); // FIXME: Isn't this redundant?

		weapons[num_weapons++] = (ai_item_pick_t) {
			.item = item,
			 .weight = weight
		};
	}

	if (num_weapons <= 1) { // if we only have 1 here, we're already using it
		return;
	}

	qsort(weapons, num_weapons, sizeof(ai_item_pick_t), Ai_ItemPick_Compare);

	const ai_item_pick_t *best_weapon = &weapons[0];

	if (CLIENT_DATA(self->client, weapon) == best_weapon->item) {
		return;
	}

	Ai_Command(self, va("use %s", ITEM_DATA(best_weapon->item, name)));
	ai->weapon_check_time = ai_level.time + 3000; // don't try again for a bit
}

/**
 * @brief Calculate a priority for the specified target.
 */
static vec_t Ai_EnemyPriority(const g_entity_t *self, const g_entity_t *target, const _Bool visible) {

	// TODO: all of this function. Enemies with more powerful weapons need a higher weight.
	// Enemies with lower health need a higher weight. Enemies carrying flags need an even higher weight.

	return 10.0;
}

/**
 * @brief Funcgoal that controls the AI's lust for blood
 */
static uint32_t Ai_FuncGoal_Hunt(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return QUETOO_TICK_MILLIS;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	if (ai_passive->integer) {

		if (ai->aim_target.type == AI_GOAL_ENEMY) {
			Ai_ClearGoal(&ai->aim_target);
		}

		if (ai->move_target.type == AI_GOAL_ENEMY) {
			Ai_ClearGoal(&ai->move_target);
		}

		return QUETOO_TICK_MILLIS;
	}

	// see if we're already hunting
	if (ai->aim_target.type == AI_GOAL_ENEMY) {

		// check to see if the enemy has gone out of our line of sight
		if (!Ai_CanTarget(self, ai->aim_target.ent)) {

			// enemy went out of our LOS, aim at where they were for a while
			if (ai->aim_target.ent->solid != SOLID_DEAD && !(ai->aim_target.ent->sv_flags & SVF_NO_CLIENT)) {
				Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_GHOST, Ai_EnemyPriority(self, ai->aim_target.ent, false), ai->aim_target.ent);
				VectorCopy(ai->aim_target.ent->s.origin, ai->ghost_position);
				Ai_ResetWander(self, ai->ghost_position);
			} else {
				Ai_ClearGoal(&ai->aim_target);
			}

			Ai_ClearGoal(&ai->move_target);
		}

		// TODO: we should change targets here based on priority.
	}

	// if we're wandering/not looking for something, move towards the target
	if (ai->move_target.type <= AI_GOAL_NAV) {

		// TODO: if we have proper navigation, then we can remove this if statement.
		// until then, we only try to move towards actual enemies and not ghosts.
		if (ai->aim_target.type == AI_GOAL_ENEMY) {
			Ai_CopyGoal(&ai->aim_target, &ai->move_target);
		}
	}

	// still have an enemy
	if (ai->aim_target.type == AI_GOAL_ENEMY) {

		if (ai->weapon_check_time < ai_level.time) { // check for a new weapon every once in a while
			Ai_PickBestWeapon(self);
		}

		return QUETOO_TICK_MILLIS * 3;
	}

	// we lost our enemy, start looking for a new one
	g_entity_t *player = NULL;

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (Ai_CanTarget(self, ent)) {

			player = ent;
			break;
		}
	}

	// found one, set it up
	if (player) {

		Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_ENEMY, Ai_EnemyPriority(self, player, true), player);

		Ai_PickBestWeapon(self);

		// re-run hunt with the new target
		return Ai_FuncGoal_Hunt(self, cmd);
	}

	return QUETOO_TICK_MILLIS * 5;
}

/**
 * @brief Funcgoal that controls the AI's weaponry.
 */
static uint32_t Ai_FuncGoal_Weaponry(g_entity_t *self, pm_cmd_t *cmd) {

	// if we're dead, just keep clicking so we respawn.
	if (self->solid == SOLID_DEAD) {

		if ((ai_level.frame_num % 2) == 0) {
			cmd->buttons = BUTTON_ATTACK;
		}
		return QUETOO_TICK_MILLIS;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	// TODO: pick the best weapon that we can hold here based on
	// our current aim_target & move_target situation

	// we're alive - if we're aiming at an enemy, start-a-firin
	if (ai->aim_target.type == AI_GOAL_ENEMY) {

		// TODO: obviously there are some cases where we may not want to attack
		// so we need to do these here.. also handnades, etc.
		cmd->buttons |= BUTTON_ATTACK;
	}

	return QUETOO_TICK_MILLIS;
}

/**
 * @brief Funcgoal that controls the AI's crouch/jumping while hunting.
 */
static uint32_t Ai_FuncGoal_Acrobatics(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return QUETOO_TICK_MILLIS;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	if (ai->aim_target.type != AI_GOAL_ENEMY) {
		return QUETOO_TICK_MILLIS * 5;
	}

	// do some acrobatics
	if (ENTITY_DATA(self, ground_entity)) {

		if (self->client->ps.pm_state.flags & PMF_DUCKED) {

			if ((Randomr(0, 32)) == 0) { // uncrouch eventually
				cmd->up = 0;
			} else {
				cmd->up = -PM_SPEED_JUMP;
			}
		} else {

			if ((Randomr(0, 32)) == 0) { // randomly crouch
				cmd->up = -PM_SPEED_JUMP;
			} else if ((Randomr(0, 86)) == 0) { // randomly pop, to confuse our enemies
				cmd->up = PM_SPEED_JUMP;
			}
		}
	} else {

		cmd->up = 0;
	}

	return QUETOO_TICK_MILLIS;
}

/**
 * @brief Wander aimlessly, hoping to find something to love.
 */
static void Ai_Wander(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);

	vec3_t forward;
	AngleVectors((const vec3_t) {
		0, ai->wander_angle, 0
	}, forward, NULL, NULL);

	vec3_t end;
	VectorMA(self->s.origin, (self->maxs[0] - self->mins[0]) * 2.0, forward, end);

	cm_trace_t tr = aim.gi->Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

	if (tr.fraction < 1.0) { // hit a wall
		vec_t angle = 45 + Randomf() * 45;

		ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
		ai->wander_angle = ClampAngle(ai->wander_angle);
	}

	vec3_t move_dir;
	VectorSubtract(ai->last_origin, self->s.origin, move_dir);
	move_dir[2] = 0.0;
	vec_t move_len = VectorLength(move_dir);

	if (move_len < (PM_SPEED_RUN * QUETOO_TICK_SECONDS) / 8.0) {
		ai->no_movement_frames++;

		if (ai->no_movement_frames >= QUETOO_TICK_RATE) { // just turn around
			vec_t angle = 45 + Randomf() * 45;

			ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
			ai->wander_angle = ClampAngle(ai->wander_angle);

			ai->no_movement_frames = 0;
		} else if (ai->no_movement_frames >= QUETOO_TICK_RATE / 2.0) { // try a jump first
			cmd->up = PM_SPEED_JUMP;
		}
	} else {
		ai->no_movement_frames = 0;
	}
}

static g_entity_t *ai_current_entity;

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t Ai_ClientMove_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
                                     const vec3_t maxs) {

	const g_entity_t *self = ai_current_entity;

	if (self->solid == SOLID_DEAD) {
		return aim.gi->Trace(start, end, mins, maxs, self, MASK_CLIP_CORPSE);
	} else {
		return aim.gi->Trace(start, end, mins, maxs, self, MASK_CLIP_PLAYER);
	}
}

/**
 * @brief Move towards our current target
 */
static void Ai_MoveToTarget(g_entity_t *self, pm_cmd_t *cmd) {

	ai_locals_t *ai = Ai_GetLocals(self);

	// TODO: node navigation.
	if (ai->move_target.type <= AI_GOAL_NAV) {
		Ai_Wander(self, cmd);
	}

	vec3_t dir, angles, dest;
	switch (ai->move_target.type) {
		default: {
				VectorSet(angles, 0.0, ai->wander_angle, 0.0);
				AngleVectors(angles, dir, NULL, NULL);
				VectorMA(self->s.origin, 1.0, dir, dest);
			}
			break;
		case AI_GOAL_ITEM:
		case AI_GOAL_ENEMY:
			VectorCopy(ai->move_target.ent->s.origin, dest);
			break;
	}

	VectorSubtract(dest, self->s.origin, dir);
	VectorNormalize(dir);

	vec3_t predicted;
	Ai_Predict(self, predicted);
	//printf("%s\n", vtos(predicted));

	VectorAdd(dir, predicted, dir);
	VectorNormalize(dir);

    VectorAngles(dir, angles);

    const vec_t delta_yaw = (&CLIENT_DATA(self->client, angles))[YAW] - angles[YAW];
    AngleVectors((vec3_t) { 0.0, delta_yaw, 0.0 }, dir, NULL, NULL);

    VectorScale(dir, PM_SPEED_RUN, dir);
	VectorScale(predicted, PM_SPEED_RUN, predicted);

    cmd->forward = dir[0];
    cmd->right = dir[1];
	cmd->up = predicted[2];

    if (ENTITY_DATA(self, water_level) >= WATER_WAIST) {
        cmd->up = PM_SPEED_JUMP;
    }

    ai_current_entity = self;

    // predict ahead
    pm_move_t pm;

    memset(&pm, 0, sizeof(pm));
    pm.s = self->client->ps.pm_state;

    VectorCopy(self->s.origin, pm.s.origin);

    /*if (self->client->locals.hook_pull) {

        if (self->client->locals.persistent.hook_style == HOOK_SWING) {
            pm.s.type = PM_HOOK_SWING;
        } else {
            pm.s.type = PM_HOOK_PULL;
        }
    } else {*/
        VectorCopy(&ENTITY_DATA(self, velocity), pm.s.velocity);
    /*}*/

    pm.s.type = PM_NORMAL;

    pm.cmd = *cmd;
    pm.ground_entity = ENTITY_DATA(self, ground_entity);
    //pm.hook_pull_speed = g_hook_pull_speed->value;

    pm.PointContents = aim.gi->PointContents;
    pm.Trace = Ai_ClientMove_Trace;

    pm.Debug = aim.gi->PmDebug_;

    // perform a move; predict a few frames ahead
    for (int32_t i = 0; i < 8; ++i) {
        Pm_Move(&pm);
    }

    if (ENTITY_DATA(self, ground_entity) && !pm.ground_entity) { // predicted ground is gone
        if (ai->move_target.type <= AI_GOAL_NAV) {
            vec_t angle = 45 + Randomf() * 45;

            ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
            ai->wander_angle = ClampAngle(ai->wander_angle); // turn around to miss the cliff
        } else {
            cmd->forward = cmd->right = 0; // stop for now
        }
    }
}

static vec_t AngleMod(const vec_t a) {
	return (360.0 / 65536) * ((int32_t) (a * (65536 / 360.0)) & 65535);
}

static vec_t Ai_CalculateAngle(g_entity_t *self, const vec_t speed, vec_t current, const vec_t ideal) {
	current = AngleMod(current);

	if (current == ideal) {
		return current;
	}

	vec_t move = ideal - current;

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
 * @brief Turn/look towards our current target
 */
static void Ai_TurnToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);
	ai_goal_t *aim_target = &ai->aim_target;

	vec3_t ideal_angles;

	// TODO: node navigation
	if (aim_target->type <= AI_GOAL_NAV) {
		VectorSet(ideal_angles, 0.0, ai->wander_angle, 0.0);
	} else {
		vec3_t aim_direction;

		if (aim_target->type == AI_GOAL_GHOST) {
			VectorSubtract(ai->ghost_position, self->s.origin, aim_direction);
		} else {
			VectorSubtract(aim_target->ent->s.origin, self->s.origin, aim_direction);
		}

		VectorNormalize(aim_direction);
		VectorAngles(aim_direction, ideal_angles);

		// fuzzy angle
		ideal_angles[0] += sin(ai_level.time / 128.0) * 4.3;
		ideal_angles[1] += cos(ai_level.time / 164.0) * 4.0;
	}

	const vec_t *view_angles = &CLIENT_DATA(self->client, angles);

	for (int32_t i = 0; i < 2; ++i) {
		ideal_angles[i] = Ai_CalculateAngle(self, 6.5, view_angles[i], ideal_angles[i]);
	}

	vec3_t delta;
	UnpackAngles(self->client->ps.pm_state.delta_angles, delta);
	VectorSubtract(ideal_angles, delta, ideal_angles);

	PackAngles(ideal_angles, cmd->angles);
}

/**
 * @brief Called every frame for every AI.
 */
static void Ai_Think(g_entity_t *self, pm_cmd_t *cmd) {

	ai_locals_t *ai = Ai_GetLocals(self);

	if (self->solid == SOLID_DEAD) {
		Ai_ClearGoal(&ai->aim_target);
		Ai_ClearGoal(&ai->move_target);
	} else {
		AngleVectors(&CLIENT_DATA(self->client, angles), ai->aim_forward, NULL, NULL);

		for (int32_t i = 0; i < 3; i++) {
			ai->eye_origin[i] = self->s.origin[i] + UnpackAngle(self->client->ps.pm_state.view_offset[i]);
		}
	}

	// run functional goals
	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		ai_funcgoal_t *funcgoal = &ai->funcgoals[i];

		if (!funcgoal->think) {
			continue;
		}

		if (funcgoal->nextthink > ai_level.time) {
			continue;
		}

		uint32_t time = funcgoal->think(self, cmd);

		if (time == AI_GOAL_COMPLETE) {
			Ai_RemoveFuncGoal(self, funcgoal->think);
		} else {
			funcgoal->nextthink = ai_level.time + time;
		}
	}

	if (self->solid != SOLID_DEAD) {
		// FIXME: should these be funcgoals?
		// they'd have to be appended to the end of the list always
		// and we can't really enforce that.
		Ai_TurnToTarget(self, cmd);
		Ai_MoveToTarget(self, cmd);
	}

	VectorCopy(self->s.origin, ai->last_origin);
}

/**
 * @brief Called every time an AI spawns
 */
static void Ai_Spawn(g_entity_t *self) {
	ai_locals_t *ai = Ai_GetLocals(self);
	memset(ai, 0, sizeof(*ai));

	if (self->solid == SOLID_NOT) { // intermission, spectator, etc
		return;
	}

	Ai_AddFuncGoal(self, Ai_FuncGoal_Hunt, 0);
	Ai_AddFuncGoal(self, Ai_FuncGoal_Weaponry, 0);
	Ai_AddFuncGoal(self, Ai_FuncGoal_Acrobatics, 0);
	Ai_AddFuncGoal(self, Ai_FuncGoal_FindItems, 0);
}

/**
 * @brief Called when an AI is first spawned and is ready to go.
 */
static void Ai_Begin(g_entity_t *self) {
}

/**
 * @brief Advance the bot simulation one frame.
 */
static void Ai_Frame(void) {

	ai_level.frame_num++;
	ai_level.time = ai_level.frame_num * QUETOO_TICK_MILLIS;
}

/**
 * @brief Sets up level state for bots.
 */
static void Ai_GameStarted(void) {

	ai_level.gameplay = atoi(aim.gi->GetConfigString(CS_GAMEPLAY));
	ai_level.match = atoi(aim.gi->GetConfigString(CS_MATCH));
	ai_level.ctf = atoi(aim.gi->GetConfigString(CS_CTF));
	ai_level.teams = atoi(aim.gi->GetConfigString(CS_TEAMS));
}

/**
 * @brief Sets up data offsets to local game data
 */
static void Ai_SetDataPointers(ai_entity_data_t *entity, ai_client_data_t *client, ai_item_data_t *item) {

	ai_entity_data = *entity;
	ai_client_data = *client;
	ai_item_data = *item;
}

/**
 * @brief Initializes the AI subsystem.
 */
static void Ai_Init(void) {

	aim.gi->Print("  ^5Ai module initialization...\n");

	const char *s = va("%s %s %s", VERSION, BUILD_HOST, REVISION);
	cvar_t *ai_version = aim.gi->AddCvar("ai_version", s, CVAR_NO_SET, NULL);

	aim.gi->Print("  ^5Version %s\n", ai_version->string);

	sv_max_clients = aim.gi->GetCvar("sv_max_clients");

	ai_passive = aim.gi->AddCvar("ai_passive", "0", 0, "Whether the bots will attack or not.");
	ai_locals = (ai_locals_t *) aim.gi->Malloc(sizeof(ai_locals_t) * sv_max_clients->integer, MEM_TAG_AI);

	Ai_InitItems();
	Ai_InitSkins();
	Ai_InitAnn();

	aim.gi->Print("  ^5Ai module initialized\n");
}

/**
 * @brief Shuts down the AI subsystem
 */
static void Ai_Shutdown(void) {

	aim.gi->Print("  ^5Ai module shutdown...\n");

	Ai_ShutdownItems();
	Ai_ShutdownSkins();
	Ai_ShutdownAnn();

	aim.gi->FreeTag(MEM_TAG_AI);
}

/**
 * @brief Load the AI subsystem.
 */
ai_export_t *Ai_LoadAi(ai_import_t *import) {

	aim = *import;

	aix.api_version = AI_API_VERSION;

	aix.Init = Ai_Init;
	aix.Shutdown = Ai_Shutdown;

	aix.Frame = Ai_Frame;

	aix.GetUserInfo = Ai_GetUserInfo;
	aix.Begin = Ai_Begin;
	aix.Spawn = Ai_Spawn;
	aix.Think = Ai_Think;
	aix.Learn = Ai_Learn;

	aix.GameRestarted = Ai_GameStarted;

	aix.RegisterItem = Ai_RegisterItem;

	aix.SetDataPointers = Ai_SetDataPointers;

	return &aix;
}
