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

#include "ScrollThumb.h"

#define _Class _ScrollThumb

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return (View *) $((ScrollThumb *) self, initWithFrame, NULL);
}

#pragma mark - Control

/**
 * @see Control::captureEvent(Control *, const SDL_Event *)
 */
static bool captureEvent(Control *self, const SDL_Event *event) {

  ScrollThumb *this = (ScrollThumb *) self;

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if ($((View *) self, didReceiveEvent, event)) {
      self->state |= ControlStateHighlighted;
      return true;
    }
  } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
    if (self->state & ControlStateHighlighted) {
      if (this->delegate.didDrag) {
        this->delegate.didDrag(this, (int) event->motion.yrel);
      }
      return true;
    }
  } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
    if (self->state & ControlStateHighlighted) {
      self->state &= ~ControlStateHighlighted;
      return true;
    }
  }

  return super(Control, self, captureEvent, event);
}

#pragma mark - ScrollThumb

/**
 * @fn ScrollThumb *ScrollThumb::initWithFrame(ScrollThumb *self, const SDL_Rect *frame)
 * @memberof ScrollThumb
 */
static ScrollThumb *initWithFrame(ScrollThumb *self, const SDL_Rect *frame) {

  self = (ScrollThumb *) super(Control, self, initWithFrame, frame);
  if (self) {
    $((View *) self, addClassName, "thumb");
  }

  return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;

  ((ControlInterface *) clazz->interface)->captureEvent = captureEvent;

  ((ScrollThumbInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *ScrollThumb::_ScrollThumb(void)
 * @memberof ScrollThumb
 */
Class *_ScrollThumb(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ScrollThumb",
      .superclass = _Control(),
      .instanceSize = sizeof(ScrollThumb),
      .interfaceOffset = offsetof(ScrollThumb, interface),
      .interfaceSize = sizeof(ScrollThumbInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
