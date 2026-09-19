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

#include "PingView.h"

#define _Class _PingView

#define PING_DROPPED_INTERVAL 1000

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  return (View *) $((CounterView *) self, initWithCaption, NULL, STAT_PING);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  PingView *this = (PingView *) self;

  $(self, setVisibility, cg_draw_ping->integer ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (data) {
    const Client *cl = cgi.client;
    const ClientFrame *frame = (const ClientFrame *) data;
    const uint32_t now = (uint32_t) SDL_GetTicks();

    if (cl->dropped != this->dropped) {
      this->dropped = cl->dropped;
      this->droppedTime = now;
    }

    const bool dropping = this->droppedTime && now - this->droppedTime < PING_DROPPED_INTERVAL;
    const bool lagging = dropping || frame->ps.stats[STAT_PING] > cg_draw_ping_warn->integer;

    View *value = (View *) this->counterView.value;

    if (lagging) {
      $(value, addClassName, "lagging");
    } else {
      $(value, removeClassName, "lagging");
    }
  }

  super(View, self, updateBindings, data);
}

#pragma mark - CounterView

/**
 * @see CounterView::textForFrame(CounterView *, const ClientFrame *)
 * @remarks Unlike the base implementation, the ping is shown while spectating.
 */
static const char *textForFrame(CounterView *self, const ClientFrame *frame) {

  snprintf(self->text, sizeof(self->text), "%d", $(self, valueForFrame, frame));

  return self->text;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

  ((CounterViewInterface *) clazz->interface)->textForFrame = textForFrame;
}

/**
 * @fn Class *PingView::_PingView(void)
 * @memberof PingView
 */
Class *_PingView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "PingView",
      .superclass = _CounterView(),
      .instanceSize = sizeof(PingView),
      .interfaceSize = sizeof(PingViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
