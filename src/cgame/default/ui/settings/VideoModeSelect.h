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

#include <ObjectivelyMVC/Select.h>

/**
 * @file
 * @brief Video mode selection.
 */

typedef struct VideoModeSelect VideoModeSelect;
typedef struct VideoModeSelectInterface VideoModeSelectInterface;

/**
 * @brief The VideoModeSelect type.
 * @extends Select
 */
struct VideoModeSelect {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Select select;

	/**
	 * @brief The interface.
	 * @private
	 */
	VideoModeSelectInterface *interface;

	/**
	 * @brief The known display modes.
	 * @private
	 */
	SDL_DisplayMode *modes;
};

/**
 * @brief The VideoModeSelect interface.
 */
struct VideoModeSelectInterface {

	/**
	 * @brief The superclass interface.
	 */
	SelectInterface selectInterface;

	/**
	 * @fn VideoModeSelect *VideoModeSelect::initWithFrame(VideoModeSelect *self, const SDL_Rect *frame)
	 * @brief Initializes this VideoModeSelect with the specified frame and style.
	 * @param frame The frame.
	 * @return The initialized VideoModeSelect, or `NULL` on error.
	 * @memberof VideoModeSelect
	 */
	VideoModeSelect *(*initWithFrame)(VideoModeSelect *self, const SDL_Rect *frame);
};

/**
 * @fn Class *VideoModeSelect::_VideoModeSelect(void)
 * @brief The VideoModeSelect archetype.
 * @return The VideoModeSelect Class.
 * @memberof VideoModeSelect
 */
CGAME_EXPORT Class *_VideoModeSelect(void);

