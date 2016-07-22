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

#include <ObjectivelyMVC.h>

/**
 * @file
 *
 * @brief The MenuViewController.
 */

#define DEFAULT_MENU_STACKVIEW_SPACING 10

typedef struct MenuViewController MenuViewController;
typedef struct MenuViewControllerInterface MenuViewControllerInterface;

/**
 * @brief The MenuViewController type.
 *
 * @extends ViewController
 *
 * @ingroup
 */
struct MenuViewController {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	ViewController viewController;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	MenuViewControllerInterface *interface;

	/**
	 * @brief The Panel.
	 */
	Panel *panel;

	/**
	 * @brief The StackView
	 */
	StackView *stackView;
};

/**
 * @brief The MenuViewController interface.
 */
struct MenuViewControllerInterface {
	
	/**
	 * @brief The parent interface.
	 */
	ViewControllerInterface viewControllerInterface;
	
	/**
	 * @fn MenuViewController *MenuViewController::init(MenuViewController *self)
	 *
	 * @brief Initializes this MenuViewController.
	 *
	 * @return The initialized MenuViewController, or `NULL` on error.
	 *
	 * @memberof MenuViewController
	 */
	MenuViewController *(*init)(MenuViewController *self);	
};

/**
 * @brief The MenuViewController Class.
 */
extern Class _MenuViewController;

