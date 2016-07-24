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
 * @brief The MainViewController.
 */

typedef struct MainViewController MainViewController;
typedef struct MainViewControllerInterface MainViewControllerInterface;

/**
 * @brief The MainViewController type.
 *
 * @extends MenuViewController
 *
 * @ingroup ViewControllers
 */
struct MainViewController {
	
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
	MainViewControllerInterface *interface;
};

/**
 * @brief The MainViewController interface.
 */
struct MainViewControllerInterface {
	
	/**
	 * @brief The parent interface.
	 */
	MenuViewControllerInterface menuViewControllerInterface;
};

/**
 * @brief The MainViewController Class.
 */
extern Class _MainViewController;

