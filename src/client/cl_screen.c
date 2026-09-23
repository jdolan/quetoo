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
 * @brief This is called at least once per frame, and more often during loading.
 * @details The menus are told they will appear again when the client enters or leaves a game
 * beneath them, since what they show depends on whether there is one to return to.
 */
void Cl_UpdateScreen(void) {

  static ClientKeyDest previous_key_dest = KEY_UI;
  static bool previous_active = false;

  const bool active = cls.state == CL_ACTIVE;

  if (cls.keyState.dest == KEY_UI) {
    if (previous_key_dest != KEY_UI || previous_active != active) {
      Ui_ViewWillAppear();
    }
  } else {
    if (previous_key_dest == KEY_UI) {
      Ui_ViewWillDisappear();
    }
  }
  previous_key_dest = cls.keyState.dest;
  previous_active = active;

  switch (cls.state) {
    case CL_UNINITIALIZED:
    case CL_DISCONNECTED:
    case CL_CONNECTING:
    case CL_CONNECTED:
      if (cls.keyState.dest == KEY_UI || cls.keyState.dest == KEY_CONSOLE) {
        Ui_Draw();
      }
      break;

    case CL_LOADING:
      Ui_Draw();
      break;

    case CL_ACTIVE:
      if (cls.keyState.dest != KEY_UI) {
        cls.cgame->UpdateScreen(&cl.frame);
      }

      Ui_Draw();
      break;
  }
}
