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

static cvar_t *cg_run;

// buttons local to cgame
static button_t cg_buttons[3];
#define in_speed cg_buttons[0]
#define in_attack cg_buttons[1]
#define in_hook cg_buttons[2]

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
 * @brief Accumulate movement and button interactions for the specified command.
 * @details This is called at ~60hz regardless of the client's framerate. This is to avoid micro-
 * commands, which introduce prediction errors (screen jitter).
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

/**
 * @brief Init cgame input system.
 */
void Cg_InitInput(void) {

	cg_run = cgi.Cvar("cl_run", "1", CVAR_ARCHIVE, NULL);
	
	cgi.Cmd("+speed", Cg_Speed_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-speed", Cg_Speed_up_f, CMD_CGAME, NULL);
	cgi.Cmd("+attack", Cg_Attack_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-attack", Cg_Attack_up_f, CMD_CGAME, NULL);
	cgi.Cmd("+hook", Cg_Hook_down_f, CMD_CGAME, NULL);
	cgi.Cmd("-hook", Cg_Hook_up_f, CMD_CGAME, NULL);

	Cg_ClearInput();
}