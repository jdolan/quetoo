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

#include <ObjectivelyMVC/StackView.h>
#include <ObjectivelyMVC/Text.h>
#include <ObjectivelyMVC/View.h>

#include "ScoreRowView.h"

/**
 * @file
 * @brief The scoreboard, shown while `STAT_SCORES` is set and through the intermission.
 */

typedef struct ScoreboardView ScoreboardView;
typedef struct ScoreboardViewInterface ScoreboardViewInterface;

/**
 * @brief The scoreboard, shown while `STAT_SCORES` is set and through the intermission.
 * @details The map title across the top, the team totals beneath it in a team game, then the
 * rows: one column per team with the spectators to their left, or one or two columns of
 * everyone. Rows are rebuilt when the scores change (Cg_ScoresGeneration) or the view
 * resizes. A module that arranges its board differently names a subclass in its
 * `ui/hud/scoreboard.json`, overriding ScoreboardView::rowForScore for the rows and
 * ScoreboardView::rebuild for the columns.
 * @extends View
 */
struct ScoreboardView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  ScoreboardViewInterface *interface[0];

  /**
   * @brief The row columns, and any a subclass adds.
   */
  StackView *columns;

  /**
   * @brief The scores generation and view height the rows were last built for.
   */
  uint32_t generation;
  int32_t height;

  /**
   * @brief The team totals, in a team game.
   */
  StackView *header;

  /**
   * @brief The width of a row, which a subclass MAY change before the first rebuild.
   */
  int32_t rowWidth;

  /**
   * @brief The map title.
   */
  Text *title;
};

struct ScoreboardViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn StackView *ScoreboardView::addColumn(ScoreboardView *self)
   * @brief Adds an empty column of rows.
   * @param self The ScoreboardView.
   * @return The column, owned by this view.
   * @memberof ScoreboardView
   */
  StackView *(*addColumn)(ScoreboardView *self);

  /**
   * @fn void ScoreboardView::rebuild(ScoreboardView *self)
   * @brief Rebuilds the header and the rows from the current scores.
   * @param self The ScoreboardView.
   * @memberof ScoreboardView
   */
  void (*rebuild)(ScoreboardView *self);

  /**
   * @fn ScoreRowView *ScoreboardView::rowForScore(ScoreboardView *self, const g_score_t *score)
   * @brief Creates the row for the given score: the stock frags and deaths, or spectating.
   * @param self The ScoreboardView.
   * @param score The score.
   * @return The row, retained. The caller owns the returned ScoreRowView, and MUST release it.
   * @memberof ScoreboardView
   */
  ScoreRowView *(*rowForScore)(ScoreboardView *self, const g_score_t *score);
};

CGAME_EXPORT Class *_ScoreboardView(void);
