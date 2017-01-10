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
 * @brief Ignore ourselves, clipping to the correct mask based on our status.
 */
static cm_trace_t G_Ai_ClientMove_Trace(const vec3_t start, const vec3_t end, const vec3_t mins,
                                     const vec3_t maxs) {

	const g_entity_t *self = g_level.current_entity;

	if (self->locals.dead) {
		return gi.Trace(start, end, mins, maxs, self, MASK_CLIP_CORPSE);
	} else {
		return gi.Trace(start, end, mins, maxs, self, MASK_CLIP_PLAYER);
	}
}

/**
 * @brief
 */
static void G_Ai_Turn(g_entity_t *self, const vec_t angle) {
	static u16vec3_t delta;
				
	PackAngles((const vec3_t) {
		0, angle, 0
	}, delta);
				
	VectorAdd(delta, self->client->ps.pm_state.delta_angles, self->client->ps.pm_state.delta_angles);
}

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

/*
	pm_move_t pm;
	pm.s = self->client->ps.pm_state;

	VectorCopy(self->s.origin, pm.s.origin);

	if (self->client->locals.hook_pull) {

		if (self->client->locals.persistent.hook_style == HOOK_SWING) {
			pm.s.type = PM_HOOK_SWING;
		} else {
			pm.s.type = PM_HOOK_PULL;
		}
	} else {
		VectorCopy(self->locals.velocity, pm.s.velocity);
	}

	pm.cmd = *cmd;
	pm.ground_entity = self->locals.ground_entity;
	pm.hook_pull_speed = g_hook_pull_speed->value;

	pm.PointContents = gi.PointContents;
	pm.Trace = G_Ai_ClientMove_Trace;

	pm.Debug = gi.PmDebug_;

	// perform a move; predict a few frames ahead
	for (int32_t i = 0; i < 4; ++i) {
		Pm_Move(&pm);
	}
	
	vec3_t move_dir;
	VectorSubtract(pm.s.origin, self->s.origin, move_dir);
	vec_t move_dist = VectorLength(move_dir);

	 else if (self->locals.ground_entity && !pm.ground_entity) { // predicted ground is gone
		if ((Random() % 64) == 0) {
			cmd->up = 10; // randomly decide to dive off the predicted cliff
		} else {
			G_Ai_Turn(self, 90 + Randomc() * 45); // miss the cliff
		}
	}
	else if (move_dist <= 20) { // predicted move went nowhere
		G_Ai_Turn(self, Randomf() * 4);
	}
*/

	vec3_t forward;
	AngleVectors((const vec3_t) { 0, ai->wander_angle, 0 }, forward, NULL, NULL);

	vec3_t end;
	VectorMA(self->s.origin, (self->maxs[0] - self->mins[0]) * 2.0, forward, end);

	cm_trace_t tr = gi.Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

	if (tr.fraction < 1.0) { // hit a wall
		vec_t angle = 45 + Randomf() * 45;

		ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
	}
	
	AngleVectors((const vec3_t) { 0, ai->wander_angle, 0 }, forward, NULL, NULL);

	VectorScale(forward, PM_SPEED_RUN, forward);

	cmd->forward = forward[0];
	cmd->right = forward[1];
}

/**
 * @brief Move towards our current target
 */
static void G_Ai_MoveToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	g_entity_ai_t *ai = self->locals.ai_locals;
	ai_goal_t *move_target = &ai->move_target;

	// TODO: node navigation.
	if (move_target->type <= AI_GOAL_NAV ||
		move_target->type == AI_GOAL_GHOST) {
		G_Ai_Wander(self, cmd);
		return;
	}

	vec3_t move_direction;
	VectorSubtract(move_target->ent->s.origin, self->s.origin, move_direction);
	VectorNormalize(move_direction);

	VectorScale(move_direction, PM_SPEED_RUN, move_direction);

	cmd->forward = move_direction[0];
	cmd->right = move_direction[1];
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

		VectorSubtract(aim_target->ent->s.origin, self->s.origin, aim_direction);
		VectorNormalize(aim_direction);
		VectorAngles(aim_direction, ideal_angles);

		// fuzzy angle
		ideal_angles[0] += sin(g_level.time / (128.0 + Randomc())) * 1.5;
		ideal_angles[1] += cos(g_level.time / (128.0 + Randomc())) * 1.5;
	}

	VectorSubtract(ideal_angles, self->client->locals.angles, ideal_angles);
			
	u16vec3_t delta;
	PackAngles(ideal_angles, delta);

	VectorAdd(delta, self->client->ps.pm_state.delta_angles, self->client->ps.pm_state.delta_angles);
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

	/*if (self->locals.dead) {
		cmd.buttons = self->client->locals.buttons ^ BUTTON_ATTACK;
	} else {
		
		g_entity_t *player = NULL;

		if (self->locals.enemy && G_Ai_Can_Target(self, self->locals.enemy)) {

			player = self->locals.enemy;
		} else {
			for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

				g_entity_t *ent = &g_game.entities[i];

				if (G_Ai_Can_Target(self, ent)) {
					player = ent;
					break;
				}
			}
		}

		if (player) {
			vec3_t dir, angles;

			self->locals.enemy = player;

			VectorSubtract(player->s.origin, self->s.origin, dir);
			VectorNormalize(dir);
			VectorAngles(dir, angles);
			VectorSubtract(angles, self->client->locals.angles, angles);
			
			angles[0] += Randomc() * 5.0;
			angles[1] += Randomc() * 5.0;

			u16vec3_t delta;
			PackAngles(angles, delta);

			VectorAdd(delta, self->client->ps.pm_state.delta_angles, self->client->ps.pm_state.delta_angles);

			cmd.forward = 500;

			if (self->locals.ground_entity) {

				if (self->client->ps.pm_state.flags & PMF_DUCKED) {
					
					if ((Random() % 32) == 0) { // uncrouch eventually
						cmd.up = 0;
					}
				} else {

					if ((Random() % 32) == 0) { // randomly crouch
						cmd.up = -10;
					} else if ((Random() % 86) == 0) { // randomly pop, to confuse our enemies
						cmd.up = 10;
					}
				}
			} else {

				cmd.up = 0;
			}

			cmd.buttons |= BUTTON_ATTACK;
		} else {
			vec3_t forward, end;

			cmd.forward = 300;

			static pm_move_t pm;

			pm.s = self->client->ps.pm_state;

			VectorCopy(self->s.origin, pm.s.origin);

			if (self->client->locals.hook_pull) {

				if (self->client->locals.persistent.hook_style == HOOK_SWING) {
					pm.s.type = PM_HOOK_SWING;
				} else {
					pm.s.type = PM_HOOK_PULL;
				}
			} else {
				VectorCopy(self->locals.velocity, pm.s.velocity);
			}

			pm.cmd = cmd;
			pm.ground_entity = self->locals.ground_entity;
			pm.hook_pull_speed = g_hook_pull_speed->value;

			pm.PointContents = gi.PointContents;
			pm.Trace = G_Ai_ClientMove_Trace;

			pm.Debug = gi.PmDebug_;

			// perform a move; predict a few frames ahead
			for (int32_t i = 0; i < 4; ++i) {
				Pm_Move(&pm);
			}

			AngleVectors((const vec3_t) { 0, self->client->locals.angles[1], 0 }, forward, NULL, NULL);

			VectorMA(self->s.origin, 28, forward, end);

			cm_trace_t tr = gi.Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

			vec3_t move_dir;
			VectorSubtract(pm.s.origin, self->s.origin, move_dir);
			vec_t move_dist = VectorLength(move_dir);

			if (tr.fraction < 1.0) { // hit a wall
				G_Ai_Turn(self, 90 + Randomc() * 45);
			} else if (self->locals.ground_entity && !pm.ground_entity) { // predicted ground is gone
				if ((Random() % 64) == 0) {
					cmd.up = 10; // randomly decide to dive off the predicted cliff
				} else {
					G_Ai_Turn(self, 90 + Randomc() * 45); // miss the cliff
				}
			} else if (move_dist <= 20) { // predicted move went nowhere
				G_Ai_Turn(self, Randomf() * 4);
			}

			self->locals.enemy = NULL;
		}
	}*/

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
