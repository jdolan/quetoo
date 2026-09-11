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

#include "OverlayText.h"

#define _Class _OverlayText

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((Text *) self, initWithText, NULL, NULL);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  const char *text = $((OverlayText *) self, textForFrame, data);

  $(self, setVisibility, text == NULL ? ViewVisibilityHidden : ViewVisibilityVisible);

  if (text) {
    $((Text *) self, setText, text);
  }
}

#pragma mark - OverlayText

/**
 * @fn const char *OverlayText::textForFrame(OverlayText *self, const cl_frame_t *frame)
 * @memberof OverlayText
 */
static const char *textForFrame(OverlayText *self, const cl_frame_t *frame) {
  return NULL;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((OverlayTextInterface *) clazz->interface)->textForFrame = textForFrame;
}

/**
 * @fn Class *OverlayText::_OverlayText(void)
 * @memberof OverlayText
 */
Class *_OverlayText(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "OverlayText",
      .superclass = _Text(),
      .instanceSize = sizeof(OverlayText),
      .interfaceSize = sizeof(OverlayTextInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
