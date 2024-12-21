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

/**
 * @file
 * @brief The TeamPlayerView.
 */

typedef struct TeamPlayerView TeamPlayerView;
typedef struct TeamPlayerViewInterface TeamPlayerViewInterface;

/**
 * @brief The TeamPlayerView type.
 * @extends View
 */
struct TeamPlayerView {

	/**
	 * @brief The superclass.
	 */
	View view;

	/**
	 * @brief The interface.
	 * @protected
	 */
	TeamPlayerViewInterface *interface;

	/**
	 * @brief The player to render.
	 */
	const cg_client_info_t *client;

	/**
	 * The player icon image.
	 */
	ImageView *icon;

	/**
	 * @brief The player name.
	 */
	Label *name;
};

/**
 * @brief The TeamPlayerView interface.
 */
struct TeamPlayerViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn TeamPlayerView *TeamPlayerView::init(TeamPlayerView *self)
	 * @brief Initializes this TeamPlayerView.
	 * @param self The TeamPlayerView.
	 * @param frame The frame, or `NULL`.
	 * @return The initialized TeamPlayerView, or `NULL` on error.
	 * @memberof TeamPlayerView
	 */
	TeamPlayerView *(*initWithFrame)(TeamPlayerView *self, const SDL_Rect *frame);


	/**
	 * @fn void TeamPlayerView::setTeam(TeamPlayerView *self, const cg_client_info_t *client)
	 * @brief Sets the client info to render.
	 * @param self The TeamPlayerView.
	 * @param client The client info.
	 * @memberof TeamPlayerView
	 */
	void (*setPlayer)(TeamPlayerView *self, const cg_client_info_t *client);
};

/**
 * @fn Class *TeamPlayerView::_TeamPlayerView(void)
 * @brief The TeamPlayerView archetype.
 * @return The TeamPlayerView Class.
 * @memberof TeamPlayerView
 */
CGAME_EXPORT Class *_TeamPlayerView(void);
