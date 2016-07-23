/*
 * ObjectivelyMVC: MVC framework for OpenGL and SDL2 in c.
 * Copyright (C) 2014 Jay Dolan <jay@jaydolan.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include <ObjectivelyMVC/Input.h>

/**
 * @file
 *
 * @brief Inputs for key bindings.
 */

#define BIND_INPUT_BIND_WIDTH 80
#define BIND_INPUT_NAME_WIDTH 120

typedef struct BindInput BindInput;
typedef struct BindInputInterface BindInputInterface;

/**
 * @brief The BindInput type.
 *
 * @extends Input
 *
 * @ingroup
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

	/**
	 * @brief The bind name (e.g. `Forward`).
	 */
	const char *name;
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
	 * @fn BindInput *BindInput::initWithBind(BindInput *self, const char *bind,)
	 *
	 * @brief Initializes this BindInput with the given bind.
	 *
	 * @param bind The bind (e.g. `+forward).
	 * @param name The bind name (e.g. `Forward`).
	 *
	 * @return The initialized BindInput, or `NULL` on error.
	 *
	 * @memberof BindInput
	 */
	BindInput *(*initWithBind)(BindInput *self, const char *bind, const char *name);
};

/**
 * @brief The BindInput Class.
 */
extern Class _BindInput;

