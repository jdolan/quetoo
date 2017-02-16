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

#include "EditorViewController.h"

#include "ui_local.h"

/**
 * @file
 *
 * @brief The ClientMenuViewController.
 */

typedef struct ClientMenuViewController ClientMenuViewController;
typedef struct ClientMenuViewControllerInterface ClientMenuViewControllerInterface;

/**
 * @brief The ClientMenuViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct ClientMenuViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	ClientMenuViewControllerInterface *interface;

	/**
	 * @brief The Panel.
	 */
	Panel *panel;
};

/**
 * @brief The ClientMenuViewController interface.
 */
struct ClientMenuViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn EditorViewController *ClientMenuViewController::editorViewController(const ClientMenuViewController *self)
	 * @return The MainViewController.
	 * @memberof ClientMenuViewController
	 */
	EditorViewController *(*editorViewController)(const ClientMenuViewController *self);
};

/**
 * @fn Class *ClientMenuViewController::_ClientMenuViewController(void)
 * @brief The ClientMenuViewController archetype.
 * @return The ClientMenuViewController Class.
 * @memberof ClientMenuViewController
 */
extern Class *_ClientMenuViewController(void);
