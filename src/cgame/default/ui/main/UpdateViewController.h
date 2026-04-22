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

#include "common/installer.h"

/**
 * @file
 *
 * @brief The UpdateViewController — Shown while the in-game installer is running, displaying progress and hero images.
 */

typedef struct UpdateViewController UpdateViewController;
typedef struct UpdateViewControllerInterface UpdateViewControllerInterface;

/**
 * @brief The UpdateViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct UpdateViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	UpdateViewControllerInterface *interface;

	/**
	 * @brief The hero image slideshow.
	 */
	SlideShowView *slideShow;

	/**
	 * @brief The background thread fetching hero images.
	 */
	thread_t *fetchThread;

	/**
	 * @brief Pending hero images fetched by the background thread, then added to slideShow on the main thread.
	 */
	MutableArray *pendingImages;

  /**
   * @brief A mutex for ensuring consistent access to the pendingImages array.
   */
  SDL_Mutex *pendingImagesLock;

	/**
	 * @brief The Quetoo logo.
	 */
	ImageView *logo;

	/**
	 * @brief The progress bar.
	 */
	ProgressBar *progressBar;
};

/**
 * @brief The UpdateViewController interface.
 */
struct UpdateViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn UpdateViewController *UpdateViewController::init(UpdateViewController *self)
	 * @brief Initializes this ViewController.
	 * @return The initialized UpdateViewController, or `NULL` on error.
	 * @memberof UpdateViewController
	 */
	UpdateViewController *(*init)(UpdateViewController *self);

	/**
	 * @fn void UpdateViewController::setStatus(UpdateViewController *self, installer_status_t status)
	 * @brief Updates progress display with the current installer status.
	 * @memberof UpdateViewController
	 */
	void (*setStatus)(UpdateViewController *self, installer_status_t status);
};

/**
 * @fn Class *UpdateViewController::_UpdateViewController(void)
 * @brief The UpdateViewController archetype.
 * @return The UpdateViewController Class.
 * @memberof UpdateViewController
 */
CGAME_EXPORT Class *_UpdateViewController(void);
