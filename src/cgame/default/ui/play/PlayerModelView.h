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

#include <ObjectivelyMVC/Control.h>
#include <ObjectivelyMVC/ImageView.h>

/**
 * @file
 * @brief A Control capable of rendering an r_mesh_model_t.
 */

typedef struct PlayerModelView PlayerModelView;
typedef struct PlayerModelViewInterface PlayerModelViewInterface;

/**
 * @brief The PlayerModelView type.
 * @extends Control
 */
struct PlayerModelView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	Control control;

	/**
	 * @brief The interface.
	 * @private
	 */
	PlayerModelViewInterface *interface;

	/**
	 * @brief The client information.
	 */
	cl_client_info_t client;

	/**
	 * @brief The entity stubs.
	 */
	r_entity_t head, torso, legs, weapon, platformBase, platformCenter;

	/**
	 * @brief The entity animations.
	 */
	cl_entity_animation_t animation1, animation2;

	/**
	 * @brief The ImageView for the model icon.
	 */
	ImageView *iconView;

	/**
	 * @brief The camera yaw and zoom.
	 */
	vec_t yaw, zoom;
};

/**
 * @brief The PlayerModelView interface.
 */
struct PlayerModelViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ControlInterface controlInterface;

	/**
	 * @fn void PlayerModelView::animate(PlayerModelView *self)
	 * @param self The PlayerModelView.
	 * @brief Animates the model.
	 * @memberof PlayerModelView
	 */
	void (*animate)(PlayerModelView *self);

	/**
	 * @fn PlayerModelView *PlayerModelView::initWithFrame(PlayerModelView *self, const SDL_Rect *frame)
	 * @brief Initializes this PlayerModelView with the given frame.
	 * @param self The PlayerModelView.
	 * @param frame The frame.
	 * @return The initialized PlayerModelView, or `NULL` on error.
	 * @memberof PlayerModelView
	 */
	PlayerModelView *(*initWithFrame)(PlayerModelView *self, const SDL_Rect *frame);
};

/**
 * @fn Class *PlayerModelView::_PlayerModelView(void)
 * @brief The PlayerModelView archetype.
 * @return The PlayerModelView Class.
 * @memberof PlayerModelView
 */
extern Class *_PlayerModelView(void);
