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

/**
 * @file
 * @brief A captioned counter in the stat column, e.g. Frags.
 */

/**
 * @brief The `stat` of a CounterView whose value comes from CounterView::valueForFrame alone.
 */
#define COUNTER_VIEW_NO_STAT -1

typedef struct CounterView CounterView;
typedef struct CounterViewInterface CounterViewInterface;

/**
 * @brief A captioned counter in the stat column, e.g. Frags.
 * @details Configured in JSON by `caption` and `stat`, a `STAT_*` name from `stats`; a module
 * adding a counter of its own uses CounterView::initWithCaption. The value blanks while
 * spectating without a chase target, keeping its row so the column does not shift.
 * @extends StackView
 */
struct CounterView {

  /**
   * @brief The superclass.
   */
  StackView stackView;

  /**
   * @brief The interface type.
   * @protected
   */
  CounterViewInterface *interface[0];

  /**
   * @brief The caption.
   */
  Text *caption;

  /**
   * @brief The index into `player_state_t::stats`.
   */
  int32_t stat;

  /**
   * @brief The value.
   */
  Text *value;
};

struct CounterViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn CounterView *CounterView::initWithCaption(CounterView *self, const char *caption, int32_t stat)
   * @brief Initializes this CounterView with the given caption, counting the given stat.
   * @param self The CounterView.
   * @param caption The caption.
   * @param stat The index into `player_state_t::stats`, or `COUNTER_VIEW_NO_STAT` when a
   * subclass overrides CounterView::valueForFrame.
   * @return The initialized CounterView, or `NULL` on error.
   * @memberof CounterView
   */
  CounterView *(*initWithCaption)(CounterView *self, const char *caption, int32_t stat);

  /**
   * @fn int32_t CounterView::valueForFrame(CounterView *self, const cl_frame_t *frame)
   * @brief Resolves the value shown for the given frame.
   * @details The default reads `stat` from the frame's player state; subclasses deriving a
   * value some other way override this.
   * @param self The CounterView.
   * @param frame The frame.
   * @return The value.
   * @memberof CounterView
   */
  int32_t (*valueForFrame)(CounterView *self, const cl_frame_t *frame);
};

CGAME_EXPORT Class *_CounterView(void);
