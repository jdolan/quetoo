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

#include "SystemViewController.h"

#define _Class _SystemViewController

#pragma mark - Delegates

/**
 * @brief SelectDelegate callback for Video Mode.
 */
static void didSelecVideoMode(Select *select, Option *option) {

  const SDL_DisplayMode *mode = option->value;
  if (mode) {
    if (mode->w != cgi.context->window_bounds.w || mode->h != cgi.context->window_bounds.h) {
      cgi.SetCvarInteger("r_window_width", mode->w);
      cgi.SetCvarInteger("r_window_height", mode->h);
    }
  }
}

/**
 * @brief ButtonDelegate for the Apply button.
 */
static void didClickApply(Button *button) {
  cgi.Cbuf("r_restart; s_restart");
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  Select *videoMode, *windowMode, *verticalSync, *anisotropy;

  Button *apply;

  Outlet outlets[] = MakeOutlets(
    MakeOutlet("videoMode", &videoMode),
    MakeOutlet("windowMode", &windowMode),
    MakeOutlet("verticalSync", &verticalSync),
    MakeOutlet("anisotropy", &anisotropy),
    MakeOutlet("apply", &apply)
  );

  $(self->view, awakeWithResourceName, "ui/settings/SystemViewController.json");
  $(self->view, resolve, outlets);

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/settings/SystemViewController.css");
  assert(self->view->stylesheet);

  videoMode->delegate.self = self;
  videoMode->delegate.didSelectOption = didSelecVideoMode;

  $(windowMode, addOption, "Windowed", (ident) 0);
  $(windowMode, addOption, "Fullscreen", (ident) 1);
  $(windowMode, addOption, "Borderless", (ident) 2);

  $(verticalSync, addOption, "Off", (ident) 0);
  $(verticalSync, addOption, "On", (ident) 1);
  $(verticalSync, addOption, "Immediate", (ident) -1);

  $(anisotropy, addOption, "Off", (ident) 0);
  $(anisotropy, addOption, "2x", (ident) 2);
  $(anisotropy, addOption, "4x", (ident) 4);
  $(anisotropy, addOption, "8x", (ident) 8);
  $(anisotropy, addOption, "16x", (ident) 16);

  apply->delegate.didClick = didClickApply;
  apply->delegate.self = self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *SystemViewController::_SystemViewController(void)
 * @memberof SystemViewController
 */
Class *_SystemViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "SystemViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(SystemViewController),
      .interfaceOffset = offsetof(SystemViewController, interface),
      .interfaceSize = sizeof(SystemViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
