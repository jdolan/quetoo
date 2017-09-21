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

#include <ObjectivelyMVC/ViewController.h>
#include <ObjectivelyMVC/HueColorPicker.h>

/**
 * @file
 * @brief Interface ViewController.
 */

typedef struct OptionsViewController OptionsViewController;
typedef struct OptionsViewControllerInterface OptionsViewControllerInterface;

/**
 * @brief The OptionsViewController type.
 * @extends ViewController
 */
struct OptionsViewController {

	/**
	 * @brief The superclass.
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @protected
	 */
	OptionsViewControllerInterface *interface;

	/**
	 * @brief The crosshair HSVColorPicker.
	 */
	HueColorPicker *crosshairColorPicker;
};

/**
 * @brief The OptionsViewController interface.
 */
struct OptionsViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *OptionsViewController::_OptionsViewController(void)
 * @brief The OptionsViewController archetype.
 * @return The OptionsViewController Class.
 * @memberof OptionsViewController
 */
OBJECTIVELY_EXPORT Class *_OptionsViewController(void);
