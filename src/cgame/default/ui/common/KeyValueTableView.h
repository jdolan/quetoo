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

#include "KeyValueView.h"

#include "cg_types.h"

/**
 * @file
 * @brief A vertical table of KeyValueView rows with inherited columns.
 * @details A KeyValueTableView is a vertical StackView that owns the column
 * widths for its rows. On every layout it pushes its `-key-width` /
 * `-value-width` (CSS attributes on its own root) into every row via
 * KeyValueView::setColumnWidths -- so all rows (including dynamically added
 * ones) share one set of columns, configured in one place and enforced in C,
 * immune to the per-widget min-width floors that CSS width rules fight.
 *
 * The table GROWS with its content; it does not scroll. Scrolling, when needed,
 * is provided by an ancestor StackView with `scroll: true`.
 */

typedef struct KeyValueTableView KeyValueTableView;
typedef struct KeyValueTableViewInterface KeyValueTableViewInterface;

/**
 * @brief A vertical table of KeyValueView rows.
 * @extends StackView
 */
struct KeyValueTableView {

  /**
   * @brief The superclass.
   * @private
   */
  StackView stackView;

  /**
   * @brief The interface.
   * @private
   */
  KeyValueTableViewInterface *interface;

  /**
   * @brief Column-0 width pushed to every row. Style attribute `-key-width`.
   */
  int keyWidth;

  /**
   * @brief Column-1 width pushed to every row. Style attribute `-value-width`.
   */
  int valueWidth;
};

/**
 * @brief The KeyValueTableView interface.
 */
struct KeyValueTableViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn KeyValueTableView *KeyValueTableView::initWithFrame(KeyValueTableView *self, const SDL_Rect *frame)
   * @brief Initializes this KeyValueTableView with the given frame.
   * @param self The KeyValueTableView.
   * @param frame The frame, or `NULL`.
   * @return The initialized KeyValueTableView, or `NULL` on error.
   * @memberof KeyValueTableView
   */
  KeyValueTableView *(*initWithFrame)(KeyValueTableView *self, const SDL_Rect *frame);

  /**
   * @fn KeyValueView *KeyValueTableView::addRow(KeyValueTableView *self, View *key, View *value)
   * @brief Appends a row holding `key` and `value`, inheriting the table columns.
   * @param self The KeyValueTableView.
   * @param key The View for column 0 (any View), or `NULL`.
   * @param value The View for column 1 (any View), or `NULL`.
   * @return The new KeyValueView row.
   * @memberof KeyValueTableView
   */
  KeyValueView *(*addRow)(KeyValueTableView *self, View *key, View *value);

  /**
   * @fn void KeyValueTableView::addRowView(KeyValueTableView *self, KeyValueView *row)
   * @brief Appends a pre-built KeyValueView (e.g. an editable subclass) as a row.
   * @param self The KeyValueTableView.
   * @param row The row to append; it inherits the table columns.
   * @memberof KeyValueTableView
   */
  void (*addRowView)(KeyValueTableView *self, KeyValueView *row);

  /**
   * @fn void KeyValueTableView::removeAllRows(KeyValueTableView *self)
   * @brief Removes every row from the table.
   * @param self The KeyValueTableView.
   * @memberof KeyValueTableView
   */
  void (*removeAllRows)(KeyValueTableView *self);
};

/**
 * @fn Class *KeyValueTableView::_KeyValueTableView(void)
 * @brief The KeyValueTableView archetype.
 * @return The KeyValueTableView Class.
 * @memberof KeyValueTableView
 */
CGAME_EXPORT Class *_KeyValueTableView(void);
