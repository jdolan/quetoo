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

#include <SDL2/SDL_video.h>

#include <Objectively/MutableArray.h>

#include <ObjectivelyMVC/Renderer.h>

/**
 * @file
 * @brief The Renderer is responsible for rasterizing the View hierarchy of a WindowController.
 */

typedef struct RendererQuetoo RendererQuetoo;
typedef struct RendererQuetooInterface RendererQuetooInterface;

/**
 * @brief The Renderer is responsible for rasterizing the View hierarchy of a WindowController.
 * @extends Object
 */
struct RendererQuetoo {

	/**
	 * @brief The parent.
	 */
	Renderer renderer;

	/**
	 * @brief The typed interface.
	 * @protected
	 */
	RendererQuetooInterface *interface;
};

/**
 * @brief The Renderer interface.
 */
struct RendererQuetooInterface {

	/**
	 * @brief The parent interface.
	 */
	RendererInterface rendererInterface;

	/**
	 * @fn RendererQuetoo *RendererQuetoo::init(RendererQuetoo *self)
	 * @brief Initializes this RendererQuetoo.
	 * @param self The RendererQuetoo.
	 * @return The initialized RendererQuetoo, or `NULL` on error.
	 * @memberof RendererQuetoo
	 */
	RendererQuetoo *(*init) (RendererQuetoo *self);
};

/**
 * @brief The RendererQuetoo Class.
 */
extern Class _RendererQuetoo;

