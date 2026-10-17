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

#include <ObjectivelyMVC/CollectionItemView.h>
#include <ObjectivelyMVC/ProgressBar.h>
#include <ObjectivelyMVC/StackView.h>

#include "cg_types.h"

/**
 * @file
 * @brief The AiCollectionItemView type.
 */

/**
 * @brief An AI player of `ai.lst`, as the create server menu shows it.
 */
typedef struct AiItemInfo {
  char name[MAX_QPATH];
  char skin[MAX_QPATH];
  float skill, aggression, awareness;
} AiItemInfo;

typedef struct AiCollectionItemView AiCollectionItemView;
typedef struct AiCollectionItemViewInterface AiCollectionItemViewInterface;

/**
 * @brief The AiCollectionItemView type: an AI player's icon, name and traits.
 * @extends CollectionItemView
 */
struct AiCollectionItemView {

  /**
   * @brief The superclass.
   * @private
   */
  CollectionItemView collectionItemView;

  /**
   * @brief The interface type.
   * @private
   */
  AiCollectionItemViewInterface *interface[0];

  /**
   * @brief The trait bars, stacked by CSS.
   */
  StackView *traits;

  /**
   * @brief The skill, aggression and awareness bars.
   */
  ProgressBar *skill, *aggression, *awareness;
};

/**
 * @brief The AiCollectionItemView interface.
 */
struct AiCollectionItemViewInterface {

  /**
   * @brief The superclass interface.
   */
  CollectionItemViewInterface collectionItemViewInterface;

  /**
   * @fn AiCollectionItemView *AiCollectionItemView::initWithFrame(AiCollectionItemView *self, const SDL_Rect *frame)
   * @brief Initializes this AiCollectionItemView with the specified frame.
   * @param frame The frame.
   * @return The initialized AiCollectionItemView, or `NULL` on error.
   * @memberof AiCollectionItemView
   */
  AiCollectionItemView *(*initWithFrame)(AiCollectionItemView *self, const SDL_Rect *frame);

  /**
   * @fn void AiCollectionItemView::setAiItemInfo(AiCollectionItemView *self, const AiItemInfo *info)
   * @brief Sets the AI player this item shows.
   * @param info The AiItemInfo.
   * @memberof AiCollectionItemView
   */
  void (*setAiItemInfo)(AiCollectionItemView *self, const AiItemInfo *info);
};

/**
 * @fn Class *AiCollectionItemView::_AiCollectionItemView(void)
 * @brief The AiCollectionItemView archetype.
 * @return The AiCollectionItemView Class.
 * @memberof AiCollectionItemView
 */
CGAME_EXPORT Class *_AiCollectionItemView(void);
