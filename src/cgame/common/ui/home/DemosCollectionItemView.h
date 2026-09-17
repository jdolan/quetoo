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

#include <ObjectivelyMVC/Button.h>
#include <ObjectivelyMVC/CollectionItemView.h>
#include <ObjectivelyMVC/StackView.h>

#include "cg_types.h"

/**
 * @file
 * @brief The DemosCollectionItemView type.
 */

/**
 * @brief Per-demo information surfaced by the browser, read from each demo's fixed-size header.
 */
typedef struct {
  char filename[MAX_QPATH];
  char map[MAX_QPATH];
  char message[MAX_QPATH];
  char title[MAX_QPATH];
  int32_t duration;
  bool favorite;
  int64_t modified;
  SDL_Surface *mapshot;
} DemoListItemInfo;

const char *DemoListItemName(const DemoListItemInfo *info);

typedef struct DemosCollectionView DemosCollectionView;

typedef struct DemosCollectionItemView DemosCollectionItemView;
typedef struct DemosCollectionItemViewInterface DemosCollectionItemViewInterface;

/**
 * @brief The DemosCollectionItemView type.
 * @extends CollectionItemView
 */
struct DemosCollectionItemView {

  /**
   * @brief The superclass.
   * @private
   */
  CollectionItemView collectionItemView;

  /**
   * @brief The interface type.
   * @private
   */
  DemosCollectionItemViewInterface *interface[0];

  /**
   * @brief The demo this item represents, or `NULL`.
   */
  DemoListItemInfo *info;

  /**
   * @brief The owning CollectionView, for favoriting/deletion callbacks.
   */
  DemosCollectionView *collectionView;

  /**
   * @brief The favorite (heart) and delete (trash) buttons, overlaid in the top-right corner.
   */
  Button *favoriteButton, *deleteButton;

  /**
   * @brief The demo's name, editable in place to retitle the demo.
   */
  TextView *titleView;
};

/**
 * @brief The DemosCollectionItemView interface.
 */
struct DemosCollectionItemViewInterface {

  /**
   * @brief The superclass interface.
   */
  CollectionItemViewInterface collectionItemViewInterface;

  /**
   * @fn DemosCollectionItemView *DemosCollectionItemView::initWithFrame(DemosCollectionItemView *self, const SDL_Rect *frame)
   * @brief Initializes this DemosCollectionItemView with the specified frame.
   * @param frame The frame.
   * @return The initialized DemosCollectionItemView, or `NULL` on error.
   * @memberof DemosCollectionItemView
   */
  DemosCollectionItemView *(*initWithFrame)(DemosCollectionItemView *self, const SDL_Rect *frame);

  /**
   * @fn void DemosCollectionItemView::setDemoListItemInfo(DemosCollectionItemView *self, DemoListItemInfo *info)
   * @brief Sets the information for this item.
   * @param info The DemoListItemInfo.
   * @memberof DemosCollectionItemView
   */
  void (*setDemoListItemInfo)(DemosCollectionItemView *self, DemoListItemInfo *info);
};

/**
 * @fn Class *DemosCollectionItemView::_DemosCollectionItemView(void)
 * @brief The DemosCollectionItemView archetype.
 * @return The DemosCollectionItemView Class.
 * @memberof DemosCollectionItemView
 */
CGAME_EXPORT Class *_DemosCollectionItemView(void);
