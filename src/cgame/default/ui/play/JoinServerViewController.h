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
 * @brief Join server ViewController.
 */

typedef struct JoinServerViewController JoinServerViewController;
typedef struct JoinServerViewControllerInterface JoinServerViewControllerInterface;

/**
 * @brief The JoinServerViewController type.
 * @extends ViewController
 * @ingroup
 */
struct JoinServerViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	JoinServerViewControllerInterface *interface;

	/**
	 * @brief A copy of the client's servers list, for sorting, etc.
	 */
	GList *servers;

	/**
	 * @brief The servers TableView.
	 */
	TableView *serversTableView;
};

/**
 * @brief The JoinServerViewController interface.
 */
struct JoinServerViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn void JoinServerViewController::reloadServers(JoinServerViewController *self)
	 * @brief Reloads the list of known servers.
	 * @param self The JoinServerViewController.
	 * @memberof JoinServerViewController
	 */
	void (*reloadServers)(JoinServerViewController *self);
};

/**
 * @fn Class *JoinServerViewController::_JoinServerViewController(void)
 * @brief The JoinServerViewController archetype.
 * @return The JoinServerViewController Class.
 * @memberof JoinServerViewController
 */
extern Class *_JoinServerViewController(void);

