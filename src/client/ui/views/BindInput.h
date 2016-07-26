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

/**
 * @file
 *
 * @brief Inputs for key bindings.
 */

#define BIND_INPUT_LABEL_WIDTH 120
#define BIND_INPUT_CONTROL_WIDTH 80

typedef struct BindInput BindInput;
typedef struct BindInputInterface BindInputInterface;

/**
 * @brief The BindInput type.
 *
 * @extends Input
 */
struct BindInput {
	
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
	BindInputInterface *interface;

	/**
	 * @brief The bind (e.g. `+forward`).
	 */
	const char *bind;
};

/**
 * @brief The BindInput interface.
 */
struct BindInputInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;
	
	/**
	 * @fn BindInput *BindInput::initWithBind(BindInput *self, const char *bind, const char *name)
	 *
	 * @brief Initializes this Input with the given bind.
	 *
	 * @param bind The bind (e.g. `+forward).
	 * @param name The bind name (e.g. `Forward`).
	 *
	 * @return The initialized BindInput, or `NULL` on error.
	 *
	 * @memberof BindInput
	 */
	BindInput *(*initWithBind)(BindInput *self, const char *bind, const char *name);

	/**
	 * @fn void BindInput::input(View *view, const char *bind, const char *name)
	 *
	 * @brief Creates and appends a new Input in the specified View.
	 *
	 * @param bind The bind (e.g. `+forward`).
	 * @param name The bind name (e.g. `Forward`).
	 *
	 * @memberof BindInput
	 *
	 * @static
	 */
	void (*input)(View *view, const char *bind, const char *name);
};

/**
 * @brief The BindInput Class.
 */
extern Class _BindInput;
