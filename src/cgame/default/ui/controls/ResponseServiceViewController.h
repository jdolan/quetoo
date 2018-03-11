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

#include <ObjectivelyMVC/HueColorPicker.h>
#include <ObjectivelyMVC/ViewController.h>

#include "CrosshairView.h"

/**
 * @file
 * @brief Controls ViewController.
 */

typedef struct ResponseServiceViewController ResponseServiceViewController;
typedef struct ResponseServiceViewControllerInterface ResponseServiceViewControllerInterface;

/**
 * @brief The ResponseServiceViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct ResponseServiceViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	ResponseServiceViewControllerInterface *interface;

	/**
	 * @brief The crosshair HueColorPicker.
	 */
	HueColorPicker *crosshairColorPicker;

	/**
	 * @brief The crosshair Select.
	 */
	Select *crosshair;

	/**
	 * @brief The crosshair health Select.
	 */
	Select *crosshairHealth;

	/**
	 * @brief The CrosshairView.
	 */
	CrosshairView *crosshairView;
};

/**
 * @brief The ResponseServiceViewController interface.
 */
struct ResponseServiceViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *ResponseServiceViewController::_ResponseServiceViewController(void)
 * @brief The ResponseServiceViewController archetype.
 * @return The ResponseServiceViewController Class.
 * @memberof ResponseServiceViewController
 */
CGAME_EXPORT Class *_ResponseServiceViewController(void);
