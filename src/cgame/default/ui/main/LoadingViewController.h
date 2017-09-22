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

/**
 * @file
 *
 * @brief The LoadingViewController.
 */

typedef struct LoadingViewController LoadingViewController;
typedef struct LoadingViewControllerInterface LoadingViewControllerInterface;

/**
 * @brief The LoadingViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct LoadingViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	LoadingViewControllerInterface *interface;

	/**
	 * @brief The mapshot of the map being loaded.
	 */
	ImageView *mapshotImage;

	/**
	 * @brief The progress bar.
	 */
	ImageView *progressBar;

	/**
	 * @brief The progress label.
	 */
	Label *progressLabel;
};

/**
 * @brief The LoadingViewController interface.
 */
struct LoadingViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn LoadingViewController *LoadingViewController::init(LoadingViewController *self)
	 * @brief Initializes this ViewController.
	 * @return The initialized LoadingViewController, or `NULL` on error.
	 * @memberof LoadingViewController
	 */
	LoadingViewController *(*init)(LoadingViewController *self);

	/**
	 * @fn void LoadingViewController::setProgress(LoadingViewController *self, const cl_loading_t loading)
	 * @brief Sets the visual progress of the loading screen.
	 * @param percent The percent loaded.
	 * @param status The currently loading media item.
	 * @memberof LoadingViewController
	 */
	void (*setProgress)(LoadingViewController *self, const cl_loading_t loading);
};

/**
 * @fn Class *LoadingViewController::_LoadingViewController(void)
 * @brief The LoadingViewController archetype.
 * @return The LoadingViewController Class.
 * @memberof LoadingViewController
 */
extern Class *_LoadingViewController(void);
