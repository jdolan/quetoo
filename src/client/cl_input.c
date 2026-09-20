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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cl_local.h"

static Cvar *cl_forwardSpeed;
static Cvar *cl_pitchSpeed;
static Cvar *cl_rightSpeed;
static Cvar *cl_upSpeed;
static Cvar *cl_yawSpeed;
static Cvar *cl_captureMediaKeys;

Cvar *m_interpolate;
Cvar *m_invert;
Cvar *m_sensitivity;
Cvar *m_sensitivityZoom;
Cvar *m_pitch;
Cvar *m_yaw;

static InputButton cl_buttons[10];
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
 * @brief Registers a key-down event for the given button, tracking which keys hold it.
 */
void Cl_KeyDown(InputButton *b) {
  SDL_Scancode k;

  const char *c = Cmd_Argv(1);
  if (c[0]) {
    k = atoi(c);
  } else {
    k = SDL_SCANCODE_COUNT; // typed manually at the console for continuous down
  }

  if (k == (SDL_Scancode) b->keys[0] || k == (SDL_Scancode) b->keys[1]) {
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
  b->downTime = (uint32_t) strtoul(Cmd_Argv(2), NULL, 0) ? : cl.unclampedTime;

  // and indicate that the key is down
  b->state |= (BUTTON_STATE_HELD | BUTTON_STATE_DOWN);
}

/**
 * @brief Registers a key-up event for the given button, releasing it when all keys are up.
 */
void Cl_KeyUp(InputButton *b) {

  if (Cmd_Argc() < 2) { // typed manually at the console, assume for un-sticking, so clear all
    b->keys[0] = b->keys[1] = 0;
    return;
  }

  const SDL_Scancode k = atoi(Cmd_Argv(1));

  if ((SDL_Scancode) b->keys[0] == k) {
    b->keys[0] = SDL_SCANCODE_UNKNOWN;
  } else if ((SDL_Scancode) b->keys[1] == k) {
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
  const uint32_t upTime = atoi(t);
  if (upTime) {
    b->msec += upTime - b->downTime;
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
float Cl_KeyState(InputButton *key, uint32_t cmdMsec) {

  uint32_t msec = key->msec;
  key->msec = 0;

  if (key->state) { // still down, reset downtime for next frame
    msec += cl.unclampedTime - key->downTime;
    key->downTime = cl.unclampedTime;
  }

  const float frac = (msec * 1000.0) / (cmdMsec * 1000.0);

  return Clampf01(frac);
}

/**
 * @brief Updates mouse state, ensuring the window has mouse focus, and draws the cursor.
 */
static void Cl_UpdateMouseState(void) {

  const SDL_WindowFlags flags = SDL_GetWindowFlags(rContext.window);

  // paused demo playback stays in KEY_GAME so the HUD (and its transport controls) keep
  // drawing, but wants a visible, ungrabbed cursor to drive those controls with
  if (cls.keyState.dest == KEY_UI || cls.keyState.dest == KEY_CONSOLE || cls.demo.paused ||
      (flags & (SDL_WINDOW_OCCLUDED | SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED))) {
    SDL_ShowCursor();
    SDL_SetWindowMouseGrab(rContext.window, false);
  } else {
    SDL_HideCursor();
    SDL_SetWindowMouseGrab(rContext.window, true);
  }

  // Cl_SetKeyDest owns relative mouse mode for key destination changes, but pausing a demo
  // doesn't change destination, so the pause state is reconciled here each frame instead - and
  // so it survives a trip through the menus and back
  if (cls.keyState.dest == KEY_GAME) {
    SDL_SetWindowRelativeMouseMode(rContext.window, !cls.demo.paused);
  }
}

/**
 * @brief Inserts source into destination at the specified offset, without
 * exceeding the specified length.
 *
 * @return The number of chars inserted.
 */
static size_t Cl_TextEvent_Insert(char *dest, const char *src, const size_t ofs, const size_t len) {
  char tmp[MAX_STRING_CHARS];

  const size_t l = q_strlen(dest);

  q_strlcpy(tmp, dest + ofs, sizeof(tmp));
  dest[ofs] = '\0';

  const size_t i = q_strlcat(dest, src, len);
  if (i < len) {
    q_strlcat(dest, tmp, len);
  }

  return q_strlen(dest) - l;
}

/**
 * @brief Handles a text input SDL event, inserting typed characters into the active console.
 */
static void Cl_TextEvent(const SDL_Event *event) {

  if (cls.keyState.dest != KEY_CONSOLE) {
    return;
  }

  ConsoleInput *in = &clConsole.input;

  const char *src = event->text.text;

  in->pos += Cl_TextEvent_Insert(in->buffer, src, in->pos, sizeof(in->buffer));
}

/**
 * @brief Handles system events, spanning all key destinations.
 *
 * @return True if the event was handled, false otherwise.
 */
static bool Cl_HandleSystemEvent(const SDL_Event *event) {

  switch (event->type) {

    case SDL_EVENT_DROP_FILE: {
      const char *data = event->drop.data;
      if (data && !q_strncmp(data, "quetoo://", 9)) {
        Cbuf_AddText(va("connect %s\n", data + q_strlen("quetoo://")));
        return true;
      }
      return false;
    }

    case SDL_EVENT_QUIT:
      Cmd_ExecuteString("quit");
      return true;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
      if (cls.keyState.dest == KEY_GAME) {
        Cl_SetKeyDest(KEY_UI);
      }
      return false;

    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
      R_UpdateContext();
      return false;

    case SDL_EVENT_KEY_DOWN:

      if (event->key.key == SDLK_ESCAPE) {

        switch (cls.state) {
          case CL_DISCONNECTED:
            if (cls.keyState.dest == KEY_CONSOLE) {
              Cl_ToggleConsole_f();
              return true;
            }
            break;
          case CL_CONNECTING:
          case CL_CONNECTED:
          case CL_LOADING:
            Com_Error(ERROR_DROP, "Connection aborted by user\n");
          case CL_ACTIVE:
            switch (cls.keyState.dest) {
              case KEY_CHAT:
              case KEY_UI:
                Cl_SetKeyDest(KEY_GAME);
                return true;
              case KEY_GAME:
                Cl_SetKeyDest(KEY_UI);
                return true;
              case KEY_CONSOLE:
                Cl_ToggleConsole_f();
                return true;
            }
          default:
            break;
        }
      }

      if (cl_captureMediaKeys->integer) {
        switch (event->key.scancode) {
          case SDL_SCANCODE_MEDIA_PLAY:
          case SDL_SCANCODE_MEDIA_PLAY_PAUSE:
            Cbuf_AddText("s_pauseMusic\n");
            Cbuf_Execute();
            return true;
          case SDL_SCANCODE_MEDIA_NEXT_TRACK:
            Cbuf_AddText("s_nextTrack\n");
            Cbuf_Execute();
            return true;
          case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK:
            Cbuf_AddText("s_prevTrack\n");
            Cbuf_Execute();
            return true;
          case SDL_SCANCODE_MUTE:
            Cbuf_AddText("toggle s_musicVolume 0 0.15\n");
            Cbuf_Execute();
            return true;
          default:
            break;
        }
      }

      // for everything other than ESC, check for system-level command binds

      SDL_Scancode key = event->key.scancode;
      if (cls.keyState.binds[key]) {
        Cmd *cmd;

        Cmd_TokenizeString(cls.keyState.binds[key]);
        if ((cmd = Cmd_Get(Cmd_Argv(0)))) {
          if (cmd->flags & CMD_SYSTEM) {
            Cbuf_AddText(cls.keyState.binds[key]);
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
 * @brief Routes an SDL input event to the appropriate key, mouse, or text handler.
 */
static void Cl_HandleEvent(const SDL_Event *event) {

  switch (event->type) {

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      Cl_KeyEvent(event);
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      Cl_MouseButtonEvent(event);
      break;

    case SDL_EVENT_MOUSE_WHEEL:
      Cl_MouseWheelEvent(event);
      break;

    case SDL_EVENT_MOUSE_MOTION:
      Cl_MouseMotionEvent(event);
      break;

    case SDL_EVENT_TEXT_INPUT:
      Cl_TextEvent(event);
      break;
  }
}

/**
 * @brief Polls and dispatches all pending SDL events for the current frame.
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

        cls.cgame->HandleEvent(&event);
      }
    } else {
      break;
    }
  }
}

/**
 * @brief Clamps the player pitch angle to prevent looking too far up or down.
 */
static void Cl_ClampPitch(const PlayerState *ps) {

  // ensure our pitch is valid
  float pitch = ps->pmState.deltaAngles.x;

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
void Cl_Look(PMoveCmd *cmd) {

  cmd->up += cl_upSpeed->value * cmd->msec * Cl_KeyState(&in_up, cmd->msec);
  cmd->up -= cl_upSpeed->value * cmd->msec * Cl_KeyState(&in_down, cmd->msec);

  cl.angles.y -= cl_yawSpeed->value * cmd->msec * Cl_KeyState(&in_right, cmd->msec);
  cl.angles.y += cl_yawSpeed->value * cmd->msec * Cl_KeyState(&in_left, cmd->msec);

  cl.angles.x -= cl_pitchSpeed->value * cmd->msec * Cl_KeyState(&in_look_up, cmd->msec);
  cl.angles.x += cl_pitchSpeed->value * cmd->msec * Cl_KeyState(&in_look_down, cmd->msec);

  cls.cgame->Look(cmd);

  Cl_ClampPitch(&cl.frame.ps);

  cmd->angles = cl.angles;
}

/**
 * @brief Accumulate movement and button interactions for the specified command.
 * @details This is called at ~60hz regardless of the client's framerate. This is to avoid micro-
 * commands, which introduce prediction errors (screen jitter).
 */
void Cl_Move(PMoveCmd *cmd) {

  cmd->forward += cl_forwardSpeed->value * cmd->msec * Cl_KeyState(&in_forward, cmd->msec);
  cmd->forward -= cl_forwardSpeed->value * cmd->msec * Cl_KeyState(&in_back, cmd->msec);

  cmd->right += cl_rightSpeed->value * cmd->msec * Cl_KeyState(&in_move_right, cmd->msec);
  cmd->right -= cl_rightSpeed->value * cmd->msec * Cl_KeyState(&in_move_left, cmd->msec);

  // pass to cgame
  cls.cgame->Move(cmd);

  //Com_Debug("%3dms: %4d forward %4d right %4d up\n", cmd->msec, cmd->forward, cmd->right, cmd->up);
}

/**
 * @brief Resets all button states, clearing any held inputs.
 * @remarks Voice is released here too, so that losing focus or dropping a key up event cannot
 * leave the microphone transmitting.
 */
void Cl_ClearInput(void) {

  memset(cl_buttons, 0, sizeof(cl_buttons));

  S_StopVoice();
}

/**
 * @brief Registers all movement, look, and weapon button commands.
 */
void Cl_InitInput(void) {

  Cmd_Add("centerView", Cl_CenterView_f, CMD_CLIENT, NULL);
  Cmd_Add("+moveUp", Cl_Up_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-moveUp", Cl_Up_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+moveDown", Cl_Down_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-moveDown", Cl_Down_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+left", Cl_Left_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-left", Cl_Left_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+right", Cl_Right_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-right", Cl_Right_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+forward", Cl_Forward_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-forward", Cl_Forward_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+back", Cl_Back_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-back", Cl_Back_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+lookUp", Cl_LookUp_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-lookUp", Cl_LookUp_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+lookDown", Cl_LookDown_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-lookDown", Cl_LookDown_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+moveLeft", Cl_MoveLeft_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-moveLeft", Cl_MoveLeft_up_f, CMD_CLIENT, NULL);
  Cmd_Add("+moveRight", Cl_MoveRight_down_f, CMD_CLIENT, NULL);
  Cmd_Add("-moveRight", Cl_MoveRight_up_f, CMD_CLIENT, NULL);

  cl_forwardSpeed = Cvar_Add("cl_forwardSpeed", "300.0", 0, NULL);
  cl_pitchSpeed = Cvar_Add("cl_pitchSpeed", "0.15", 0, NULL);
  cl_rightSpeed = Cvar_Add("cl_rightSpeed", "300.0", 0, NULL);
  cl_upSpeed = Cvar_Add("cl_upSpeed", "300.0", 0, NULL);
  cl_yawSpeed = Cvar_Add("cl_yawSpeed", "0.15", 0, NULL);
  cl_captureMediaKeys = Cvar_Add("cl_captureMediaKeys", "1", CVAR_ARCHIVE, "Handle media keys (play/pause, next, previous, mute) for in-game music.");

  m_sensitivity = Cvar_Add("m_sensitivity", "3.0", CVAR_ARCHIVE, NULL);
  m_sensitivityZoom = Cvar_Add("m_sensitivityZoom", "1.0", CVAR_ARCHIVE, NULL);
  m_interpolate = Cvar_Add("m_interpolate", "0", CVAR_ARCHIVE, NULL);
  m_invert = Cvar_Add("m_invert", "0", CVAR_ARCHIVE, "Invert the mouse");
  m_pitch = Cvar_Add("m_pitch", "0.022", 0, NULL);
  m_yaw = Cvar_Add("m_yaw", "0.022", 0, NULL);

  Cl_ClearInput();
}
