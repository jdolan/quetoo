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

#include <ObjectivelyMVC/Select.h>

/**
 * @file
 *
 * @brief A Select exposing a cvar_t.
 */

typedef struct CvarSelect CvarSelect;
typedef struct CvarSelectInterface CvarSelectInterface;

/**
 * @brief The CvarSelect type.
 *
 * @extends Select
 */
struct CvarSelect {

	/**
	 * @brief The superclass.
	 *
	 * @private
	 */
	Select select;

	/**
	 * @brief The interface.
	 *
	 * @private
	 */
	CvarSelectInterface *interface;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;

	/**
	 * @brief Set to true if the variable expects a string value, false for integer.
	 */
	_Bool expectsStringValue;
};

/**
 * @brief The CvarSelect interface.
 */
struct CvarSelectInterface {

	/**
	 * @brief The superclass interface.
	 */
	SelectInterface selectInterface;

	/**
	 * @fn CvarSelect *CvarSelect::initWithVariable(CvarSelect *self, cvar_t *var)
	 * @brief Initializes this CvarSelect with the given variable.
	 * @param var The variable.
	 * @return The initialized CvarSelect, or `NULL` on error.
	 * @memberof CvarSelect
	 */
	CvarSelect *(*initWithVariable)(CvarSelect *self, cvar_t *var);

	/**
	 * @fn CvarSelect *CvarSelect::initWithVariabeName(CvarSelect *self, const char (name)
	 * @brief Initializes this CvarSelect with the given variable name.
	 * @param name The variable name.
	 * @return The initialized CvarSelect, or `NULL`.
	 * @memberof CvarSelect
	 */
	CvarSelect *(*initWithVariableName)(CvarSelect *self, const char *name);
};

/**
 * @fn Class *CvarSelect::_CvarSelect(void)
 * @brief The CvarSelect archetype.
 * @return The CvarSelect Class.
 * @memberof CvarSelect
 */
CGAME_EXPORT Class *_CvarSelect(void);
