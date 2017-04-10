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

#include <ObjectivelyMVC/Button.h>

/**
 * @file
 * @brief Primary Button.
 */

#define DEFAULT_PRIMARY_BUTTON_WIDTH 200

typedef struct TabButton TabButton;
typedef struct TabButtonInterface TabButtonInterface;

/**
 * @brief The TabButton type.
 * @extends Button
 */
struct TabButton {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Button button;

	/**
	 * @brief The interface.
	 * @private
	 */
	TabButtonInterface *interface;

	/**
	 * @brief If the tab is selected.
	 */
	 _Bool isSelected;

	/**
	 * @brief The selection highlight's ImageVIew.
	 */
	ImageView *selectionImage;
};

/**
 * @brief The TabButton interface.
 */
struct TabButtonInterface {

	/**
	 * @brief The superclass interface.
	 */
	ButtonInterface buttonInterface;

	/**
	 * @fn TabButton *TabButton::initWithFrame(TabButton *self, const SDL_Rect *frame, ControlStyle style)
	 * @brief Initializes this TabButton with the specified frame and style.
	 * @param frame The frame.
	 * @param style The ControlStyle.
	 * @return The initialized TabButton, or `NULL` on error.
	 * @memberof TabButton
	 */
	TabButton *(*initWithFrame)(TabButton *self, const SDL_Rect *frame, ControlStyle style);
};

/**
 * @fn Class *TabButton::_TabButton(void)
 * @brief The TabButton archetype.
 * @return The TabButton Class.
 * @memberof TabButton
 */
extern Class *_TabButton(void);
