/*
 * ObjectivelyMVC: MVC framework for OpenGL and SDL2 in c.
 * Copyright (C) 2014 Jay Dolan <jay@jaydolan.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include "MenuViewController.h"

/**
 * @file
 *
 * @brief Controls ViewController.
 */

typedef struct ControlsViewController ControlsViewController;
typedef struct ControlsViewControllerInterface ControlsViewControllerInterface;

/**
 * @brief The ControlsViewController type.
 *
 * @extends MenuViewController
 *
 * @ingroup
 */
struct ControlsViewController {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	MenuViewController menuViewController;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	ControlsViewControllerInterface *interface;
};

/**
 * @brief The ControlsViewController interface.
 */
struct ControlsViewControllerInterface {
	
	/**
	 * @brief The parent interface.
	 */
	MenuViewControllerInterface menuViewControllerInterface;
};

/**
 * @brief The ControlsViewController Class.
 */
extern Class _ControlsViewController;

