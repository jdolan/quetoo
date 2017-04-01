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
#include "CrosshairView.h"

/**
 * @file
 * @brief System ViewController.
 */

typedef struct InterfaceViewController InterfaceViewController;
typedef struct InterfaceViewControllerInterface InterfaceViewControllerInterface;

/**
 * @brief The InterfaceViewController type.
 * @extends TabViewController
 * @ingroup
 */
struct InterfaceViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	TabViewController tabViewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	InterfaceViewControllerInterface *interface;

	/**
	 * @brief The CrosshairView.
	 */
	CrosshairView *crosshairView;
};

/**
 * @brief The InterfaceViewController interface.
 */
struct InterfaceViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	TabViewControllerInterface tabViewControllerInterface;
};

/**
 * @fn Class *InterfaceViewController::_InterfaceViewController(void)
 * @brief The InterfaceViewController archetype.
 * @return The InterfaceViewController Class.
 * @memberof InterfaceViewController
 */
extern Class *_InterfaceViewController(void);
