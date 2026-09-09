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

#include <ObjectivelyMVC/ImageView.h>
#include <ObjectivelyMVC/Text.h>
#include <ObjectivelyMVC/View.h>

#include "cg_types.h"

/**
 * @file
 * @brief One player on the scoreboard.
 */

typedef struct ScoreView ScoreView;
typedef struct ScoreViewInterface ScoreViewInterface;

/**
 * @brief One column of the scoreboard: a caption, and the width its values need.
 * @details The fields are what a module says its board counts, and the arrangement is what
 * a variant does with them: the cards layout writes them out as prose beneath the name, the
 * table layout gives each one a column under its caption.
 */
typedef struct {
  const char *caption;
  int32_t width;
} ScoreField;

/**
 * @brief The most fields a board may show.
 */
#define SCORE_FIELDS_MAX 4

/**
 * @brief One player on the scoreboard: the icon, a fill in the team colour, the name and
 * ping on the first line, and two more lines a module fills in through
 * ScoreView::setDetails. The local player's row carries the class name `self`.
 * @extends View
 */
struct ScoreView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  ScoreViewInterface *interface[0];

  /**
   * @brief A small image over the icon's corner, e.g. the flag a player carries.
   */
  ImageView *badge;

  /**
   * @brief The detail lines beneath the name, left and right. Hidden in the table layout,
   * which gives each field a column of its own instead.
   */
  Text *detail, *aside;

  /**
   * @brief The team colour behind the text, spanning the row's full width: it is aligned
   * `ViewAlignmentInternal` so the stylesheet's padding insets the text but not the fill.
   */
  View *fill;

  /**
   * @brief The player's icon.
   */
  ImageView *icon;

  /**
   * @brief The name and the ping.
   */
  Text *name, *ping;
};

struct ScoreViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn ScoreView *ScoreView::initWithScore(ScoreView *self, const g_score_t *score, int32_t width)
   * @brief Initializes this ScoreView for the given score, `width` wide.
   * @param self The ScoreView.
   * @param score The score.
   * @param width The row width.
   * @return The initialized ScoreView, or `NULL` on error.
   * @memberof ScoreView
   */
  ScoreView *(*initWithScore)(ScoreView *self, const g_score_t *score, int32_t width);

  /**
   * @fn void ScoreView::setDetails(ScoreView *self, const char *detail, const char *aside)
   * @brief Sets the lines beneath the name.
   * @param self The ScoreView.
   * @param detail The left text, which MAY span two lines, or `NULL`.
   * @param aside The right text, or `NULL`.
   * @memberof ScoreView
   */
  void (*setDetails)(ScoreView *self, const char *detail, const char *aside);

  /**
   * @fn void ScoreView::setFields(ScoreView *self, const ScoreField *fields, const char **values, size_t count)
   * @brief Lays the row out as table cells, one per field, right aligned in their columns.
   * @details The columns are measured from the right, so they line up with the header the
   * board draws above them however long a name is. Hides the prose lines.
   * @param self The ScoreView.
   * @param fields The fields, which give the column widths.
   * @param values The value of each field for this row.
   * @param count The number of fields.
   * @memberof ScoreView
   */
  void (*setFields)(ScoreView *self, const ScoreField *fields, const char **values, size_t count);
};

CGAME_EXPORT Class *_ScoreView(void);
