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

#include "cl_local.h"

static cvar_t *cl_forward_speed;
static cvar_t *cl_pitch_speed;
static cvar_t *cl_right_speed;
static cvar_t *cl_up_speed;
static cvar_t *cl_yaw_speed;

cvar_t *m_interpolate;
cvar_t *m_invert;
cvar_t *m_sensitivity;
cvar_t *m_sensitivity_zoom;
cvar_t *m_pitch;
cvar_t *m_yaw;

static button_t cl_buttons[10];
#define in_left cl_buttons[0]
#define in_right cl_buttons[1]
#define in_forward cl_buttons[2]
#define in_back cl_buttons[3]
#define in_look_up cl_buttons[4]
#define in_look_down cl_buttons[5]
#define in_move_left cl_buttons[6]
#define in_move_right cl_buttons[7]
#define in_up cl_buttons[8]
#define in_down cl_buttons[9]

/**
 * @brief
 */
void Cl_KeyDown(button_t *b) {
	SDL_Scancode k;

	const char *c = Cmd_Argv(1);
	if (c[0]) {
		k = atoi(c);
	} else {
		k = SDL_NUM_SCANCODES; // typed manually at the console for continuous down
	}

	if (k == b->keys[0] || k == b->keys[1]) {
		return; // repeating key
	}

	if (b->keys[0] == SDL_SCANCODE_UNKNOWN) {
		b->keys[0] = k;
	} else if (b->keys[1] == SDL_SCANCODE_UNKNOWN) {
		b->keys[1] = k;
	} else {
		Com_Debug(DEBUG_CLIENT, "3 keys down for button\n");
		return;
	}

	if (b->state & BUTTON_STATE_HELD) {
		return; // still down
	}

	// save the down time so that we can calculate fractional time later
	b->down_time = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0) ? : cl.unclamped_time;

	// and indicate that the key is down
	b->state |= (BUTTON_STATE_HELD | BUTTON_STATE_DOWN);
}

/**
 * @brief
 */
void Cl_KeyUp(button_t *b) {

	if (Cmd_Argc() < 2) { // typed manually at the console, assume for un-sticking, so clear all
		b->keys[0] = b->keys[1] = 0;
		return;
	}

	const SDL_Scancode k = atoi(Cmd_Argv(1));

	if (b->keys[0] == k) {
		b->keys[0] = SDL_SCANCODE_UNKNOWN;
	} else if (b->keys[1] == k) {
		b->keys[1] = SDL_SCANCODE_UNKNOWN;
	} else {
		return; // key up without corresponding down
	}

	if (b->keys[0] || b->keys[1]) {
		return; // some other key is still holding it down
	}

	if (!(b->state & BUTTON_STATE_HELD)) {
		return; // still up (this should not happen)
	}

	// save timestamp
	const char *t = Cmd_Argv(2);
	const uint32_t up_time = atoi(t);
	if (up_time) {
		b->msec += up_time - b->down_time;
	} else {
		b->msec += 10;
	}

	b->state &= ~(BUTTON_STATE_HELD | BUTTON_STATE_DOWN); // now up
}

static void Cl_Up_down_f(void) {
	Cl_KeyDown(&in_up);
}
static void Cl_Up_up_f(void) {
	Cl_KeyUp(&in_up);
}
static void Cl_Down_down_f(void) {
	Cl_KeyDown(&in_down);
}
static void Cl_Down_up_f(void) {
	Cl_KeyUp(&in_down);
}
static void Cl_Left_down_f(void) {
	Cl_KeyDown(&in_left);
}
static void Cl_Left_up_f(void) {
	Cl_KeyUp(&in_left);
}
static void Cl_Right_down_f(void) {
	Cl_KeyDown(&in_right);
}
static void Cl_Right_up_f(void) {
	Cl_KeyUp(&in_right);
}
static void Cl_Forward_down_f(void) {
	Cl_KeyDown(&in_forward);
}
static void Cl_Forward_up_f(void) {
	Cl_KeyUp(&in_forward);
}
static void Cl_Back_down_f(void) {
	Cl_KeyDown(&in_back);
}
static void Cl_Back_up_f(void) {
	Cl_KeyUp(&in_back);
}
static void Cl_LookUp_down_f(void) {
	Cl_KeyDown(&in_look_up);
}
static void Cl_LookUp_up_f(void) {
	Cl_KeyUp(&in_look_up);
}
static void Cl_LookDown_down_f(void) {
	Cl_KeyDown(&in_look_down);
}
static void Cl_LookDown_up_f(void) {
	Cl_KeyUp(&in_look_down);
}
static void Cl_MoveLeft_down_f(void) {
	Cl_KeyDown(&in_move_left);
}
static void Cl_MoveLeft_up_f(void) {
	Cl_KeyUp(&in_move_left);
}
static void Cl_MoveRight_down_f(void) {
	Cl_KeyDown(&in_move_right);
}
static void Cl_MoveRight_up_f(void) {
	Cl_KeyUp(&in_move_right);
}
static void Cl_CenterView_f(void) {
	cl.angles.x = 0;
}

/**
 * @brief Returns the fraction of the command interval for which the key was down.
 */
float Cl_KeyState(button_t *key, uint32_t cmd_msec) {

	uint32_t msec = key->msec;
	key->msec = 0;

	if (key->state) { // still down, reset downtime for next frame
		msec += cl.unclamped_time - key->down_time;
		key->down_time = cl.unclamped_time;
	}

	const float frac = (msec * 1000.0) / (cmd_msec * 1000.0);

	return Clampf(frac, 0.0, 1.0);
}

/**
 * @brief Inserts source into destination at the specified offset, without
 * exceeding the specified length.
 *
 * @return The number of chars inserted.
 */
static size_t Cl_TextEvent_Insert(char *dest, const char *src, const size_t ofs, const size_t len) {
	char tmp[MAX_STRING_CHARS];

	const size_t l = strlen(dest);

	g_strlcpy(tmp, dest + ofs, sizeof(tmp));
	dest[ofs] = '\0';

	const size_t i = g_strlcat(dest, src, len);
	if (i < len) {
		g_strlcat(dest, tmp, len);
	}

	return strlen(dest) - l;
}

/**
 * @brief
 */
static void Cl_TextEvent(const SDL_Event *event) {

	console_input_t *in;

	if (cls.key_state.dest == KEY_CONSOLE) {
		in = &cl_console.input;
	} else if (cls.key_state.dest == KEY_CHAT) {
		in = &cl_chat_console.input;
	} else {
		return;
	}

	const char *src = event->text.text;

	in->pos += Cl_TextEvent_Insert(in->buffer, src, in->pos, sizeof(in->buffer));
}

/**
 * @brief Handles system events, spanning all key destinations.
 *
 * @return True if the event was handled, false otherwise.
 */
static _Bool Cl_HandleSystemEvent(const SDL_Event *event) {

	switch (event->type) {

		case SDL_QUIT:
			Cmd_ExecuteString("quit");
			return true;

		case SDL_WINDOWEVENT:
			if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				if (r_context.fullscreen == false) {
					const int32_t w = event->window.data1;
					const int32_t h = event->window.data2;
					if (w != r_width->integer || h != r_height->integer) {
						Cvar_ForceSetInteger(r_width->name, event->window.data1);
						Cvar_ForceSetInteger(r_height->name, event->window.data2);
						Cbuf_AddText("r_restart\n");
						return true;
					}
				}
			} else if (event->window.event == SDL_WINDOWEVENT_EXPOSED) {
				const int32_t display = SDL_GetWindowDisplayIndex(SDL_GL_GetCurrentWindow());
				if (display != r_display->integer) {
					Cvar_ForceSetInteger(r_display->name, display);
					Cbuf_AddText("r_restart\n");
					return true;
				}
			}
			break;

		case SDL_KEYDOWN:

			if (event->key.keysym.sym == SDLK_ESCAPE) { // escape can cancel a few things

				// connecting to a server
				switch (cls.state) {
					case CL_CONNECTING:
					case CL_CONNECTED:
						Com_Error(ERROR_DROP, "Connection aborted by user\n");
					case CL_LOADING:
						return false;
					default:
						break;
				}

				// message mode
				if (cls.key_state.dest == KEY_CHAT) {
					Cl_SetKeyDest(KEY_GAME);
					return true;
				}

				// console
				if (cls.key_state.dest == KEY_CONSOLE) {
					Cl_ToggleConsole_f();
					return true;
				}

				// and menus
				if (cls.key_state.dest == KEY_UI) {

					// if we're in the game, just hide the menus
					if (cls.state == CL_ACTIVE) {
						Cl_SetKeyDest(KEY_GAME);
						return true;
					}

					return false;
				}

				Cl_SetKeyDest(KEY_UI);
				return true;
			}

			// for everything other than ESC, check for system-level command binds

			SDL_Scancode key = event->key.keysym.scancode;
			if (cls.key_state.binds[key]) {
				cmd_t *cmd;

				if ((cmd = Cmd_Get(cls.key_state.binds[key]))) {
					if (cmd->flags & CMD_SYSTEM) {
						Cbuf_AddText(cls.key_state.binds[key]);
						Cbuf_Execute();
						return true;
					}
				}
			}
			break;
	}

	return false;
}

/**
 * @brief
 */
static void Cl_HandleEvent(const SDL_Event *event) {

	switch (event->type) {

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			Cl_KeyEvent(event);
			break;

		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			Cl_MouseButtonEvent(event);
			break;

		case SDL_MOUSEWHEEL:
			Cl_MouseWheelEvent(event);
			break;

		case SDL_MOUSEMOTION:
			Cl_MouseMotionEvent(event);
			break;

		case SDL_TEXTINPUT:
			Cl_TextEvent(event);
			break;
	}
}

/**
 * @brief Updates mouse state, ensuring the window has mouse focus, and draws the cursor.
 */
static void Cl_UpdateMouseState(void) {

	if (cls.key_state.dest == KEY_UI || cls.key_state.dest == KEY_CONSOLE) {
		if (SDL_GetWindowGrab(r_context.window)) {
			SDL_ShowCursor(true);
			SDL_SetWindowGrab(r_context.window, false);
		}
	} else {
		if (!SDL_GetWindowGrab(r_context.window)) {
			SDL_ShowCursor(false);
			SDL_SetWindowGrab(r_context.window, true);
		}
	}
}

/**
 * @brief
 */
void Cl_HandleEvents(void) {

	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		return;
	}

	Cl_UpdateMouseState();

	// handle new key events
	while (true) {
		SDL_Event event;

		memset(&event, 0, sizeof(event));

		if (SDL_PollEvent(&event)) {
			if (Cl_HandleSystemEvent(&event) == false) {
				Ui_HandleEvent(&event);
				Cl_HandleEvent(&event);
			}
		} else {
			break;
		}
	}
}

/**
 * @brief
 */
static void Cl_ClampPitch(const player_state_t *ps) {

	// ensure our pitch is valid
	float pitch = ps->pm_state.delta_angles.x;

	if (cl.angles.x + pitch < -360.0) {
		cl.angles.x += 360.0; // wrapped
	}
	if (cl.angles.x + pitch > 360.0) {
		cl.angles.x -= 360.0; // wrapped
	}

	if (cl.angles.x + pitch > 89.0) {
		cl.angles.x = 89.0 - pitch;
	}
	if (cl.angles.x + pitch < -89.0) {
		cl.angles.x = -89.0 - pitch;
	}
}

/**
 * @brief Accumulate view offset and angle modifications for the specified command.
 * @details The resulting view offset and angles are used as early as possible for prediction.
 */
void Cl_Look(pm_cmd_t *cmd) {

	cmd->up += cl_up_speed->value * cmd->msec * Cl_KeyState(&in_up, cmd->msec);
	cmd->up -= cl_up_speed->value * cmd->msec * Cl_KeyState(&in_down, cmd->msec);

	cl.angles.y -= cl_yaw_speed->value * cmd->msec * Cl_KeyState(&in_right, cmd->msec);
	cl.angles.y += cl_yaw_speed->value * cmd->msec * Cl_KeyState(&in_left, cmd->msec);

	cl.angles.x -= cl_pitch_speed->value * cmd->msec * Cl_KeyState(&in_look_up, cmd->msec);
	cl.angles.x += cl_pitch_speed->value * cmd->msec * Cl_KeyState(&in_look_down, cmd->msec);

	cls.cgame->Look(cmd);

	Cl_ClampPitch(&cl.frame.ps);

	cmd->angles = cl.angles;
}

/**
 * @brief Accumulate movement and button interactions for the specified command.
 * @details This is called at ~60hz regardless of the client's framerate. This is to avoid micro-
 * commands, which introduce prediction errors (screen jitter).
 */
void Cl_Move(pm_cmd_t *cmd) {

	cmd->forward += cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_forward, cmd->msec);
	cmd->forward -= cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_back, cmd->msec);

	cmd->right += cl_right_speed->value * cmd->msec * Cl_KeyState(&in_move_right, cmd->msec);
	cmd->right -= cl_right_speed->value * cmd->msec * Cl_KeyState(&in_move_left, cmd->msec);

	// pass to cgame
	cls.cgame->Move(cmd);

	//Com_Debug("%3dms: %4d forward %4d right %4d up\n", cmd->msec, cmd->forward, cmd->right, cmd->up);
}

/**
 * @brief
 */
void Cl_ClearInput(void) {

	memset(cl_buttons, 0, sizeof(cl_buttons));
}

/**
 * @brief
 */
void Cl_InitInput(void) {

	Cmd_Add("center_view", Cl_CenterView_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_up", Cl_Up_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_up", Cl_Up_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_down", Cl_Down_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_down", Cl_Down_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+left", Cl_Left_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-left", Cl_Left_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+right", Cl_Right_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-right", Cl_Right_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+forward", Cl_Forward_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-forward", Cl_Forward_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+back", Cl_Back_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-back", Cl_Back_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+look_up", Cl_LookUp_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-look_up", Cl_LookUp_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+look_down", Cl_LookDown_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-look_down", Cl_LookDown_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_left", Cl_MoveLeft_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_left", Cl_MoveLeft_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_right", Cl_MoveRight_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_right", Cl_MoveRight_up_f, CMD_CLIENT, NULL);

	cl_forward_speed = Cvar_Add("cl_forward_speed", "300.0", 0, NULL);
	cl_pitch_speed = Cvar_Add("cl_pitch_speed", "0.15", 0, NULL);
	cl_right_speed = Cvar_Add("cl_right_speed", "300.0", 0, NULL);
	cl_up_speed = Cvar_Add("cl_up_speed", "300.0", 0, NULL);
	cl_yaw_speed = Cvar_Add("cl_yaw_speed", "0.15", 0, NULL);

	m_sensitivity = Cvar_Add("m_sensitivity", "3.0", CVAR_ARCHIVE, NULL);
	m_sensitivity_zoom = Cvar_Add("m_sensitivity_zoom", "1.0", CVAR_ARCHIVE, NULL);
	m_interpolate = Cvar_Add("m_interpolate", "0", CVAR_ARCHIVE, NULL);
	m_invert = Cvar_Add("m_invert", "0", CVAR_ARCHIVE, "Invert the mouse");
	m_pitch = Cvar_Add("m_pitch", "0.022", 0, NULL);
	m_yaw = Cvar_Add("m_yaw", "0.022", 0, NULL);

	Cl_ClearInput();
}

