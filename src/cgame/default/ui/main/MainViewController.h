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
#include <ObjectivelyMVC/NavigationViewController.h>

#include "MainView.h"

/**
 * @file
 *
 * @brief The MainViewController.
 */

typedef struct MainViewController MainViewController;
typedef struct MainViewControllerInterface MainViewControllerInterface;

/**
 * @brief The MainViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct MainViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	MainViewControllerInterface *interface;

	/**
	 * @brief The MainView.
	 */
	MainView *mainView;

	/**
	 * @brief The NavigationViewController.
	 */
	NavigationViewController *navigationViewController;
};

/**
 * @brief The MainViewController interface.
 */
struct MainViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn MainViewController *MainViewController::init(MainViewController *self)
	 * @brief Initializes this ViewController.
	 * @return The initialized MainViewController, or `NULL` on error.
	 * @memberof MainViewController
	 */
	MainViewController *(*init)(MainViewController *self);

	/**
	 * @fn void MainViewController::primaryButton(const MainViewController *self, const char *title, ActionFunction action, ident data)
	 * @brief Adds a Button to the primary menu.
	 * @param self The MainViewController.
	 * @param title The title text.
	 * @param action The ActionFunction to bind to click events.
	 * @param data The user data.
	 * @memberof MainViewController
	 */
	void (*primaryButton)(MainViewController *self, const char *title, ActionFunction action, ident data);

	/**
	 * @fn void MainViewController::secondaryButton(const MainViewController *self, const char *title, ActionFunction action, ident data)
	 * @brief Adds a Button to the secondary menu.
	 * @param self The MainViewController.
	 * @param title The title text.
	 * @param action The ActionFunction to bind to click events.
	 * @param data The user data.
	 * @memberof MainViewController
	 */
	void (*secondaryButton)(MainViewController *self, const char *title, ActionFunction action, ident data);
};

/**
 * @fn Class *MainViewController::_MainViewController(void)
 * @brief The MainViewController archetype.
 * @return The MainViewController Class.
 * @memberof MainViewController
 */
CGAME_EXPORT Class *_MainViewController(void);
