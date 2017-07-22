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
 * @see ViewController::addChildViewController(ViewController *, ViewController *)
 */
static void addChildViewController(ViewController *self, ViewController *childViewController) {

	super(ViewController, self, addChildViewController, childViewController);

	MenuViewController *this = (MenuViewController *) self;

	$((View *) this->panel->contentView, addSubview, childViewController->view);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->panel = $(alloc(Panel), initWithFrame, NULL);
	assert(this->panel);

	this->panel->isResizable = false;
	this->panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
//	this->panel->stackView.view.backgroundColor = QColors.Main;
//	this->panel->stackView.view.borderColor = QColors.BorderLight;

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

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->addChildViewController = addChildViewController;
	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((MenuViewControllerInterface *) clazz->def->interface)->mainViewController = mainViewController;
}

/**
 * @fn Class *MenuViewController::_MenuViewController(void)
 * @memberof MenuViewController
 */
Class *_MenuViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MenuViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(MenuViewController);
		clazz.interfaceOffset = offsetof(MenuViewController, interface);
		clazz.interfaceSize = sizeof(MenuViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
