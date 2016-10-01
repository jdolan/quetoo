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
 *
 * @brief Player skin Select.
 */

typedef struct SkinSelect SkinSelect;
typedef struct SkinSelectInterface SkinSelectInterface;

/**
 * @brief The SkinSelect type.
 *
 * @extends Select
 */
struct SkinSelect {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	Select select;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	SkinSelectInterface *interface;	
};

/**
 * @brief The SkinSelect interface.
 */
struct SkinSelectInterface {
	
	/**
	 * @brief The parent interface.
	 */
	SelectInterface selectInterface;
	
	/**
	 * @fn SkinSelect *SkinSelect::initWithFrame(SkinSelect *self, const SDL_Rect *frame, ControlStyle style)
	 *
	 * @brief Initializes this SkinSelect with the specified frame and style.
	 *
	 * @param frame The frame.
	 * @param style The ControlStyle.
	 *
	 * @return The initialized SkinSelect, or `NULL` on error.
	 *
	 * @memberof SkinSelect
	 */
	SkinSelect *(*initWithFrame)(SkinSelect *self, const SDL_Rect *frame, ControlStyle style);
};

/**
 * @brief The SkinSelect Class.
 */
extern Class _SkinSelect;

