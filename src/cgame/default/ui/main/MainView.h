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

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/ImageView.h>

/**
 * @file
 * @brief The MainViewController's View.
 */

typedef struct MainView MainView;
typedef struct MainViewInterface MainViewInterface;

/**
 * @brief The MainView type.
 * @extends View
 */
struct MainView {

	/**
	 * @brief The superclass.
	 */
	View view;

	/**
	 * @brief The interface.
	 * @protected
	 */
	MainViewInterface *interface;

	/**
	 * @brief The background image.
	 */
	ImageView *background;

	/**
	 * @brief The logo image.
	 */
	ImageView *logo;

	/**
	 * @brief The version string.
	 */
	Label *version;

	/**
	 * @brief The content View.
	 */
	View *contentView;

	/**
	 * @brief The top menu bar.
	 */
	StackView *primaryMenu;

	/**
	 * @brief The bottom menu bar.
	 */
	StackView *secondaryMenu;
};

/**
 * @brief The MainView interface.
 */
struct MainViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn MainView *MainView::init(MainView *self)
	 * @brief Initializes this MainView.
	 * @param self The MainView.
	 * @param frame The frame, or `NULL`.
	 * @return The initialized MainView, or `NULL` on error.
	 * @memberof MainView
	 */
	MainView *(*initWithFrame)(MainView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *MainView::_MainView(void)
 * @brief The MainView archetype.
 * @return The MainView Class.
 * @memberof MainView
 */
CGAME_EXPORT Class *_MainView(void);
