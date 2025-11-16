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

#include "MainViewController.h"

#include "ControlsViewController.h"
#include "HomeViewController.h"
#include "PlayViewController.h"
#include "SettingsViewController.h"

#include "TeamsViewController.h"

#include "DialogViewController.h"

#define _Class _MainViewController

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for menu navigation.
 */
static void didClickNavigateViewController(Button *button) {

  MainViewController *this = button->delegate.self;

  Class *clazz = button->delegate.data;

  if (clazz) {
    ViewController *topViewController = $(this->navigationViewController, topViewController);
    if (topViewController && $((Object *) topViewController, isKindOfClass, clazz)) {
      return;
    }

    $(this->navigationViewController, popToRootViewController);
    $(this->navigationViewController, popViewController);

    ViewController *viewController = $((ViewController *) _alloc(clazz), init);
    $(this->navigationViewController, pushViewController, viewController);
    release(viewController);
  } else {
    Cg_Warn("Menu item does not provide a ViewController class\n");
  }
}

/**
 * @brief Quit the game.
 */
static void quit(ident data) {
  cgi.Cbuf("quit\n");
}

/**
 * @brief ButtonDelegate for Quit.
 */
static void didClickQuit(Button *button) {

  MainViewController *this = button->delegate.self;

  const Dialog dialog = {
    .message = "Are you sure you want to quit?",
    .ok = "Yes",
    .cancel = "No",
    .okFunction = quit
  };

  ViewController *viewController = (ViewController *) $(alloc(DialogViewController), initWithDialog, &dialog);
  $((ViewController *) this, addChildViewController, viewController);
}

/**
 * @brief Disconnect from the current game.
 */
static void disconnect(ident data) {
  cgi.Cbuf("disconnect\n");
}

/**
 * @brief ButtonDelegate for Disconnect.
 */
static void didClickDisconnect(Button *button) {

  MainViewController *this = button->delegate.self;

  const Dialog dialog = {
    .message = "Are you sure you want to disconnect?",
    .ok = "Yes",
    .cancel = "No",
    .okFunction = disconnect
  };

  ViewController *viewController = (ViewController *) $(alloc(DialogViewController), initWithDialog, &dialog);
  $((ViewController *) this, addChildViewController, viewController);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  MainViewController *this = (MainViewController *) self;

  release(this->mainView);
  release(this->navigationViewController);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  MainViewController *this = (MainViewController *) self;

  this->mainView = $(alloc(MainView), initWithFrame, NULL);
  assert(this->mainView);

  $(self, setView, (View *) this->mainView);

  $(this, primaryButton, "Home", &(const ButtonDelegate) {
    .didClick = didClickNavigateViewController,
    .self = self,
    .data = _HomeViewController()
  });

  $(this, primaryButton, "Play", &(const ButtonDelegate) {
    .didClick = didClickNavigateViewController,
    .self = self,
    .data = _PlayViewController()
  });

  $(this, primaryButton, "Controls", &(const ButtonDelegate) {
    .didClick = didClickNavigateViewController,
    .self = self,
    .data = _ControlsViewController()
  });

  $(this, primaryButton, "Settings", &(const ButtonDelegate) {
    .didClick = didClickNavigateViewController,
    .self = self,
    .data = _SettingsViewController()
  });

  $(this, primaryButton, "Quit", &(const ButtonDelegate) {
    .didClick = didClickQuit,
    .self = self
  });

  $(this, secondaryButton, "Teams", &(const ButtonDelegate) {
    .didClick = didClickNavigateViewController,
    .self = self,
    .data = _TeamsViewController()
  });

  $(this, secondaryButton, "Disconnect", &(const ButtonDelegate) {
    .didClick = didClickDisconnect,
    .self = self
  });

  $(self, addChildViewController, (ViewController *) this->navigationViewController);
  $(this->mainView->contentView, addSubview, this->navigationViewController->viewController.view);

  $(this, navigateToViewController, _HomeViewController());
}

#pragma mark - MainViewController

/**
 * @fn MainViewController *MainViewController::init(MainViewController *self)
 *
 * @memberof MainViewController
 */
static MainViewController *init(MainViewController *self) {

  self = (MainViewController *) super(ViewController, self, init);
  if (self) {
    self->navigationViewController = $(alloc(NavigationViewController), init);
    assert(self->navigationViewController);
  }
  return self;
}

/**
 * @fn void MainViewController::navigateToViewController(MainViewController *self, Class *clazz)
 * @memberof MainViewController
 */
static void navigateToViewController(MainViewController *self, Class *clazz) {

  assert(clazz);

  ViewController *topViewController = $(self->navigationViewController, topViewController);
  if (topViewController && $((Object *) topViewController, isKindOfClass, clazz)) {
    return;
  }

  $(self->navigationViewController, popToRootViewController);
  $(self->navigationViewController, popViewController);

  ViewController *viewController = $((ViewController *) _alloc(clazz), init);
  $(self->navigationViewController, pushViewController, viewController);
  release(viewController);
}

/**
 * @fn void MainViewController::primaryButton(MainViewController *self, const char *title, const ButtonDelegate *delegate)
 * @memberof MainViewController
 */
static void primaryButton(MainViewController *self, const char *title, const ButtonDelegate *delegate) {

  Button *button = $(alloc(Button), initWithTitle, title);
  assert(button);

  button->control.view.identifier = strdup(title);
  assert(button->control.view.identifier);

  button->delegate = *delegate;

  $((View *) self->mainView->primaryMenu, addSubview, (View *) button);
  release(button);
}

/**
 * @fn void MainViewController::secondaryButton(MainViewController *self, const char *title, const ButtonDelegate *delegate)
 * @memberof MainViewController
 */
static void secondaryButton(MainViewController *self, const char *title, const ButtonDelegate *delegate) {

  Button *button = $(alloc(Button), initWithTitle, title);
  assert(button);

  button->delegate = *delegate;

  $((View *) self->mainView->secondaryMenu, addSubview, (View *) button);
  release(button);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((MainViewControllerInterface *) clazz->interface)->init = init;
  ((MainViewControllerInterface *) clazz->interface)->navigateToViewController = navigateToViewController;
  ((MainViewControllerInterface *) clazz->interface)->primaryButton = primaryButton;
  ((MainViewControllerInterface *) clazz->interface)->secondaryButton = secondaryButton;
}

/**
 * @fn Class *MainViewController::_MainViewController(void)
 * @memberof MainViewController
 */
Class *_MainViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "MainViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(MainViewController),
      .interfaceOffset = offsetof(MainViewController, interface),
      .interfaceSize = sizeof(MainViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
