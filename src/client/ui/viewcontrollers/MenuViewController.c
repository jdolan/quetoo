/*
 * ObjectivelyMVC: MVC framework for OpenGL and SDL2 in c.
 * Copyright (C) 2014 Jay Dolan <jay@jaydolan.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "MenuViewController.h"

#define _Class _MenuViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MenuViewController *this = (MenuViewController *) self;

	release(this->stackView);

	super(Object, self, dealloc);
}

#pragma mark - MenuViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->frame.w = 400;
	self->view->frame.h = 300;
	self->view->frame.x = 200;
	self->view->frame.y = 100;
	self->view->backgroundColor = Colors.DefaultColor;
	self->view->backgroundColor.a = 192;
	self->view->borderWidth = 1;

	MenuViewController *this = (MenuViewController *) self;

	this->stackView = $(alloc(StackView), initWithFrame, NULL);
	this->stackView->view.autoresizingMask = ViewAutoresizingFill;
	this->stackView->spacing = 10;
	this->stackView->view.padding.top = this->stackView->view.padding.bottom = 10;
	this->stackView->view.padding.left = this->stackView->view.padding.right = 10;

	$(self->view, addSubview, (View *) this->stackView);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
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

