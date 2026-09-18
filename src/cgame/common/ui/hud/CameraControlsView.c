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

#define _Class _CameraControlsView

#pragma mark - Delegates

/**
 * @brief ButtonDelegate: cycles first-person, third-person orbit, and free-flight cameras.
 */
static void didClickCameraMode(Button *button) {
  cgi.Cbuf("camera_mode_cycle\n");
}

#pragma mark - CameraControlsView

/**
 * @fn CameraControlsView *CameraControlsView::initWithFrame(CameraControlsView *self, const SDL_Rect *frame)
 * @memberof CameraControlsView
 */
static CameraControlsView *initWithFrame(CameraControlsView *self, const SDL_Rect *frame) {

  self = (CameraControlsView *) super(StackView, self, initWithFrame, frame);
  if (self) {

    Outlet outlets[] = MakeOutlets(
      MakeOutlet("cameraMode", &self->cameraModeButton)
    );

    View *this = (View *) self;

    $(this, awakeWithResourceName, "ui/hud/CameraControlsView.json");
    $(this, resolve, outlets);

    self->cameraModeButton->delegate.self = self;
    self->cameraModeButton->delegate.didClick = didClickCameraMode;
  }

  return self;
}

/**
 * @fn void CameraControlsView::update(CameraControlsView *self)
 * @memberof CameraControlsView
 */
static void update(CameraControlsView *self) {

  const char *label;

  if (cgi.client->demo_server) {
    switch (cg_state.demo_camera_mode) {
      case CAMERA_FIRST_PERSON:
        label = "1st Person";
        break;
      case CAMERA_THIRD_PERSON:
        label = "3rd Person";
        break;
      default:
        label = "Free Flight";
        break;
    }
  } else {
    const player_state_t *ps = &cgi.client->frame.ps;

    if (!ps->stats[STAT_CHASE]) {
      label = "Free Flight";
    } else if (cg_third_person_chasecam->value) {
      label = "3rd Person";
    } else {
      label = "1st Person";
    }
  }

  $(self->cameraModeButton->title, setText, label);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((CameraControlsViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
  ((CameraControlsViewInterface *) clazz->interface)->update = update;
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
