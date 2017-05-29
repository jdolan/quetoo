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
 * @brief Server browser View.
 */

typedef struct ServerBrowserView ServerBrowserView;
typedef struct ServerBrowserViewInterface ServerBrowserViewInterface;

/**
 * @brief The ServerBrowserView type.
 * @extends View
 * @ingroup
 */
struct ServerBrowserView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	View view;

	/**
	 * @brief The interface.
	 * @private
	 */
	ServerBrowserViewInterface *interface;

	/**
	 * @brief A copy of the client's servers list, for sorting, etc.
	 */
	GList *servers;

	/**
	 * @brief The TableView.
	 */
	TableView *serversTableView;
};

/**
 * @brief The ServerBrowserView interface.
 */
struct ServerBrowserViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	ViewInterface viewInterface;

	/**
	 * @fn ServerBrowserView *ServerBrowserView::initWithFrame(ServerBrowserView *self, const SDL_Rect *frame)
	 * @brief Initializes this ServerBrowserView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized ServerBrowserView, or `NULL` on error.
	 * @memberof ServerBrowserView
	 */
	ServerBrowserView *(*initWithFrame)(ServerBrowserView *self, const SDL_Rect *frame);

	/**
	 * @fn void ServerBrowserView::reloadServers(ServerBrowserView *self)
	 * @brief Reloads the list of known servers.
	 * @param self The ServerBrowserView.
	 * @memberof ServerBrowserView
	 */
	void (*reloadServers)(ServerBrowserView *self);
};

/**
 * @fn Class *ServerBrowserView::_ServerBrowserView(void)
 * @brief The ServerBrowserView archetype.
 * @return The ServerBrowserView Class.
 * @memberof ServerBrowserView
 */
extern Class *_ServerBrowserView(void);
