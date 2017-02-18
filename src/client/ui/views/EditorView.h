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
	TextView *materialName;

	/**
	 * @brief The diffuse texture.
	 */
	TextView *diffuseTexture;

	/**
	 * @brief The normalmap texture.
	 */
	TextView *normalmapTexture;

	/**
	 * @brief The bump slider.
	 */
	Slider *bumpSlider;

	/**
	 * @brief The hardness slider.
	 */
	Slider *hardnessSlider;

	/**
	 * @brief The specular slider.
	 */
	Slider *specularSlider;

	/**
	 * @brief The parallax slider.
	 */
	Slider *parallaxSlider;

	/**
	 * @brief The save button.
	 */
	Button *saveButton;
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

};

/**
 * @fn Class *EditorView::_EditorView(void)
 * @brief The EditorView archetype.
 * @return The EditorView Class.
 * @memberof EditorView
 */
OBJECTIVELY_EXPORT Class *_EditorView(void);
