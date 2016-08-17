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

#include "ui_data.h"

#define _Class _ControlsViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->stackView->axis = StackViewAxisHorizontal;
	this->stackView->spacing = DEAFULT_MENU_STACKVIEW_HORIZONTAL_SPACING;

	StackView *leftColumn = $(alloc(StackView), initWithFrame, NULL);
	StackView *rightColumn = $(alloc(StackView), initWithFrame, NULL);

	leftColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;
	rightColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "MOVEMENT");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Ui_Bind((View *) stackView, "Forward", "+forward");
		Ui_Bind((View *) stackView, "Back", "+back");
		Ui_Bind((View *) stackView, "Move left", "+move_left");
		Ui_Bind((View *) stackView, "Move right", "+move_right");
		Ui_Bind((View *) stackView, "Jump", "+move_up");
		Ui_Bind((View *) stackView, "Crouch", "+move_down");
		Ui_Bind((View *) stackView, "Turn left", "+left");
		Ui_Bind((View *) stackView, "Turn right", "+right");
		Ui_Bind((View *) stackView, "Center view", "center_view");
		Ui_Bind((View *) stackView, "Run / walk", "+speed");

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

		$(box->label, setText, "COMMUNICATIONS");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Ui_Bind((View *) stackView, "Say", "cl_message_mode");
		Ui_Bind((View *) stackView, "Say Team", "cl_message_mode_2");
		Ui_Bind((View *) stackView, "Show score", "+SCORE");
		Ui_Bind((View *) stackView, "Take screenshot", "r_screenshot");

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
		extern cvar_t *m_sensitivity_zoom;
		extern cvar_t *m_invert;

		Ui_CvarSlider((View *) stackView, "Sensitivity", m_sensitivity, 0.1, 6.0, 0.1);
		Ui_CvarSlider((View *) stackView, "Zoom Sensitivity", m_sensitivity_zoom, 0.1, 6.0, 0.1);
		Ui_CvarCheckbox((View *) stackView, "Invert mouse", m_invert);

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

		Ui_Bind((View *) stackView, "Attack", "+attack");
		Ui_Bind((View *) stackView, "Next weapon", "weapon_next");
		Ui_Bind((View *) stackView, "Previous weapon", "weapon_previous");
		Ui_Bind((View *) stackView, "Zoom", "+ZOOM");

		Ui_Bind((View *) stackView, "Blaster", "use blaster");
		Ui_Bind((View *) stackView, "Shotgun", "use shotgun");
		Ui_Bind((View *) stackView, "Super shotgun", "use super shotgun");
		Ui_Bind((View *) stackView, "Machinegun", "use machinegun");
		Ui_Bind((View *) stackView, "Hand grenades", "use grenades");
		Ui_Bind((View *) stackView, "Grenade launcher", "use grenade launcher");
		Ui_Bind((View *) stackView, "Rocket launcher", "use rocket launcher");
		Ui_Bind((View *) stackView, "Hyperblaster", "use hyperblaster");
		Ui_Bind((View *) stackView, "Lightning", "use lightning");
		Ui_Bind((View *) stackView, "Railgun", "use railgun");
		Ui_Bind((View *) stackView, "BFG-10K", "use bfg10k");

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

