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

#include "PlayerSetupViewController.h"

#include "ui_data.h"

#include "PlayerModelView.h"
#include "SkinSelect.h"

#define _Class _PlayerSetupViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->stackView->axis = StackViewAxisHorizontal;
	this->stackView->spacing = 10;

	StackView *leftColumn = $(alloc(StackView), initWithFrame, NULL);
	leftColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;

	StackView *rightColumn = $(alloc(StackView), initWithFrame, NULL);
	rightColumn->spacing = DEFAULT_MENU_STACKVIEW_VERTICAL_SPACING;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "PLAYER SETUP");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		extern cvar_t *name;
		Ui_CvarTextView((View *) stackView, "Name", name);

		Control *skinSelect = (Control *) $(alloc(SkinSelect), initWithFrame, NULL, ControlStyleDefault);
		Ui_Input((View *) stackView, "Player skin", skinSelect);

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) leftColumn, addSubview, (View *) box);
		release(box);
	}

	{
		const SDL_Rect frame = { .w = 400, .h = 500 };
		PlayerModelView *mesh = $(alloc(PlayerModelView), initWithFrame, &frame);

		$((View *) rightColumn, addSubview, (View *) mesh);
		release(mesh);
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

Class _PlayerSetupViewController = {
	.name = "PlayerSetupViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(PlayerSetupViewController),
	.interfaceOffset = offsetof(PlayerSetupViewController, interface),
	.interfaceSize = sizeof(PlayerSetupViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

