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

#include "cg_types.h"

/**
 * @file
 * @brief A single key -> value row holding an arbitrary View in each column.
 * @details A KeyValueView is a horizontal StackView holding two arbitrary
 * Views: the key (column 0) and the value (column 1). Any View may be used for
 * either (Label, TextView, Select, Slider, Checkbox, ...). Column widths are
 * enforced by setColumnWidths, which rigidly clamps each widget's own
 * minSize/maxSize width (the same mechanism View::resize honors for any widget,
 * which is why it never loses to a stylesheet min-width floor). Alignment thus
 * never depends on per-widget CSS — only the widget's own look (font, color) is
 * styled, by type-level rules that bind reliably. Authored declaratively via
 * the "key" and "value" JSON inlets, or in C via setKey/setValue.
 */

typedef struct KeyValueView KeyValueView;
typedef struct KeyValueViewInterface KeyValueViewInterface;

/**
 * @brief A key -> value row of two arbitrary Views.
 * @extends StackView
 */
struct KeyValueView {

  /**
   * @brief The superclass.
   * @private
   */
  StackView stackView;

  /**
   * @brief The interface.
   * @private
   */
  KeyValueViewInterface *interface;

  /**
   * @brief The current key widget (column 0). Weak; owned as a subview.
   */
  View *key;

  /**
   * @brief The current value widget (column 1). Weak; owned as a subview.
   */
  View *value;

  /**
   * @brief The column widths last set, re-applied when a widget is replaced.
   * @private
   */
  int keyWidth, valueWidth;
};

/**
 * @brief The KeyValueView interface.
 */
struct KeyValueViewInterface {

  /**
   * @brief The superclass interface.
   */
  StackViewInterface stackViewInterface;

  /**
   * @fn KeyValueView *KeyValueView::initWithFrame(KeyValueView *self, const SDL_Rect *frame)
   * @brief Initializes this KeyValueView with the given frame.
   * @param self The KeyValueView.
   * @param frame The frame, or `NULL`.
   * @return The initialized KeyValueView, or `NULL` on error.
   * @memberof KeyValueView
   */
  KeyValueView *(*initWithFrame)(KeyValueView *self, const SDL_Rect *frame);

  /**
   * @fn void KeyValueView::setKey(KeyValueView *self, View *key)
   * @brief Places `key` (any View) into the key cell, filling its width.
   * @param self The KeyValueView.
   * @param key The View to display in column 0, or `NULL` to clear it.
   * @memberof KeyValueView
   */
  void (*setKey)(KeyValueView *self, View *key);

  /**
   * @fn void KeyValueView::setValue(KeyValueView *self, View *value)
   * @brief Places `value` (any View) into the value cell, filling its width.
   * @param self The KeyValueView.
   * @param value The View to display in column 1, or `NULL` to clear it.
   * @memberof KeyValueView
   */
  void (*setValue)(KeyValueView *self, View *value);

  /**
   * @fn void KeyValueView::setColumnWidths(KeyValueView *self, int keyWidth, int valueWidth)
   * @brief Rigidly sizes the two cells, establishing the row's columns.
   * @param self The KeyValueView.
   * @param keyWidth The column-0 width, in points. Ignored if <= 0.
   * @param valueWidth The column-1 width, in points. Ignored if <= 0.
   * @memberof KeyValueView
   */
  void (*setColumnWidths)(KeyValueView *self, int keyWidth, int valueWidth);
};

/**
 * @fn Class *KeyValueView::_KeyValueView(void)
 * @brief The KeyValueView archetype.
 * @return The KeyValueView Class.
 * @memberof KeyValueView
 */
CGAME_EXPORT Class *_KeyValueView(void);
