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

#include "ConsoleView.h"

/**
 * @file
 * @brief The console layer.
 */

typedef struct ConsoleViewController ConsoleViewController;
typedef struct ConsoleViewControllerInterface ConsoleViewControllerInterface;

/**
 * @brief The console layer, above the menus and the HUD, holding the drop-down console.
 * @details The layer fills the window but never takes a hit, so the menus beneath stay clickable.
 * The console shows while the key destination is the console, filling the window until there is
 * a game to show and `cl_console_height` of it after.
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
   * @brief The console.
   */
  ConsoleView *console;
};

struct ConsoleViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;

  /**
   * @fn void ConsoleViewController::update(ConsoleViewController *self)
   * @brief Updates the console for the frame about to be drawn.
   * @param self The ConsoleViewController.
   * @memberof ConsoleViewController
   */
  void (*update)(ConsoleViewController *self);
};

Class *_ConsoleViewController(void);
