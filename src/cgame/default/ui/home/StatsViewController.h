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
 * @brief The kills-by-weapon block of the StatsResponse shape.
 */
typedef struct {
  char weapon[64];
  int32_t frags;
} KillsByWeapon;

/**
 * @brief The nemesis block of the StatsResponse shape.
 */
typedef struct {
  char name[64];
} Nemesis;

/**
 * @brief The Stats API JSON response shape.
 */
typedef struct {
  int32_t rank;
  int32_t frags;
  int32_t deaths;
  int32_t captures;
  int32_t time_played;
  Nemesis nemesis;
  KillsByWeapon kills_by_weapon[20];
} StatsResponse;

/**
 * @file
 * @brief Stats ViewController.
 */
typedef struct StatsViewController StatsViewController;
typedef struct StatsViewControllerInterface StatsViewControllerInterface;

/**
 * @brief The StatsViewController type.
 * @extends ViewController
 * @ingroup ViewControllers
 */
struct StatsViewController {

  /**
   * @brief The superclass.
   * @private
   */
  ViewController viewController;

  /**
   * @brief The interface.
   * @private
   */
  StatsViewControllerInterface *interface;

  /**
   * @brief The StatsResponse.
   */
  StatsResponse stats;

  /**
   * @brief Stat tile labels.
   */
  Label *nameLabel;
  Label *rankLabel;
  Label *fragsLabel;
  Label *deathsLabel;
  Label *kdLabel;
  Label *timeLabel;
  Label *nemesisLabel;

  /**
   * @brief Kills-by-weapon breakdown table.
   */
  TableView *weaponsTable;
};

/**
 * @brief The StatsViewController interface.
 */
struct StatsViewControllerInterface {

  /**
   * @brief The superclass interface.
   */
  ViewControllerInterface viewControllerInterface;
};

/**
 * @fn Class *StatsViewController::_StatsViewController(void)
 * @brief The StatsViewController archetype.
 * @return The StatsViewController Class.
 * @memberof StatsViewController
 */
CGAME_EXPORT Class *_StatsViewController(void);
