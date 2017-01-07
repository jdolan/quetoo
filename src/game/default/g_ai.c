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

		for (int32_t i = 1; i <= sv_max_clients->integer; i++) {

			g_entity_t *ent = &g_game.entities[i];

			if (ent != self && ent->in_use) {

				cm_trace_t tr = gi.Trace(self->s.origin, ent->s.origin, vec3_origin, vec3_origin, self, MASK_CLIP_PROJECTILE);

				if (tr.fraction < 1.0 && tr.ent == ent) {

					player = ent;
					break;
				}
			}
		}

		if (player) {
			vec3_t dir, angles;

			VectorSubtract(player->s.origin, self->s.origin, dir);
			VectorNormalize(dir);
			VectorAngles(dir, angles);
			VectorSubtract(angles, self->client->locals.angles, angles);

			u16vec3_t delta;
			PackAngles(angles, delta);

			VectorAdd(delta, self->client->ps.pm_state.delta_angles, self->client->ps.pm_state.delta_angles);

			cmd.forward = 100;

			if ((Random() % 128) == 0) {
				cmd.up = 10;
			}

			cmd.buttons |= BUTTON_ATTACK;
		} else {
			vec3_t forward, end;

			AngleVectors((const vec3_t) { 0, self->client->locals.angles[1], 0 }, forward, NULL, NULL);

			VectorMA(self->s.origin, 24, forward, end);

			cm_trace_t tr = gi.Trace(self->s.origin, end, vec3_origin, vec3_origin, self, MASK_CLIP_PLAYER);

			if (tr.fraction < 1.0) {
				cmd.forward = 100;
			} else {
				u16vec3_t delta;
				
				PackAngles((const vec3_t) {
					0, 90 + Randomc() * 45, 0
				}, delta);
				
				VectorAdd(delta, self->client->ps.pm_state.delta_angles, self->client->ps.pm_state.delta_angles);
			}
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
}

/**
 * @brief
 */
void G_Ai_Shutdown(void) {

}
