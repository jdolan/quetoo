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
 * @brief Input View.
 */

typedef struct InputView InputView;
typedef struct InputViewInterface InputViewInterface;

/**
 * @brief The InputView type.
 * @extends View
 * @ingroup
 */
struct InputView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	InputViewInterface *interface;
};

/**
 * @brief The InputView interface.
 */
struct InputViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn InputView *InputView::initWithFrame(InputView *self, const SDL_Rect *frame)
	 * @brief Initializes this InputView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized InputView, or `NULL` on error.
	 * @memberof InputView
	 */
	InputView *(*initWithFrame)(InputView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *InputView::_InputView(void)
 * @brief The InputView archetype.
 * @return The InputView Class.
 * @memberof InputView
 */
extern Class *_InputView(void);
