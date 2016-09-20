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

#include <assert.h>

#include "MenuViewController.h"

#define _Class _MenuViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MenuViewController *this = (MenuViewController *) self;

	release(this->panel);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->panel = alloc(Panel, initWithFrame, NULL);
	assert(this->panel);

	this->panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
	this->panel->stackView.view.needsLayout = true;

	$(self->view, addSubview, (View *) this->panel);
}

#pragma mark - MenuViewController

/**
 * @fn MainViewController *MenuViewController::mainViewController(const MenuViewController *self)
 *
 * @memberof MenuViewController
 */
static MainViewController *mainViewController(const MenuViewController *self) {
	return (MainViewController *) ((ViewController *) self)->parentViewController;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((MenuViewControllerInterface *) clazz->interface)->mainViewController = mainViewController;
}

Class _MenuViewController = {
	.name = "MenuViewController",
	.superclass = &_ViewController,
	.instanceSize = sizeof(MenuViewController),
	.interfaceOffset = offsetof(MenuViewController, interface),
	.interfaceSize = sizeof(MenuViewControllerInterface),
	.initialize = initialize,
};

#undef _Class
