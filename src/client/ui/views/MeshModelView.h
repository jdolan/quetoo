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

#include <ObjectivelyMVC/View.h>

#include "../../renderer/r_types.h"

/**
 * @file
 *
 * @brief A View capable of rendering an r_mesh_model_t.
 */

typedef struct MeshModelView MeshModelView;
typedef struct MeshModelViewInterface MeshModelViewInterface;

/**
 * @brief The MeshModelView type.
 *
 * @extends View
 *
 * @ingroup
 */
struct MeshModelView {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	View view;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	MeshModelViewInterface *interface;

	/**
	 * @brief The model.
	 */
	const char *model;

	/**
	 * @brief The skin.
	 */
	const char *skin;

	/**
	 * @brief The scale.
	 */
	vec_t scale;
};

/**
 * @brief The MeshModelView interface.
 */
struct MeshModelViewInterface {
	
	/**
	 * @brief The parent interface.
	 */
	ViewInterface viewInterface;
	
	/**
	 * @fn MeshModelView *MeshModelView::initWithFrame(MeshModelView *self)
	 *
	 * @brief Initializes this MeshModelView.
	 *
	 * @param frame The frame.
	 *
	 * @return The initialized MeshModelView, or `NULL` on error.
	 *
	 * @memberof MeshModelView
	 */
	MeshModelView *(*initWithFrame)(MeshModelView *self, const SDL_Rect *frame);
};

/**
 * @brief The MeshModelView Class.
 */
extern Class _MeshModelView;

