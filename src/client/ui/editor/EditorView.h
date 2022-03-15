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

#include "ui_types.h"

#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/Slider.h>

/**
 * @file
 * @brief The in-game editor View.
 */

typedef struct EditorView EditorView;
typedef struct EditorViewInterface EditorViewInterface;

/**
 * @brief The EditorView type.
 * @extends View
 */
struct EditorView {

	/**
	 * @brief The superclass.
	 */
	View view;

	/**
	 * @brief The interface.
	 * @protected
	 */
	EditorViewInterface *interface;

	/**
	 * @brief The material the player is looking at.
	 */
	r_material_t *material;

	/**
	 * @brief The material name.
	 */
	TextView *name;

	/**
	 * @brief The diffusemap texture.
	 */
	TextView *diffusemap;

	/**
	 * @brief The normalmap texture.
	 */
	TextView *normalmap;

	/**
	 * @brief The glossmap texture.
	 */
	TextView *glossmap;

	/**
	 * @brief The specularmap texture.
	 */
	TextView *specularmap;

	/**
	 * @brief The roughness slider.
	 */
	Slider *roughness;

	/**
	 * @brief The hardness slider.
	 */
	Slider *hardness;

	/**
	 * @brief The specularity slider.
	 */
	Slider *specularity;

	/**
	 * @brief The bloom slider.
	 */
	Slider *bloom;

	/**
	 * @brief The alpha test slider.
	 */
	Slider *alphaTest;

	/**
	 * @brief The save button.
	 */
	Button *save;
};

/**
 * @brief The EditorView interface.
 */
struct EditorViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn EditorView *EditorView::initWithFrame(EditorView *self, const SDL_Rect *frame)
	 * @brief Initializes this EditorView.
	 * @param The EditorView.
	 * @param frame The frame.
	 * @return The initialized EditorView, or `NULL` on error.
	 * @memberof EditorView
	 */
	EditorView *(*initWithFrame)(EditorView *self, const SDL_Rect *frame);

	/**
	 * @fn EditorView::setMaterial(EditorView *self, r_material_t *material)
	 * @brief Sets the Material for this EditorView.
	 * @param self The EditorView.
	 * @param material The material.
	 * @memberof EditorView
	 */
	void (*setMaterial)(EditorView *self, r_material_t *material);
};

/**
 * @fn Class *EditorView::_EditorView(void)
 * @brief The EditorView archetype.
 * @return The EditorView Class.
 * @memberof EditorView
 */
Class *_EditorView(void);
