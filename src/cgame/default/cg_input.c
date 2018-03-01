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

button_t cg_buttons[4];

static cvar_t *cg_run;

typedef struct {
	vec3_t prev, next, kick;
	uint32_t timestamp;
	uint32_t interval;
} cg_kick_t;

static cg_kick_t cg_kick;

/**
 * @brief Parse a view kick message from the server, updating the interpolation target.
 */
void Cg_ParseViewKick(void) {

	const vec3_t kick = { cgi.ReadAngle(), 0.0, cgi.ReadAngle() };

	VectorCopy(cg_kick.kick, cg_kick.prev);
	VectorAdd(cg_kick.prev, kick, cg_kick.next);

	cg_kick.timestamp = cgi.client->unclamped_time;
	cg_kick.interval = 64;
}

/**
 * @brief Applies damage kick for the current command, ensuring that kick affects the player's aim.
 */
static void Cg_ViewKick(const pm_cmd_t *cmd) {

	if (cg_kick.timestamp > cgi.client->unclamped_time) {
		memset(&cg_kick, 0, sizeof(cg_kick));
	}

	if (cgi.client->previous_frame) {
		const player_state_t *ps0 = &cgi.client->previous_frame->ps;
		const player_state_t *ps1 = &cgi.client->frame.ps;

		if (ps0->pm_state.type == PM_DEAD && ps1->pm_state.type != PM_DEAD) {
			cgi.Debug("Respawned, clearing kick %s\n", vtos(cg_kick.kick));
			memset(&cg_kick, 0, sizeof(cg_kick));
		} else {
			vec3_t delta0, delta1;

			UnpackAngles(ps0->pm_state.delta_angles, delta0);
			UnpackAngles(ps1->pm_state.delta_angles, delta1);

			if (!VectorCompare(delta0, delta1)) {
				static int32_t frame;

				if (cgi.client->frame.frame_num != frame) {
					cgi.Debug("Delta kick %s\n", vtos(cg_kick.kick));

					VectorCopy(cg_kick.kick, cg_kick.next);
					VectorClear(cg_kick.kick);
					VectorClear(cg_kick.prev);

					cg_kick.timestamp = cgi.client->unclamped_time;
					cg_kick.interval = 1;

					frame = cgi.client->frame.frame_num;
				}
			}
		}
	}

	const uint32_t delta = cgi.client->unclamped_time - cg_kick.timestamp;
	if (delta < cg_kick.interval) {
		const vec_t frac = Min(delta, cmd->msec) / (vec_t) cg_kick.interval;

		vec3_t kick;
		VectorSubtract(cg_kick.next, cg_kick.prev, kick);
		VectorScale(kick, frac, kick);

		VectorAdd(cg_kick.kick, kick, cg_kick.kick);
		VectorAdd(cgi.client->angles, kick, cgi.client->angles);

	} else if (!VectorCompare(cg_kick.kick, vec3_origin)) {

		if (cgi.client->frame.ps.pm_state.type == PM_DEAD) {
			return;
		}

		const vec_t len = VectorLength(cg_kick.kick);
		if (len < 0.1) {
			VectorSubtract(cgi.client->angles, cg_kick.kick, cgi.client->angles);
			memset(&cg_kick, 0, sizeof(cg_kick));
		} else {

			VectorCopy(cg_kick.kick, cg_kick.prev);
			VectorClear(cg_kick.next);

			cg_kick.timestamp = cgi.client->unclamped_time;
			cg_kick.interval = 240;
		}
	}
}

/**
 * @brief
 */
static void Cg_WeaponKick(const pm_cmd_t *cmd) {
	static vec_t kick;

	if (cgi.client->third_person) {
		return;
	}

	const cl_entity_t *ent = Cg_Self();

	if (!ent) {
		return;
	}

	vec_t delta = 0.0;

	if (ent->animation1.animation == ANIM_TORSO_ATTACK1 && ent->animation1.fraction <= 0.33) {

		const player_state_t *ps = &cgi.client->frame.ps;

		vec_t degrees, interval = 64.0;

		switch (ps->stats[STAT_WEAPON_TAG] & 0xFF) {
			case WEAPON_BLASTER:
				degrees = 1.0;
				break;
			case WEAPON_SHOTGUN:
				degrees = 1.5;
				break;
			case WEAPON_SUPER_SHOTGUN:
				degrees = 2.0;
				break;
			case WEAPON_MACHINEGUN:
				degrees = 6.0;
				interval = 1372.0;
				break;
			case WEAPON_HAND_GRENADE:
				degrees = 2.0;
				break;
			case WEAPON_GRENADE_LAUNCHER:
				degrees = 2.6;
				break;
			case WEAPON_ROCKET_LAUNCHER:
				degrees = 2.4;
				break;
			case WEAPON_HYPERBLASTER:
				degrees = 5.0;
				interval = 1176.0;
				break;
			case WEAPON_LIGHTNING:
				degrees = 2.0;
				interval = 784.0;
				break;
			case WEAPON_RAILGUN:
				degrees = 5.0;
				break;
			case WEAPON_BFG10K:
				degrees = 20.0;
				break;
			default:
				return;
		}

		delta = Min(degrees - kick, degrees * (cmd->msec / interval));

	} else {
		delta = -Min(kick, kick * (cmd->msec / 196.0));
	}

	kick += delta;
	cgi.client->angles[PITCH] -= delta;
}

/**
 * @brief Augments the view offset and angles for the specified command.
 * @see Cl_Look(pm_cmd_t)
 */
void Cg_Look(pm_cmd_t *cmd) {

	Cg_ViewKick(cmd);

	Cg_WeaponKick(cmd);
}

/**
 * @brief Accumulate movement and button interactions for the specified command.
 */
void Cg_Move(pm_cmd_t *cmd) {

	if (in_attack.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
		if (!((in_attack.state & BUTTON_STATE_DOWN) && Cg_AttemptSelectWeapon(&cgi.client->frame.ps))) {
			cmd->buttons |= BUTTON_ATTACK;
		}
	}

	if (in_hook.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
		cmd->buttons |= BUTTON_HOOK;
	}

	if (in_score.state & (BUTTON_STATE_HELD | BUTTON_STATE_DOWN)) {
		cmd->buttons |= BUTTON_SCORE;
	}

	in_attack.state &= ~BUTTON_STATE_DOWN;

	if (cg_run->value) {
		if (in_speed.state & BUTTON_STATE_HELD) {
			cmd->buttons |= BUTTON_WALK;
		}
	} else {
		if (!(in_speed.state & BUTTON_STATE_HELD)) {
			cmd->buttons |= BUTTON_WALK;
		}
	}

	if (cgi.client->frame.ps.stats[STAT_CHASE]) {
		if (cmd->up) {
			static uint32_t time;

			if (time > cgi.client->unclamped_time) {
				time = 0;
			}

			if (cgi.client->unclamped_time - time > 200) {
				cgi.ToggleCvar(cg_third_person_chasecam->name);
				time = cgi.client->unclamped_time;
			}
		}
	}
}

/**
 * @brief Clear button states.
 */
void Cg_ClearInput(void) {
	memset(&cg_kick, 0, sizeof(cg_kick));
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

static void Cg_Score_down_f(void) {
	cgi.KeyDown(&in_score);
}

static void Cg_Score_up_f(void) {
	cgi.KeyUp(&in_score);
}

/**
 * @brief Init cgame input system.
 */
void Cg_InitInput(void) {

	cg_run = cgi.AddCvar("cg_run", "1", CVAR_ARCHIVE, NULL);

	cgi.AddCmd("+speed", Cg_Speed_down_f, CMD_CGAME, NULL);
	cgi.AddCmd("-speed", Cg_Speed_up_f, CMD_CGAME, NULL);
	cgi.AddCmd("+attack", Cg_Attack_down_f, CMD_CGAME, NULL);
	cgi.AddCmd("-attack", Cg_Attack_up_f, CMD_CGAME, NULL);
	cgi.AddCmd("+hook", Cg_Hook_down_f, CMD_CGAME, NULL);
	cgi.AddCmd("-hook", Cg_Hook_up_f, CMD_CGAME, NULL);
	cgi.AddCmd("+score", Cg_Score_down_f, CMD_CGAME, NULL);
	cgi.AddCmd("-score", Cg_Score_up_f, CMD_CGAME, NULL);

	Cg_ClearInput();
}
