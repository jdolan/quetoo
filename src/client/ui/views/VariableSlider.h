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

#include <ObjectivelyMVC/Input.h>
#include <ObjectivelyMVC/StackView.h>

#include "cvar.h"

/**
 * @file
 *
 * @brief An Input containing a Slider bound to a cvar_t.
 */

#define VARIABLE_SLIDER_INPUT_SLIDER_WIDTH 80
#define VARIABLE_SLIDER_INPUT_NAME_WIDTH 100

typedef struct VariableSlider VariableSlider;
typedef struct VariableSliderInterface VariableSliderInterface;

/**
 * @brief The VariableSlider type.
 *
 * @extends Input
 *
 * @ingroup Containers
 */
struct VariableSlider {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	StackView stackView;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	VariableSliderInterface *interface;

	/**
	 * @brief The variable name (e.g. `Sensitivity`).
	 */
	const char *name;

	/**
	 *
	 */
	Label *output;

	/**
	 * @brief The slider.
	 */
	Slider *slider;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;
};

/**
 * @brief The VariableSlider interface.
 */
struct VariableSliderInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;
	
	/**
	 * @fn VariableSlider *VariableSlider::initWithVariable(VariableSlider *self, cvar_t *var, const char *name)
	 *
	 * @brief Initializes this VariableSlider with the given variable.
	 *
	 * @param var The variable.
	 * @param name The variable name (e.g. `Sensitivity`).
	 *
	 * @return The initialized VariableSlider, or `NULL` on error.
	 *
	 * @memberof VariableSlider
	 */
	VariableSlider *(*initWithVariable)(VariableSlider *self, cvar_t *var, const char *name);
};

/**
 * @brief The VariableSlider Class.
 */
extern Class _VariableSlider;

