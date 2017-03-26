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

#include "TabViewController.h"

/**
 * @file
 * @brief Quick join ViewController.
 */

typedef struct QuickJoinViewController QuickJoinViewController;
typedef struct QuickJoinViewControllerInterface QuickJoinViewControllerInterface;

/**
 * @brief The QuickJoinViewController type.
 * @extends TabViewController
 * @ingroup
 */
struct QuickJoinViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	TabViewController menuViewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	QuickJoinViewControllerInterface *interface;
};

/**
 * @brief The QuickJoinViewController interface.
 */
struct QuickJoinViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	TabViewControllerInterface menuViewControllerInterface;
};

/**
 * @fn Class *QuickJoinViewController::_QuickJoinViewController(void)
 * @brief The QuickJoinViewController archetype.
 * @return The QuickJoinViewController Class.
 * @memberof QuickJoinViewController
 */
extern Class *_QuickJoinViewController(void);

