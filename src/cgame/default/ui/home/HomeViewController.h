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

#include "cg_types.h"

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief Home ViewController.
 */

typedef struct HomeViewController HomeViewController;
typedef struct HomeViewControllerInterface HomeViewControllerInterface;

/**
 * @brief The HomeViewController type.
 * @extends HomeViewController
 * @ingroup
 */
struct HomeViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	HomeViewControllerInterface *interface;

	/**
	 * @brief The message of the day.
	 */
	Label *motd;
};

/**
 * @brief The HomeViewController interface.
 */
struct HomeViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @brief The HomeViewController archetype.
 * @return The HomeViewController Class.
 * @memberof HomeViewController
 */
extern Class *_HomeViewController(void);
