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
 * @brief
 */
static void G_Ai_ClientThink(g_entity_t *self) {
	pm_cmd_t cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msec = QUETOO_TICK_MILLIS;

	if (self->locals.dead) {
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
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
