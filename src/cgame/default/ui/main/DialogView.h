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

/**
 * @file
 * @brief Popup dialog.
 */

typedef struct DialogView DialogView;
typedef struct DialogViewInterface DialogViewInterface;

/**
 * @brief The DialogView type.
 * @extends Panel
 * @ingroup
 */
struct DialogView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Panel panel;

	/**
	 * @brief The interface.
	 * @private
	 */
	DialogViewInterface *interface;

	/**
	 * @brief The message.
	 */
	Label *message;

	/**
	 * @brief The Ok button.
	 */
	Button *okButton;

	/**
	 * @brief The Cancel button.
	 */
	Button *cancelButton;
};

/**
 * @brief The DialogView interface.
 */
struct DialogViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	PanelInterface panelInterface;

	/**
	 * @fn DialogView *DialogView::init(CvarSlider *self)
	 * @brief Initializes a dialog
	 * @return The initialized DialogView, or `NULL` on error.
	 * @memberof DialogView
	 */
	DialogView *(*init)(DialogView *self);

	/**
	 * @fn DialogView *DialogView::showDialog(DialogView *self, const char *text, void (*okFUnction)(void))
	 * @brief Shows the dialog
	 * @param text The message to display
	 * @param okFunction The function to call when the Ok button was clicked
	 * @memberof DialogView
	 */
	void (*showDialog)(DialogView *self, const char *text, const char *cancelText, const char *okText, void (*okFunction)(void));

	/**
	 * @fn DialogView *DialogView::hideView(CvarSlider *self)
	 * @brief Hides the dialog
	 * @memberof DialogView
	 */
	void (*hideDialog)(DialogView *self);
};

/**
 * @fn Class *DialogView::_DialogView(void)
 * @brief The DialogView archetype.
 * @return The DialogView Class.
 * @memberof DialogView
 */
CGAME_EXPORT Class *_DialogView(void);
