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

#include <ObjectivelyMVC/ViewController.h>

/**
 * @file
 * @brief The console layer: the console, the notify lines, and chat.
 */

typedef struct ConsoleViewController ConsoleViewController;
typedef struct ConsoleViewControllerInterface ConsoleViewControllerInterface;

/**
 * @brief The console layer, above the menus: the drop-down console, the notify lines in the
 * top left, and chat with its input two thirds of the way down.
 * @details These Views render the client's `console_t` state; keys still feed that state
 * through the client's own key handling, so only the drawing lives here.
 * @extends ViewController
 */
struct ConsoleViewController {

  /**
   * @brief The superclass.
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @protected
   */
  ConsoleViewControllerInterface *interface[0];

  /**
   * @brief The console, holding `background`, `buffer` and `input`.
   */
  View *console;

  /**
   * @brief The console art, scaled to cover the screen and anchored to the console's bottom
   * edge, so the console slides down over it as it always has; a plain fill when it is missing.
   */
  ImageView *background;

  /**
   * @brief The console's visible tail and its input line.
   */
  Text *buffer, *input;

  /**
   * @brief The notify lines.
   */
  Text *notify;

  /**
   * @brief Recent chat, and the chat input line.
   */
  Text *chat, *chatInput;
};

struct ConsoleViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn void ConsoleViewController::update(ConsoleViewController *self)
   * @brief Updates the console, notify and chat for the frame about to be drawn.
   * @param self The ConsoleViewController.
   * @memberof ConsoleViewController
   */
  void (*update)(ConsoleViewController *self);
};

Class *_ConsoleViewController(void);
