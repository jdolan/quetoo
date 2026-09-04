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

#include "TimeView.h"

#define _Class _TimeView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) $((Text *) self, initWithText, NULL, NULL);
  if (self) {
    ((Text *) self)->colorEscapes = true;
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  $((Text *) self, setText, cgi.ConfigString(CS_TIME));
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *TimeView::_TimeView(void)
 * @memberof TimeView
 */
Class *_TimeView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TimeView",
      .superclass = _Text(),
      .instanceSize = sizeof(TimeView),
      .interfaceSize = sizeof(TimeViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
