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

#include "FpsView.h"

#define _Class _FpsView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  return (View *) $((CounterView *) self, initWithCaption, NULL, COUNTER_VIEW_NO_STAT);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  FpsView *this = (FpsView *) self;

  $(self, setHidden, !cg_draw_fps->integer);

  if (data) {
    this->frames++;

    const uint32_t now = (uint32_t) SDL_GetTicks();
    if (now - this->time >= 1000) {
      this->fps = this->frames;
      this->frames = 0;
      this->time = now;
    }
  }

  super(View, self, updateBindings, data);
}

#pragma mark - CounterView

/**
 * @see CounterView::valueForFrame(CounterView *, const cl_frame_t *)
 */
static int32_t valueForFrame(CounterView *self, const cl_frame_t *frame) {
  return ((FpsView *) self)->fps;
}

/**
 * @see CounterView::textForFrame(CounterView *, const cl_frame_t *)
 */
static const char *textForFrame(CounterView *self, const cl_frame_t *frame) {

  snprintf(self->text, sizeof(self->text), "%3d", $(self, valueForFrame, frame));

  return self->text;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((CounterViewInterface *) clazz->interface)->textForFrame = textForFrame;
  ((CounterViewInterface *) clazz->interface)->valueForFrame = valueForFrame;
}

/**
 * @fn Class *FpsView::_FpsView(void)
 * @memberof FpsView
 */
Class *_FpsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "FpsView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(FpsView),
      .interfaceSize = sizeof(FpsViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
