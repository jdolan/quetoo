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

#include "HomeViewController.h"

#include "DialogViewController.h"
#include "LeaderboardViewController.h"
#include "StatsViewController.h"

#define _Class _HomeViewController

#pragma mark - Delegates

/**
 * @brief Dialog::okFunction for opening the releases page.
 */
static void openReleasesPage(ident data) {
  SDL_OpenURL(QUETOO_RELEASES_URL);
}

#pragma mark - Object

static void dealloc(Object *self) {

  HomeViewController *this = (HomeViewController *) self;

  release(this->tabViewController);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  HomeViewController *this = (HomeViewController *) self;

  View *view = $$(View, viewWithResourceName, "ui/home/HomeViewController.json", NULL);
  assert(view);

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/home/HomeViewController.css");
  assert(view->stylesheet);

  $(self, setView, view);
  release(view);

  this->tabViewController = $(alloc(TabViewController), init);
  assert(this->tabViewController);

  ViewController *viewController, *tabViewController = (ViewController *) this->tabViewController;

  viewController = $((ViewController *) alloc(StatsViewController), init);
  $(tabViewController, addChildViewController, viewController);
  release(viewController);

  viewController = $((ViewController *) alloc(LeaderboardViewController), init);
  $(tabViewController, addChildViewController, viewController);
  release(viewController);

  $(self, addChildViewController, tabViewController);
  $((View *) ((Panel *) view)->contentView, addSubview, tabViewController->view);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

  super(ViewController, self, viewWillAppear);

  if (Cg_IsUpdateAvailable()) {
    const Dialog dialog = {
      .message = "A new version of Quetoo is available. Download now?",
      .ok = "Yes",
      .cancel = "No",
      .okFunction = openReleasesPage
    };

    ViewController *viewController = (ViewController *) $(alloc(DialogViewController), initWithDialog, &dialog);
    $(self, addChildViewController, viewController);
    release(viewController);
  }
}

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
  ((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @memberof HomeViewController
 */
Class *_HomeViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "HomeViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(HomeViewController),
      .interfaceOffset = offsetof(HomeViewController, interface),
      .interfaceSize = sizeof(HomeViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
