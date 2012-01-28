/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

cvar_t *cl_run;

cvar_t *m_sensitivity;
cvar_t *m_sensitivity_zoom;
cvar_t *m_interpolate;
cvar_t *m_invert;
cvar_t *m_yaw;
cvar_t *m_pitch;

/* power of two please */
#define MAX_KEYQ 64

struct {
	unsigned int key;
	unsigned short unicode;
	int down;
	int repeat;
} keyq[MAX_KEYQ];

static int keyq_head = 0;
static int keyq_tail = 0;

#define EVENT_ENQUEUE(keyNum, keyUnicode, keyDown) \
	if(keyNum > 0){ \
		keyq[keyq_head].key = (keyNum); \
		keyq[keyq_head].unicode = (keyUnicode); \
		keyq[keyq_head].down = (keyDown); \
		keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1); \
	}

/*
 * KEY BUTTONS
 *
 * Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 *
 * state bit 0 is the current state of the key
 * state bit 1 is edge triggered on the up to down transition
 * state bit 2 is edge triggered on the down to up transition
 */

typedef struct {
	int down[2]; // key nums holding it down
	unsigned down_time; // msec timestamp
	unsigned msec; // msec down this frame
	int state;
} kbutton_t;

static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_look_up, in_look_down;
static kbutton_t in_move_left, in_move_right;
static kbutton_t in_speed, in_attack;
static kbutton_t in_up, in_down;

/*
 * Cl_KeyDown
 */
static void Cl_KeyDown(kbutton_t *b) {
	int k;
	char *c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1; // typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Print("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return; // still down

	// save timestamp
	c = Cmd_Argv(2);
	b->down_time = atoi(c);
	if (!b->down_time)
		b->down_time = cls.real_time - 100;

	b->state |= 1;
}

/*
 * Cl_KeyUp
 */
static void Cl_KeyUp(kbutton_t *b) {
	int k;
	char *c;
	unsigned uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else { // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return; // key up without coresponding down

	if (b->down[0] || b->down[1])
		return; // some other key is still holding it down

	if (!(b->state & 1))
		return; // still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->down_time;
	else
		b->msec += 10;

	b->state &= ~1; // now up
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
static void Cl_Speed_down_f(void) {
	Cl_KeyDown(&in_speed);
}
static void Cl_Speed_up_f(void) {
	Cl_KeyUp(&in_speed);
}
static void Cl_Attack_down_f(void) {
	Cl_KeyDown(&in_attack);
}
static void Cl_Attack_up_f(void) {
	Cl_KeyUp(&in_attack);
}
static void Cl_CenterView_f(void) {
	cl.angles[PITCH] = 0;
}

/*
 * Cl_KeyState
 *
 * Returns the fraction of the command interval for which the key was down.
 */
static float Cl_KeyState(kbutton_t *key, int cmd_msec) {
	int msec;
	float v;

	msec = key->msec;
	key->msec = 0;

	if (key->state) { // still down, reset downtime for next frame
		msec += cls.real_time - key->down_time;
		key->down_time = cls.real_time;
	}

	v = (msec * 1000.0) / (cmd_msec * 1000.0);

	if (v > 1.0)
		v = 1.0;
	else if (v < 0.0)
		v = 0.0;

	return v;
}

/*
 * Cl_KeyMap
 */
static void Cl_KeyMap(SDL_Event *event, unsigned int *ascii,
		unsigned short *unicode) {
	int key = 0;
	const unsigned int keysym = event->key.keysym.sym;

	switch (keysym) {

	case SDLK_NUMLOCK:
		key = K_NUMLOCK;
		break;

	case SDLK_KP9:
		key = K_KP_PGUP;
		break;
	case SDLK_PAGEUP:
		key = K_PGUP;
		break;

	case SDLK_KP3:
		key = K_KP_PGDN;
		break;
	case SDLK_PAGEDOWN:
		key = K_PGDN;
		break;

	case SDLK_KP7:
		key = K_KP_HOME;
		break;
	case SDLK_HOME:
		key = K_HOME;
		break;

	case SDLK_KP1:
		key = K_KP_END;
		break;
	case SDLK_END:
		key = K_END;
		break;

	case SDLK_KP4:
		key = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		key = K_LEFTARROW;
		break;

	case SDLK_KP6:
		key = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		key = K_RIGHTARROW;
		break;

	case SDLK_KP2:
		key = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		key = K_DOWNARROW;
		break;

	case SDLK_KP8:
		key = K_KP_UPARROW;
		break;
	case SDLK_UP:
		key = K_UPARROW;
		break;

	case SDLK_ESCAPE:
		key = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		key = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		key = K_ENTER;
		break;

	case SDLK_TAB:
		key = K_TAB;
		break;

	case SDLK_F1:
		if (event->type == SDL_KEYDOWN)
			Cbuf_AddText("yes\n");
		break;
	case SDLK_F2:
		if (event->type == SDL_KEYDOWN)
			Cbuf_AddText("no\n");
		break;

	case SDLK_F3:
		key = K_F3;
		break;
	case SDLK_F4:
		key = K_F4;
		break;
	case SDLK_F5:
		key = K_F5;
		break;
	case SDLK_F6:
		key = K_F6;
		break;
	case SDLK_F7:
		key = K_F7;
		break;
	case SDLK_F8:
		key = K_F8;
		break;
	case SDLK_F9:
		key = K_F9;
		break;

	case SDLK_F10:
		key = K_F10;
		break;

	case SDLK_F11:
		if (event->type == SDL_KEYDOWN) {
			Cvar_Toggle("r_fullscreen");
			Cbuf_AddText("r_restart");
		}
		break;

	case SDLK_F12:
		key = K_F12;
		break;

	case SDLK_BACKSPACE:
		key = K_BACKSPACE;
		break;

	case SDLK_KP_PERIOD:
		key = K_KP_DEL;
		break;
	case SDLK_DELETE:
		key = K_DEL;
		break;

	case SDLK_PAUSE:
		key = K_PAUSE;
		break;

	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		key = K_SHIFT;
		break;

	case SDLK_LCTRL:
	case SDLK_RCTRL:
		key = K_CTRL;
		break;

	case SDLK_LMETA:
	case SDLK_RMETA:
	case SDLK_LALT:
	case SDLK_RALT:
		key = K_ALT;
		break;

	case SDLK_KP5:
		key = K_KP_5;
		break;

	case SDLK_INSERT:
		key = K_INS;
		break;
	case SDLK_KP0:
		key = K_KP_INS;
		break;

	case SDLK_KP_MULTIPLY:
		key = '*';
		break;
	case SDLK_KP_PLUS:
		key = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		key = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		key = K_KP_SLASH;
		break;

	case SDLK_WORLD_7:
		key = '`';
		break;

	default:
		key = keysym;
		break;
	}

	// if unicode is empty, use ascii instead
	*unicode = event->key.keysym.unicode == 0 ? key : event->key.keysym.unicode;
	*ascii = key;
}

/*
 * Cl_HandleEvent
 */
static void Cl_HandleEvent(SDL_Event *event) {
	static boolean_t first_key_event = true;
	unsigned int key;
	unsigned short unicode;

	if (cls.key_state.dest == KEY_UI) { // let the menus handle events

		if (Ui_Event(event))
			return;
	}

	switch (event->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
		switch (event->button.button) {
		case 1:
			key = K_MOUSE1;
			break;
		case 2:
			key = K_MOUSE3;
			break;
		case 3:
			key = K_MOUSE2;
			break;
		case 4:
			key = K_MWHEELUP;
			break;
		case 5:
			key = K_MWHEELDOWN;
			break;
		case 6:
			key = K_MOUSE4;
			break;
		case 7:
			key = K_MOUSE5;
			break;
		default:
			key = K_AUX1 + (event->button.button - 8) % 16;
			break;
		}

		EVENT_ENQUEUE(key, key, (event->type == SDL_MOUSEBUTTONDOWN))
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		/*
		 * We use the first key event to determine the state of number lock.
		 */
		if (first_key_event) {
			EVENT_ENQUEUE(K_NUMLOCK, 151, (event->key.keysym.mod & KMOD_NUM) != 0);
			first_key_event = false;
		}

		Cl_KeyMap(event, &key, &unicode);
		EVENT_ENQUEUE(key, unicode, (event->type == SDL_KEYDOWN))
		break;

	case SDL_QUIT:
		Cmd_ExecuteString("quit");
		break;

	case SDL_VIDEORESIZE:
		Cvar_SetValue("r_windowed_width", event->resize.w);
		Cvar_SetValue("r_windowed_height", event->resize.h);
		Cbuf_AddText("r_restart\n");
		break;
	}
}

/*
 * Cl_MouseMove
 */
static void Cl_MouseMove(int mx, int my) {

	if (m_sensitivity->modified) { // clamp sensitivity

		if (m_sensitivity->value < 0.1) {
			m_sensitivity->value = 3.0f;
		} else if (m_sensitivity->value > 20.0f) {
			m_sensitivity->value = 3.0f;
		}

		m_sensitivity->modified = false;
	}

	if (m_interpolate->value) { // interpolate movements
		cls.mouse_state.x = (mx + cls.mouse_state.old_x) * 0.5;
		cls.mouse_state.y = (my + cls.mouse_state.old_y) * 0.5;
	} else {
		cls.mouse_state.x = mx;
		cls.mouse_state.y = my;
	}

	cls.mouse_state.old_x = mx;
	cls.mouse_state.old_y = my;

	// for active, connected players, add it to their move
	if (cls.state == CL_ACTIVE && cls.key_state.dest == KEY_GAME) {

		cls.mouse_state.x -= r_context.width / 2; // first normalize to center
		cls.mouse_state.y -= r_context.height / 2;

		cls.mouse_state.x *= m_sensitivity->value; // then amplify
		cls.mouse_state.y *= m_sensitivity->value;

		if (m_invert->value) // and finally invert
			cls.mouse_state.y = -cls.mouse_state.y;

		// add horizontal and vertical movement
		cl.angles[YAW] -= m_yaw->value * cls.mouse_state.x;
		cl.angles[PITCH] += m_pitch->value * cls.mouse_state.y;
	}

	if (cls.key_state.dest != KEY_UI && cls.mouse_state.grabbed) {
		// warp the cursor back to the center of the screen
		SDL_WarpMouse(r_context.width / 2, r_context.height / 2);
	}
}

/*
 * Cl_HandleEvents
 */
void Cl_HandleEvents(void) {
	static cl_key_dest_t prev_key_dest;
	static int throwaway = 0;
	SDL_Event event;
	int mx, my;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
		return;

	// handle key events
	while (true) {

		memset(&event, 0, sizeof(event));

		if (SDL_PollEvent(&event))
			Cl_HandleEvent(&event);
		else
			break;
	}

	if (cls.key_state.dest == KEY_CONSOLE || cls.key_state.dest == KEY_UI) {
		if (!r_context.fullscreen) {
			// allow cursor to move outside window on console or menu
			SDL_WM_GrabInput(SDL_GRAB_OFF);
			cls.mouse_state.grabbed = false;
		}
	} else {
		if (!cls.mouse_state.grabbed) {
			// grab the cursor for everything else
			SDL_WM_GrabInput(SDL_GRAB_ON);
			cls.mouse_state.grabbed = true;

			// ignore crappy values after SDL grabs mouse
			// http://bugzilla.libsdl.org/show_bug.cgi?id=341
			throwaway = 2;
		} else if (prev_key_dest == KEY_UI) {
			// ignore mouse position from menu
			throwaway = 1;
		}
	}
	prev_key_dest = cls.key_state.dest;

	SDL_GetMouseState(&mx, &my);

	if (throwaway) {
		throwaway--;
		mx = r_context.width / 2;
		my = r_context.height / 2;
	}

	Cl_MouseMove(mx, my);

	while (keyq_head != keyq_tail) { // then check for keys
		Cl_KeyEvent(keyq[keyq_tail].key, keyq[keyq_tail].unicode,
				keyq[keyq_tail].down, cls.real_time);
		keyq_tail = (keyq_tail + 1) & (MAX_KEYQ - 1);
	}
}

/*
 * Cl_ClampPitch
 */
static void Cl_ClampPitch(void) {
	float pitch;

	// add frame's delta angles to our local input movement
	// wrapping where appropriate
	pitch = SHORT2ANGLE(cl.frame.ps.pmove.delta_angles[PITCH]);

	if (pitch > 180.0)
		pitch -= 360.0;

	if (cl.angles[PITCH] + pitch < -360.0)
		cl.angles[PITCH] += 360.0;
	if (cl.angles[PITCH] + pitch > 360.0)
		cl.angles[PITCH] -= 360.0;

	if (cl.angles[PITCH] + pitch > 89.0)
		cl.angles[PITCH] = 89.0 - pitch;
	if (cl.angles[PITCH] + pitch < -89.0)
		cl.angles[PITCH] = -89.0 - pitch;
}

/*
 * Cl_Move
 */
void Cl_Move(user_cmd_t *cmd) {
	float mod;
	int i;

	if (cmd->msec < 1)
		cmd->msec = 1;

	mod = 500.0;

	// keyboard move forward / back
	cmd->forward += mod * Cl_KeyState(&in_forward, cmd->msec);
	cmd->forward -= mod * Cl_KeyState(&in_back, cmd->msec);

	// keyboard strafe left / right
	cmd->side += mod * Cl_KeyState(&in_move_right, cmd->msec);
	cmd->side -= mod * Cl_KeyState(&in_move_left, cmd->msec);

	// keyboard jump / crouch
	cmd->up += mod * Cl_KeyState(&in_up, cmd->msec);
	cmd->up -= mod * Cl_KeyState(&in_down, cmd->msec);

	mod = 2.0; // reduce sensitivity for turning

	// keyboard turn left / right
	cl.angles[YAW] -= mod * Cl_KeyState(&in_right, cmd->msec);
	cl.angles[YAW] += mod * Cl_KeyState(&in_left, cmd->msec);

	// keyboard look up / down
	cl.angles[PITCH] -= mod * Cl_KeyState(&in_look_up, cmd->msec);
	cl.angles[PITCH] += mod * Cl_KeyState(&in_look_down, cmd->msec);

	Cl_ClampPitch(); // clamp, accounting for frame delta angles

	for (i = 0; i < 3; i++) // pack the angles into the command
		cmd->angles[i] = ANGLE2SHORT(cl.angles[i]);

	// set any button hits that occurred since last frame
	if (in_attack.state & 3)
		cmd->buttons |= BUTTON_ATTACK;

	in_attack.state &= ~2;

	if (cl_run->value) { // run by default, walk on speed toggle
		if (in_speed.state & 1)
			cmd->buttons |= BUTTON_WALK;
	} else { // walk by default, run on speed toggle
		if (!(in_speed.state & 1))
			cmd->buttons |= BUTTON_WALK;
	}
}

/*
 * Cl_InitInput
 */
void Cl_InitInput(void) {
	Cmd_AddCommand("center_view", Cl_CenterView_f, NULL);
	Cmd_AddCommand("+move_up", Cl_Up_down_f, NULL);
	Cmd_AddCommand("-move_up", Cl_Up_up_f, NULL);
	Cmd_AddCommand("+move_down", Cl_Down_down_f, NULL);
	Cmd_AddCommand("-move_down", Cl_Down_up_f, NULL);
	Cmd_AddCommand("+left", Cl_Left_down_f, NULL);
	Cmd_AddCommand("-left", Cl_Left_up_f, NULL);
	Cmd_AddCommand("+right", Cl_Right_down_f, NULL);
	Cmd_AddCommand("-right", Cl_Right_up_f, NULL);
	Cmd_AddCommand("+forward", Cl_Forward_down_f, NULL);
	Cmd_AddCommand("-forward", Cl_Forward_up_f, NULL);
	Cmd_AddCommand("+back", Cl_Back_down_f, NULL);
	Cmd_AddCommand("-back", Cl_Back_up_f, NULL);
	Cmd_AddCommand("+look_up", Cl_LookUp_down_f, NULL);
	Cmd_AddCommand("-look_up", Cl_LookUp_up_f, NULL);
	Cmd_AddCommand("+look_down", Cl_LookDown_down_f, NULL);
	Cmd_AddCommand("-look_down", Cl_LookDown_up_f, NULL);
	Cmd_AddCommand("+move_left", Cl_MoveLeft_down_f, NULL);
	Cmd_AddCommand("-move_left", Cl_MoveLeft_up_f, NULL);
	Cmd_AddCommand("+move_right", Cl_MoveRight_down_f, NULL);
	Cmd_AddCommand("-move_right", Cl_MoveRight_up_f, NULL);
	Cmd_AddCommand("+speed", Cl_Speed_down_f, NULL);
	Cmd_AddCommand("-speed", Cl_Speed_up_f, NULL);
	Cmd_AddCommand("+attack", Cl_Attack_down_f, NULL);
	Cmd_AddCommand("-attack", Cl_Attack_up_f, NULL);

	cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE, NULL);

	m_sensitivity = Cvar_Get("m_sensitivity", "3.0", CVAR_ARCHIVE, NULL);
	m_sensitivity_zoom = Cvar_Get("m_sensitivity_zoom", "1.0", CVAR_ARCHIVE,
			NULL);
	m_interpolate = Cvar_Get("m_interpolate", "0", CVAR_ARCHIVE, NULL);
	m_invert = Cvar_Get("m_invert", "0", CVAR_ARCHIVE, "Invert the mouse");
	m_pitch = Cvar_Get("m_pitch", "0.022", 0, NULL);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0, NULL);

	cls.mouse_state.grabbed = true;
}
