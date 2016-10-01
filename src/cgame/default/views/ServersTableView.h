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

#include <ObjectivelyMVC/TableView.h>

/**
 * @file
 *
 * @brief The multiplayer servers table.
 */

typedef struct ServersTableView ServersTableView;
typedef struct ServersTableViewInterface ServersTableViewInterface;

/**
 * @brief The ServersTableView type.
 *
 * @extends TableView
 */
struct ServersTableView {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	TableView tableView;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	ServersTableViewInterface *interface;
};

/**
 * @brief The ServersTableView interface.
 */
struct ServersTableViewInterface {
	
	/**
	 * @brief The parent interface.
	 */
	TableViewInterface tableViewInterface;
	
	/**
	 * @fn ServersTableView *ServersTableView::initWithFrame(ServersTableView *self, const SDL_Rect *frame, ControlStyle style)
	 *
	 * @brief Initializes this ServersTableView with the specified frame and style.
	 *
	 * @param frame The frame.
	 * @param style The ControlStyle.
	 *
	 * @return The initialized ServersTableView, or `NULL` on error.
	 *
	 * @memberof ServersTableView
	 */
	ServersTableView *(*initWithFrame)(ServersTableView *self, const SDL_Rect *frame, ControlStyle style);
};

/**
 * @brief The ServersTableView Class.
 */
extern Class _ServersTableView;

