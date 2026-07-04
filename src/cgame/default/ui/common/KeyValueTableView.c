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

#include "cg_local.h"

#include "KeyValueTableView.h"

#define _Class _KeyValueTableView

#pragma mark - View

/**
 * @see View::applyStyle(View *, const Style *)
 */
static void applyStyle(View *self, const Style *style) {

  super(View, self, applyStyle, style);

  KeyValueTableView *this = (KeyValueTableView *) self;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("-key-width", InletTypeInteger, &this->keyWidth, NULL),
    MakeInlet("-value-width", InletTypeInteger, &this->valueWidth, NULL)
  );

  $(self, bind, inlets, (Dictionary *) style->attributes);

  self->needsLayout = true;
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((KeyValueTableView *) self, initWithFrame, NULL);
}

/**
 * @see View::layoutSubviews(View *)
 */
static void layoutSubviews(View *self) {

  KeyValueTableView *this = (KeyValueTableView *) self;

  // Push the table's fixed columns into every row so all rows share one set of
  // column widths. setColumnWidths only writes when a clamp actually changes, so
  // once the rows match this is a no-op -- it cannot churn the layout. Using the
  // fixed -value-width (not a bounds-derived fill) avoids a re-layout feedback
  // loop with the global `TextView { min-width }` floor.
  const Array *subviews = (Array *) self->subviews;
  for (size_t i = 0; i < subviews->count; i++) {
    KeyValueView *row = $(subviews, objectAtIndex, i);

    if (row->keyWidth != this->keyWidth || row->valueWidth != this->valueWidth) {
      $(row, setColumnWidths, this->keyWidth, this->valueWidth);
    }
  }

  super(View, self, layoutSubviews);
}

#pragma mark - KeyValueTableView

/**
 * @fn KeyValueTableView *KeyValueTableView::initWithFrame(KeyValueTableView *self, const SDL_Rect *frame)
 * @memberof KeyValueTableView
 */
static KeyValueTableView *initWithFrame(KeyValueTableView *self, const SDL_Rect *frame) {

  self = (KeyValueTableView *) super(StackView, self, initWithFrame, frame);
  if (self) {

    $((View *) self, addClassName, "keyValueTable");

    self->stackView.axis = StackViewAxisVertical;

    // A small gap between rows so the table does not feel cramped.
    self->stackView.spacing = 6;

    // Grow in height with the rows; fill the parent's width so the table claims
    // the box. CSS may override either of these via applyStyle.
    ((View *) self)->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

    // Default columns so rows have widths before the stylesheet's -key-width /
    // -value-width are applied; CSS overrides these in applyStyle.
    self->keyWidth = 140;
    self->valueWidth = 280;
  }

  return self;
}

/**
 * @fn KeyValueView *KeyValueTableView::addRow(KeyValueTableView *self, View *key, View *value)
 * @memberof KeyValueTableView
 */
static KeyValueView *addRow(KeyValueTableView *self, View *key, View *value) {

  KeyValueView *row = $(alloc(KeyValueView), initWithFrame, NULL);
  assert(row);

  if (key) {
    $(row, setKey, key);
  }

  if (value) {
    $(row, setValue, value);
  }

  $(row, setColumnWidths, self->keyWidth, self->valueWidth);

  $((View *) self, addSubview, (View *) row);
  release(row);

  ((View *) self)->needsLayout = true;

  return row;
}

/**
 * @fn void KeyValueTableView::addRowView(KeyValueTableView *self, KeyValueView *row)
 * @memberof KeyValueTableView
 */
static void addRowView(KeyValueTableView *self, KeyValueView *row) {

  $(row, setColumnWidths, self->keyWidth, self->valueWidth);

  $((View *) self, addSubview, (View *) row);

  ((View *) self)->needsLayout = true;
}

/**
 * @fn void KeyValueTableView::removeAllRows(KeyValueTableView *self)
 * @memberof KeyValueTableView
 */
static void removeAllRows(KeyValueTableView *self) {

  $((View *) self, removeAllSubviews);
  ((View *) self)->needsLayout = true;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->applyStyle = applyStyle;
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->layoutSubviews = layoutSubviews;

  ((KeyValueTableViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((KeyValueTableViewInterface *) clazz->interface)->addRow = addRow;
  ((KeyValueTableViewInterface *) clazz->interface)->addRowView = addRowView;
  ((KeyValueTableViewInterface *) clazz->interface)->removeAllRows = removeAllRows;
}

/**
 * @fn Class *KeyValueTableView::_KeyValueTableView(void)
 * @memberof KeyValueTableView
 */
Class *_KeyValueTableView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "KeyValueTableView",
      .superclass = _StackView(),
      .instanceSize = sizeof(KeyValueTableView),
      .interfaceOffset = offsetof(KeyValueTableView, interface),
      .interfaceSize = sizeof(KeyValueTableViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
