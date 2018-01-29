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

#include "DialogView.h"

/**
 * @file
 * @brief Dialog ViewController.
 */

typedef struct Dialog Dialog;

typedef struct DialogViewController DialogViewController;
typedef struct DialogViewControllerInterface DialogViewControllerInterface;

/**
 * @brief The Dialog type.
 */
struct Dialog {

	/**
	 * @brief The data.
	 */
	ident data;

	/**
	 * @brief The message.
	 */
	const char *message;

	/**
	 * @brief The Ok text.
	 */
	const char *ok;

	/**
	 * @brief the Cancel text.
	 */
	const char *cancel;

	/**
	 * @brief The Ok callback.
	 */
	void (*okFunction)(ident data);

	/**
	 * @brief The Cancel callback.
	 */
	void (*cancelFunction)(ident data);
};

/**
 * @brief The DialogViewController type.
 * @extends ViewController
 */
struct DialogViewController {

	/**
	 * @brief The superclass.
	 */
	ViewController viewController;

	/**
	 * @brief The interface.
	 * @protected
	 */
	DialogViewControllerInterface *interface;

	/**
	 * @brief The dialog.
	 */
	Dialog dialog;

	/**
	 * @brief The DialogView.
	 */
	DialogView *dialogView;
};

/**
 * @brief The DialogViewController interface.
 */
struct DialogViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewControllerInterface viewControllerInterface;

	/**
	 * @fn DialogViewController *DialogViewController::initWithDialog(DialogViewController *self, const Dialog *dialog)
	 * @brief Initializes this DialogViewController.
	 * @param self The DialogViewController.
	 * @param dialog The Dialog.
	 * @return The initialized DialogViewController, or `NULL` on error.
	 * @memberof DialogViewController
	 */
	DialogViewController *(*initWithDialog)(DialogViewController *self, const Dialog *dialog);
};

/**
 * @fn Class *DialogViewController::_DialogViewController(void)
 * @brief The DialogViewController archetype.
 * @return The DialogViewController Class.
 * @memberof DialogViewController
 */
Class *_DialogViewController(void);
