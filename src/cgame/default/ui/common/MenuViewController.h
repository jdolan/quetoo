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

#include "MainViewController.h"

#include "cg_local.h"

/**
 * @file
 *
 * @brief The MenuViewController.
 */

typedef struct MenuViewController MenuViewController;
typedef struct MenuViewControllerInterface MenuViewControllerInterface;

/**
 * @brief The MenuViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct MenuViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	MenuViewControllerInterface *interface;

	/**
	 * @brief The Panel.
	 */
	Panel *panel;
};

/**
 * @brief The MenuViewController interface.
 */
struct MenuViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn MainViewController *MenuViewController::mainViewController(const MenuViewController *self)
	 * @return The MainViewController.
	 * @memberof MenuViewController
	 */
	MainViewController *(*mainViewController)(const MenuViewController *self);
};

/**
 * @fn Class *MenuViewController::_MenuViewController(void)
 * @brief The MenuViewController archetype.
 * @return The MenuViewController Class.
 * @memberof MenuViewController
 */
extern Class *_MenuViewController(void);
