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

/**
 * @file
 * @brief Audio View
 */

typedef struct AudioView AudioView;
typedef struct AudioViewInterface AudioViewInterface;

/**
 * @brief The AudioView type.
 * @extends View
 * @ingroup
 */
struct AudioView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	AudioViewInterface *interface;
};

/**
 * @brief The AudioView interface.
 */
struct AudioViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn AudioView *AudioView::initWithFrame(AudioView *self, const SDL_Rect *frame)
	 * @brief Initializes this AudioView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized AudioView, or `NULL` on error.
	 * @memberof AudioView
	 */
	AudioView *(*initWithFrame)(AudioView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *AudioView::_AudioView(void)
 * @brief The AudioView archetype.
 * @return The AudioView Class.
 * @memberof AudioView
 */
extern Class *_AudioView(void);
