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

#define VARIABLE_CHECKBOX_INPUT_LABEL_WIDTH 120
#define VARIABLE_CHECKBOX_INPUT_CONTROL_WIDTH 20

typedef struct VariableCheckboxInput VariableCheckboxInput;
typedef struct VariableCheckboxInputInterface VariableCheckboxInputInterface;

/**
 * @brief The VariableCheckboxInput type.
 *
 * @extends Input
 */
struct VariableCheckboxInput {
	
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
	VariableCheckboxInputInterface *interface;

	/**
	 * @brief The variable name (e.g. `Invert mouse`).
	 */
	const char *name;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;
};

/**
 * @brief The VariableCheckboxInput interface.
 */
struct VariableCheckboxInputInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;
	
	/**
	 * @fn VariableCheckboxInput *VariableCheckboxInput::initWithVariable(VariableCheckboxInput *self, cvar_t *var, const char *name)
	 *
	 * @brief Initializes this VariableCheckboxInput with the given variable.
	 *
	 * @param var The variable.
	 * @param name The variable name (e.g. `Invert mouse`).
	 *
	 * @return The initialized VariableCheckboxInput, or `NULL` on error.
	 *
	 * @memberof VariableCheckboxInput
	 */
	VariableCheckboxInput *(*initWithVariable)(VariableCheckboxInput *self, cvar_t *var, const char *name);
};

/**
 * @brief The VariableCheckboxInput Class.
 */
extern Class _VariableCheckboxInput;

