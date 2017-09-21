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
 * @brief Info ViewController.
 */

typedef struct InfoViewController InfoViewController;
typedef struct InfoViewControllerInterface InfoViewControllerInterface;

/**
 * @brief The InfoViewController type.
 * @extends ViewController
 * @ingroup
 */
struct InfoViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController menuViewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	InfoViewControllerInterface *interface;

	/**
	 * @brief TabViewController to contain tabs.
	 */
	TabViewController *tabViewController;
};

/**
 * @brief The InfoViewController interface.
 */
struct InfoViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *InfoViewController::_InfoViewController(void)
 * @brief The InfoViewController archetype.
 * @return The InfoViewController Class.
 * @memberof InfoViewController
 */
extern Class *_InfoViewController(void);
