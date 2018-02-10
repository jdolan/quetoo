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

#include "MapListCollectionView.h"

/**
 * @file
 *
 * @brief Create Server ViewController.
 */

typedef struct CreateServerViewController CreateServerViewController;
typedef struct CreateServerViewControllerInterface CreateServerViewControllerInterface;

/**
 * @brief The CreateServerViewController type.
 *
 * @extends ViewController
 */
struct CreateServerViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	CreateServerViewControllerInterface *interface;

	/**
	 * @brief The create Button.
	 */
	Button *create;

	/**
	 * @brief The gameplay Select.
	 */
	Select *gameplay;

	/**
	 * @brief The MapListCollectionView.
	 */
	MapListCollectionView *mapList;

	/**
	 * @brief The teamsplay Select.
	 */
	Select *teams;
};

/**
 * @brief The CreateServerViewController interface.
 */
struct CreateServerViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *CreateServerViewController::_CreateServerViewController(void)
 * @brief The CreateServerViewController archetype.
 * @return The CreateServerViewController Class.
 * @memberof CreateServerViewController
 */
CGAME_EXPORT Class *_CreateServerViewController(void);

