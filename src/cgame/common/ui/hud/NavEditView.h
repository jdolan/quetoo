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

#pragma once

#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief The navigation edit mode instructions, shown in place of the HUD while editing.
 */

typedef struct NavEditView NavEditView;
typedef struct NavEditViewInterface NavEditViewInterface;

/**
 * @brief The navigation edit mode instructions, shown in place of the HUD while editing.
 * @details Shown while `cg_state.nav_edit` is on; the key names it quotes follow the binds.
 * @extends View
 */
struct NavEditView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  NavEditViewInterface *interface[0];

  /**
   * @brief The instructions.
   */
  Text *text;
};

struct NavEditViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;
};

CGAME_EXPORT Class *_NavEditView(void);
