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
 * @brief The TeamView.
 */

typedef struct TeamView TeamView;
typedef struct TeamViewInterface TeamViewInterface;

/**
 * @brief The TeamView type.
 * @extends View
 */
struct TeamView {

	/**
	 * @brief The superclass.
	 */
	View view;

	/**
	 * @brief The interface.
	 * @protected
	 */
	TeamViewInterface *interface;

	/**
	 * @brief The team info to render.
	 */
	const cg_team_info_t *team;

	/**
	 * @brief The team name.
	 */
	Label *name;

	/**
	 * @brief The players StackView.
	 */
	StackView *players;
};

/**
 * @brief The TeamView interface.
 */
struct TeamViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn TeamView *TeamView::init(TeamView *self)
	 * @brief Initializes this TeamView.
	 * @param self The TeamView.
	 * @param frame The frame, or `NULL`.
	 * @return The initialized TeamView, or `NULL` on error.
	 * @memberof TeamView
	 */
	TeamView *(*initWithFrame)(TeamView *self, const SDL_Rect *frame);


	/**
	 * @fn void TeamView::setTeam(TeamView *self, const cg_team_info_t *team)
	 * @brief Sets the team info to render.
	 * @param self The TeamView.
	 * @param team The team info.
	 * @memberof TeamView
	 */
	void (*setTeam)(TeamView *self, const cg_team_info_t *team);
};

/**
 * @fn Class *TeamView::_TeamView(void)
 * @brief The TeamView archetype.
 * @return The TeamView Class.
 * @memberof TeamView
 */
CGAME_EXPORT Class *_TeamView(void);
