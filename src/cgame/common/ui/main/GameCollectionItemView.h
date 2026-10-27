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

#include "cg_types.h"

/**
 * @file
 * @brief The GameCollectionItemView type.
 */

/**
 * @brief An installed game, as the games menu shows it.
 */
typedef struct GameItemInfo {

  /**
   * @brief The game's directory name, which the `game` command takes.
   */
  char game[MAX_QPATH];

  /**
   * @brief The absolute directory that holds the game's `game.lst`.
   */
  char dir[MAX_OS_PATH];

  /**
   * @brief The game's `game.lst`.
   */
  GameInfo info;
} GameItemInfo;

typedef struct GameCollectionItemView GameCollectionItemView;
typedef struct GameCollectionItemViewInterface GameCollectionItemViewInterface;

/**
 * @brief The GameCollectionItemView type: a game's card, with its image, title and description.
 * @extends CollectionItemView
 */
struct GameCollectionItemView {

  /**
   * @brief The superclass.
   * @private
   */
  CollectionItemView collectionItemView;

  /**
   * @brief The interface type.
   * @private
   */
  GameCollectionItemViewInterface *interface[0];

  /**
   * @brief The description, which wraps to the card's width.
   */
  Text *description;
};

/**
 * @brief The GameCollectionItemView interface.
 */
struct GameCollectionItemViewInterface {

  /**
   * @brief The superclass interface.
   */
  CollectionItemViewInterface collectionItemViewInterface;

  /**
   * @fn GameCollectionItemView *GameCollectionItemView::initWithFrame(GameCollectionItemView *self, const SDL_Rect *frame)
   * @brief Initializes this GameCollectionItemView with the specified frame.
   * @param frame The frame.
   * @return The initialized GameCollectionItemView, or `NULL` on error.
   * @memberof GameCollectionItemView
   */
  GameCollectionItemView *(*initWithFrame)(GameCollectionItemView *self, const SDL_Rect *frame);

  /**
   * @fn void GameCollectionItemView::setGameItemInfo(GameCollectionItemView *self, const GameItemInfo *info)
   * @brief Shows the game `info`.
   * @param info The game.
   * @memberof GameCollectionItemView
   */
  void (*setGameItemInfo)(GameCollectionItemView *self, const GameItemInfo *info);
};

/**
 * @fn Class *GameCollectionItemView::_GameCollectionItemView(void)
 * @brief The GameCollectionItemView archetype.
 * @return The GameCollectionItemView Class.
 * @memberof GameCollectionItemView
 */
CGAME_EXPORT Class *_GameCollectionItemView(void);
