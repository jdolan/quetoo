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

#include <ctype.h>

#include "cl_local.h"

static char **clKeyNames;

/**
 * @brief  Sets the key state destination.
 */
void Cl_SetKeyDest(ClientKeyDest dest) {

  if (dest == cls.keyState.dest) {
    if (dest == KEY_CONSOLE || dest == KEY_CHAT) {
      SDL_StartTextInput(rContext.window);
    }
    return;
  }

  // release keys and re-center the mouse when leaving KEY_GAME

  if (cls.keyState.dest == KEY_GAME) {
    SDL_Event e = { .type = SDL_EVENT_KEY_UP };

    for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
      if (cls.keyState.down[k]) {
        if (cls.keyState.binds[k] && cls.keyState.binds[k][0] == '+') {
          e.key.scancode = k;
          Cl_KeyEvent(&e);
        }
      }
    }

    SDL_SetWindowRelativeMouseMode(rContext.window, false);

    const int32_t cx = rContext.windowBounds.w * 0.5;
    const int32_t cy = rContext.windowBounds.h * 0.5;

    SDL_WarpMouseInWindow(rContext.window, cx, cy);
  }

  switch (dest) {
    case KEY_CONSOLE:
    case KEY_CHAT:
      SDL_StartTextInput(rContext.window);
      break;
    case KEY_UI:
      SDL_StopTextInput(rContext.window);
      break;
    case KEY_GAME:
      SDL_StopTextInput(rContext.window);
      SDL_SetWindowRelativeMouseMode(rContext.window, true);
      break;
  }

  SDL_PumpEvents();

  SDL_FlushEvent(SDL_EVENT_TEXT_INPUT);

  cls.keyState.dest = dest;

  Cvar_ForceSetInteger(active->name, dest == KEY_GAME);
}

/**
 * @brief Returns the current key state destination.
 */
ClientKeyDest Cl_GetKeyDest(void) {
  return cls.keyState.dest;
}

/**
 * @brief Interactive line editing and console scrollback.
 */
static void Cl_KeyConsole(const SDL_Event *event) {

  if (event->type == SDL_EVENT_KEY_UP) { // don't care
    return;
  }

  ConsoleInput *in = &clConsole.input;

  const SDL_Keycode key = event->key.key;
  switch (key) {

    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      Con_SubmitInput(&clConsole);
      break;

    case SDLK_TAB:
    case SDLK_KP_TAB:
      Con_CompleteInput(&clConsole);
      break;

    case SDLK_BACKSPACE:
    case SDLK_KP_BACKSPACE:
      if (in->pos > 0) {
        char *c = in->buffer + in->pos - 1;
        while (*c) {
          *c = *(c + 1);
          c++;
        }
        in->pos--;
      }
      break;

    case SDLK_DELETE:
      if (in->pos < q_strlen(in->buffer)) {
        char *c = in->buffer + in->pos;
        while (*c) {
          *c = *(c + 1);
          c++;
        }
      }
      break;

    case SDLK_UP:
      Con_NavigateHistory(&clConsole, CON_HISTORY_PREV);
      break;

    case SDLK_DOWN:
      Con_NavigateHistory(&clConsole, CON_HISTORY_NEXT);
      break;

    case SDLK_LEFT:
      if (SDL_GetModState() & SDL_KMOD_CTRL) { // move one word left
        while (in->pos > 0 && in->buffer[in->pos] == ' ') {
          in->pos--; // off current word
        }
        while (in->pos > 0 && in->buffer[in->pos] != ' ') {
          in->pos--; // and behind previous word
        }
      } else if (in->pos > 0) {
        in->pos--;
      }
      break;

    case SDLK_RIGHT:
      if (SDL_GetModState() & SDL_KMOD_CTRL) { // move one word right
        const size_t len = q_strlen(in->buffer);
        while (in->pos < len && in->buffer[in->pos] == ' ') {
          in->pos++; // off current word
        }
        while (in->pos < len && in->buffer[in->pos] != ' ') {
          in->pos++; // and in front of next word
        }
        if (in->pos < len) { // all the way in front
          in->pos++;
        }
      } else if (in->pos < q_strlen(in->buffer)) {
        in->pos++;
      }
      break;

    case SDLK_PAGEUP:
      if (clConsole.scroll + clConsole.height < consoleState.strings->count) {
        clConsole.scroll += clConsole.height;
      } else {
        clConsole.scroll = consoleState.strings->count;
      }
      break;

    case SDLK_PAGEDOWN:
      if (clConsole.scroll > clConsole.height) {
        clConsole.scroll -= clConsole.height;
      } else {
        clConsole.scroll = 0;
      }
      break;

    case SDLK_HOME:
      in->pos = 0;
      break;

    case SDLK_END:
      in->pos = q_strlen(in->buffer);
      break;

    case SDLK_A:
      if (SDL_GetModState() & SDL_KMOD_CTRL) {
        in->pos = 0;
      }
      break;
    case SDLK_E:
      if (SDL_GetModState() & SDL_KMOD_CTRL) {
        in->pos = q_strlen(in->buffer);
      }
      break;
    case SDLK_C:
      if (SDL_GetModState() & SDL_KMOD_CTRL) {
        in->buffer[0] = '\0';
        in->pos = 0;
      }
      break;

    case SDLK_V:
      if ((SDL_GetModState() & SDL_KMOD_CLIPBOARD) && SDL_HasClipboardText()) {
        char *tail = q_strdup(in->buffer + in->pos);
        in->buffer[in->pos] = '\0';

        char *text = SDL_GetClipboardText();
        q_strlcat(in->buffer, text, sizeof(in->buffer));
        q_strlcat(in->buffer, tail, sizeof(in->buffer));
        free(tail);

        in->pos = Minf(in->pos + q_strlen(text), sizeof(in->buffer) - 1);
        SDL_free(text);
      }
      break;

    default:
      break;
  }
}

/**
 * @brief Executes the bound command for the current key event in game mode.
 */
static void Cl_KeyGame(const SDL_Event *event) {
  char cmd[MAX_STRING_CHARS];

  const SDL_Scancode key = event->key.scancode;
  const char *bind = cls.keyState.binds[key];

  if (!bind) {
    return;
  }

  // A demo no longer merely steers itself: the camera modes drive themselves from movement
  // input, so button commands are let through. The transport keys are the exception, because
  // they collide with movement binds outright - space is bound to +moveUp, left and right to
  // +left and +right, so stepping a frame would also turn the view
  if (cl.demoServer && bind[0] == '+') {
    switch (key) {
      case SDL_SCANCODE_LEFT:
      case SDL_SCANCODE_RIGHT:
      case SDL_SCANCODE_SPACE:
      case SDL_SCANCODE_COMMA:
      case SDL_SCANCODE_PERIOD:
        return;
      default:
        break;
    }
  }

  cmd[0] = '\0';

  if (bind[0] == '+') { // button commands add key and time as a param
    if (event->type == SDL_EVENT_KEY_DOWN) {
      if (cls.keyState.down[key] == false) {
        q_snprintf(cmd, sizeof(cmd), "%s %i %i\n", bind, key, cl.unclampedTime);
        cls.keyState.latched[key] = true;
      }
    } else {
      if (cls.keyState.down[key] == true && cls.keyState.latched[key] == true) {
        q_snprintf(cmd, sizeof(cmd), "-%s %i %i\n", bind + 1, key, cl.unclampedTime);
        cls.keyState.latched[key] = false;
      }
    }
  } else {
    if (event->type == SDL_EVENT_KEY_DOWN) {
      q_snprintf(cmd, sizeof(cmd), "%s\n", bind);
    }
  }

  if (cmd[0]) { // send the command
    Cbuf_AddText(cmd);
  }
}

/**
 * @brief Returns the name of the specified key.
 */
const char *Cl_KeyName(SDL_Scancode key) {

  if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) {
    return va("<unknown %d>", key);
  }

  return clKeyNames[key];
}

/**
 * @brief Returns the number for the specified key name.
 */
SDL_Scancode Cl_KeyForName(const char *name) {

  if (!name || !name[0]) {
    return SDL_SCANCODE_COUNT;
  }

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (clKeyNames[k]) {
      if (!q_strcasecmp(name, clKeyNames[k])) {
        return k;
      }
    }
  }

  return SDL_SCANCODE_COUNT;
}

/**
 * @brief Returns the key bound to the given command.
 */
SDL_Scancode Cl_KeyForBind(SDL_Scancode from, const char *binding) {

  for (SDL_Scancode k = from + 1; k < SDL_SCANCODE_COUNT; k++) {
    if (q_strcmp(binding, cls.keyState.binds[k]) == 0) {
      return k;
    }
  }

  return SDL_SCANCODE_UNKNOWN;
}

/**
 * @brief Rewrites `bind` so that its command is spelled the way the command is
 * registered now, leaving any arguments alone.
 * @remarks A bind is opaque text that no lookup resolves, so one written with
 * an older command name would otherwise keep it, and resolve by the legacy
 * path on every key event.
 */
static void Cl_CanonicalizeBind(char *bind, size_t size) {

  char *args = q_strchr(bind, ' ');
  if (args) {
    *args = '\0';
  }

  const Cmd *cmd = Cmd_Get(bind);
  if (cmd) {
    q_strlcpy(bind, cmd->name, size);
  }

  if (args) {
    q_strlcat(bind, " ", size);
    q_strlcat(bind, args + 1, size);
  }
}

/**
 * @brief Binds the specified key to the given command.
 */
void Cl_Bind(SDL_Scancode key, const char *bind) {

  if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) {
    return;
  }

  // free the old binding
  if (cls.keyState.binds[key]) {
    Mem_Free(cls.keyState.binds[key]);
    cls.keyState.binds[key] = NULL;
  }

  if (!bind) {
    return;
  }

  // allocate for new binding and copy it in
  cls.keyState.binds[key] = Mem_TagMalloc(q_strlen(bind) + 1, MEM_TAG_CLIENT);
  strcpy(cls.keyState.binds[key], bind);
}


/**
 * @brief Handles the `unbind` console command, removing the binding for a named key.
 */
static void Cl_Unbind_f(void) {

  if (Cmd_Argc() != 2) {
    Com_Print("Usage: %s <key> : remove commands from a key\n", Cmd_Argv(0));
    return;
  }

  const SDL_Scancode k = Cl_KeyForName(Cmd_Argv(1));

  if (k == SDL_SCANCODE_COUNT) {
    Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
    return;
  }

  Cl_Bind(k, NULL);
}

/**
 * @brief Handles the `unbindAll` console command, clearing all key bindings.
 */
static void Cl_UnbindAll_f(void) {

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (cls.keyState.binds[k]) {
      Cl_Bind(k, NULL);
    }
  }
}

/**
 * @brief Bind command autocomplete
 */
static void Cl_Bind_Autocomplete_f(const uint32_t argi, List *matches) {

  if (argi != 1) {
    return;
  }

  const char *pattern = va("%s*", Cmd_Argv(argi));

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (clKeyNames[k]) {
      const char *keyName = clKeyNames[k];

      if (GlobMatch(pattern, keyName, GLOB_CASE_INSENSITIVE)) {
        Con_AutocompleteMatch(matches, keyName, NULL);
      }
    }
  }
}

/**
 * @brief Handles the `bind` console command, assigning a command string to a named key.
 */
static void Cl_Bind_f(void) {
  char cmd[MAX_STRING_CHARS];

  const int32_t c = Cmd_Argc();
  if (c < 2) {
    Com_Print("Usage: %s <key> [command] : bind a command to a key\n", Cmd_Argv(0));
    return;
  }

  const SDL_Scancode k = Cl_KeyForName(Cmd_Argv(1));

  if (k == SDL_SCANCODE_COUNT) {
    Com_Print("\"%s\" isn't a valid key\n", Cmd_Argv(1));
    return;
  }

  if (c == 2) {
    if (cls.keyState.binds[k]) {
      Com_Print("\"%s\" = \"%s\"\n", Cmd_Argv(1), cls.keyState.binds[k]);
    } else {
      Com_Print("\"%s\" is not bound\n", Cmd_Argv(1));
    }
    return;
  }

  // copy the rest of the command line
  cmd[0] = 0; // start out with a null string
  for (int32_t i = 2; i < c; i++) {
    strcat(cmd, Cmd_Argv(i));
    if (i != (c - 1)) {
      strcat(cmd, " ");
    }
  }

  // check for compound bindings
  if (q_strchr(cmd, ';')) {
    Com_Print("Complex bind \"%s\" ignored; use 'alias' instead\n", cmd);
    return;
  }

  Cl_CanonicalizeBind(cmd, sizeof(cmd));

  Cl_Bind(k, cmd);
}

/**
 * @brief Writes lines containing "bind key value"
 */
void Cl_WriteBindings(File *f) {

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (cls.keyState.binds[k] && cls.keyState.binds[k][0]) {
      Fs_Print(f, "bind \"%s\" \"%s\"\n", Cl_KeyName(k), cls.keyState.binds[k]);
    }
  }
}

/**
 * @brief Handles the `bindList` console command, printing all active key bindings.
 */
static void Cl_BindList_f(void) {

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (cls.keyState.binds[k] && cls.keyState.binds[k][0]) {
      Com_Print("\"%s\" \"%s\"\n", Cl_KeyName(k), cls.keyState.binds[k]);
    }
  }
}

#include "cl_binds.h"

/**
 * @brief Initializes key name tables, default bindings, and key-related console commands.
 */
void Cl_InitKeys(void) {

  clKeyNames = Mem_TagMalloc(SDL_SCANCODE_COUNT * sizeof(char *), MEM_TAG_CLIENT);

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    const char *name = SDL_GetScancodeName(k);
    if (q_strlen(name)) {
      clKeyNames[k] = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CLIENT), clKeyNames);
    }
  }

  for (SDL_Buttoncode b = SDL_SCANCODE_MOUSE1; b <= SDL_SCANCODE_MOUSE15; b++) {

    const char *name = va("Mouse %d", b - SDL_SCANCODE_MOUSE1 + 1);
    clKeyNames[b] = Mem_Link(Mem_TagCopyString(name, MEM_TAG_CLIENT), clKeyNames);
  }

  clKeyNames[SDL_SCANCODE_MWHEELUP] = Mem_Link(Mem_TagCopyString("Mouse Wheel Up", MEM_TAG_CLIENT), clKeyNames);
  clKeyNames[SDL_SCANCODE_MWHEELDOWN] = Mem_Link(Mem_TagCopyString("Mouse Wheel Down", MEM_TAG_CLIENT), clKeyNames);

  memset(&cls.keyState, 0, sizeof(ClientKeyState));

  // register our functions
  Cmd *bindCmd = Cmd_Add("bind", Cl_Bind_f, CMD_CLIENT, NULL);
  Cmd *unbindCmd = Cmd_Add("unbind", Cl_Unbind_f, CMD_CLIENT, NULL);

  Cmd_SetAutocomplete(bindCmd, Cl_Bind_Autocomplete_f);
  Cmd_SetAutocomplete(unbindCmd, Cl_Bind_Autocomplete_f);

  Cmd_Add("unbindAll", Cl_UnbindAll_f, CMD_CLIENT, NULL);
  Cmd_Add("bindList", Cl_BindList_f, CMD_CLIENT, NULL);

  Cbuf_AddText(DEFAULT_BINDS);
  Cbuf_Execute();
}

/**
 * @brief Rewrites every bind to the command names registered now.
 * @remarks The default binds and quetoo.cfg are executed from Cl_InitKeys,
 * before the rest of the client registers its commands, so a bind naming a
 * command that does not exist yet cannot be resolved as it is set.
 */
void Cl_CanonicalizeBinds(void) {
  char bind[MAX_STRING_CHARS];

  for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_SCANCODE_COUNT; k++) {
    if (cls.keyState.binds[k] == NULL || *cls.keyState.binds[k] == '\0') {
      continue;
    }

    q_strlcpy(bind, cls.keyState.binds[k], sizeof(bind));
    Cl_CanonicalizeBind(bind, sizeof(bind));

    if (q_strcmp(bind, cls.keyState.binds[k])) {
      Cl_Bind(k, bind);
    }
  }
}

/**
 * @brief Frees the key name table allocated during initialization.
 */
void Cl_ShutdownKeys(void) {

  Mem_Free(clKeyNames);
}

/**
 * @brief Routes an SDL key event to the game or console handler; the UI handles its own.
 */
void Cl_KeyEvent(const SDL_Event *event) {

  switch (cls.keyState.dest) {
    case KEY_UI:
    case KEY_CHAT:
      break;
    case KEY_GAME:
      Cl_KeyGame(event);
      break;
    case KEY_CONSOLE:
      Cl_KeyConsole(event);
      break;

    default:
      Com_Debug(DEBUG_CLIENT, "Bad cl_key_dest: %d\n", cls.keyState.dest);
      break;
  }

  cls.keyState.down[event->key.scancode] = event->type == SDL_EVENT_KEY_DOWN;
}
