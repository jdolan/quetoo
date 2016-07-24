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

	this->stackView->axis = StackViewAxisHorizontal;

	StackView *leftColumn = $(alloc(StackView), initWithFrame, NULL);
	leftColumn->spacing = DEFAULT_MENU_STACKVIEW_SPACING;

	StackView *rightColumn = $(alloc(StackView), initWithFrame, NULL);
	rightColumn->spacing = DEFAULT_MENU_STACKVIEW_SPACING;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Movement");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		addBindInput(stackView, "+forward", "Forward");
		addBindInput(stackView, "+back", "Back");
		addBindInput(stackView, "+move_left", "Move left");
		addBindInput(stackView, "+move_right", "Move right");
		addBindInput(stackView, "+left", "Turn left");
		addBindInput(stackView, "+right", "Turn right");
		addBindInput(stackView, "+move_up", "Jump");
		addBindInput(stackView, "+move_down", "Crouch");
		addBindInput(stackView, "+speed", "Run / walk");

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) leftColumn, addSubview, (View *) box);
		release(box);
	}

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Aim");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) rightColumn, addSubview, (View *) box);
		release(box);
	}

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Weapons");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		addBindInput(stackView, "+attack", "Attack");
		addBindInput(stackView, "weapon_next", "Next weapon");
		addBindInput(stackView, "weapon_previous", "Previous weapon");
		addBindInput(stackView, "+ZOOM", "Zoom");

		addBindInput(stackView, "use blaster", "Blaster");
		addBindInput(stackView, "use shotgun", "Shotgun");
		addBindInput(stackView, "use super shotgun", "Super shotgun");
		addBindInput(stackView, "use machinegun", "Machinegun");
		addBindInput(stackView, "use grenades", "Hand grenades");
		addBindInput(stackView, "use grenade launcher", "Grenade launcher");
		addBindInput(stackView, "use rocket launcher", "Rocket launcher");
		addBindInput(stackView, "use hyperblaster", "Hyperblaster");
		addBindInput(stackView, "use lightning", "Lightning");
		addBindInput(stackView, "use railgun", "Railgun");
		addBindInput(stackView, "use bfg10k", "BFG-10K");

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) rightColumn, addSubview, (View *) box);
		release(box);
	}

	$((View *) leftColumn, sizeToFit);

	$((View *) this->stackView, addSubview, (View *) leftColumn);
	release(leftColumn);

	$((View *) rightColumn, sizeToFit);

	$((View *) this->stackView, addSubview, (View *) rightColumn);
	release(rightColumn);

	$((View *) this->stackView, sizeToFit);
	$((View *) this->panel, sizeToFit);
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

