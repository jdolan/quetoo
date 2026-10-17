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

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>

#include "cg_types.h"

/**
 * @file
 * @brief What follows this level: the local player's match stats, the countdown, and the
 * map it is counting down to.
 */

typedef struct IntermissionView IntermissionView;
typedef struct IntermissionViewInterface IntermissionViewInterface;

/**
 * @brief What follows this level: the local player's match stats, the countdown, and the
 * map it is counting down to.
 * @details One tile per candidate, each a thumbnail over the map's name. With a vote
 * running the tiles carry the key that picks them and the tally they have drawn; with
 * none, the single tile just says what is next.
 * @remarks This shows when the HUD does not, so like the scoreboard it belongs to the
 * HudViewController rather than to the hud.
 * @extends View
 */
struct IntermissionView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  IntermissionViewInterface *interface[0];

  /**
   * @brief The vote prompt and the tiles beneath it, along the bottom of the screen.
   */
  StackView *strip;

  /**
   * @brief The result of a practice match: Victory or Defeat.
   */
  Text *result;

  /**
   * @brief The keys that cast a ballot while a vote runs, or that choose what follows a
   * practice match.
   */
  Text *prompt;

  /**
   * @brief The practice result shown, -1 until it is final.
   */
  int32_t practiceResult;

  /**
   * @brief The tiles, one per candidate.
   */
  StackView *maps;

  /**
   * @brief The tally beneath each tile, or `NULL` where no vote is running.
   */
  Text *votes[MAX_NEXT_MAPS];

  /**
   * @brief The countdown.
   */
  Text *countdown;

  /**
   * @brief The candidates the tiles were built for.
   */
  uint32_t generation;

  /**
   * @brief The local player's match stats: the totals, then a row for each weapon.
   */
  StackView *stats;

  /**
   * @brief The match stats the panel was built for.
   */
  uint32_t statsGeneration;
};

struct IntermissionViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn void IntermissionView::rebuild(IntermissionView *self)
   * @brief Rebuilds the tiles for the candidates now published.
   * @memberof IntermissionView
   */
  void (*rebuild)(IntermissionView *self);

  /**
   * @fn void IntermissionView::rebuildStats(IntermissionView *self)
   * @brief Rebuilds the stats panel for the match stats now received.
   * @memberof IntermissionView
   */
  void (*rebuildStats)(IntermissionView *self);
};

CGAME_EXPORT Class *_IntermissionView(void);
