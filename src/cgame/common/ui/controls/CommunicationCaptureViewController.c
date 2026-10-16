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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

#include "CommunicationCaptureViewController.h"

#define _Class _CommunicationCaptureViewController

#pragma mark - Delegates

/**
 * @brief TextViewDelegate callback for binding keys.
 */
static void didBindKey(TextView *textView) {

  const ViewController *this = textView->delegate.self;

  $(this->view, updateBindings, NULL);
}

/**
 * @brief ViewEnumerator for setting the TextViewDelegate on BindTextViews.
 */
static void setDelegate(View *view, ident data) {

  ((TextView *) view)->delegate = (TextViewDelegate) {
    .self = data,
    .didEndEditing = didBindKey
  };
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

  super(ViewController, self, loadView);

  $(self->view, awakeWithResourceName, "ui/controls/CommunicationCaptureViewController.json");

  self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/controls/CommunicationCaptureViewController.css");
  assert(self->view->stylesheet);

  $(self->view, enumerateSelection, "BindTextView", setDelegate, self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

  ((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *CommunicationCaptureViewController::_CommunicationCaptureViewController(void)
 * @memberof CommunicationCaptureViewController
 */
Class *_CommunicationCaptureViewController(void) {
  static Class *clazz;
  static Once once;

  do_once(&once, {
    clazz = _initialize(&(const ClassDef) {
      .name = "CommunicationCaptureViewController",
      .superclass = _ViewController(),
      .instanceSize = sizeof(CommunicationCaptureViewController),
      .interfaceSize = sizeof(CommunicationCaptureViewControllerInterface),
      .initialize = initialize,
    });
  });

  return clazz;
}

#undef _Class
