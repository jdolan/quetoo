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

#include "TargetNameView.h"

#define _Class _TargetNameView

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
  return super(View, self, init);
}

#pragma mark - OverlayText

/**
 * @see OverlayText::textForFrame(OverlayText *, const ClientFrame *)
 */
static const char *textForFrame(OverlayText *self, const ClientFrame *frame) {

  static uint32_t time;
  static char name[MAX_INFO_STRING_VALUE];

  if (!cg_draw_target_name->integer) {
    return NULL;
  }

  if (time > cgi.client->unclamped_time) {
    time = 0;
  }

  const Vec3 pos = Vec3_Fmaf(cgi.view->origin, MAX_WORLD_DIST, cgi.view->forward);

  const CmTrace tr = cgi.Trace(cgi.view->origin, pos, Box3_Zero(), NULL, CONTENTS_MASK_CLIP_PROJECTILE);
  if (tr.fraction < 1.f) {

    const ClientEntity *ent = tr.ent;

    // a corpse is not someone to name: it cannot be spoken to, teamed with, or shot at to any
    // purpose, and naming it reads as though they were still standing where they fell
    if (ent->current.model1 == MODEL_CLIENT && !(ent->current.effects & EF_CORPSE)) {

      const ClientGameClientInfo *client = Cg_ClientInfo(ent);

      q_strlcpy(name, client->name, sizeof(name));
      time = cgi.client->unclamped_time;
    }
  }

  if (cgi.client->unclamped_time - time > 500) {
    return NULL;
  }

  return *name ? name : NULL;
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
 * @fn Class *TargetNameView::_TargetNameView(void)
 * @memberof TargetNameView
 */
Class *_TargetNameView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "TargetNameView",
      .superclass = _OverlayText(),
      .instanceSize = sizeof(TargetNameView),
      .interfaceSize = sizeof(TargetNameViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
