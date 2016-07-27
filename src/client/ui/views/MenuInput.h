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

#include "client.h"

/**
 * @file
 *
 * @brief Menu Inputs.
 */

#define MENU_INPUT_LABEL_WIDTH 140

typedef struct MenuInput MenuInput;
typedef struct MenuInputInterface MenuInputInterface;

/**
 * @brief The MenuInput type.
 *
 * @extends Input
 */
struct MenuInput {
	
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
	MenuInputInterface *interface;
};

/**
 * @brief The MenuInput interface.
 */
struct MenuInputInterface {
	
	/**
	 * @brief The parent interface.
	 */
	InputInterface inputInterface;


	void (*bindTextView)(View *view, const char *bind, const char *name);

	void (*cvarCheckbox)(View *view, cvar_t *var, const char *name);

	void (*cvarSlider)(View *view, cvar_t *var, const char *name, double min, double max, double step);

	void (*input)(View *view, Control *control, const char *name);
};

/**
 * @brief The MenuInput Class.
 */
extern Class _MenuInput;

