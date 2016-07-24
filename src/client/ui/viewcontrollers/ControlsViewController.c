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

#include "ControlsViewController.h"

#include "../views/BindInput.h"
#include "../views/VariableCheckboxInput.h"
#include "../views/VariableSliderInput.h"

#include "client.h"

#define _Class _ControlsViewController

#pragma mark - ViewController

/**
 * @brief Adds a new BindInput to the given StackView.
 */
static void addBindInput(StackView *stackView, const char *bind, const char *name) {

	BindInput *input = $(alloc(BindInput), initWithBind, bind, name);
	assert(input);

	$((View *) stackView, addSubview, (View *) input);

	release(input);
}

/**
 * @brief Adds a new SliderVariableInput to the given StackView.
 */
static void addSliderVariableInput(StackView *stackView, cvar_t *var,
		const char *name, double min, double max, double step) {

	VariableSliderInput *input = $(alloc(VariableSliderInput), initWithVariable, var, name, min, max, step);
	assert(input);

	$((View *) stackView, addSubview, (View *) input);

	release(input);
}

/**
 * @brief Adds a new CheckboxVariableInput to the given StackView.
 */
static void addCheckboxVariableInput(StackView *stackView, cvar_t *var, const char *name) {

	VariableCheckboxInput *input = $(alloc(VariableCheckboxInput), initWithVariable, var, name);
	assert(input);

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
	this->stackView->spacing = DEAFULT_MENU_STACKVIEW_HORIZONTAL_SPACING;

	StackView *leftColumn = $(alloc(StackView), initWithFrame, NULL);
	leftColumn->view.autoresizingMask = ViewAutoresizingHeight;

	StackView *rightColumn = $(alloc(StackView), initWithFrame, NULL);
	rightColumn->view.autoresizingMask = ViewAutoresizingHeight;

	leftColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;
	rightColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "MOVEMENT");

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
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "AIM");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		extern cvar_t *m_sensitivity;
		addSliderVariableInput(stackView, m_sensitivity, "Sensitivity", 0.1, 6.0, 0.1);

		extern cvar_t *m_sensitivity_zoom;
		addSliderVariableInput(stackView, m_sensitivity_zoom, "Zoom Sensitivity", 0.1, 6.0, 0.1);

		extern cvar_t *m_invert;
		addCheckboxVariableInput(stackView, m_invert, "Invert mouse");

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) rightColumn, addSubview, (View *) box);
		release(box);
	}

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "COMBAT");

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

