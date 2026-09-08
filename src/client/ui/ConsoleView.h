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

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/Text.h>

/**
 * @file
 * @brief The drop-down console.
 */

typedef struct ConsoleView ConsoleView;
typedef struct ConsoleViewInterface ConsoleViewInterface;

/**
 * @brief The drop-down console: the console art, the buffer's visible tail and the input line.
 * @details Renders the client's `cl_console`; keys feed that state through the client's own key
 * handling, so only the drawing lives here.
 * @extends View
 */
struct ConsoleView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  ConsoleViewInterface *interface[0];

  /**
   * @brief The console art, scaled to cover the screen and anchored to the console's bottom
   * edge, so the console slides down over it as it always has; a plain fill when it is missing.
   */
  ImageView *background;

  /**
   * @brief The console's visible tail and its input line.
   */
  Text *buffer, *input;
};

struct ConsoleViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn void ConsoleView::update(ConsoleView *self, int32_t height)
   * @brief Sizes the console to `height` and fills the buffer and input for the frame.
   * @param self The ConsoleView.
   * @param height The console height, in points.
   * @remarks Does nothing until the font has resolved and the superview has a size.
   * @memberof ConsoleView
   */
  void (*update)(ConsoleView *self, int32_t height);
};

Class *_ConsoleView(void);
