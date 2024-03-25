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

ai_level_t ai_level;

cvar_t *ai_no_target;
cvar_t *ai_node_dev;

/**
 * @brief Ptr to AI locals that are hooked to bot entities.
 */
static ai_locals_t *ai_locals;

/**
 * @brief Get the locals for the specified bot entity.
 */
ai_locals_t *Ai_GetLocals(const g_entity_t *ent) {
	return ai_locals + ent->s.client;
}

/**
 * @brief
 */
static bool Ai_CanSee(const g_entity_t *self, const g_entity_t *other) {

	// see if we're even facing the object
	ai_locals_t *ai = Ai_GetLocals(self);

	const vec3_t dir = Vec3_Normalize(Vec3_Subtract(other->s.origin, ai->eye_origin));

	float dot = Vec3_Dot(ai->aim_forward, dir);

	if (dot < 0.1f) {
		return false;
	}

	cm_trace_t tr = gi.Trace(ai->eye_origin, other->s.origin, Box3_Zero(), self, CONTENTS_MASK_CLIP_PROJECTILE);

	if (tr.ent == other) {
		return true;
	}

	return Box3_ContainsPoint(Box3_Expand(other->abs_bounds, 1.f), tr.end);
}

/**
 * @brief
 */
static inline bool Ai_IsTargetable(const g_entity_t *self, const g_entity_t *other) {

	return !(other == self || !other->in_use || other->solid == SOLID_DEAD || other->solid == SOLID_NOT ||
			(other->sv_flags & SVF_NO_CLIENT) || (other->client && G_OnSameTeam(self, other)));
}

/**
 * @brief
 */
static inline bool Ai_CanTarget(const g_entity_t *self, const g_entity_t *other) {

	return Ai_IsTargetable(self, other) && Ai_CanSee(self, other);
}

/**
 * @brief Executes a console command as if the bot typed it.
 */
static void Ai_Command(g_entity_t *self, const char *command) {

	gi.TokenizeString(command);
	ge.ClientCommand(self);
}

/**
 * @brief The max distance we'll try to hunt an item at.
 */
#define AI_MAX_ITEM_DISTANCE 768.f

/**
 * @brief
 */
typedef struct {
	const g_entity_t *entity;
	const g_item_t *item;
	float weight;
} ai_item_pick_t;

/**
 * @brief
 */
static int32_t Ai_ItemPick_Compare(const void *a, const void *b) {

	const ai_item_pick_t *w0 = (const ai_item_pick_t *) a;
	const ai_item_pick_t *w1 = (const ai_item_pick_t *) b;

	return SignOf(w1->weight - w0->weight);
}

#define AI_ITEM_UNREACHABLE -1.0

/**
 * @brief
 */
static float Ai_ItemReachable(const g_entity_t *self, const g_entity_t *other) {

	const float dist = Vec3_Distance(self->s.origin, other->s.origin);

	if (dist > AI_MAX_ITEM_DISTANCE) {
		return AI_ITEM_UNREACHABLE;
	}

	return dist;
}

static inline void Ai_BackupMainPath(ai_locals_t *ai) {

	if (!ai->backup_move_target.type && ai->move_target.type == AI_GOAL_PATH) {
		Ai_CopyGoal(&ai->move_target, &ai->backup_move_target);
		Ai_Debug("Backing up main path; new temporary path incoming\n");
	}
}

static inline void Ai_RestoreMainPath(const g_entity_t *self, ai_locals_t *ai) {

	if (ai->backup_move_target.type == AI_GOAL_PATH) {
		// generate a new path to the old target, because we might have gotten a bit out
		// of sync with moving to the item
		const ai_node_id_t src = Ai_Node_FindClosest(self->s.origin, 512.f, true, true);
		const ai_node_id_t dest = g_array_index(ai->backup_move_target.path.path, ai_node_id_t, ai->backup_move_target.path.path->len - 1);
		GArray *path = Ai_Node_FindPath(src, dest, Ai_Node_DefaultHeuristic, NULL);

		if (!path) {
			Ai_ClearGoal(&ai->move_target);
		} else {
			Ai_SetPathGoal(self, &ai->move_target, ai->backup_move_target.priority, path, ai->backup_move_target.path.path_target);
			g_array_unref(path);
		}
	} else {
		Ai_ClearGoal(&ai->move_target);
		Ai_Debug("No main path?\n");
	}

	Ai_ClearGoal(&ai->backup_move_target);
	Ai_Debug("Returning to main path\n");

	// no matter what, clear long range func goal so we can try it immediately
	ai->funcgoal_nextthinks[AI_FUNCGOAL_LONGRANGE] = 0;
}

/**
 * @brief Seek for items if we're not doing anything better.
 */
static uint32_t Ai_FuncGoal_FindItems(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return 1;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	// if we got stuck, don't hunt for items for a little bit
	if (ai->reacquire_time > g_level.time) {
		return ai->reacquire_time - g_level.time; 
	}

	// see if we're already hunting something.
	// early exit if we're under water or in air; we have a goal already
	// in that case.
	if (ai->combat_target.type || !self->locals.ground.ent) {
		return 5;
	}
	
	// we're not attacking, so we probably care about items.
	if (ai->move_target.type == AI_GOAL_ENTITY || ai->move_target.type == AI_GOAL_PATH) {
		const g_entity_t *target = (ai->move_target.type == AI_GOAL_ENTITY) ? ai->move_target.entity.ent : ai->move_target.path.path_target;

		// check to see if the thing we are moving to has been taken
		if (target && target->locals.item) {

			if (!Ai_IsTargetable(self, target) ||
				!Ai_CanPickupItem(self, target)) {

				Ai_ClearGoal(&ai->move_target);

				// if we had a backup, return to it now
				if (ai->backup_move_target.type) {

					Ai_RestoreMainPath(self, ai);
				}
			// still a good goal
			} else {
				return 5;
			}
		}
	}

	// we have nothing to do, start looking for a new one
	GArray *items_visible = g_array_new(false, false, sizeof(ai_item_pick_t));

	for (int32_t i = sv_max_clients->integer + 1; i < ge.num_entities; i++) {

		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (!ent->in_use) {
			continue;
		}

		if (ent->s.solid != SOLID_TRIGGER) {
			continue;
		}

		const g_item_t *item = ent->locals.item;

		if (!item) {
			continue;
		}

		if (!gi.inPVS(self->s.origin, ent->s.origin)) {
			continue;
		}

		// we're already pathing to this item, so ignore it in our short range goal finding
		if (ai->move_target.type == AI_GOAL_PATH && ai->move_target.path.path_target == ent && ai->move_target.path.path_target_spawn_id == ent->s.spawn_id) {
			continue;
		}

		// most likely an item!
		float distance;

		if (!Ai_CanTarget(self, ent) ||
			!Ai_CanPickupItem(self, ent) ||
			(distance = Ai_ItemReachable(self, ent)) <= AI_ITEM_UNREACHABLE) {
			continue;
		}

		float weight = (AI_MAX_ITEM_DISTANCE - distance) * item->priority;

		g_array_append_vals(items_visible, &(const ai_item_pick_t) {
			.entity = ent,
			.item = item,
			.weight = weight
		}, 1);
	}

	// found one, set it up
	if (items_visible->len) {

		if (items_visible->len > 1) {
			g_array_sort(items_visible, Ai_ItemPick_Compare);
		}

		for (guint i = 0; i < items_visible->len; i++) {
			const ai_item_pick_t pick = g_array_index(items_visible, ai_item_pick_t, 0);
			const bool found = pick.weight > ai->move_target.priority;

			if (!found) {
				continue;
			}

			bool path_found = false;

			if (pick.entity->locals.node != AI_NODE_INVALID) {
				const ai_node_id_t src = Ai_Node_FindClosest(self->s.origin, 512.f, true, true);
				const ai_node_id_t dest = pick.entity->locals.node;

				if (src != AI_NODE_INVALID) {
					float length;
					GArray *path = Ai_Node_FindPath(src, dest, Ai_Node_DefaultHeuristic, &length);

					// item is too far or not pathable despite dropping a node
					if (!path || length > AI_MAX_ITEM_DISTANCE) {
						g_array_unref(path);
						continue;
					}

					Ai_BackupMainPath(ai);
					Ai_SetPathGoal(self, &ai->move_target, pick.weight, path, pick.entity);	
					g_array_unref(path);

					path_found = true;
				}
			}

			if (!path_found && ai_node_dev->integer) {
				gi.WriteByte(SV_CMD_TEMP_ENTITY);
				gi.WriteByte(TE_TRACER);
				gi.WritePosition(self->s.origin);
				gi.WritePosition(pick.entity->s.origin);
				gi.Multicast(self->s.origin, MULTICAST_PHS, NULL);
			}

			// if we can't find a path to the item, don't try to get it.
			// entity goals are too finnicky for this to work properly.
		}
	}

	g_array_free(items_visible, true);
	return 5;
}

/**
 * @brief Range constants.
 */
typedef enum {
	RANGE_DONT_CARE = 0,
	RANGE_MELEE = 32,
	RANGE_SHORT = 128,
	RANGE_MED = 512,
	RANGE_LONG = 1024
} ai_range_t;

/**
 * @brief
 */
static ai_range_t Ai_GetRange(const float distance) {
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

	ai->weapon_check_time = g_level.time + 250; // don't try again for a bit

	ai_range_t targ_range;

	if (ai->combat_target.type == AI_GOAL_ENTITY) {
		targ_range = Ai_GetRange(Vec3_Distance(self->s.origin, ai->combat_target.entity.ent->s.origin));
	} else {
		targ_range = RANGE_DONT_CARE;
	}

	ai_item_pick_t weapons[ai_num_weapons];
	size_t num_weapons = 0;

	const int16_t *inventory = self->client->locals.inventory;

	for (size_t i = 0; i < g_num_items; i++) {
		const g_item_t *item = G_ItemByIndex(i);

		if (item->type != ITEM_WEAPON) { // not weapon
			continue;
		}

		if (!inventory[i]) { // not in stock
			continue;
		}

		if (item->ammo_item) {
			const int32_t ammo_have = inventory[item->ammo_item->index];
			const int32_t ammo_need = item->quantity;
			if (ammo_have < ammo_need) {
				continue;
			}
		}

		// calculate weight, start with base weapon priority
		float weight = item->priority;

		switch (targ_range) { // range bonus
			case RANGE_DONT_CARE:
				break;
			case RANGE_MELEE:
			case RANGE_SHORT:
				if (item->flags & WF_SHORT_RANGE) {
					weight *= 2.5f;
				} else {
					weight /= 2.5f;
				}
				break;
			case RANGE_MED:
				if (item->flags & WF_MED_RANGE) {
					weight *= 2.5f;
				} else {
					weight /= 2.5f;
				}
				break;
			case RANGE_LONG:
				if (item->flags & WF_LONG_RANGE) {
					weight *= 2.5f;
				} else {
					weight /= 2.5f;
				}
				break;
		}

		if (ai->combat_target.type == AI_GOAL_ENTITY) {
			if ((ai->combat_target.entity.ent->locals.health < 25) &&
					(item->flags & WF_EXPLOSIVE)) { // bonus for explosive at low enemy health
				weight *= 1.5f;
			}
		}

		// additional penalty for long range + projectile unless explicitly long range
		if ((item->flags & WF_PROJECTILE) &&
				!(item->flags & WF_LONG_RANGE)) {
			weight /= 2.f;
		}

		// penalty for explosive weapons at short range
		if ((item->flags & WF_EXPLOSIVE) &&
				(targ_range != RANGE_DONT_CARE && targ_range <= RANGE_SHORT)) {
			weight /= 2.f;
		}

		// penalty for explosive weapons at low self health
		if ((self->locals.health < 25) &&
				(item->flags & WF_EXPLOSIVE)) {
			weight /= 2.f;
		}

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

	if (self->client->locals.weapon == best_weapon->item) {
		return;
	}

	Ai_Command(self, va("use %s", best_weapon->item->name));
	ai->weapon_check_time = g_level.time + 300; // don't try again for a bit
	Ai_Debug("weapon choice: %s (%d choices)\n", best_weapon->item->name, (int32_t) num_weapons);
}

/**
 * @brief Calculate a priority for the specified target.
 */
static float Ai_EnemyPriority(const g_entity_t *self, const g_entity_t *target, const bool visible) {

	// TODO: all of this function. Enemies with more powerful weapons need a higher weight.
	// Enemies with lower health need a higher weight. Enemies carrying flags need an even higher weight.

	return 10.0;
}

/**
 * @brief Figure out if we should chase the enemy and close in on them.
 */
static bool Ai_ShouldChaseEnemy(const g_entity_t *self, const g_entity_t *target) {

	if (target->solid == SOLID_DEAD || (target->sv_flags & SVF_NO_CLIENT)) {
		return false;
	}

	// base chance is our weapon's weight
	const g_item_t *const weapon = self->client->locals.weapon;
	float chance = weapon->priority;

	// if they're low health, higher chance
	if (target->locals.health < 50) {
		chance *= 1.5f;
	}

	// if they have a flag, higher chance
	const int16_t *inventory = target->client->locals.inventory;

	for (size_t i = 0; i < g_num_items; i++) {
		const g_item_t *item = G_ItemByIndex(i);

		if (item->type != ITEM_FLAG) { // not flag
			continue;
		} else if (!inventory[i]) {
			continue;
		}

		chance *= 2.0f;
		break;
	}

	return Randomf() < chance;
}

/**
 * @brief Funcgoal that controls the AI's lust for blood
 */
static uint32_t Ai_FuncGoal_Hunt(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return 1;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	if (ai_no_target->integer || ai_node_dev->integer) {

		if (ai->combat_target.type == AI_GOAL_ENTITY) {
			Ai_ClearGoal(&ai->combat_target);
		}

		if (ai->move_target.type == AI_GOAL_ENTITY && ai->move_target.entity.ent->client) {
			Ai_ClearGoal(&ai->move_target);
		}

		return 1;
	}

	// see if we're already hunting
	if (ai->combat_target.type == AI_GOAL_ENTITY) {

		// check to see if the enemy has gone out of our line of sight
		if (!Ai_CanTarget(self, ai->combat_target.entity.ent)) {

			// enemy dead/out of LOS/disconnected; chase them!
			if (Ai_ShouldChaseEnemy(self, ai->combat_target.entity.ent)) {

				const vec3_t where_to = ai->combat_target.entity.ent->s.origin;
				
				const ai_node_id_t closest = Ai_Node_FindClosest(self->s.origin, 128.f, true, true);
				const ai_node_id_t closest_to_target = Ai_Node_FindClosest(where_to, 128.f, true, true);
				GArray *path = Ai_Node_FindPath(closest, closest_to_target, Ai_Node_DefaultHeuristic, NULL);

				if (path) {
					Ai_SetPathGoal(self, &ai->move_target, 0.7f, path, ai->combat_target.entity.ent);
					g_array_unref(path);
					Ai_Debug("Enemy out of sight & chasing\n");
				}
			}
			
			// if we had a move target still set on hunting that entity, reset to wander.
			if (ai->move_target.type == AI_GOAL_ENTITY && ai->move_target.entity.ent == ai->combat_target.entity.ent) {
				Ai_ClearGoal(&ai->move_target);
			}

			Ai_ClearGoal(&ai->combat_target);
		}

		// TODO: we should change targets here based on priority.
	}

	// we have somebody to kill; go get'em!
	if (ai->combat_target.type == AI_GOAL_ENTITY && !Ai_GoalHasEntity(&ai->move_target, ai->combat_target.entity.ent) && Ai_ShouldChaseEnemy(self, ai->combat_target.entity.ent)) {
		Ai_CopyGoal(&ai->combat_target, &ai->move_target);
		Ai_Debug("Decided to chase/close in on enemy\n");
	}

	// still have an enemy
	if (ai->combat_target.type == AI_GOAL_ENTITY) {

		return 3;
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

		Ai_SetEntityGoal(self, &ai->combat_target, Ai_EnemyPriority(self, player, true), player);

		Ai_PickBestWeapon(self);

		ai->combat_target.entity.combat_type = RandomRangei(AI_COMBAT_CLOSE, AI_COMBAT_TOTAL);
		ai->combat_target.entity.lock_on_time = g_level.time + RandomRangeu(250, 1000);

		if (ai->combat_target.entity.combat_type == AI_COMBAT_FLANK) {
			ai->combat_target.entity.flank_angle = Randomb() ? -90 : 90;
		}

		// re-run hunt with the new target
		return Ai_FuncGoal_Hunt(self, cmd);
	}

	return 5;
}

/**
 * @brief Funcgoal that controls the AI's weaponry.
 */
static uint32_t Ai_FuncGoal_Weaponry(g_entity_t *self, pm_cmd_t *cmd) {

	// if we're dead, just keep clicking so we respawn.
	if (self->solid == SOLID_DEAD) {

		if (g_level.frame_num & 1) {
			cmd->buttons = BUTTON_ATTACK;
		}
		return 1;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	if (ai->weapon_check_time < g_level.time) { // check for a new weapon every once in a while
		Ai_PickBestWeapon(self);
	}

	// we're alive - if we're aiming at an enemy, start-a-firin
	if (ai->combat_target.type == AI_GOAL_ENTITY) {
		if (ai->combat_target.entity.lock_on_time < g_level.time) {

			const uint32_t grenade_hold_time = self->client->locals.grenade_hold_time;
			if (grenade_hold_time) {
				if (g_level.time - grenade_hold_time < RandomRangeu(1500, 2500)) {
					cmd->buttons |= BUTTON_ATTACK;
				}
			} else {
				cmd->buttons |= BUTTON_ATTACK;
			}
		}
	}

	return 1;
}

/**
 * @brief Funcgoal that controls the AI's crouch/jumping while hunting.
 */
static uint32_t Ai_FuncGoal_Acrobatics(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->solid == SOLID_DEAD) {
		return 1;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	if (ai->combat_target.type != AI_GOAL_ENTITY) {
		return 200;
	}

	// do some acrobatics
	if (self->locals.ground.ent) {

		if (self->client->ps.pm_state.flags & PMF_DUCKED) {

			if ((RandomRangeu(0, 32)) == 0) { // uncrouch eventually
				cmd->up = 0;
			} else {
				cmd->up = -PM_SPEED_JUMP;
			}
		} else {

			if ((RandomRangeu(0, 32)) == 0) { // randomly crouch
				cmd->up = -PM_SPEED_JUMP;
			} else if ((RandomRangeu(0, 86)) == 0) { // randomly pop, to confuse our enemies
				cmd->up = PM_SPEED_JUMP;
			}
		}
	} else {

		cmd->up = 0;
	}

	return 1;
}

/**
 * @brief Wander aimlessly, hoping to find something to love.
 */
static inline float Ai_Wander(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);
	float *angle = ai->combat_target.type == AI_GOAL_ENTITY ? &ai->combat_target.entity.flank_angle : &ai->move_target.wander.angle;

	vec3_t forward;
	Vec3_Vectors(Vec3(0.f, *angle, 0.f), &forward, NULL, NULL);

	const vec3_t end = Vec3_Fmaf(self->s.origin, Box3_Size(self->bounds).x * 2.0f, forward);
	const cm_trace_t tr = gi.Trace(self->s.origin, end, Box3_Zero(), self, CONTENTS_MASK_CLIP_PLAYER);

	if (tr.fraction < 1.0f) { // hit a wall
	
		if (ai->combat_target.type == AI_GOAL_ENTITY) {
			if (ai->combat_target.entity.combat_type == AI_COMBAT_FLANK && Randomb()) {
				ai->combat_target.entity.combat_type = Randomb() ? AI_COMBAT_CLOSE : AI_COMBAT_WANDER;
			}

			if (ai->combat_target.entity.combat_type == AI_COMBAT_FLANK) {
				*angle = -*angle;
			} else {
				float angle_change = 45 + Randomf() * 45;
				*angle += Randomb() ? -angle_change : angle_change;
			}
		} else {
			float angle_change = 45 + Randomf() * 45;
			*angle += Randomb() ? -angle_change : angle_change;
		}
	}

	return *angle;
}

static g_entity_t *ai_current_entity;

/**
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t Ai_ClientMove_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {

	const g_entity_t *self = ai_current_entity;

	if (self->solid == SOLID_DEAD) {
		return gi.Trace(start, end, bounds, self, CONTENTS_MASK_CLIP_CORPSE);
	} else {
		return gi.Trace(start, end, bounds, self, CONTENTS_MASK_CLIP_PLAYER);
	}
}

/**
 * @brief Increase our path pointer.
 */
static bool Ai_TryNextNodeInPath(g_entity_t *self, ai_goal_t *goal) {

	goal->path.path_index++;

	// we're done with this path
	if (goal->path.path_index == goal->path.path->len) {
		return false;
	}

	goal->path.path_position = Ai_Node_GetPosition(g_array_index(goal->path.path, ai_node_id_t, goal->path.path_index));
	goal->path.next_path_position = Ai_Node_GetPosition(g_array_index(goal->path.path, ai_node_id_t, Mini(goal->path.path->len - 1, goal->path.path_index + 1)));
	goal->distress = 0;
	goal->distress_extension = false;
	goal->last_distance = FLT_MAX;

	return true;
}

/**
 * @brief See if we're in a good spot to keep going towards our node goal.
 */
static bool Ai_CheckNodeNav(g_entity_t *self, ai_goal_t *goal) {

	/*
	 * The AI's bbox is expanded for collision checks. This is so
	 * nodes on ledges don't cause the bot to misjudge a jump as much.
	 */
	const vec3_t padding = { .x = 8.f, .y = 8.f, .z = 0.f };

	// if we're touching our nav goal, we can go next
	if (Box3_ContainsPoint(Box3_Expand3(self->abs_bounds, padding), goal->path.path_position)) {

		return Ai_TryNextNodeInPath(self, goal);
	}

	return true;
}

/**
 * @brief 
 */
static bool Ai_CheckGoalDistress(g_entity_t *self, ai_goal_t *goal, const vec3_t dest) {
	ai_locals_t *ai = Ai_GetLocals(self);
	const float path_dist = Vec3_Distance(self->s.origin, dest);

	// wander's distress is handled elsewhere
	if (goal->type) {
		// closing in
		if (path_dist < goal->last_distance) {
			goal->last_distance = path_dist;
			goal->distress /= 2;
		// getting further away
		} else {
			goal->distress += (gi.PointContents(self->s.origin) & CONTENTS_MASK_LIQUID ? 0.05f : 0.25f);
		}

		vec3_t dest = Vec3_Zero();

		switch (goal->type) {
		default:
			assert(false);
			break;
		case AI_GOAL_POSITION:
			dest = goal->position.pos;
			break;
		case AI_GOAL_PATH:
			dest = goal->path.path_position;
			break;
		case AI_GOAL_ENTITY:
			dest = goal->entity.ent->s.origin;
			break;
		}

		// something is blocking our destination
		const cm_trace_t tr = gi.Trace(ai->eye_origin, dest, Box3_Zero(), self, CONTENTS_MASK_CLIP_CORPSE);

		if (tr.fraction < 1.0f) {
			goal->distress += 0.25f;
		}
	}

	if (goal->last_distress != goal->distress) {
		//Ai_Debug("Distress: %f\n", goal->distress);
		goal->last_distress = goal->distress;
	}

	if (goal->distress > (goal->distress_extension ? (QUETOO_TICK_RATE * 15) : QUETOO_TICK_RATE)) {
		goal->distress = 0;
		goal->last_distance = 0;
		goal->distress_extension = false;
		ai->reacquire_time = g_level.time + 1000;
			
		Ai_Debug("Distress threshold reached\n");
		return false;
	}

	return true;
}

/**
 * @brief
 */
static inline bool Ai_Path_IsLinked(const GArray *path, const guint a, const guint b) {

	return Ai_Node_IsLinked(g_array_index(path, ai_node_id_t, a), g_array_index(path, ai_node_id_t, b));
}

/**
 * @brief 
 */
static bool Ai_FacingTarget(const g_entity_t *self, const vec3_t target) {
	vec3_t sub = Vec3_Subtract(target, self->s.origin);
	sub.z = 0;

	sub = Vec3_Normalize(sub);

	vec3_t fwd;
	Vec3_Vectors(Vec3(0, self->s.angles.y, 0), &fwd, NULL, NULL);

	const float dot = Vec3_Dot(sub, fwd);

	return dot > 0.8f;
}

/**
 * @brief A slow-drop occurs when a connection is mono-directional, not far horizontally
 * but far vertically.
 */
bool Ai_ShouldSlowDrop(const ai_node_id_t from_node, const ai_node_id_t to_node) {

	if (from_node <= 0 || to_node <= 0) {
		return false;
	}

	if (Ai_Node_IsLinked(to_node, from_node)) {
		return false;
	}
	
	const vec3_t from = Ai_Node_GetPosition(from_node);
	const vec3_t to = Ai_Node_GetPosition(to_node);

	return (to.z - from.z) <= -PM_STEP_HEIGHT &&
		Vec2_Distance(Vec3_XY(to), Vec3_XY(from)) < PM_STEP_HEIGHT * 8.f;
}

/**
 * @brief Move towards our current target
 */
static uint32_t Ai_MoveToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);

	bool target_enemy = false;

	vec3_t dir, angles, dest = Vec3_Zero();
	bool move_wander = false;

	switch (ai->move_target.type) {
		default:
			dest = self->s.origin;
			move_wander = true;
			break;
		case AI_GOAL_POSITION:
			dest = ai->move_target.position.pos;
			break;
		case AI_GOAL_ENTITY:
			switch (ai->move_target.entity.combat_type) {
			case AI_COMBAT_NONE:
			default:
				dest = ai->move_target.entity.ent->s.origin;
				break;
			case AI_COMBAT_CLOSE:
				dest = ai->move_target.entity.ent->s.origin;
				target_enemy = true;
				break;
			case AI_COMBAT_FLANK:
			case AI_COMBAT_WANDER:
				move_wander = true;
				target_enemy = true;
				break;
			}

			break;
		case AI_GOAL_PATH:
			if (!Ai_CheckNodeNav(self, &ai->move_target)) {
				Ai_RestoreMainPath(self, ai);
				Ai_MoveToTarget(self, cmd);
				return 1;
			}

			if (ai->move_target.path.trick_jump) {
				dest = ai->move_target.path.trick_position;
			} else {
				dest = ai->move_target.path.path_position;
			}
			break;
	}
	
	if (!Ai_CheckGoalDistress(self, &ai->move_target, dest)) {
		Ai_ClearGoal(&ai->move_target);
		Ai_MoveToTarget(self, cmd);
		return 1;
	}

	if (move_wander) {
		const float wander_angle = Ai_Wander(self, cmd);

		angles = Vec3(0.0, wander_angle, 0.0);
		Vec3_Vectors(angles, &dir, NULL, NULL);
		dest = Vec3_Add(self->s.origin, dir);
	}

	dir = Vec3_Subtract(dest, self->s.origin);
	const float len = Vec3_Length(dir);

	if (target_enemy && len < 200.0f) {
		// switch to flank/wander, this helps us recover from being up close to enemies
		if (ai->combat_target.entity.combat_type == AI_COMBAT_CLOSE && Randomb()) {
			ai->combat_target.entity.combat_type = Randomb() ? AI_COMBAT_FLANK : AI_COMBAT_WANDER;
		}
	}

	dir = Vec3_Normalize(dir);
	angles = Vec3_Euler(dir);

	const float delta_yaw = self->client->locals.angles.y - angles.y;
	Vec3_Vectors(Vec3(0.0, delta_yaw, 0.0), &dir, NULL, NULL);

	const bool swimming = self->locals.water_level >= WATER_WAIST;
	bool wait_politely = false;

	if (ai->move_target.type == AI_GOAL_PATH) {
		// the next step(s) will be onto a mover, so if we can't move yet, we wait.
		if (!(gi.PointContents(self->s.origin) & CONTENTS_MASK_LIQUID) && !Ai_Path_CanPathTo(ai->move_target.path.path, ai->move_target.path.path_index)) {
			dir = Vec3_Scale(dir, PM_SPEED_RUN);
			wait_politely = true;
			ai->move_target.distress_extension = true;
		// trick-jumping requires a bit of finesse
		} else if (ai->move_target.path.trick_jump) {
			if (ai->move_target.path.trick_jump == TRICK_JUMP_TURNING) {
				dir = Vec3_Zero();
			} else {
				dir = Vec3_Scale(dir, Clampf(len * 2.f, 10.f, PM_SPEED_RUN));
			}
		// if we aren't underwater, have no ground entity & falling downwards, we're probably in the air;
		// rather than full-blasting, pull us towards what we're trying to drop on.
		} else if (!swimming && !self->locals.ground.ent && self->locals.velocity.z < 0 &&
			Vec3_Dot(Vec3_Subtract(self->s.origin, ai->move_target.path.path_position), Vec3_Down()) > 0.7f &&
			(ai->move_target.path.path_position.z - self->s.origin.z) <= -PM_STEP_HEIGHT) {
			dir = Vec3_Scale(dir, Clampf(len, 10.f, PM_SPEED_RUN));
			Ai_Debug("Trying to land on target\n");
		// we're navigating on land, and our next target is below us & a drop node (one-way connection); rather than full-blast
		// running off the edge, transition to walking so we don't overshoot targets beneath us
		} else if (!swimming && Ai_ShouldSlowDrop(ai->move_target.path.path_index, ai->move_target.path.path_index - 1)) {
			dir = Vec3_Scale(dir, PM_SPEED_RUN * 0.5f);
		// run full speed towards the target
		} else {
			dir = Vec3_Scale(dir, PM_SPEED_RUN);
		}

		// if we're swimming, node is above us and we're "sticky feet", jump
		if (swimming && self->locals.ground.ent) {
			cmd->up = PM_SPEED_JUMP;
		// if we're on a ladder and the node is a bbox below us, crouch to get down,
		// otherwise hold jump
		} else if (self->client->ps.pm_state.flags & PMF_ON_LADDER) {

			if ((ai->move_target.path.path_position.z - self->s.origin.z) < -Box3_Size(PM_BOUNDS).z) {
				cmd->up = -PM_SPEED_DUCKED;
			} else {
				cmd->up = PM_SPEED_JUMP;
			}

		}
	// run full speed towards the target
	} else {
		dir = Vec3_Scale(dir, PM_SPEED_RUN);

		// if we're not navving, hold jump under water
		if (self->locals.water_level >= WATER_WAIST) {
			cmd->up = PM_SPEED_JUMP;
		}
	}

	cmd->forward = dir.x;
	cmd->right = dir.y;

	ai_current_entity = self;

	// predict ahead
	pm_move_t pm;

	memset(&pm, 0, sizeof(pm));
	pm.s = self->client->ps.pm_state;

	pm.s.origin = self->s.origin;

	pm.s.velocity = self->locals.velocity;

	pm.s.type = PM_NORMAL;

	pm.cmd = *cmd;
	pm.ground = self->locals.ground;
	//pm.hook_pull_speed = g_hook_pull_speed->value;

	pm.PointContents = gi.PointContents;
	pm.BoxContents = gi.BoxContents;
	
	pm.Trace = Ai_ClientMove_Trace;

	pm.Debug = gi.Debug_;
	pm.DebugMask = gi.DebugMask;
	pm.debug_mask = DEBUG_PMOVE_SERVER;

	// perform a move; predict our next frame
	Pm_Move(&pm);

	// predict a few frames ahead, for timely stoppage
	pm_move_t pm_ahead = pm;

	pm_ahead.cmd.msec = 100;
	Pm_Move(&pm_ahead);

	// we weren't trying to jump and predicted ground is gone
	if (cmd->up <= 0 && self->locals.ground.ent && (!pm_ahead.ground.ent || !pm.ground.ent)) {

		//Ai_Debug("Lacking ground entity. In 5 frames: %s, in 1 frame: %s\n", pm_ahead.ground_entity ? "no" : "yes", pm.ground_entity ? "no" : "yes");

		if (ai->move_target.type == AI_GOAL_PATH) {
			const float xy_dist = Vec2_Distance(Vec3_XY(ai->move_target.path.path_position), Vec3_XY(self->s.origin));
			
			// we're most likely on or going to a mover; if we'll be falling in a few frames, stop us early
			if (!(gi.PointContents(self->s.origin) & CONTENTS_MASK_LIQUID) && !Ai_Path_CanPathTo(ai->move_target.path.path, ai->move_target.path.path_index)) {
				cmd->forward = -cmd->forward;
				cmd->right = -cmd->right; // stop for now
				Ai_Debug("Stopping early to prevent mover issues\n");
			// if the node is above us step-wise OR it's not far below us & across a big distance, we gotta jump
			} else if (((ai->move_target.path.path_position.z - self->s.origin.z) > -PM_STEP_HEIGHT ||
				(xy_dist > fabsf(ai->move_target.path.path_position.z - self->s.origin.z) && (xy_dist >= PM_STEP_HEIGHT * 6.f))) && !pm.ground.ent) {
				cmd->up = PM_SPEED_JUMP;
			}
		} else if (move_wander) {
			const float angle = 45 + Randomf() * 45;

			if (target_enemy) {
				ai->move_target.entity.combat_type = Randomb() ? AI_COMBAT_CLOSE : AI_COMBAT_WANDER;
				ai->move_target.entity.flank_angle += Randomb() ? -angle : angle;
			} else {
				ai->move_target.wander.angle += Randomb() ? -angle : angle;
			}
		} else {
			const float xy_dist = Vec2_Distance(Vec3_XY(dest), Vec3_XY(self->s.origin));

			if (((dest.z - self->s.origin.z) > -PM_STEP_HEIGHT &&
				(xy_dist > fabsf(ai->move_target.path.path_position.z - self->s.origin.z) && (xy_dist >= PM_STEP_HEIGHT * 6.f))) && !pm.ground.ent) {
				cmd->up = PM_SPEED_JUMP;
			}
		}
	// trick jump code
	} else if (ai->move_target.type == AI_GOAL_PATH) {

		if (ai->move_target.path.trick_jump == TRICK_JUMP_START) {
			ai->move_target.path.trick_jump++;
			cmd->up = 0;
			Ai_Debug("Trick jump: letting go for a frame\n");
		} else if (ai->move_target.path.trick_jump == TRICK_JUMP_WAITING) {
			cmd->up = PM_SPEED_JUMP;
			Ai_Debug("Trick jump: holding jump again!!\n");
		}
	}
	
	// check for getting stuck
	const vec3_t move_dir = Vec3_Subtract(self->s.origin, pm.s.origin);
	const float move_len = Vec3_Length(move_dir);
	
	// check for teleport
	if (move_len > 64.f) {
		if (ai->move_target.type == AI_GOAL_PATH) {
			if (!Ai_TryNextNodeInPath(self, &ai->move_target)) {
				Ai_RestoreMainPath(self, ai);
			}
		}
	// if we're not waiting to turn...
	// and not waiting politely...
	// and not riding a mover...
	} else if (((ai->move_target.type == AI_GOAL_PATH && ai->move_target.path.trick_jump != TRICK_JUMP_TURNING) || ai->move_target.type != AI_GOAL_PATH) &&
		!wait_politely && (!self->locals.ground.ent || self->locals.ground.ent->s.number == 0)) {

		// we'll be pushed up against something
		float smol_dist = PM_SPEED_RUN * PM_SPEED_MOD_WALK * MILLIS_TO_SECONDS(cmd->msec);

		if (gi.PointContents(self->s.origin) & CONTENTS_MASK_LIQUID) {
			smol_dist *= 0.1f;
		}

		if (move_len < smol_dist) {
			
			if (ai->distress_jump_offset <= g_level.time) {
				// if we're navving, node is above us, and we're on ground, jump; we're probably trying
				// to trick-jump or something
				if (ai->move_target.type == AI_GOAL_PATH && self->locals.ground.ent && pm.ground.ent) {
					if ((ai->move_target.path.path_position.z - self->s.origin.z) > PM_STEP_HEIGHT * 7.f) {

						ai->move_target.path.trick_jump = TRICK_JUMP_START;
						ai->move_target.path.trick_position = ai->move_target.path.path_position;
						cmd->up = PM_SPEED_JUMP;

						Ai_Debug("Node *far* above us, and we're probably stuck; trick jump most likely!\n");
					} else if ((ai->move_target.path.path_position.z - self->s.origin.z) > PM_STEP_HEIGHT &&
						Vec2_Distance(Vec3_XY(ai->move_target.path.path_position), Vec3_XY(self->s.origin)) < PM_STEP_HEIGHT * 7.f &&
						Ai_FacingTarget(self, ai->move_target.path.path_position)) {

						cmd->up = PM_SPEED_JUMP;
						Ai_Debug("Node above us & close, and we're probably stuck; regular jump\n");
					}
				}
			}

			// if we're on a mover, distress differently so we don't unexpectedly
			// jump off of it
			if (self->locals.ground.ent && self->locals.ground.ent->s.number != 0) {
				ai->move_target.distress += 0.02f;
			} else {
				ai->move_target.distress += 0.2f;

				// try moving left/right if we weren't already trying this
				if (ai->move_target.distress > 8.f && !cmd->right) {
					cmd->right = cmd->forward;

					if (ai->move_target.time & 1) {
						cmd->right = -cmd->right;
					}
				// try a jump
				} else if (ai->move_target.distress > 12.f) {
					cmd->up = PM_SPEED_JUMP;
				}
			}
		// we're making some distance, reduce our distress
		} else {
			if (ai->move_target.distress > 0) {
				ai->move_target.distress -= 0.2f;
			}
		}
	}

	return 0;
}

// note: this is not the same as AngleMod
static inline float Ai_AngleMod(const float a) {
	return (360.0f / 65536) * ((int32_t) (a * (65536 / 360.0f)) & 65535);
}

static float Ai_CalculateAngle(g_entity_t *self, const float speed, float current, float ideal) {
	current = Ai_AngleMod(current);
	ideal = Ai_AngleMod(ideal);

	if (current == ideal) {
		return current;
	}

	float move = ideal - current;

	if (ideal > current) {
		if (move >= 180.0f) {
			move = move - 360.0f;
		}
	} else {
		if (move <= -180.0f) {
			move = move + 360.0f;
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

	return Ai_AngleMod(current + move);
}

/**
 * @brief Turn/look towards our current target
 */
static uint32_t Ai_TurnToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);
	ai_goal_t *combat_target = &ai->combat_target;

	vec3_t ideal_angles;

	if (combat_target->type != AI_GOAL_ENTITY) {
		if (ai->move_target.type == AI_GOAL_NONE) {
			ideal_angles = Vec3(0.0, ai->move_target.wander.angle, 0.0);
		} else {
			vec3_t aim_target = Vec3_Zero();

			if (ai->move_target.type == AI_GOAL_PATH) {

				// if we're trick-jumping, aim towards where we intend to land
				if (ai->move_target.path.trick_jump) {
					aim_target = ai->move_target.path.trick_position;
				// if we're above ground & on-land, and our next path is bidirectional, assume it's normal
				// pathing; aim towards our *next* target to look a bit more natural.
				} else if (((self->locals.water_level < WATER_WAIST && !(self->client->ps.pm_state.flags & PMF_ON_LADDER)) || !(gi.PointContents(ai->move_target.path.path_position) & CONTENTS_MASK_LIQUID)) &&
					ai->move_target.path.path_index && Ai_Path_IsLinked(ai->move_target.path.path, ai->move_target.path.path_index, ai->move_target.path.path_index - 1)) {
					aim_target = ai->move_target.path.next_path_position;
				// otherwise, aim directly at the next position
				} else {
					aim_target = ai->move_target.path.path_position;
				}
			} else if (ai->move_target.type == AI_GOAL_POSITION) {
				aim_target = ai->move_target.position.pos;
			} else if (ai->move_target.type == AI_GOAL_ENTITY) {
				aim_target = ai->move_target.entity.ent->s.origin;
			} else {
				assert(false);
			}

			const vec3_t aim_direction = Vec3_Normalize(Vec3_Subtract(aim_target, self->s.origin));
			ideal_angles = Vec3_Euler(aim_direction);
			ideal_angles.z = 0.f;

			// if underwater or in air we have to directly face our target, otherwise
			// just yaw us.
			// FIXME: bug in PMove prevents this from working for *all* in air situations
			// (you get less forward momentum on a jump if you are looking up/down)
			// so for now this is hardcoded to ladders
			if (self->client->ps.pm_state.flags & PMF_ON_LADDER) {
				if ((ai->move_target.path.path_position.z - self->s.origin.z) < -Box3_Size(PM_BOUNDS).z) {
					ideal_angles.x = Clampf(ideal_angles.x, -10.f, -180.f);
				} else {
					ideal_angles.x = Clampf(ideal_angles.x, 10.f, 180.f);
				}
				Ai_Debug("Clamping X to %f\n", ideal_angles.x);
			} else if (self->locals.water_level < WATER_WAIST) {
				ideal_angles.x = 0.f;
			}
		}
	} else {
		vec3_t aim_direction = Vec3_Subtract(combat_target->entity.ent->s.origin, self->s.origin);

		const g_item_t *const weapon = self->client->locals.weapon;

		if (weapon->flags & WF_PROJECTILE) {
			const float dist = Vec3_Length(aim_direction);
			// FIXME: *real* projectile speed generally factors into this...
			const float speed = RandomRangef(900, 1200);
			const float time = dist / speed;
			const vec3_t target_velocity = combat_target->entity.ent->locals.velocity;
			const vec3_t target_pos = Vec3_Fmaf(combat_target->entity.ent->s.origin, time, target_velocity);
			aim_direction = Vec3_Subtract(target_pos, self->s.origin);
		} else {
			aim_direction = Vec3_Subtract(combat_target->entity.ent->s.origin, self->s.origin);
		}

		aim_direction = Vec3_Normalize(aim_direction);
		ideal_angles = Vec3_Euler(aim_direction);

		// fuzzy angle
		ideal_angles.x += sinf(g_level.time / 128.0f) * 4.3f;
		ideal_angles.y += cosf(g_level.time / 164.0f) * 4.0f;
	}

	const vec3_t view_angles = self->client->locals.angles;

	for (int32_t i = 0; i < 2; ++i) {
		ideal_angles.xyz[i] = Ai_CalculateAngle(self, 12.5f * (cmd->msec / (float)QUETOO_TICK_MILLIS), view_angles.xyz[i], ideal_angles.xyz[i]);
	}

	if (ai->move_target.type == AI_GOAL_PATH && ai->move_target.path.trick_jump == TRICK_JUMP_TURNING && view_angles.y == ideal_angles.y) {
		ai->move_target.path.trick_jump = TRICK_JUMP_NONE;
	}

	ai->ideal_angles = ideal_angles;
	cmd->angles = Vec3_Subtract(ideal_angles, self->client->ps.pm_state.delta_angles);
	return 0;
}

/**
 * @brief Called just before an AI leaves this mortal plane.
 */
void G_Ai_Disconnect(g_entity_t *self) {
	ai_locals_t *ai = Ai_GetLocals(self);

	// clear any dynamic memory
	Ai_ClearGoal(&ai->combat_target);
	Ai_ClearGoal(&ai->move_target);
	Ai_ClearGoal(&ai->backup_move_target);

	memset(ai, 0, sizeof(*ai));
}

/**
 * @brief Long range goal picking
 */
static uint32_t Ai_FuncGoal_LongRange(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);

	// if we already have a long range goal, try again later.
	// TODO: we know what entity we're heading towards, so we can
	// give up a goal if the entity is gone, but it should be logical
	// (for stuff that's "close" we can give up if the item appears to 
	// be visually missing; for flags, we can give up if the flag was taken
	// so we can switch to hunting the taker, or trying to support the carrier)
	if (ai->move_target.type != AI_GOAL_NONE) {
		return 200;
	}

	// check to be sure we're in a navicable spot
	const ai_node_id_t closest = Ai_Node_FindClosest(self->s.origin, 256.f, true, true);

	if (closest == AI_NODE_INVALID) {
		return 200;
	}

	GArray *goal_possibilities = g_array_new(false, false, sizeof(ai_item_pick_t));

	for (int32_t i = sv_max_clients->integer; i < ge.num_entities; i++) {
		g_entity_t *ent = ENTITY_FOR_NUM(i);

		// not in use, dead/invisible, and/or clients can't be long term goals
		if (!ent->in_use || ent->solid == SOLID_NOT || ent->solid == SOLID_DEAD || (ent->sv_flags & SVF_NO_CLIENT) || ent->client) {
			continue;
		}

		// TODO: atm only items can be goals. in future,
		// other players should be able to be goals (supporting
		// carrier for instance)
		if (!ent->locals.item) {
			continue;
		}

		float weight = 0.f;

		// if we're an item, check that we can pick it up
		// and check weight
		if (ent->locals.item) {

			if (!Ai_CanPickupItem(self, ent)) {
				continue;
			}

			// TODO: situational weighting
			weight = ent->locals.item->priority;
		}

		// didn't want this
		if (!weight) {
			continue;
		}

		// randomize weights
		weight = Randomf() * weight;

		// add!!
		g_array_append_vals(goal_possibilities, &(ai_item_pick_t) {
			.weight = weight,
			.entity = ent
		}, 1);
	}

	// sort!
	if (goal_possibilities->len > 1) {
		g_array_sort(goal_possibilities, Ai_ItemPick_Compare);
	}

	// go down the list, high priority wins but might not be pickable
	for (guint i = 0; i < goal_possibilities->len; i++) {
		const ai_item_pick_t *pick = &g_array_index(goal_possibilities, ai_item_pick_t, i);
		const ai_node_id_t closest_to_item = Ai_Node_FindClosest(pick->entity->s.origin, 256.f, true, true);
		GArray *path = Ai_Node_FindPath(closest, closest_to_item, Ai_Node_DefaultHeuristic, NULL);

		if (path) {
			Ai_SetPathGoal(self, &ai->move_target, pick->weight, path, pick->entity);
			g_array_unref(path);
			break;
		}
	}

	g_array_free(goal_possibilities, true);

	// got one; don't try again for a while.
	return 1000;
}

/**
 * @brief Static list of func goal functions
 */
static const Ai_GoalFunc ai_goalfuncs[AI_FUNCGOAL_TOTAL] = {
	[AI_FUNCGOAL_LONGRANGE] = Ai_FuncGoal_LongRange,
	[AI_FUNCGOAL_HUNT] = Ai_FuncGoal_Hunt,
	[AI_FUNCGOAL_WEAPONRY] = Ai_FuncGoal_Weaponry,
	[AI_FUNCGOAL_ACROBATICS] = Ai_FuncGoal_Acrobatics,
	[AI_FUNCGOAL_FINDITEMS] = Ai_FuncGoal_FindItems,
	[AI_FUNCGOAL_TURN] = Ai_TurnToTarget,
	[AI_FUNCGOAL_MOVE] = Ai_MoveToTarget
};

/**
 * @brief Called every frame for every AI.
 */
void G_Ai_Think(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);

	if (self->solid == SOLID_DEAD) {
		Ai_ClearGoal(&ai->combat_target);
		Ai_ClearGoal(&ai->move_target);
		Ai_ClearGoal(&ai->backup_move_target);
	} else {
		Vec3_Vectors(self->client->locals.angles, &ai->aim_forward, NULL, NULL);
		ai->eye_origin = Vec3_Add(self->s.origin, self->client->ps.pm_state.view_offset);
	}

	// run functional goals
	for (int32_t i = 0; i < AI_FUNCGOAL_TOTAL; i++) {

		if (ai->funcgoal_nextthinks[i] <= g_level.time) {
			ai->funcgoal_nextthinks[i] = g_level.time + ai_goalfuncs[i](self, cmd);
		}
	}

	ai->last_origin = self->s.origin;
	
	// run client think
	G_ClientThink(self, cmd);

	// can't trick jump when we hit the ground.
	if (ai->move_target.type == AI_GOAL_PATH && self->locals.ground.ent && ai->move_target.path.trick_jump) {

		if (Vec3_Distance(ai->move_target.path.trick_position, self->s.origin) < 24.f) {
			ai->move_target.path.trick_jump = TRICK_JUMP_TURNING;
			Ai_Debug("Trick jump: mission accomplished\n");
		} else {
			ai->move_target.path.trick_jump = TRICK_JUMP_NONE;
			Ai_Debug("Trick jump: mission accomplished\n");
		}
	}
}
/**
 * @brief Called every time an AI spawns
 */
void G_Ai_Respawn(g_entity_t *self) {

	// clean up state
	G_Ai_Disconnect(self);

	if (self->solid == SOLID_NOT) { // intermission, spectator, etc
		return;
	}
}

/**
 * @brief Called when an AI is first spawned and is ready to go.
 */
void G_Ai_Begin(g_entity_t *self) {
}

/**
 * @brief 
 */
static void Ai_SaveNodes_f(void) {

	Ai_SaveNodes();
}

/**
 * @brief 
 */
static void Ai_TestPath_f(void) {

	GArray *path = Ai_Node_TestPath();

	if (!path) {
		return;
	}

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {
		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (ent->in_use && !(ent->sv_flags & SVF_NO_CLIENT) && ent->client && ent->client->ai) {
			ai_locals_t *ai = Ai_GetLocals(ent);
			const ai_node_id_t closest_to_player = Ai_Node_FindClosest(ent->s.origin, 256.f, true, true);

			if (closest_to_player == AI_NODE_INVALID) {
				Ai_Debug("Can't find a node near this bot\n");
				continue;
			}

			GArray *path_to_start = Ai_Node_FindPath(closest_to_player, g_array_index(path, ai_node_id_t, 0), Ai_Node_DefaultHeuristic, NULL);

			if (path_to_start == NULL) {
				Ai_Debug("Can't find a path to the test path\n");
				continue;
			}

			g_array_append_vals(path_to_start, &g_array_index(path, ai_node_id_t, 1), path->len - 1);
			Ai_SetPathGoal(ent, &ai->move_target, 1.0, path_to_start, NULL);
			g_array_unref(path_to_start);
		}
	}
}

void Ai_OffsetNodes_f(void);

/**
 * @brief Initializes the AI subsystem.
 */
void G_Ai_InitLocals(void) {

	gi.Print("Ai initialization...\n");
	gi.Mkdir("ai");

	ai_no_target = gi.AddCvar("ai_no_target", "0", CVAR_DEVELOPER, "Disables bots targeting enemies");

	ai_node_dev = gi.AddCvar("ai_node_dev", "0", CVAR_DEVELOPER | CVAR_LATCH, "Toggles node development mode. '1' is full development mode, '2' is live debug mode.");
	
	if (ai_node_dev->integer) {
		gi.SetCvarInteger("g_cheats", 1);
	}

	gi.SetConfigString(CS_NAV_EDIT, ai_node_dev->string);

	ai_locals = (ai_locals_t *) gi.Malloc(sizeof(ai_locals_t) * sv_max_clients->integer, MEM_TAG_AI);

	gi.AddCmd("ai_save_nodes", Ai_SaveNodes_f, CMD_AI, "Save current node data");

	gi.AddCmd("ai_test_path", Ai_TestPath_f, CMD_AI, "Save current node data");

	gi.AddCmd("ai_offset_nodes", Ai_OffsetNodes_f, CMD_AI, "Offset the loaded nodes by the specified translation");

	Ai_InitItems();
	Ai_InitSkins();

	gi.Print("Ai module initialized\n");
}

/**
 * @brief Loads map data for the AI subsystem.
 */
void G_Ai_Load(void) {

	ai_level.load_finished = false;

	Ai_InitNodes();
}

/**
 * @brief Shuts down the AI subsystem
 */
void G_Ai_Shutdown(void) {

	gi.Print("  ^5Ai module shutdown...\n");

	Ai_ShutdownSkins();

	gi.FreeTag(MEM_TAG_AI);
}

/**
 * @brief 
 */
bool G_Ai_InDeveloperMode(void) {

	return ai_node_dev->integer == 1;
}
