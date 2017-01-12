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

ai_level_t ai_level;
static cvar_t *sv_max_clients;
cvar_t *ai_passive;

/**
 * @brief Game imports ptr.
 */
ai_import_t aii;

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
 * @brief Type for a single skin string
 */
typedef char ai_skin_t[MAX_QPATH];

/**
 * @brief List of AI skins.
 */
static GArray *ai_skins;

/**
 * @brief Fs_EnumerateFunc for resolving available skins for a given model.
 */
static void Ai_EnumerateSkins(const char *path, void *data) {
	char name[MAX_QPATH];
	char *s = strstr(path, "players/");

	if (s) {
		StripExtension(s + strlen("players/"), name);

		if (g_str_has_suffix(name, "_i")) {
			name[strlen(name) - strlen("_i")] = '\0';

			for (size_t i = 0; i < ai_skins->len; i++) {

				if (g_strcmp0(g_array_index(ai_skins, ai_skin_t, i), name) == 0) {
					return;
				}
			}

			g_array_append_val(ai_skins, name);
		}
	}
}

/**
 * @brief Fs_EnumerateFunc for resolving available models.
 */
static void Ai_EnumerateModels(const char *path, void *data) {
	
	aii.gi->EnumerateFiles(va("%s/*.tga", path), Ai_EnumerateSkins, NULL);
}

/**
 * @brief
 */
static void Ai_InitSkins(void) {

	ai_skins = g_array_new(false, false, sizeof(ai_skin_t));
	aii.gi->EnumerateFiles("players/*", Ai_EnumerateModels, NULL);
}

/**
 * @brief
 */
static const char ai_names[][MAX_USER_INFO_VALUE] = {
	"Stroggo",
	"Enforcer",
	"Berserker",
	"Gunner",
	"Gladiator",
	"Makron",
	"Brain"
};

/**
 * @brief
 */
static const uint32_t ai_names_count = lengthof(ai_names);

/**
 * @brief Integers for current name index in g_ai_names table
 * and number of times we've wrapped around it.
 */
static uint32_t ai_name_index;
static uint32_t ai_name_suffix;

/**
 * @brief Create the user info for the specified bot entity.
 */
void Ai_GetUserInfo(const g_entity_t *self, char *userinfo) {
	g_strlcpy(userinfo, DEFAULT_BOT_INFO, MAX_USER_INFO_STRING);
	SetUserInfo(userinfo, "skin", g_array_index(ai_skins, ai_skin_t, Random() % ai_skins->len));
	SetUserInfo(userinfo, "color", va("%i", Random() % 256));
	
	if (ai_name_suffix == 0) {
		SetUserInfo(userinfo, "name", ai_names[ai_name_index]);
	} else {
		SetUserInfo(userinfo, "name", va("%s %i", ai_names[ai_name_index], ai_name_suffix + 1));
	}

	ai_name_index++;

	if (ai_name_index == ai_names_count) {
		ai_name_index = 0;
		ai_name_suffix++;
	}
}

/**
 * @brief
 */
static _Bool Ai_Can_Target(const g_entity_t *self, const g_entity_t *other) {

	if (ai_passive->integer) {
		return false;
	}

	if (other == self || !other->client || !other->in_use || other->solid == SOLID_DEAD || (other->sv_flags & SVF_NO_CLIENT)) {
		return false;
	}

	cm_trace_t tr = aii.gi->Trace(self->s.origin, other->s.origin, vec3_origin, vec3_origin, self, MASK_CLIP_PROJECTILE);

	if (tr.fraction < 1.0 && tr.ent == other) {
		return true;
	}

	return false;
}

#ifdef AI_UNUSED
/**
 * @brief Executes a console command as if the bot typed it.
 */
static void Ai_Command(g_entity_t *self, const char *command) {

	gi.TokenizeString(command);
	G_ClientCommand(self);
}
#endif

/**
 * @brief Calculate a priority for the specified target.
 */
static vec_t Ai_EnemyPriority(g_entity_t *self, g_entity_t *target, const _Bool visible) {

	// TODO: all of this function. Enemies with more powerful weapons need a higher weight.
	// Enemies with lower health need a higher weight. Enemies carrying flags need an even higher weight.

	return 10.0;
}

/**
 * @brief Yields a pointer to the edict by the given number by negotiating the
 * edicts array based on the reported size of g_entity_t.
 */
#define ENTITY_FOR_NUM(n) ( (g_entity_t *) ((byte *) aii.ge->entities + aii.ge->entity_size * (n)) )

/**
 * @brief Funcgoal that controls the AI's lust for blood
 */
static uint32_t Ai_FuncGoal_Hunt(g_entity_t *self, pm_cmd_t *cmd) {
	
	if (self->solid == SOLID_DEAD) {
		return QUETOO_TICK_MILLIS;
	}

	ai_locals_t *ai = Ai_GetLocals(self);

	// see if we're already hunting
	if (ai->aim_target.type == AI_GOAL_ENEMY) {

		// check to see if the enemy has gone out of our line of sight
		if (!Ai_Can_Target(self, ai->aim_target.ent)) {

			// enemy went out of our LOS, aim at where they were for a while
			if (ai->aim_target.ent->solid != SOLID_DEAD && !(ai->aim_target.ent->sv_flags & SVF_NO_CLIENT)) {
				Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_GHOST, Ai_EnemyPriority(self, ai->aim_target.ent, false), ai->aim_target.ent);
				VectorCopy(ai->aim_target.ent->s.origin, ai->ghost_position);
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
		
		return QUETOO_TICK_MILLIS * 3;
	}

	// we lost our enemy, start looking for a new one
	g_entity_t *player = NULL;

	for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

		g_entity_t *ent = ENTITY_FOR_NUM(i);

		if (Ai_Can_Target(self, ent)) {

			player = ent;
			break;
		}
	}

	// found one, set it up
	if (player) {

		Ai_SetEntityGoal(&ai->aim_target, AI_GOAL_ENEMY, Ai_EnemyPriority(self, player, true), player);

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
	if (G_Ai_GroundEntity(self)) {

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
static void Ai_Wander(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);

	vec3_t forward;
	AngleVectors((const vec3_t) { 0, ai->wander_angle, 0 }, forward, NULL, NULL);

	vec3_t end;
	VectorMA(self->s.origin, (self->maxs[0] - self->mins[0]) * 2.0, forward, end);

	cm_trace_t tr = aii.gi->Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

	if (tr.fraction < 1.0) { // hit a wall
		vec_t angle = 45 + Randomf() * 45;

		ai->wander_angle += (Randomf() < 0.5) ? -angle : angle;
		ai->wander_angle = ClampAngle(ai->wander_angle);
	}
}

/**
 * @brief Move towards our current target
 */
static void Ai_MoveToTarget(g_entity_t *self, pm_cmd_t *cmd) {
	ai_locals_t *ai = Ai_GetLocals(self);
	ai_goal_t *move_target = &ai->move_target;

	vec3_t movement_goal;

	// TODO: node navigation.
	if (move_target->type <= AI_GOAL_NAV) {
		Ai_Wander(self, cmd);
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

	const vec3_t view_direction = { 0.0, G_Ai_ViewAngles(self)[1], 0.0};
	VectorSubtract(view_direction, move_direction, move_direction);

	AngleVectors(move_direction, move_direction, NULL, NULL);

	VectorScale(move_direction, PM_SPEED_RUN, move_direction);

	cmd->forward = move_direction[0];
	cmd->right = move_direction[1];
}

static vec_t AngleMod(const vec_t a) {
	return (360.0 / 65536) * ((int32_t) (a * (65536 / 360.0)) & 65535);
}

static vec_t Ai_CalculateAngle(g_entity_t *self, const vec_t speed, vec_t current, const vec_t ideal) {
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
		ideal_angles[0] += sin(ai_level.time / (128.0 + Randomc())) * 3.0;
		ideal_angles[1] += cos(ai_level.time / (128.0 + Randomc())) * 3.0;
	}

	const vec_t *view_angles = G_Ai_ViewAngles(self);

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
void Ai_Think(g_entity_t *self, pm_cmd_t *cmd) {

	ai_locals_t *ai = Ai_GetLocals(self);

	if (self->solid == SOLID_DEAD) {
		Ai_ClearGoal(&ai->aim_target);
		Ai_ClearGoal(&ai->move_target);
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
		Ai_MoveToTarget(self, cmd);
		Ai_TurnToTarget(self, cmd);
	}
}

/**
 * @brief Called when an AI is first spawned and is ready to go.
 */
void Ai_Begin(g_entity_t *self) {

	ai_locals_t *ai = Ai_GetLocals(self);
	memset(ai, 0, sizeof(*ai));

	Ai_AddFuncGoal(self, Ai_FuncGoal_Hunt, 0);
	Ai_AddFuncGoal(self, Ai_FuncGoal_Weaponry, 0);
	Ai_AddFuncGoal(self, Ai_FuncGoal_Acrobatics, 0);
}

/**
 * @brief Called every frame.
 */
void Ai_Frame(void) {

	ai_level.frame_num++;
	ai_level.time = ai_level.frame_num * QUETOO_TICK_MILLIS;
}

/**
 * @brief Initializes the AI subsystem.
 */
void Ai_Init(ai_import_t *import) {
	aii = *import;

	sv_max_clients = aii.gi->Cvar("sv_max_clients", 0, 0, "");

	ai_passive = aii.gi->Cvar("ai_passive", "0", 0, "Whether the bots will attack or not");
	ai_locals = (ai_locals_t *) aii.gi->Malloc(sizeof(ai_locals_t) * sv_max_clients->integer, MEM_TAG_AI);

	Ai_InitSkins();
}

/**
 * @brief Shuts down the AI subsystem.
 */
void Ai_Shutdown(void) {
	aii.gi->FreeTag(MEM_TAG_AI);
}
