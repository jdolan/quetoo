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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include <Objectively/Lock.h>
#include <Objectively/Object.h>
#include <Objectively/PointerArray.h>

#include "MapListCollectionItemView.h"

/**
 * @file
 * @brief MapList: a thread-safe, sorted collection of discovered maps.
 */

typedef struct MapList MapList;
typedef struct MapListInterface MapListInterface;

/**
 * @brief MapList synchronizes all access to its backing store internally, so it may be safely
 * shared between the View that reads it and the background thread that populates it. Retaining
 * a MapList for a worker thread, rather than the View, means the View's own teardown never has to
 * wait on or race that worker.
 * @extends Object
 */
struct MapList {

  /**
   * @brief The superclass.
   * @private
   */
  Object object;

  /**
   * @brief The interface type.
   * @private
   */
  MapListInterface *interface[0];

  /**
   * @brief A lock guarding `maps`, held for the duration of every method below.
   * @private
   */
  Lock *lock;

  /**
   * @brief The discovered maps, sorted by title.
   * @private
   */
  PointerArray *maps;
};

/**
 * @brief The MapList interface.
 */
struct MapListInterface {

  /**
   * @brief The superclass interface.
   */
  ObjectInterface objectInterface;

  /**
   * @fn MapList *MapList::init(MapList *self)
   * @brief Initializes this MapList.
   * @return The initialized MapList, or `NULL` on error.
   * @memberof MapList
   */
  MapList *(*init)(MapList *self);

  /**
   * @fn size_t MapList::count(const MapList *self)
   * @return The number of maps currently in this list.
   * @memberof MapList
   */
  size_t (*count)(const MapList *self);

  /**
   * @fn MapListItemInfo *MapList::get(const MapList *self, size_t index)
   * @param index The index.
   * @return The MapListItemInfo at `index`.
   * @memberof MapList
   */
  MapListItemInfo *(*get)(const MapList *self, size_t index);

  /**
   * @fn void MapList::add(MapList *self, MapListItemInfo *info)
   * @brief Adds `info` to this list, keeping it sorted, unless a map with the same mapname is
   * already present, in which case `info` is freed.
   * @param self The MapList.
   * @param info The MapListItemInfo to add. Ownership transfers to this list.
   * @memberof MapList
   */
  void (*add)(MapList *self, MapListItemInfo *info);
};

/**
 * @fn Class *MapList::_MapList(void)
 * @brief The MapList archetype.
 * @return The MapList Class.
 * @memberof MapList
 */
CGAME_EXPORT Class *_MapList(void);
