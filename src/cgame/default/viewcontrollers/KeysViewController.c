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

#include "KeysViewController.h"

#include "BindTextView.h"

#define _Class _KeysViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	$((View *) this->panel, removeFromSuperview);
	release(this->panel);

	this->panel = (Panel *) Cg_LoadView(_Class.name, NULL);
	$(self->view, addSubview, (View *) this->panel);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	_initialize(&_BindTextView);

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _KeysViewController = {
	.name = "KeysViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(KeysViewController),
	.interfaceOffset = offsetof(KeysViewController, interface),
	.interfaceSize = sizeof(KeysViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

