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

#include "ui_local.h"
#include "cl_local.h"

#include "ConsoleViewController.h"

#define _Class _ConsoleViewController

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

  ConsoleViewController *this = (ConsoleViewController *) self;

  release(this->console);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  self->view->pointerEvents = false;

  ConsoleViewController *this = (ConsoleViewController *) self;

  this->console = (ConsoleView *) $((View *) alloc(ConsoleView), init);
  assert(this->console);

  $(self->view, addSubview, (View *) this->console);
}

#pragma mark - ConsoleViewController

/**
 * @fn void ConsoleViewController::update(ConsoleViewController *self)
 * @memberof ConsoleViewController
 */
static void update(ConsoleViewController *self) {

  View *view = (View *) self->console;

  const bool console = cls.key_state.dest == KEY_CONSOLE && cls.state != CL_LOADING;

  $(view, setHidden, !console);

  if (console) {
    const int32_t height = self->viewController.view->frame.h;
    const float fraction = cls.state == CL_ACTIVE ? Clampf01(cl_console_height->value) : 1.f;

    $(self->console, update, height * fraction);
  }
}

#pragma mark - Class lifecycle

static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;

  ((ConsoleViewControllerInterface *) clazz->interface)->update = update;
}

/**
 * @fn Class *ConsoleViewController::_ConsoleViewController(void)
 * @memberof ConsoleViewController
 */
Class *_ConsoleViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "ConsoleViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(ConsoleViewController),
      .interfaceSize = sizeof(ConsoleViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
