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

#include <Objectively/Lock.h>

#include <ObjectivelyMVC/CollectionView.h>

/**
 * @file
 * @brief The MapListCollectionView type.
 */

typedef struct MapListCollectionView MapListCollectionView;
typedef struct MapListCollectionViewInterface MapListCollectionViewInterface;

/**
 * @brief The MapListCollectionView type.
 * @extends CollectionView
 */
struct MapListCollectionView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	CollectionView collectionView;

	/**
	 * @brief The interface.
	 * @private
	 */
	MapListCollectionViewInterface *interface;

	/**
	 * @brief A lock used for asynchronous map loading.
	 */
	Lock *lock;

	/**
	 * @brief Available maps.
	 */
	MutableArray *maps;
};

/**
 * @brief The MapListCollectionView interface.
 */
struct MapListCollectionViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	CollectionViewInterface collectionViewInterface;

	/**
	 * @fn MapListCollectionView *MapListCollectionView::initWithFrame(MapListCollectionView *self, const SDL_Rect *frame, ControlStyle style)
	 * @brief Initializes this MapListCollectionView with the specified frame and style.
	 * @param frame The frame.
	 * @param style The ControlStyle.
	 * @return The initialized MapListCollectionView, or `NULL` on error.
	 * @memberof MapListCollectionView
	 */
	MapListCollectionView *(*initWithFrame)(MapListCollectionView *self, const SDL_Rect *frame,
	                                        ControlStyle style);

	/**
	 * @fn Array *MapListCollectionView::selectedMaps(const MapListCollectionView *self)
	 * @return An Array of selected MapListItemInfo Values.
	 * @memberof MapListCollectionView
	 */
	Array *(*selectedMaps)(const MapListCollectionView *self);
};

/**
 * @fn Class *MapListCollectionView::_MapListCollectionView(void)
 * @brief The MapListCollectionView archetype.
 * @return The MapListCollectionView Class.
 * @memberof MapListCollectionView
 */
extern Class *_MapListCollectionView(void);

