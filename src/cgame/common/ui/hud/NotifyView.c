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

#include "NotifyView.h"

#define _Class _NotifyView

#define NOTIFY_MAX_LINES 12

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = super(View, self, init);
  if (self) {
    ((ConsoleText *) self)->console.level = PRINT_MEDIUM | PRINT_HIGH;
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  ConsoleText *this = (ConsoleText *) self;

  const size_t lines = Clampf(cg_notify_lines->integer, 0, NOTIFY_MAX_LINES);
  const bool hidden = lines == 0 || cgi.GetKeyDest() != KEY_GAME;

  $(self, setVisibility, hidden ? ViewVisibilityHidden : ViewVisibilityVisible);

  if (data && !hidden && self->superview) {
    const uint32_t now = (uint32_t) SDL_GetTicks();
    const uint32_t millis = cg_notify_time->value * 1000;

    const uint32_t since = now > millis ? now - millis : 0;

    this->console.whence = since > cg_hud_state.clear_time ? since : cg_hud_state.clear_time;

    $(this, tail, self->superview->frame.w, lines);
  }

  super(View, self, updateBindings, data);
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *NotifyView::_NotifyView(void)
 * @memberof NotifyView
 */
Class *_NotifyView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "NotifyView",
      .superclass = _ConsoleText(),
      .instanceSize = sizeof(NotifyView),
      .interfaceSize = sizeof(NotifyViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
