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

#include <ObjectivelyMVC/ColorSelect.h>

#include "CrosshairView.h"

/**
 * @file
 * @brief Interface View.
 */

typedef struct InterfaceView InterfaceView;
typedef struct InterfaceViewInterface InterfaceViewInterface;

/**
 * @brief The InterfaceView type.
 * @extends View
 * @ingroup
 */
struct InterfaceView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	InterfaceViewInterface *interface;

	/**
	 * @brief The CrosshairView.
	 */
	CrosshairView *crosshairView;

	/**
	 * @brief The ColorSelect for the crosshair.
	 */
	ColorSelect *crosshairColorSelect;
};

/**
 * @brief The InterfaceView interface.
 */
struct InterfaceViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn InterfaceView *InterfaceView::initWithFrame(InterfaceView *self, const SDL_Rect *frame)
	 * @brief Initializes this InterfaceView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized InterfaceView, or `NULL` on error.
	 * @memberof InterfaceView
	 */
	InterfaceView *(*initWithFrame)(InterfaceView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *InterfaceView::_InterfaceView(void)
 * @brief The InterfaceView archetype.
 * @return The InterfaceView Class.
 * @memberof InterfaceView
 */
extern Class *_InterfaceView(void);
