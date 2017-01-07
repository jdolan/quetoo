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

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/View.h>

/**
 * @file
 * @brief The CrosshairView type.
 */

typedef struct CrosshairView CrosshairView;
typedef struct CrosshairViewInterface CrosshairViewInterface;

/**
 * @brief The CrosshairView type.
 * @extends View
 */
struct CrosshairView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	CrosshairViewInterface *interface;

	/**
	 * @brief The ImageView.
	 */
	ImageView *imageView;
};

/**
 * @brief The CrosshairView interface.
 */
struct CrosshairViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ImageViewInterface imageViewInterface;

	/**
	 * @fn CrosshairView *CrosshairView::initWithFrame(CrosshairView *self, const SDL_Rect *frame)
	 * @brief Initializes this CrosshairView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized CrosshairView, or `NULL` on error.
	 * @memberof CrosshairView
	 */
	CrosshairView *(*initWithFrame)(CrosshairView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *CrosshairView::_CrosshairView(void)
 * @brief The CrosshairView archetype.
 * @return The CrosshairView Class.
 * @memberof CrosshairView
 */
extern Class *_CrosshairView(void);

