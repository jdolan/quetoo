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

#include <assert.h>

#include "MenuViewController.h"

#define _Class _MenuViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MenuViewController *this = (MenuViewController *) self;

	release(this->panel);

	release(this->stackView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	const SDL_Rect frame = { .w = 400, .h = 300 };

	this->panel = $(alloc(Panel), initWithFrame, &frame);
	assert(this->panel);

	this->panel->view.alignment = ViewAlignmentMiddleCenter;

	this->stackView = $(alloc(StackView), initWithFrame, NULL);
	assert(this->stackView);

	this->stackView->spacing = DEFAULT_MENU_STACKVIEW_SPACING;

	$((View *) this->panel, addSubview, (View *) this->stackView);

	$(self->view, addSubview, (View *) this->panel);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;
	
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

