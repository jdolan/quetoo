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

#include "ControlsViewController.h"
#include "MultiplayerViewController.h"
#include "PlayerSetupViewController.h"
#include "SettingsViewController.h"

#include "../views/PrimaryButton.h"

#include "client.h"

#define _Class _MainViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for main menu PrimaryButtons.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	ViewController *this = (ViewController *) sender;
	Class *clazz = (Class *) data;
	if (clazz) {

		ViewController *viewController = NULL;

		Array *childViewControllers = (Array *) this->childViewControllers;
		for (size_t i = 0; i < childViewControllers->count; i++) {

			ViewController *childViewController = $(childViewControllers, objectAtIndex, i);
			childViewController->view->hidden = true;

			if ($((Object *) childViewController, isKindOfClass, clazz)) {
				viewController = childViewController;
			}
		}

		if (viewController == NULL) {
			viewController = $((ViewController *) _alloc(clazz), init);
			$(viewController, moveToParentViewController, this);
		}

		viewController->view->hidden = false;
	} else {
		Cbuf_AddText("quit\n");
		Cbuf_Execute();
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->stackView->axis = StackViewAxisHorizontal;
	this->stackView->distribution = StackViewDistributionFillEqually;

	this->stackView->view.frame.w = min(r_context.window_width, 1024);

	this->panel->isDraggable = false;
	this->panel->isResizable = false;

	this->panel->view.alignment = ViewAlignmentTopCenter;

	$$(PrimaryButton, button, (View *) this->stackView, "MULTIPLAYER", action, this, &_MultiplayerViewController);
	$$(PrimaryButton, button, (View *) this->stackView, "CONTROLS", action, this, &_ControlsViewController);
	$$(PrimaryButton, button, (View *) this->stackView, "PLAYER SETUP", action, this, &_PlayerSetupViewController);
	$$(PrimaryButton, button, (View *) this->stackView, "SETTINGS", action, this, &_SettingsViewController);
	$$(PrimaryButton, button, (View *) this->stackView, "QUIT", action, this, NULL);

//	$((View *) this->stackView, sizeToFit);
	$((View *) this->panel, sizeToFit);
}

#pragma mark - MainViewController

/**
 * @fn MainViewController *MainViewController::init(MainViewController *self)
 *
 * @memberof MainViewController
 */
static MainViewController *init(MainViewController *self) {

	return (MainViewController *) super(ViewController, self, init);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((MainViewControllerInterface *) clazz->interface)->init = init;
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

