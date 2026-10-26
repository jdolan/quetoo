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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

#include "RespawnTimerView.h"

#define _Class _RespawnTimerView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {
    RespawnTimerView *this = (RespawnTimerView *) self;

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("text", &this->text)
    );

    $(self, awakeWithResourceName, "ui/hud/RespawnTimerView.json");
    $(self, resolve, outlets);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  RespawnTimerView *this = (RespawnTimerView *) self;

  if (data == NULL) {
    return;
  }

  const PlayerState *ps = &((const ClientFrame *) data)->ps;

  const int16_t seconds = ps->stats[STAT_RESPAWN];

  $(self, setVisibility, seconds > 0 ? ViewVisibilityVisible : ViewVisibilityHidden);

  if (seconds > 0 && seconds != this->seconds) {
    $(this->text, setText, va("Respawn in %d", seconds));
  }

  this->seconds = seconds;
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *RespawnTimerView::_RespawnTimerView(void)
 * @memberof RespawnTimerView
 */
Class *_RespawnTimerView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "RespawnTimerView",
      .superclass = _StackView(),
      .instanceSize = sizeof(RespawnTimerView),
      .interfaceSize = sizeof(RespawnTimerViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
