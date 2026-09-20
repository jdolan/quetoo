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

#include "CameraControlsView.h"
#include "HudViewController.h"

#define _Class _CameraControlsView

/**
 * @brief The icon and name for each camera, indexed by `CGameCameraMode`, with the detached
 * camera last: it is the absence of a subject rather than a way of framing one.
 */
static const struct {
  const char *icon;
  const char *name;
} cg_cameras[CAMERA_MODE_TOTAL + 1] = {
  { "pics/camera-first_person", "1st Person" },
  { "pics/camera-third_person", "3rd Person" },
  { "pics/camera-follow", "Follow" },
  { "pics/camera-spectate", "Free Flight" }
};

#pragma mark - View

/**
 * @see View::init(View *)
 */
static View *init(View *self) {

  self = (View *) super(StackView, self, initWithFrame, NULL);
  if (self) {

    CameraControlsView *this = (CameraControlsView *) self;

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("icon", &this->icon),
      MakeOutlet("name", &this->name)
    );

    $(self, awakeWithResourceName, "ui/hud/CameraControlsView.json");
    $(self, resolve, outlets);
  }

  return self;
}

/**
 * @see View::updateBindings(View *, ident)
 * @remarks The camera is announced rather than displayed: it appears when it changes and hides
 * itself again after `cg_selectWeaponInterval`, which is what the weapon bar lingers for.
 */
static void updateBindings(View *self, ident data) {

  super(View, self, updateBindings, data);

  CameraControlsView *this = (CameraControlsView *) self;

  if (data == NULL) {
    return;
  }

  const PlayerState *ps = &((const ClientFrame *) data)->ps;

  const bool detached = !Cg_CameraSubject(ps);

  if (detached != this->detached || cgState.cameraMode != this->mode) {

    this->detached = detached;
    this->mode = cgState.cameraMode;

    const size_t camera = detached ? CAMERA_MODE_TOTAL : this->mode;

    $(this->icon, setImage, (Image *) Cg_HudImage(cg_cameras[camera].icon));
    $(this->name, setText, cg_cameras[camera].name);

    this->time = cgi.client->unclampedTime + cg_selectWeaponInterval->integer;
  }

  const bool visible = cgi.client->unclampedTime < this->time;

  $(self, setVisibility, visible ? ViewVisibilityVisible : ViewVisibilityHidden);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewInterface *) clazz->interface)->init = init;
  ((ViewInterface *) clazz->interface)->updateBindings = updateBindings;
}

/**
 * @fn Class *CameraControlsView::_CameraControlsView(void)
 * @memberof CameraControlsView
 */
Class *_CameraControlsView(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CameraControlsView",
      .superclass = _StackView(),
      .instanceSize = sizeof(CameraControlsView),
      .interfaceSize = sizeof(CameraControlsViewInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
