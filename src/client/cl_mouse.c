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

/**
 * @brief Handles mouse button events by dispatching them as key events.
 */
void Cl_MouseButtonEvent(const SDL_Event *event) {
  SDL_Event e;
  memset(&e, 0, sizeof(e));

  e.type = event->type == SDL_EVENT_MOUSE_BUTTON_UP ? SDL_EVENT_KEY_UP : SDL_EVENT_KEY_DOWN;
  e.key.scancode = SDL_SCANCODE_MOUSE1 + (event->button.button - 1);
  e.key.key = SDL_SCANCODE_TO_KEYCODE(e.key.scancode);

  Cl_KeyEvent(&e);
}

/**
 * @brief Handles an SDL mouse wheel event, scrolling the console or dispatching key events.
 * @details In KEY_GAME mode only one event per direction is dispatched per frame to prevent
 * high-resolution or Wayland scroll wheels from delivering multiple events per physical notch
 * and causing the weapon selector to skip weapons.
 */
void Cl_MouseWheelEvent(const SDL_Event *event) {

  if (event->wheel.y == 0) {
    return;
  }

  switch (cls.keyState.dest) {
    case KEY_UI:
      break;

    case KEY_CONSOLE: {
        const int64_t scroll = clConsole.scroll + event->wheel.y;
        clConsole.scroll = Clampf(scroll, 0, (int64_t) consoleState.strings->count);
      }
      break;

    case KEY_GAME: {
        static uint32_t lastUp, lastDown;

        const SDL_Buttoncode scancode = event->wheel.y > 0 ? SDL_SCANCODE_MWHEELUP : SDL_SCANCODE_MWHEELDOWN;
        uint32_t *last = scancode == SDL_SCANCODE_MWHEELUP ? &lastUp : &lastDown;

        if (*last == cl.unclampedTime) {
          break; // already fired this direction this frame
        }
        *last = cl.unclampedTime;

        SDL_Event e;
        memset(&e, 0, sizeof(e));

        e.type = SDL_EVENT_KEY_DOWN;
        e.key.scancode = (SDL_Scancode) scancode;
        e.key.key = SDL_SCANCODE_TO_KEYCODE(e.key.scancode);

        Cl_KeyEvent(&e);

        e.type = SDL_EVENT_KEY_UP;

        Cl_KeyEvent(&e);
      }
      break;

    default:
      break;
  }
}

/**
 * @brief Handles an SDL mouse motion event, applying sensitivity and updating view angles.
 */
void Cl_MouseMotionEvent(const SDL_Event *event) {

  if (cls.keyState.dest != KEY_GAME) {
    return;
  }

  if (m_sensitivity->modified) {
    m_sensitivity->value = Clampf(m_sensitivity->value, 0.1, 20.0);
    m_sensitivity->modified = false;
  }

  cls.mouseState.oldX = cls.mouseState.x;
  cls.mouseState.oldY = cls.mouseState.y;

  cls.mouseState.x = event->motion.xrel * m_sensitivity->value;
  cls.mouseState.y = event->motion.yrel * m_sensitivity->value;

  if (m_interpolate->value) {
    cls.mouseState.x = (cls.mouseState.x + cls.mouseState.oldX) * 0.5f;
    cls.mouseState.y = (cls.mouseState.y + cls.mouseState.oldY) * 0.5f;
  }

  if (cls.state == CL_ACTIVE) {
    if (m_invert->value) {
      cls.mouseState.y = -cls.mouseState.y;
    }

    cl.angles.y -= m_yaw->value * cls.mouseState.x;
    cl.angles.x += m_pitch->value * cls.mouseState.y;
  }
}
