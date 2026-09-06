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

#include "VoteView.h"

#define _Class _VoteView

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

  if (!cg_state.vote.active) {
    return NULL;
  }

  const uint32_t time = cgi.client->time;
  const uint32_t left = cg_state.vote.deadline > time ? (cg_state.vote.deadline - time) / 1000 : 0;

  return va("^2%s called a vote: %s%s%s\n^7Yes %d  No %d  of %d  %us",
            cg_state.vote.initiator, cg_state.vote.type, *cg_state.vote.arg ? " " : "", cg_state.vote.arg,
            cg_state.vote.yes, cg_state.vote.no, cg_state.vote.eligible, left);
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
 * @fn Class *VoteView::_VoteView(void)
 * @memberof VoteView
 */
Class *_VoteView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "VoteView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(VoteView),
      .interfaceSize = sizeof(VoteViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
