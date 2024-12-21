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
 * @brief Teams ViewController.
 */

typedef struct TeamsViewController TeamsViewController;
typedef struct TeamsViewControllerInterface TeamsViewControllerInterface;

/**
 * @brief The TeamsViewController type.
 * @extends ViewController
 * @ingroup
 */
struct TeamsViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	TeamsViewControllerInterface *interface;

	/**
	 * @brief The teams CollectionView.
	 */
	CollectionView *teamsCollectionView;

	/**
	 * @brief The spectate Button.
	 */
	Button *spectate;

	/**
	 * @brief The join Button.
	 */
	Button *join;
};

/**
 * @brief The TeamsViewController interface.
 */
struct TeamsViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *TeamsViewController::_TeamsViewController(void)
 * @brief The TeamsViewController archetype.
 * @return The TeamsViewController Class.
 * @memberof TeamsViewController
 */
CGAME_EXPORT Class *_TeamsViewController(void);
