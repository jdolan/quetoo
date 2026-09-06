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

#include "ChaseView.h"

#define _Class _ChaseView

#pragma mark - OverlayText

/**
 * @see OverlayText::textForFrame(OverlayText *, const cl_frame_t *)
 */
static const char *textForFrame(OverlayText *self, const cl_frame_t *frame) {

  const player_state_t *ps = &frame->ps;

  const int32_t e = ps->stats[STAT_CHASE];
  if (e <= 0 || e >= MAX_ENTITIES) {
    return NULL;
  }

  const cl_entity_t *ent = cgi.client->entities + e;
  const cg_client_info_t *ci = &cg_state.clients[ent->current.client];

  static char string[MAX_INFO_STRING_VALUE * 2];
  q_snprintf(string, sizeof(string), "Chasing ^7%s", ci->name);

  char *s = q_strchr(string, '\\');
  if (s) {
    *s = '\0';
  }

  return string;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((OverlayTextInterface *) clazz->interface)->textForFrame = textForFrame;
}

/**
 * @fn Class *ChaseView::_ChaseView(void)
 * @memberof ChaseView
 */
Class *_ChaseView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ChaseView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(ChaseView),
      .interfaceSize = sizeof(ChaseViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
