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

#include "TeamBannerView.h"

#define _Class _TeamBannerView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return $(self, initWithFrame, NULL);
}

/**
 * @see View::updateBindings(View *, ident)
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  if (data == NULL) {
    return;
  }

  const player_state_t *ps = &((const cl_frame_t *) data)->ps;

  const int16_t team = ps->stats[STAT_TEAM];

  $(self, setHidden, team == -1);

  if (team != -1) {
    const color32_t color = Color_Color32(ColorHSVA(cg_state.teams[team].hue, 1.f, 1.f, .14f));
    self->backgroundColor = (SDL_Color) { color.r, color.g, color.b, color.a };
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {
  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *TeamBannerView::_TeamBannerView(void)
 * @memberof TeamBannerView
 */
Class *_TeamBannerView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TeamBannerView",
      .superclass = _View(),
      .instanceSize = sizeof(TeamBannerView),
      .interfaceSize = sizeof(TeamBannerViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
