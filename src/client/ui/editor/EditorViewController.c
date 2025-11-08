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

#include "EditorViewController.h"
#include "EntityViewController.h"
#include "MaterialViewController.h"

#pragma mark - Delegates

/**
 * @brief ButtonDelegate for Save .map and .mat.
 */
static void didSave(Button *button) {

  Cbuf_AddText("save_editor_map\n");

  EditorViewController *this = button->delegate.self;

  const r_model_t *model = this->materialViewController->model;
  if (model) {
    Cmd_ExecuteString(va("r_save_materials %s", model->media.name));
  }
}

#define _Class _EditorViewController

#pragma mark - Object

static void dealloc(Object *self) {

  EditorViewController *this = (EditorViewController *) self;

  release(this->tabViewController);
  release(this->entityViewController);
  release(this->materialViewController);

  super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  EditorViewController *this = (EditorViewController *) self;

  View *view = $$(View, viewWithResourceName, "ui/editor/EditorViewController.json", NULL);
  assert(view);

  view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/editor.css");
  assert(view->stylesheet);

  $(self, setView, view);
  release(view);

  this->tabViewController = $(alloc(TabViewController), init);
  assert(this->tabViewController);

  this->entityViewController = $(alloc(EntityViewController), init);
  assert(this->entityViewController);

  this->materialViewController = $(alloc(MaterialViewController), init);
  assert(this->materialViewController);

  ViewController *tabViewController = (ViewController *) this->tabViewController;

  $(tabViewController, addChildViewController, (ViewController *) this->entityViewController);
  $(tabViewController, addChildViewController, (ViewController *) this->materialViewController);

  $(self, addChildViewController, tabViewController);
  $((View *) ((Panel *) view)->contentView, addSubview, tabViewController->view);

  Outlet outlets[] = MakeOutlets(MakeOutlet("save", &this->save));
  $(self->view, resolve, outlets);

  this->save->delegate.self = this;
  this->save->delegate.didClick = didSave;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ObjectInterface *) clazz->interface)->dealloc = dealloc;

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *EditorViewController::_EditorViewController(void)
 * @memberof EditorViewController
 */
Class *_EditorViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "EditorViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(EditorViewController),
      .interfaceOffset = offsetof(EditorViewController, interface),
      .interfaceSize = sizeof(EditorViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
