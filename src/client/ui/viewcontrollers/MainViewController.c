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

#include "MainViewController.h"
#include "../../client.h"

#define _Class _MainViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for Button.
 */
static void buttonClicked(ident sender, const SDL_Event *event, ident data) {
	Com_Print("It's alive!\n");
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	Button *button = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);
	$(button->title, setText, "Testing, testing 123");
	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, buttonClicked, NULL);

	$((View *) this->stackView, addSubview, (View *) button);

	release(button);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _MainViewController = {
	.name = "MainViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(MainViewController),
	.interfaceOffset = offsetof(MainViewController, interface),
	.interfaceSize = sizeof(MainViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

