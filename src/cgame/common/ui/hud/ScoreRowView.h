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

typedef struct ScoreRowView ScoreRowView;
typedef struct ScoreRowViewInterface ScoreRowViewInterface;

/**
 * @brief One player on the scoreboard: the icon, a fill in the team colour, the name and
 * ping on the first line, and two more lines a module fills in through
 * ScoreRowView::setDetails. The local player's row carries the class name `self`.
 * @extends View
 */
struct ScoreRowView {

  /**
   * @brief The superclass.
   */
  View view;

  /**
   * @brief The interface type.
   * @protected
   */
  ScoreRowViewInterface *interface[0];

  /**
   * @brief A small image over the icon's corner, e.g. the flag a player carries.
   */
  ImageView *badge;

  /**
   * @brief The detail lines beneath the name, left and right.
   */
  Text *detail, *aside;

  /**
   * @brief The team colour behind the text.
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

struct ScoreRowViewInterface {

  /**
   * @brief The superclass interface.
   */
  ViewInterface viewInterface;

  /**
   * @fn ScoreRowView *ScoreRowView::initWithScore(ScoreRowView *self, const g_score_t *score, int32_t width)
   * @brief Initializes this ScoreRowView for the given score, `width` wide.
   * @param self The ScoreRowView.
   * @param score The score.
   * @param width The row width.
   * @return The initialized ScoreRowView, or `NULL` on error.
   * @memberof ScoreRowView
   */
  ScoreRowView *(*initWithScore)(ScoreRowView *self, const g_score_t *score, int32_t width);

  /**
   * @fn void ScoreRowView::setDetails(ScoreRowView *self, const char *detail, const char *aside)
   * @brief Sets the lines beneath the name.
   * @param self The ScoreRowView.
   * @param detail The left text, which MAY span two lines, or `NULL`.
   * @param aside The right text, or `NULL`.
   * @memberof ScoreRowView
   */
  void (*setDetails)(ScoreRowView *self, const char *detail, const char *aside);
};

CGAME_EXPORT Class *_ScoreRowView(void);
