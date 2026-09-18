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
#include <Objectively/Object.h>
#include <Objectively/PointerArray.h>

#include "DemosCollectionItemView.h"

/**
 * @file
 * @brief DemoList: a thread-safe, sorted and filterable collection of discovered demos.
 */

typedef struct DemoList DemoList;
typedef struct DemoListInterface DemoListInterface;

/**
 * @brief DemoList synchronizes all access to its backing store internally, so it may be safely
 * shared between the View that reads it and however many background loaders are in flight
 * populating it. Retaining a DemoList for a worker thread, rather than the View, means the View's
 * own teardown never has to wait on or race that worker.
 * @extends Object
 */
struct DemoList {

  /**
   * @brief The superclass.
   * @private
   */
  Object object;

  /**
   * @brief The interface type.
   * @private
   */
  DemoListInterface *interface[0];

  /**
   * @brief A lock guarding all fields below, held for the duration of every method below.
   * @private
   */
  Lock *lock;

  /**
   * @brief All demos discovered on disk.
   * @private
   */
  PointerArray *demos;

  /**
   * @brief The subset of `demos` currently visible, per `filter`.
   * @private
   */
  PointerArray *filtered;

  /**
   * @brief A case-insensitive map name substring filter, or `NULL` for none.
   * @private
   */
  char *filter;
};

/**
 * @brief The DemoList interface.
 */
struct DemoListInterface {

  /**
   * @brief The superclass interface.
   */
  ObjectInterface objectInterface;

  /**
   * @fn DemoList *DemoList::init(DemoList *self)
   * @brief Initializes this DemoList.
   * @return The initialized DemoList, or `NULL` on error.
   * @memberof DemoList
   */
  DemoList *(*init)(DemoList *self);

  /**
   * @fn size_t DemoList::count(const DemoList *self)
   * @return The number of demos currently visible, per the current filter.
   * @memberof DemoList
   */
  size_t (*count)(const DemoList *self);

  /**
   * @fn DemoListItemInfo *DemoList::get(const DemoList *self, size_t index)
   * @param index The index.
   * @return The visible DemoListItemInfo at `index`.
   * @memberof DemoList
   */
  DemoListItemInfo *(*get)(const DemoList *self, size_t index);

  /**
   * @fn void DemoList::add(DemoList *self, DemoListItemInfo *info)
   * @brief Adds `info` to this list, keeping it sorted, unless a demo with the same filename is
   * already present, in which case `info` is freed.
   * @param self The DemoList.
   * @param info The DemoListItemInfo to add. Ownership transfers to this list.
   * @memberof DemoList
   */
  void (*add)(DemoList *self, DemoListItemInfo *info);

  /**
   * @fn void DemoList::remove(DemoList *self, const char *filename)
   * @brief Removes the demo with the given filename, e.g. after deletion.
   * @param self The DemoList.
   * @param filename The demo's filename, as enumerated (e.g. `"demos/foo.demo"`).
   * @memberof DemoList
   */
  void (*remove)(DemoList *self, const char *filename);

  /**
   * @fn void DemoList::setFilter(DemoList *self, const char *filter)
   * @brief Filters the visible demos by map name substring, case-insensitively.
   * @param self The DemoList.
   * @param filter The filter string, or `NULL`/empty for none.
   * @memberof DemoList
   */
  void (*setFilter)(DemoList *self, const char *filter);
};

/**
 * @fn Class *DemoList::_DemoList(void)
 * @brief The DemoList archetype.
 * @return The DemoList Class.
 * @memberof DemoList
 */
CGAME_EXPORT Class *_DemoList(void);
