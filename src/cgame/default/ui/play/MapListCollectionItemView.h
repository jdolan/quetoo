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

#include <ObjectivelyMVC/CollectionItemView.h>

#include "cg_types.h"

/**
 * @file
 * @brief The MapListCollectionItemView type.
 */

typedef struct {
	char mapname[MAX_QPATH];
	char message[MAX_TOKEN_CHARS];
	SDL_Surface *mapshot;
} MapListItemInfo;

typedef struct MapListCollectionItemView MapListCollectionItemView;
typedef struct MapListCollectionItemViewInterface MapListCollectionItemViewInterface;

/**
 * @brief The MapListCollectionItemView type.
 * @extends CollectionItemView
 */
struct MapListCollectionItemView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	CollectionItemView collectionItemView;

	/**
	 * @brief The interface.
	 * @private
	 */
	MapListCollectionItemViewInterface *interface;
};

/**
 * @brief The MapListCollectionItemView interface.
 */
struct MapListCollectionItemViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	CollectionItemViewInterface collectionItemViewInterface;

	/**
	 * @fn MapListCollectionItemView *MapListCollectionItemView::initWithFrame(MapListCollectionItemView *self, const SDL_Rect *frame)
	 * @brief Initializes this MapListCollectionItemView with the specified frame.
	 * @param frame The frame.
	 * @return The initialized MapListCollectionItemView, or `NULL` on error.
	 * @memberof MapListCollectionItemView
	 */
	MapListCollectionItemView *(*initWithFrame)(MapListCollectionItemView *self, const SDL_Rect *frame);

	/**
	 * @fn void MapListCollectionItemView::setMapListItemInfo(MapListCollectionItemView *self, MapListItemInfo *info);
	 * @brief Sets the information for this item.
	 * @param info The MapListItemInfo.
	 * @memberof MapListCollectionItemView
	 */
	void (*setMapListItemInfo)(MapListCollectionItemView *self, MapListItemInfo *info);
};

/**
 * @fn Class *MapListCollectionItemView::_MapListCollectionItemView(void)
 * @brief The MapListCollectionItemView archetype.
 * @return The MapListCollectionItemView Class.
 * @memberof MapListCollectionItemView
 */
CGAME_EXPORT Class *_MapListCollectionItemView(void);

