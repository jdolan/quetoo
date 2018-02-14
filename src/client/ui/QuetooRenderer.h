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

#include "src/client/renderer/renderer.h"

#include <ObjectivelyMVC/Renderer.h>

/**
 * @file
 * @brief A Renderer implementation for Quetoo.
 */

typedef struct QuetooRenderer QuetooRenderer;
typedef struct QuetooRendererInterface QuetooRendererInterface;

/**
 * @brief A Renderer implementation for Quetoo.
 * @extends Renderer
 */
struct QuetooRenderer {

	/**
	 * @brief The superclass.
	 */
	Renderer renderer;

	/**
	 * @brief The interface.
	 * @protected
	 */
	QuetooRendererInterface *interface;
};

/**
 * @brief The Renderer interface.
 */
struct QuetooRendererInterface {

	/**
	 * @brief The superclass interface.
	 */
	RendererInterface rendererInterface;

	/**
	 * @fn QuetooRenderer *QuetooRenderer::init(QuetooRenderer *self)
	 * @brief Initializes this QuetooRenderer.
	 * @param self The QuetooRenderer.
	 * @return The initialized QuetooRenderer, or `NULL` on error.
	 * @memberof QuetooRenderer
	 */
	QuetooRenderer *(*init)(QuetooRenderer *self);
};

/**
 * @fn Class *QuetooRenderer::_QuetooRenderer(void)
 * @brief The QuetooRenderer archetype.
 * @return The QuetooRenderer Class.
 * @memberof QuetooRenderer
 */
extern Class *_QuetooRenderer(void);

