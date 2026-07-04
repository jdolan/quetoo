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

#include "KeyValueView.h"

#define _Class _KeyValueView

#pragma mark - Helpers

/**
 * @brief Rigidly clamps `view`'s width, establishing its column. Only writes when
 * the clamp actually changes, so re-asserting it every layout cannot churn.
 */
static void applyColumnWidth(View *view, const int width) {
  if (view && width > 0 && (view->minSize.w != width || view->maxSize.w != width)) {
    view->minSize.w = view->maxSize.w = width;
    view->needsLayout = true;
  }
}

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

  super(View, self, awakeWithDictionary, dictionary);

  KeyValueView *this = (KeyValueView *) self;

  View *key = NULL, *value = NULL;

  const Inlet inlets[] = MakeInlets(
    MakeInlet("key", InletTypeView, &key, NULL),
    MakeInlet("value", InletTypeView, &value, NULL)
  );

  $(self, bind, inlets, dictionary);

  // The inlet binding hands us a retained View; parenting it retains it again,
  // so release our reference once it is in place.
  if (key) {
    $(this, setKey, key);
    release(key);
  }

  if (value) {
    $(this, setValue, value);
    release(value);
  }
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((KeyValueView *) self, initWithFrame, NULL);
}

/**
 * @see View::applyStyle(View *, const Style *)
 * @brief Re-assert the column clamps right after the stylesheet is applied. This
 * is the only place that reliably wins the column width: applyStyle runs AFTER
 * super applies the global `TextView { min-width: 200 }` floor (common.css), so
 * clamping here always overrides it -- regardless of selector cascade. And unlike
 * a layoutSubviews clamp, this does NOT loop: applyStyle is not re-run when its
 * applyColumnWidth marks the view needsLayout, so it settles in one pass. This is
 * what keeps every row's key cell a rigid keyWidth without a per-frame fight.
 */
static void applyStyle(View *self, const Style *style) {

  super(View, self, applyStyle, style);

  KeyValueView *this = (KeyValueView *) self;

  applyColumnWidth(this->key, this->keyWidth);
  applyColumnWidth(this->value, this->valueWidth);
}

#pragma mark - KeyValueView

/**
 * @fn KeyValueView *KeyValueView::initWithFrame(KeyValueView *self, const SDL_Rect *frame)
 * @memberof KeyValueView
 */
static KeyValueView *initWithFrame(KeyValueView *self, const SDL_Rect *frame) {

  self = (KeyValueView *) super(StackView, self, initWithFrame, frame);
  if (self) {
    ((StackView *) self)->axis = StackViewAxisHorizontal;

    // Fill the table's width so the value column can expand to the right edge.
    ((View *) self)->autoresizingMask = ViewAutoresizingContain | ViewAutoresizingWidth;

    $((View *) self, addClassName, "keyValue");
  }

  return self;
}

/**
 * @fn void KeyValueView::setKey(KeyValueView *self, View *key)
 * @memberof KeyValueView
 */
static void setKey(KeyValueView *self, View *key) {

  if (self->key) {
    $(self->key, removeFromSuperview);
    self->key = NULL;
  }

  if (key) {
    if (self->value) {
      $((View *) self, addSubviewRelativeTo, key, self->value, ViewPositionBefore);
    } else {
      $((View *) self, addSubview, key);
    }
    self->key = key;
    applyColumnWidth(key, self->keyWidth);
  }
}

/**
 * @fn void KeyValueView::setValue(KeyValueView *self, View *value)
 * @memberof KeyValueView
 */
static void setValue(KeyValueView *self, View *value) {

  if (self->value) {
    $(self->value, removeFromSuperview);
    self->value = NULL;
  }

  if (value) {
    $((View *) self, addSubview, value);
    self->value = value;
    applyColumnWidth(value, self->valueWidth);
  }
}

/**
 * @fn void KeyValueView::setColumnWidths(KeyValueView *self, int keyWidth, int valueWidth)
 * @memberof KeyValueView
 */
static void setColumnWidths(KeyValueView *self, int keyWidth, int valueWidth) {

  self->keyWidth = keyWidth;
  self->valueWidth = valueWidth;

  applyColumnWidth(self->key, keyWidth);
  applyColumnWidth(self->value, valueWidth);

  ((View *) self)->needsLayout = true;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
  ((ViewInterface *) clazz->interface)->applyStyle = applyStyle;
  ((ViewInterface *) clazz->interface)->init = init;

  ((KeyValueViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((KeyValueViewInterface *) clazz->interface)->setKey = setKey;
  ((KeyValueViewInterface *) clazz->interface)->setValue = setValue;
  ((KeyValueViewInterface *) clazz->interface)->setColumnWidths = setColumnWidths;
}

/**
 * @fn Class *KeyValueView::_KeyValueView(void)
 * @memberof KeyValueView
 */
Class *_KeyValueView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "KeyValueView",
      .superclass = _StackView(),
      .instanceSize = sizeof(KeyValueView),
      .interfaceOffset = offsetof(KeyValueView, interface),
      .interfaceSize = sizeof(KeyValueViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
