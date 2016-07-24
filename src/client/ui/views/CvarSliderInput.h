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

#include "cvar.h"

/**
 * @file
 *
 * @brief An Input exposing a cvar_t with a Slider.
 */

#define CVAR_SLIDER_INPUT_LABEL_WIDTH 120
#define CVAR_SLIDER_INPUT_SLIDER_WIDTH 80
#define CVAR_SLIDER_INPUT_OUTPUT_WIDTH 20

typedef struct CvarSliderInput CvarSliderInput;
typedef struct CvarSliderInputInterface CvarSliderInputInterface;

/**
 * @brief The CvarSliderInput type.
 *
 * @extends Input
 */
struct CvarSliderInput {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	Input input;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	CvarSliderInputInterface *interface;

	/**
	 * @brief The label that displays the variable's current value.
	 */
	Label *output;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;
};

/**
 * @brief The CvarSliderInput interface.
 */
struct CvarSliderInputInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;

	/**
	 * @fn CvarSliderInput *CvarSliderInput::initWithVariable(CvarSliderInput *self, cvar_t *var, const char *name, double min, double max, double step)
	 *
	 * @brief Initializes this Input with the given variable.
	 *
	 * @param view The View.
	 * @param var The variable.
	 * @param name The variable name (e.g. `Sensitivity`).
	 * @param min The minimum value.
	 * @param max The maximum value.
	 * @param step The step.
	 *
	 * @return The initialized CvarSliderInput, or `NULL` on error.
	 *
	 * @memberof CvarSliderInput
	 */
	CvarSliderInput *(*initWithVariable)(CvarSliderInput *self, cvar_t *var, const char *name, double min, double max, double step);

	/**
	 * @fn void CvarSliderInput::input(View *view, cvar_t *var, const char *name, double min, double max, double step)
	 *
	 * @brief Creates and appends a new Input in the specified View.
	 *
	 * @param view The View.
	 * @param var The variable.
	 * @param name The variable name (e.g. `Sensitivity`).
	 * @param min The minimum value.
	 * @param max The maximum value.
	 * @param step The step.
	 *
	 * @memberof CvarSliderInput
	 *
	 * @static
	 */
	void (*input)(View *view, cvar_t *var, const char *name, double min, double max, double step);
};

/**
 * @brief The CvarSliderInput Class.
 */
extern Class _CvarSliderInput;

