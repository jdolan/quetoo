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

#include "cg_types.h"

#include <ObjectivelyMVC.h>

#include "GameCollectionItemView.h"

/**
 * @file
 * @brief Games ViewController: one card for each installed game. A click on a card makes that
 * game current.
 */

typedef struct GamesViewController GamesViewController;
typedef struct GamesViewControllerInterface GamesViewControllerInterface;

/**
 * @brief The GamesViewController type.
 * @extends ViewController
 */
struct GamesViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @private
   */
  GamesViewControllerInterface *interface[0];

  /**
   * @brief The games CollectionView.
   */
  CollectionView *gamesList;

  /**
   * @brief The installed games, sorted with `default` first, then by title.
   */
  GameItemInfo *games;

  /**
   * @brief The count of `games`.
   */
  size_t numGames;
};

/**
 * @brief The GamesViewController interface.
 */
struct GamesViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *GamesViewController::_GamesViewController(void)
 * @brief The GamesViewController archetype.
 * @return The GamesViewController Class.
 * @memberof GamesViewController
 */
CGAME_EXPORT Class *_GamesViewController(void);
