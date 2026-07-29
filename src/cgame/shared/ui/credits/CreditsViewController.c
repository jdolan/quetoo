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

#include "CreditsViewController.h"

#define _Class _CreditsViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  View *view = $$(View, viewWithResourceName, "ui/credits/CreditsViewController.json", NULL);
  assert(view);

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/credits/CreditsViewController.css");
  assert(view->stylesheet);

  $(self, setView, view);
  release(view);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *CreditsViewController::_CreditsViewController(void)
 * @memberof CreditsViewController
 */
Class *_CreditsViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CreditsViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(CreditsViewController),
      .interfaceOffset = offsetof(CreditsViewController, interface),
      .interfaceSize = sizeof(CreditsViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class

