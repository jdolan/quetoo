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
 * @brief The TabViewController.
 */

typedef struct TabViewController TabViewController;
typedef struct TabViewControllerInterface TabViewControllerInterface;

/**
 * @brief The TabViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct TabViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	TabViewControllerInterface *interface;

	/**
	 * @brief The StackView.
	 */
	StackView *stackView;
};

/**
 * @brief The TabViewController interface.
 */
struct TabViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn MainViewController *TabViewController::mainViewController(const TabViewController *self)
	 * @return The MainViewController.
	 * @memberof TabViewController
	 */
	MainViewController *(*mainViewController)(const TabViewController *self);
};

/**
 * @fn Class *TabViewController::_TabViewController(void)
 * @brief The TabViewController archetype.
 * @return The TabViewController Class.
 * @memberof TabViewController
 */
extern Class *_TabViewController(void);
