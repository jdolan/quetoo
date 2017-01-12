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

static cvar_t *g_ai_passive;
static g_entity_ai_t *g_ai_locals;

/**
 * @brief
 */
static _Bool G_Ai_Can_Target(const g_entity_t *self, const g_entity_t *other) {

	if (g_ai_passive->integer) {
		return false;
	}

	if (other == self || !other->client || !other->in_use || other->locals.dead || (other->sv_flags & SVF_NO_CLIENT)) {
		return false;
	}

	cm_trace_t tr = gi.Trace(self->s.origin, other->s.origin, vec3_origin, vec3_origin, self, MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0 && tr.ent == other) {
		return true;
	}

	return false;
}

#ifdef AI_UNUSED
/**
 * @brief Executes a console command as if the bot typed it.
 */
static void G_Ai_Command(g_entity_t *self, const char *command) {

	gi.TokenizeString(command);
	G_ClientCommand(self);
}
#endif

/**
 * @brief Calculate a priority for the specified target.
 */
static vec_t G_Ai_EnemyPriority(g_entity_t *self, g_entity_t *target, const _Bool visible) {

	// TODO: all of this function. Enemies with more powerful weapons need a higher weight.
	// Enemies with lower health need a higher weight. Enemies carrying flags need an even higher weight.

	return 10.0;
}

/**
 * @brief Funcgoal that controls the AI's lust for blood
 */
static uint32_t G_Ai_FuncGoal_Hunt(g_entity_t *self, pm_cmd_t *cmd) {
	
	if (self->locals.dead) {
		return QUETOO_TICK_MILLIS;
	}

	g_entity_ai_t *ai = self->locals.ai_locals;

	// see if we're already hunting
	if (ai->aim_target.type == AI_GOAL_ENEMY) {

		// check to see if the enemy has gone out of our line of sight
		if (!G_Ai_Can_Target(self, ai->aim_target.ent)) {

			// enemy went out of our LOS, aim at where they were for a while
			if (!ai->aim_target.ent->locals.dead && !(ai->aim_target.ent->sv_flags & SVF_NO_CLIENT)) {
				G_Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_GHOST, G_Ai_EnemyPriority(self, ai->aim_target.ent, false), ai->aim_target.ent);
				VectorCopy(ai->aim_target.ent->s.origin, ai->ghost_position);
			} else {
				G_Ai_ClearGoal(&ai->aim_target);
			}

			G_Ai_ClearGoal(&ai->move_target);
		}

		// TODO: we should change targets here based on priority.
	}

	// if we're wandering/not looking for something, move towards the target
	if (ai->move_target.type <= AI_GOAL_NAV) {

		// TODO: if we have proper navigation, then we can remove this if statement.
		// until then, we only try to move towards actual enemies and not ghosts.
		if (ai->aim_target.type == AI_GOAL_ENEMY) {
			G_Ai_CopyGoal(&ai->aim_target, &ai->move_target);
		}
	}

	// still have an enemy
	if (ai->aim_target.type == AI_GOAL_ENEMY) {
		
		return QUETOO_TICK_MILLIS * 3;
	}

	// we lost our enemy, start looking for a new one
	g_entity_t *player = NULL;

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

		g_entity_t *ent = &g_game.entities[i];

		if (G_Ai_Can_Target(self, ent)) {

			player = ent;
			break;
		}
	}

	// found one, set it up
	if (player) {

		G_Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_ENEMY, G_Ai_EnemyPriority(self, player, true), player);

		// re-run hunt with the new target
		return G_Ai_FuncGoal_Hunt(self, cmd);
	}

	return QUETOO_TICK_MILLIS * 5;
}

/**
 * @brief Funcgoal that controls the AI's weaponry.
 */
static uint32_t G_Ai_FuncGoal_Weaponry(g_entity_t *self, pm_cmd_t *cmd) {
	
	// if we're dead, just keep clicking so we respawn.
	if (self->locals.dead) {
		cmd->buttons = self->client->locals.buttons ^ BUTTON_ATTACK;
		return QUETOO_TICK_MILLIS;
	}

	g_entity_ai_t *ai = self->locals.ai_locals;

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
static uint32_t G_Ai_FuncGoal_Acrobatics(g_entity_t *self, pm_cmd_t *cmd) {

	if (self->locals.dead) {
		return QUETOO_TICK_MILLIS;
	}

	g_entity_ai_t *ai = self->locals.ai_locals;
	
	if (ai->aim_target.type != AI_GOAL_ENEMY) {
		return QUETOO_TICK_MILLIS * 5;
	}

	// do some acrobatics
	if (self->locals.ground_entity) {

		if (self->client->ps.pm_state.flags & PMF_DUCKED) {
					
			if ((Random() % 32) == 0) { // uncrouch eventually
				cmd->up = 0;
			} else {
				cmd->up = -10;
			}
		} else {

			if ((Random() % 32) == 0) { // randomly crouch
				cmd->up = -10;
			} else if ((Random() % 86) == 0) { // randomly pop, to confuse our enemies
				cmd->up = 10;
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
static void G_Ai_Wander(g_entity_t *self, pm_cmd_t *cmd) {
	g_entity_ai_t *ai = self->locals.ai_locals;

	vec3_t forward;
	AngleVectors((const vec3_t) { 0, ai->wander_angle, 0 }, forward, NULL, NULL);

	vec3_t end;
	VectorMA(self->s.origin, (self->maxs[0] - self->mins[0]) * 2.0, forward, end);

	cm_trace_t tr = gi.Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

	if (tr.fraction < 1.0) { // hit a wall
		vec_t angle = 45 + Randomf() * 45;

		ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
		ai->wander_angle = ClampAngle(ai->wander_angle);
	}
}

/**
 * @brief Move towards our current target
 */
static void G_Ai_MoveToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	g_entity_ai_t *ai = self->locals.ai_locals;
	ai_goal_t *move_target = &ai->move_target;

	vec3_t movement_goal;

	// TODO: node navigation.
	if (move_target->type <= AI_GOAL_NAV) {
		G_Ai_Wander(self, cmd);
	}

	switch (move_target->type) {
	default: {
		vec3_t movement_dir = { 0, ai->wander_angle, 0 };
		AngleVectors(movement_dir, movement_dir, NULL, NULL);
		VectorMA(self->s.origin, 1.0, movement_dir, movement_goal);
	}
		break;
	case AI_GOAL_ITEM:
	case AI_GOAL_ENEMY:
		VectorCopy(move_target->ent->s.origin, movement_goal);
		break;
	}

	vec3_t move_direction;
	VectorSubtract(movement_goal, self->s.origin, move_direction);
	VectorNormalize(move_direction);
	VectorAngles(move_direction, move_direction);
	move_direction[0] = move_direction[2] = 0.0;

	const vec3_t view_direction = { 0.0, self->client->locals.angles[1], 0.0};
	VectorSubtract(view_direction, move_direction, move_direction);

	AngleVectors(move_direction, move_direction, NULL, NULL);

	VectorScale(move_direction, PM_SPEED_RUN, move_direction);

	cmd->forward = move_direction[0];
	cmd->right = move_direction[1];
}

static vec_t AngleMod(const vec_t a) {
	return (360.0 / 65536) * ((int32_t) (a * (65536 / 360.0)) & 65535);
}

static vec_t G_Ai_CalculateAngle(g_entity_t *self, const vec_t speed, vec_t current, const vec_t ideal) {
	current = AngleMod(current);

	if (current == ideal)
		return current;

	vec_t move = ideal - current;

	if (ideal > current) {
		if (move >= 180.0) {
			move = move - 360.0;
		}
	}
	else {
		if (move <= -180.0) {
			move = move + 360.0;
		}
	}

	if (move > 0) {
		if (move > speed) {
			move = speed;
		}
	}
	else {
		if (move < -speed) {
			move = -speed;
		}
	}
	
	return AngleMod(current + move);
}

/**
 * @brief Turn/look towards our current target
 */
static void G_Ai_TurnToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	g_entity_ai_t *ai = self->locals.ai_locals;
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
		ideal_angles[0] += sin(g_level.time / (128.0 + Randomc())) * 3.0;
		ideal_angles[1] += cos(g_level.time / (128.0 + Randomc())) * 3.0;
	}

	for (int32_t i = 0; i < 2; ++i) {
		ideal_angles[i] = G_Ai_CalculateAngle(self, 6.5, self->client->locals.angles[i], ideal_angles[i]);
	}

	vec3_t delta;
	UnpackAngles(self->client->ps.pm_state.delta_angles, delta);
	VectorSubtract(ideal_angles, delta, ideal_angles);

	PackAngles(ideal_angles, cmd->angles);
}

/**
 * @brief
 */
static void G_Ai_ClientThink(g_entity_t *self) {
	g_entity_ai_t *ai = self->locals.ai_locals;
	pm_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msec = QUETOO_TICK_MILLIS;

	if (self->locals.dead) {
		G_Ai_ClearGoal(&ai->aim_target);
		G_Ai_ClearGoal(&ai->move_target);
	}

	// run functional goals
	for (int32_t i = 0; i < MAX_AI_FUNCGOALS; i++) {
		g_ai_funcgoal_t *funcgoal = &ai->funcgoals[i];
		
		if (!funcgoal->think) {
			continue;
		}

		if (funcgoal->nextthink > g_level.time) {
			continue;
		}

		uint32_t time = funcgoal->think(self, &cmd);

		if (time == AI_GOAL_COMPLETE) {
			G_Ai_RemoveFuncGoal(self, funcgoal->think);
		} else {
			funcgoal->nextthink = g_level.time + time;
		}
	}
	
	if (!self->locals.dead) {
		// FIXME: should these be funcgoals?
		// they'd have to be appended to the end of the list always
		// and we can't really enforce that.
		G_Ai_MoveToTarget(self, &cmd);
		G_Ai_TurnToTarget(self, &cmd);
	}

	G_ClientThink(self, &cmd);

	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_Spawn(g_entity_t *self) {

	self->ai = true; // and away we go!

	G_ClientConnect(self, DEFAULT_USER_INFO);
	G_ClientUserInfoChanged(self, DEFAULT_USER_INFO);
	G_ClientBegin(self);

	self->locals.ai_locals = &g_ai_locals[self->s.number - 1];
	memset(self->locals.ai_locals, 0, sizeof(g_entity_ai_t));

	G_Ai_AddFuncGoal(self, G_Ai_FuncGoal_Hunt, 0);
	G_Ai_AddFuncGoal(self, G_Ai_FuncGoal_Weaponry, 0);
	G_Ai_AddFuncGoal(self, G_Ai_FuncGoal_Acrobatics, 0);

	gi.Debug("Spawned %s at %s", self->client->locals.persistent.net_name, vtos(self->s.origin));

	self->locals.Think = G_Ai_ClientThink;
	self->locals.next_think = g_level.time + QUETOO_TICK_MILLIS;
}

/**
 * @brief
 */
static void G_Ai_Add_f(void) {
	int32_t i, count = 1;

	if (gi.Argc() > 1) {
		count = Clamp(atoi(gi.Argv(1)), 1, sv_max_clients->integer);
	}

	for (i = 0; i < count; i++) {

		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (!ent->in_use) {
				G_Ai_Spawn(ent);
				break;
			}
		}

		if (j > sv_max_clients->integer) {
			gi.Print("No client slots available, increase sv_max_clients\n");
			break;
		}
	}
}

/**
 * @brief
 */
static void G_Ai_Remove_f(void) {
	int32_t i, count = 1;

	if (gi.Argc() > 1) {
		count = Clamp(atoi(gi.Argv(1)), 1, sv_max_clients->integer);
	}
	
	for (i = 0; i < count; i++) {

		g_entity_t *ent = &g_game.entities[1];
		int32_t j;

		for (j = 1; j <= sv_max_clients->integer; j++, ent++) {
			if (ent->in_use && ent->ai) {
				G_ClientDisconnect(ent);
				break;
			}
		}
	}
}

/**
 * @brief
 */
void G_Ai_Init(void) {
	
	gi.Cmd("g_ai_add", G_Ai_Add_f, CMD_GAME, "Add one or more AI to the game");
	gi.Cmd("g_ai_remove", G_Ai_Remove_f, CMD_GAME, "Remove one or more AI from the game");

	g_ai_passive = gi.Cvar("g_ai_passive", "0", 0, "Whether the bots will attack or not");

	g_ai_locals = (g_entity_ai_t *) gi.Malloc(sizeof(g_entity_ai_t) * sv_max_clients->integer, MEM_TAG_AI);
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
