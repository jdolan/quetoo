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
#include "cg_practice.h"

#include <ObjectivelyMVC.h>

#include "AiCollectionView.h"
#include "MapListCollectionView.h"

/**
 * @file
 * @brief Practice ViewController.
 */

/**
 * @brief The most tiers that a ladder shows.
 */
#define PRACTICE_MAX_TIERS 8

/**
 * @brief The most entries that a tier shows.
 */
#define PRACTICE_MAX_TIER_ENTRIES 16

typedef struct PracticeViewController PracticeViewController;
typedef struct PracticeViewControllerInterface PracticeViewControllerInterface;

/**
 * @brief One row of the ladder: the entries of a tier, as a CollectionView.
 */
typedef struct {

  /**
   * @brief The PracticeViewController.
   */
  PracticeViewController *self;

  /**
   * @brief The tier, from 1.
   */
  int32_t tier;

  /**
   * @brief The entries of the tier, in ladder order.
   */
  const CGamePracticeEntry *entries[PRACTICE_MAX_TIER_ENTRIES];

  /**
   * @brief The number of entries.
   */
  size_t numEntries;

  /**
   * @brief The CollectionView of the entries.
   */
  CollectionView *collectionView;
} PracticeTier;

/**
 * @brief The PracticeViewController type: the practice ladders, from which the player selects a
 * match against AI players.
 * @extends ViewController
 */
struct PracticeViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface type.
   * @private
   */
  PracticeViewControllerInterface *interface[0];

  /**
   * @brief The ladder Select.
   */
  Select *ladder;

  /**
   * @brief The gameplay Select.
   */
  Select *gameplay;

  /**
   * @brief The tier rows.
   */
  StackView *tiers;

  /**
   * @brief The selected entry's map, limits and status.
   */
  Text *map, *limits, *status;

  /**
   * @brief The selected entry's opponents.
   */
  AiCollectionView *opponents;

  /**
   * @brief The play Button.
   */
  Button *play;

  /**
   * @brief The tiers of the ladder.
   */
  PracticeTier tier[PRACTICE_MAX_TIERS];

  /**
   * @brief The number of tiers.
   */
  size_t numTiers;

  /**
   * @brief The selected entry, by its index in the ladder, or -1.
   */
  int32_t selected;

  /**
   * @brief The map of each entry of the ladder, by its index in the ladder, read from its BSP.
   */
  MapListItemInfo *maps;

  /**
   * @brief The ladder generation that `maps` belongs to.
   */
  uint32_t mapsGeneration;

  /**
   * @brief The ladder generation that the tiers point into.
   */
  uint32_t tiersGeneration;
};

/**
 * @brief The PracticeViewController interface.
 */
struct PracticeViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *PracticeViewController::_PracticeViewController(void)
 * @brief The PracticeViewController archetype.
 * @return The PracticeViewController Class.
 * @memberof PracticeViewController
 */
CGAME_EXPORT Class *_PracticeViewController(void);
