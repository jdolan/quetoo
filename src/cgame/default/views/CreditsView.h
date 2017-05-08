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

/**
 * @file
 * @brief Credits View
 */

typedef struct CreditsView CreditsView;
typedef struct CreditsViewInterface CreditsViewInterface;

/**
 * @brief The CreditsView type.
 * @extends View
 * @ingroup
 */
struct CreditsView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	CreditsViewInterface *interface;

	/*
	 * @brief The list of credits, loaded from a file.
	 */
	GSList *credits;

	/*
	 * @brief The credits TableView.
	 */
	TableView *tableView;
};

/**
 * @brief The CreditsView interface.
 */
struct CreditsViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn CreditsView *CreditsView::initWithFrame(CreditsView *self, const SDL_Rect *frame)
	 * @brief Initializes this CreditsView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized CreditsView, or `NULL` on error.
	 * @memberof CreditsView
	 */
	CreditsView *(*initWithFrame)(CreditsView *self, const SDL_Rect *frame);

	/**
	 * @fn void CreditsView::loadCredits(CreditsView *self, const char *path)
	 * @brief Loads the credits from a file
	 * @memberof CreditsView
	 */
	void (*loadCredits)(CreditsView *self, const char *path);
};

/**
 * @fn Class *CreditsView::_CreditsView(void)
 * @brief The CreditsView archetype.
 * @return The CreditsView Class.
 * @memberof CreditsView
 */
extern Class *_CreditsView(void);
