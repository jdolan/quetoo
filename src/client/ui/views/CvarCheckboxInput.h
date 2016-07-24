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
 * @brief An Input exposing a cvar_t with a Checkbox.
 */

#define CVAR_CHECKBOX_INPUT_LABEL_WIDTH 120
#define CVAR_CHECKBOX_INPUT_CONTROL_WIDTH 20

typedef struct CvarCheckboxInput CvarCheckboxInput;
typedef struct CvarCheckboxInputInterface CvarCheckboxInputInterface;

/**
 * @brief The CvarCheckboxInput type.
 *
 * @extends Input
 */
struct CvarCheckboxInput {
	
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
	CvarCheckboxInputInterface *interface;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;
};

/**
 * @brief The CvarCheckboxInput interface.
 */
struct CvarCheckboxInputInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;
	
	/**
	 * @fn CvarCheckboxInput *CvarCheckboxInput::initWithVariable(CvarCheckboxInput *self, cvar_t *var, const char *name)
	 *
	 * @brief Initializes this Input with the given variable.
	 *
	 * @param var The variable.
	 * @param name The variable name (e.g. `Invert mouse`).
	 *
	 * @return The initialized CvarCheckboxInput, or `NULL` on error.
	 *
	 * @memberof CvarCheckboxInput
	 */
	CvarCheckboxInput *(*initWithVariable)(CvarCheckboxInput *self, cvar_t *var, const char *name);

	/**
	 * @fn void CvarCheckboxInput::input(View *view, cvar_t *var, const char *name)
	 *
	 * @brief Creates and appends a new Input in the specified View.
	 *
	 * @param view The View to which to append the new input.
	 * @param var The variable.
	 * @param name The variable name (e.g. `Invert mouse`).
	 *
	 * @memberof CvarCheckboxInput
	 *
	 * @static
	 */
	void (*input)(View *view, cvar_t *var, const char *name);
};

/**
 * @brief The CvarCheckboxInput Class.
 */
extern Class _CvarCheckboxInput;

