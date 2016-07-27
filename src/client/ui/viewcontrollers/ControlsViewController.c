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

#include "../views/BindTextView.h"
#include "../views/CvarCheckbox.h"
#include "../views/CvarSlider.h"
#include "../views/MenuInput.h"

#include "client.h"

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

		$$(MenuInput, bindTextView, (View *) stackView, "+forward", "Forward");
		$$(MenuInput, bindTextView, (View *) stackView, "+back", "Back");
		$$(MenuInput, bindTextView, (View *) stackView, "+move_left", "Move left");
		$$(MenuInput, bindTextView, (View *) stackView, "+move_right", "Move right");
		$$(MenuInput, bindTextView, (View *) stackView, "+move_up", "Jump");
		$$(MenuInput, bindTextView, (View *) stackView, "+move_down", "Crouch");
		$$(MenuInput, bindTextView, (View *) stackView, "+left", "Turn left");
		$$(MenuInput, bindTextView, (View *) stackView, "+right", "Turn right");
		$$(MenuInput, bindTextView, (View *) stackView, "center_view", "Center view");
		$$(MenuInput, bindTextView, (View *) stackView, "+speed", "Run / walk");

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

		$$(MenuInput, bindTextView, (View *) stackView, "cl_message_mode", "Say");
		$$(MenuInput, bindTextView, (View *) stackView, "cl_message_mode_2", "Say Team");
		$$(MenuInput, bindTextView, (View *) stackView, "+SCORE", "Show score");
		$$(MenuInput, bindTextView, (View *) stackView, "r_screenshot", "Take screenshot");

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
		$$(MenuInput, cvarSlider, (View *) stackView, m_sensitivity, "Sensitivity", 0.1, 6.0, 0.1);

		extern cvar_t *m_sensitivity_zoom;
		$$(MenuInput, cvarSlider, (View *) stackView, m_sensitivity_zoom, "Zoom Sensitivity", 0.1, 6.0, 0.1);

		extern cvar_t *m_invert;
		$$(MenuInput, cvarCheckbox, (View *) stackView, m_invert, "Invert mouse");

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

		$$(MenuInput, bindTextView, (View *) stackView, "+attack", "Attack");
		$$(MenuInput, bindTextView, (View *) stackView, "weapon_next", "Next weapon");
		$$(MenuInput, bindTextView, (View *) stackView, "weapon_previous", "Previous weapon");
		$$(MenuInput, bindTextView, (View *) stackView, "+ZOOM", "Zoom");

		$$(MenuInput, bindTextView, (View *) stackView, "use blaster", "Blaster");
		$$(MenuInput, bindTextView, (View *) stackView, "use shotgun", "Shotgun");
		$$(MenuInput, bindTextView, (View *) stackView, "use super shotgun", "Super shotgun");
		$$(MenuInput, bindTextView, (View *) stackView, "use machinegun", "Machinegun");
		$$(MenuInput, bindTextView, (View *) stackView, "use grenades", "Hand grenades");
		$$(MenuInput, bindTextView, (View *) stackView, "use grenade launcher", "Grenade launcher");
		$$(MenuInput, bindTextView, (View *) stackView, "use rocket launcher", "Rocket launcher");
		$$(MenuInput, bindTextView, (View *) stackView, "use hyperblaster", "Hyperblaster");
		$$(MenuInput, bindTextView, (View *) stackView, "use lightning", "Lightning");
		$$(MenuInput, bindTextView, (View *) stackView, "use railgun", "Railgun");
		$$(MenuInput, bindTextView, (View *) stackView, "use bfg10k", "BFG-10K");

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

