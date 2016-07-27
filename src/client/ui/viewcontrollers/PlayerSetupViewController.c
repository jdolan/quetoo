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

#include "../views/MenuInput.h"
#include "../views/SkinSelect.h"

#include "client.h"

#define _Class _PlayerSetupViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "PLAYER SETUP");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Control *skin = (Control *) $(alloc(SkinSelect), initWithFrame, NULL, ControlStyleDefault);
		$$(MenuInput, input, (View *) stackView, skin, "Player skin");

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) this->stackView, addSubview, (View *) box);
		release(box);
	}

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

