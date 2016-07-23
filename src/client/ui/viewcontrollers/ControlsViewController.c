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

#include "ControlsViewController.h"

#include "../views/BindInput.h"

#include "client.h"

extern cl_static_t cls;

#define _Class _ControlsViewController

#pragma mark - ViewController

/**
 * @brief Adds a new BindInput to the given StackView.
 */
static void addBindInput(StackView *stackView, const char *bind, const char *name) {

	BindInput *input = $(alloc(BindInput), initWithBind, bind, name);

	$((View *) stackView, addSubview, (View *) input);

	release(input);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	addBindInput(this->stackView, "+forward", "Forward");
	addBindInput(this->stackView, "+back", "Back");
	addBindInput(this->stackView, "+move_left", "Move left");
	addBindInput(this->stackView, "+move_right", "Move right");
	addBindInput(this->stackView, "+left", "Turn left");
	addBindInput(this->stackView, "+right", "Turn right");
	addBindInput(this->stackView, "+move_up", "Jump");
	addBindInput(this->stackView, "+move_down", "Crouch");
	addBindInput(this->stackView, "+speed", "Run / walk");

	addBindInput(this->stackView, "+attack", "Attack");
	addBindInput(this->stackView, "weapon_next", "Next weapon");
	addBindInput(this->stackView, "weapon_previous", "Previous weapon");
	addBindInput(this->stackView, "+ZOOM", "Zoom");

	addBindInput(this->stackView, "use blaster", "Blaster");
	addBindInput(this->stackView, "use shotgun", "Shotgun");
	addBindInput(this->stackView, "use super shotgun", "Super shotgun");
	addBindInput(this->stackView, "use machinegun", "Machinegun");
	addBindInput(this->stackView, "use grenades", "Hand grenades");
	addBindInput(this->stackView, "use grenade launcher", "Grenade launcher");
	addBindInput(this->stackView, "use rocket launcher", "Rocket launcher");
	addBindInput(this->stackView, "use hyperblaster", "Hyperblaster");
	addBindInput(this->stackView, "use lightning", "Lightning");
	addBindInput(this->stackView, "use railgun", "Railgun");
	addBindInput(this->stackView, "use bfg10k", "BFG-10K");

	$((View *) this->stackView, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _ControlsViewController = {
	.name = "ControlsViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(ControlsViewController),
	.interfaceOffset = offsetof(ControlsViewController, interface),
	.interfaceSize = sizeof(ControlsViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

