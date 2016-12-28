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

#include "cg_local.h"
#include "game/default/bg_pmove.h"

static button_t cg_buttons[3];
#define in_speed cg_buttons[0]
#define in_attack cg_buttons[1]
#define in_hook cg_buttons[2]

static cvar_t *cg_run;

typedef struct {
	vec3_t prev, next, kick;
	uint32_t timestamp;
	uint32_t interval;
} cg_view_kick_t;

static cg_view_kick_t cg_view_kick;

/**
 * @brief Parse a view kick message from the server, updating the interpolation target.
 */
void Cg_ParseViewKick(void) {

	const vec3_t kick = { cgi.ReadAngle(), 0.0, cgi.ReadAngle() };

	VectorCopy(cg_view_kick.kick, cg_view_kick.prev);
	VectorAdd(cg_view_kick.prev, kick, cg_view_kick.next);

	cg_view_kick.timestamp = cgi.client->unclamped_time;
	cg_view_kick.interval = 64;
}

/**
 * @brief Augments the view offset and angles for the specified command.
 * @see Cl_Look(pm_cmd_t)
 */
void Cg_Look(pm_cmd_t *cmd) {

	if (cg_view_kick.timestamp > cgi.client->unclamped_time) {
		memset(&cg_view_kick, 0, sizeof(cg_view_kick));
	}

	const uint32_t delta = cgi.client->unclamped_time - cg_view_kick.timestamp;
	if (delta < cg_view_kick.interval) {
		const vec_t frac = Min(delta, cmd->msec) / (vec_t) cg_view_kick.interval;

		vec3_t kick;
		VectorSubtract(cg_view_kick.next, cg_view_kick.prev, kick);
		VectorScale(kick, frac, kick);

		VectorAdd(cg_view_kick.kick, kick, cg_view_kick.kick);
		VectorAdd(cgi.client->angles, kick, cgi.client->angles);
		
	} else if (!VectorCompare(cg_view_kick.kick, vec3_origin)) {

		const vec_t len = VectorLength(cg_view_kick.kick);
		if (len < 0.1) {
			VectorSubtract(cgi.client->angles, cg_view_kick.kick, cgi.client->angles);
			VectorClear(cg_view_kick.kick);
		} else {

			VectorCopy(cg_view_kick.kick, cg_view_kick.prev);
			VectorClear(cg_view_kick.next);

			cg_view_kick.timestamp = cgi.client->unclamped_time;
			cg_view_kick.interval = 240;
		}
	}
}

/**
 * @brief Accumulate movement and button interactions for the specified command.
 */
void Cg_Move(pm_cmd_t *cmd) {

	if (in_attack.state & 3) {
		cmd->buttons |= BUTTON_ATTACK;
	}

	if (in_hook.state & 3) {
		cmd->buttons |= BUTTON_HOOK;
	}

	in_attack.state &= ~2;
	in_hook.state &= ~2;

	if (cg_run->value) {
		if (in_speed.state & 1) {
			cmd->buttons |= BUTTON_WALK;
		}
	} else {
		if (!(in_speed.state & 1)) {
			cmd->buttons |= BUTTON_WALK;
		}
	}
}

/**
 * @brief Clear button states.
 */
void Cg_ClearInput(void) {

	memset(cg_buttons, 0, sizeof(cg_buttons));
}

static void Cg_Speed_down_f(void) {
	cgi.KeyDown(&in_speed);
}

static void Cg_Speed_up_f(void) {
	cgi.KeyUp(&in_speed);
}

static void Cg_Attack_down_f(void) {
	cgi.KeyDown(&in_attack);
}

static void Cg_Attack_up_f(void) {
	cgi.KeyUp(&in_attack);
}

static void Cg_Hook_down_f(void) {
	cgi.KeyDown(&in_hook);
}

static void Cg_Hook_up_f(void) {
	cgi.KeyUp(&in_hook);
}

/**
 * @brief Init cgame input system.
 */
void Cg_InitInput(void) {

	cg_run = cgi.Cvar("cg_run", "1", CVAR_ARCHIVE, NULL);

	cgi.Cmd("+speed", Cg_Speed_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-speed", Cg_Speed_up_f, CMD_CGAME, NULL);
	cgi.Cmd("+attack", Cg_Attack_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-attack", Cg_Attack_up_f, CMD_CGAME, NULL);
	cgi.Cmd("+hook", Cg_Hook_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-hook", Cg_Hook_up_f, CMD_CGAME, NULL);

	Cg_ClearInput();
}
