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

#include "SpectatorView.h"

#define _Class _SpectatorView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return super(View, self, init);
}

#pragma mark - OverlayText

/**
 * @see OverlayText::textForFrame(OverlayText *, const cl_frame_t *)
 */
static const char *textForFrame(OverlayText *self, const cl_frame_t *frame) {

  const player_state_t *ps = &frame->ps;

  if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
    return "Spectating";
  }

  return NULL;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;

  ((OverlayTextInterface *) clazz->interface)->textForFrame = textForFrame;
}

/**
 * @fn Class *SpectatorView::_SpectatorView(void)
 * @memberof SpectatorView
 */
Class *_SpectatorView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SpectatorView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(SpectatorView),
      .interfaceSize = sizeof(SpectatorViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
