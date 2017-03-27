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

#include <ObjectivelyMVC/Slider.h>

#include "cg_types.h"

/**
 * @file
 * @brief A Slider that visualizes hue.
 */

typedef struct HueSlider HueSlider;
typedef struct HueSliderInterface HueSliderInterface;

/**
 * @brief The HueSlider type.
 * @extends Slider
 */
struct HueSlider {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Slider slider;

	/**
	 * @brief The interface.
	 * @private
	 */
	HueSliderInterface *interface;

	/**
	 * @brief The function that's called when the slider's value changes.
	 */
	void (*changeFunction)(double hue);
};

/**
 * @brief The HueSlider interface.
 */
struct HueSliderInterface {

	/**
	 * @brief The superclass interface.
	 */
	SliderInterface sliderInterface;

	/**
	 * @fn HueSlider *HueSlider::initWithVariable(HueSlider *self, double hue)
	 * @brief Initializes this Slider with the given variable.
	 * @param hue The hue.
	 * @param changeFunction The function that's called when the hud slider's value changes
	 * @return The initialized HueSlider, or `NULL` on error.
	 * @memberof HueSlider
	 */
	HueSlider *(*initWithVariable)(HueSlider *self, double hue, void (*changeFunction)(double hue));
};

/**
 * @fn Class *HueSlider::_HueSlider(void)
 * @brief The HueSlider archetype.
 * @return The HueSlider Class.
 * @memberof HueSlider
 */
extern Class *_HueSlider(void);
