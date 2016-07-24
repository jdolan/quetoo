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
	this->panel->view.autoresizingMask = ViewAutoresizingContain;

	this->stackView = $(alloc(StackView), initWithFrame, NULL);
	assert(this->stackView);

	this->stackView->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;
	this->stackView->view.autoresizingMask = ViewAutoresizingContain;

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

