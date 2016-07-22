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

