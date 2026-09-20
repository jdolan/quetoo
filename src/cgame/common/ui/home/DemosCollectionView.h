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

#include <ObjectivelyMVC/CollectionView.h>

#include "DemoList.h"
#include "DemosCollectionItemView.h"

/**
 * @file
 * @brief The DemosCollectionView type.
 */

typedef struct DemosCollectionView DemosCollectionView;
typedef struct DemosCollectionViewInterface DemosCollectionViewInterface;

/**
 * @brief The DemosCollectionView type.
 * @extends CollectionView
 */
struct DemosCollectionView {

  /**
   * @brief The superclass.
   * @private
   */
  CollectionView collectionView;

  /**
   * @brief The interface type.
   * @private
   */
  DemosCollectionViewInterface *interface[0];

  /**
   * @brief The discovered demos, shared with any in-flight `reloadDemos` loader.
   */
  DemoList *demos;

  /**
   * @brief A sibling View to show in place of this one when `demos` has no visible items, or
   * `NULL`.
   */
  View *emptyStateView;
};

/**
 * @brief The DemosCollectionView interface.
 */
struct DemosCollectionViewInterface {

  /**
   * @brief The superclass interface.
   */
  CollectionViewInterface collectionViewInterface;

  /**
   * @fn DemosCollectionView *DemosCollectionView::initWithFrame(DemosCollectionView *self, const SDL_Rect *frame)
   * @brief Initializes this DemosCollectionView with the specified frame.
   * @param frame The frame.
   * @return The initialized DemosCollectionView, or `NULL` on error.
   * @memberof DemosCollectionView
   */
  DemosCollectionView *(*initWithFrame)(DemosCollectionView *self, const SDL_Rect *frame);

  /**
   * @fn void DemosCollectionView::reloadDemos(DemosCollectionView *self)
   * @brief Re-enumerates demo files from disk, asynchronously.
   * @param self The DemosCollectionView.
   * @memberof DemosCollectionView
   */
  void (*reloadDemos)(DemosCollectionView *self);

  /**
   * @fn void DemosCollectionView::setFilter(DemosCollectionView *self, const char *filter)
   * @brief Filters the visible demos by map name substring, case-insensitively.
   * @param self The DemosCollectionView.
   * @param filter The filter string, or `NULL`/empty for none.
   * @memberof DemosCollectionView
   */
  void (*setFilter)(DemosCollectionView *self, const char *filter);

  /**
   * @fn DemoListItemInfo *DemosCollectionView::selectedDemo(const DemosCollectionView *self)
   * @return The selected demo, or `NULL` if none is selected.
   * @memberof DemosCollectionView
   */
  DemoListItemInfo *(*selectedDemo)(const DemosCollectionView *self);

  /**
   * @fn void DemosCollectionView::removeDemo(DemosCollectionView *self, const char *filename)
   * @brief Removes the demo with the given filename from the in-memory list, e.g. after deletion.
   * @param self The DemosCollectionView.
   * @param filename The demo's filename, as enumerated (e.g. `"demos/foo.demo"`).
   * @memberof DemosCollectionView
   */
  void (*removeDemo)(DemosCollectionView *self, const char *filename);
};

/**
 * @fn Class *DemosCollectionView::_DemosCollectionView(void)
 * @brief The DemosCollectionView archetype.
 * @return The DemosCollectionView Class.
 * @memberof DemosCollectionView
 */
CGAME_EXPORT Class *_DemosCollectionView(void);
