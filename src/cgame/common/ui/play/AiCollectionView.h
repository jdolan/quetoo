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

#include "AiCollectionItemView.h"

/**
 * @file
 * @brief The AiCollectionView type.
 */

typedef struct AiCollectionView AiCollectionView;
typedef struct AiCollectionViewInterface AiCollectionViewInterface;

/**
 * @brief The AiCollectionView type: the AI players of `ai.lst`, from which the player selects
 * the ones that join a server first.
 * @extends CollectionView
 */
struct AiCollectionView {

  /**
   * @brief The superclass.
   * @private
   */
  CollectionView collectionView;

  /**
   * @brief The interface type.
   * @private
   */
  AiCollectionViewInterface *interface[0];

  /**
   * @brief The AI players, sorted by skill, then by name.
   */
  AiItemInfo *ai;

  /**
   * @brief The number of AI players.
   */
  size_t numAi;
};

/**
 * @brief The AiCollectionView interface.
 */
struct AiCollectionViewInterface {

  /**
   * @brief The superclass interface.
   */
  CollectionViewInterface collectionViewInterface;

  /**
   * @fn AiCollectionView *AiCollectionView::initWithFrame(AiCollectionView *self, const SDL_Rect *frame)
   * @brief Initializes this AiCollectionView with the specified frame, and loads `ai.lst`.
   * @param frame The frame.
   * @return The initialized AiCollectionView, or `NULL` on error.
   * @memberof AiCollectionView
   */
  AiCollectionView *(*initWithFrame)(AiCollectionView *self, const SDL_Rect *frame);

  /**
   * @fn PointerArray *AiCollectionView::selectedAi(const AiCollectionView *self)
   * @return A PointerArray of the selected AiItemInfo pointers.
   * @memberof AiCollectionView
   */
  PointerArray *(*selectedAi)(const AiCollectionView *self);
};

/**
 * @fn Class *AiCollectionView::_AiCollectionView(void)
 * @brief The AiCollectionView archetype.
 * @return The AiCollectionView Class.
 * @memberof AiCollectionView
 */
CGAME_EXPORT Class *_AiCollectionView(void);
