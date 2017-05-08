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

#include "MapListCollectionView.h"

/**
 * @file
 *
 * @brief Create server View.
 */

typedef struct CreateServerView CreateServerView;
typedef struct CreateServerViewInterface CreateServerViewInterface;

/**
 * @brief The CreateServerView type.
 *
 * @extends View
 */
struct CreateServerView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	CreateServerViewInterface *interface;

	/**
	 * @brief The gameplay Select.
	 */
	Select *gameplay;

	/**
	 * @brief The MapListCollectionView.
	 */
	MapListCollectionView *mapList;

	/**
	 * @brief The teamsplay Select.
	 */
	Select *teamsplay;

	/**
	 * @brief The match mode Checkbox.
	 */
	Checkbox *matchMode;
};

/**
 * @brief The CreateServerView interface.
 */
struct CreateServerViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn CreateServerView *CreateServerView::initWithFrame(CreateServerView *self, const SDL_Rect *frame)
	 * @brief Initializes this CreateServerView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized CreateServerView, or `NULL` on error.
	 * @memberof CreateServerView
	 */
	CreateServerView *(*initWithFrame)(CreateServerView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *CreateServerView::_CreateServerView(void)
 * @brief The CreateServerView archetype.
 * @return The CreateServerView Class.
 * @memberof CreateServerView
 */
extern Class *_CreateServerView(void);
