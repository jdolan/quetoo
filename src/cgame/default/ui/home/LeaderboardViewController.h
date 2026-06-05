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

#include "cg_types.h"

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief Leaderboard ViewController.
 */

#define LEADERBOARD_MAX_ENTRIES 50

/**
 * @brief A single row from the global leaderboard API.
 */
typedef struct {
  int32_t rank;
  char    name[64];
  char    guid[68];
  int32_t frags;
  int32_t deaths;
  int32_t damage;
  int32_t captures;
  int32_t time_played;
} LeaderboardEntry;

typedef struct LeaderboardViewController LeaderboardViewController;
typedef struct LeaderboardViewControllerInterface LeaderboardViewControllerInterface;

/**
 * @brief The LeaderboardViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct LeaderboardViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface.
   * @private
   */
  LeaderboardViewControllerInterface *interface;

  /**
   * @brief The global leaderboard table.
   */
  TableView *leaderboard;

  /**
   * @brief Our hashed GUID from the stats API (empty until fetched).
   */
  char guid[68];

  /**
   * @brief Cached leaderboard entries fetched from the stats API.
   */
  LeaderboardEntry entries[LEADERBOARD_MAX_ENTRIES];
  size_t num_entries;
};

/**
 * @brief The LeaderboardViewController interface.
 */
struct LeaderboardViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *LeaderboardViewController::_LeaderboardViewController(void)
 * @brief The LeaderboardViewController archetype.
 * @return The LeaderboardViewController Class.
 * @memberof LeaderboardViewController
 */
CGAME_EXPORT Class *_LeaderboardViewController(void);
