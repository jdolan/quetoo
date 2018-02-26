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

#include <ObjectivelyMVC/Slider.h>

/**
 * @file
 * @brief A Slider bound to a cvar_t.
 */

typedef struct CvarSlider CvarSlider;
typedef struct CvarSliderInterface CvarSliderInterface;

/**
 * @brief The CvarSlider type.
 * @extends Slider
 */
struct CvarSlider {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Slider slider;

	/**
	 * @brief The interface.
	 * @private
	 */
	CvarSliderInterface *interface;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;
};

/**
 * @brief The CvarSlider interface.
 */
struct CvarSliderInterface {

	/**
	 * @brief The superclass interface.
	 */
	SliderInterface sliderInterface;

	/**
	 * @fn CvarSlider *CvarSlider::initWithVariable(CvarSlider *self, cvar_t *var, double min, double max, double step)
	 * @brief Initializes this Slider with the given variable.
	 * @param var The variable.
	 * @param min The minimum value.
	 * @param max The maximum value.
	 * @param step The step.
	 * @return The initialized CvarSlider, or `NULL` on error.
	 * @memberof CvarSlider
	 */
	CvarSlider *(*initWithVariable)(CvarSlider *self, cvar_t *var, double min, double max, double step);
};

/**
 * @fn Class *CvarSlider::_CvarSlider(void)
 * @brief The CvarSlider archetype.
 * @return The CvarSlider Class.
 * @memberof CvarSlider
 */
CGAME_EXPORT Class *_CvarSlider(void);

