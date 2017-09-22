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
 * @brief Credits ViewController
 */

typedef struct CreditsViewController CreditsViewController;
typedef struct CreditsViewControllerInterface CreditsViewControllerInterface;

/**
 * @brief The CreditsViewController type.
 * @extends ViewController
 * @ingroup
 */
struct CreditsViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	CreditsViewControllerInterface *interface;

	/**
	 * @brief The list of credits, loaded from a file.
	 */
	GSList *credits;

	/**
	 * @brief The credits TableView.
	 */
	TableView *tableView;
};

/**
 * @brief The CreditsViewController interface.
 */
struct CreditsViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewInterface;

	/**
	 * @fn CreditsViewController *CreditsViewController::init(CreditsViewController *self)
	 * @brief Initializes this ViewController.
	 * @return The initialized CreditsViewController, or `NULL` on error.
	 * @memberof CreditsViewController
	 */
	CreditsViewController *(*init)(CreditsViewController *self);

	/**
	 * @fn void CreditsViewController::loadCredits(CreditsViewController *self, const char *path)
	 * @brief Loads the credits from a file
	 * @memberof CreditsViewController
	 */
	void (*loadCredits)(CreditsViewController *self, const char *path);
};

/**
 * @fn Class *CreditsViewController::_CreditsViewController(void)
 * @brief The CreditsViewController archetype.
 * @return The CreditsViewController Class.
 * @memberof CreditsViewController
 */
extern Class *_CreditsViewController(void);
