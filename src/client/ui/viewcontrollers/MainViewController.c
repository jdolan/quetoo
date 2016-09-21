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

#include "KeysViewController.h"
#include "MouseViewController.h"
#include "MultiplayerViewController.h"
#include "PlayerViewController.h"
#include "SystemViewController.h"

#include "PrimaryButton.h"

#include "client.h"

#define _Class _MainViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for main menu PrimaryButtons.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	NavigationViewController *this = (NavigationViewController *) sender;
	Class *clazz = (Class *) data;
	if (clazz) {
		$(this, popToRootViewController);
		
		ViewController *viewController = $((ViewController *) _alloc(clazz), init);

		$(this, pushViewController, viewController);

		release(viewController);

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

	Panel *panel = alloc(Panel, initWithFrame, NULL);

	panel->isDraggable = false;
	panel->isResizable = false;

	panel->stackView.view.alignment = ViewAlignmentTopCenter;

	panel->contentView->axis = StackViewAxisHorizontal;
	panel->contentView->distribution = StackViewDistributionFillEqually;
	panel->contentView->view.autoresizingMask = ViewAutoresizingNone;

	Ui_PrimaryButton((View *) panel->contentView, "MULTIPLAYER", action, self, &_MultiplayerViewController);
	Ui_PrimaryButton((View *) panel->contentView, "KEYS", action, self, &_KeysViewController);
	Ui_PrimaryButton((View *) panel->contentView, "MOUSE", action, self, &_MouseViewController);
	Ui_PrimaryButton((View *) panel->contentView, "PLAYER", action, self, &_PlayerViewController);
	Ui_PrimaryButton((View *) panel->contentView, "SYSTEM", action, self, &_SystemViewController);
	Ui_PrimaryButton((View *) panel->contentView, "QUIT", action, self, NULL);

	SDL_Size size = MakeSize(min(r_context.window_width, 1024), 36);
	$((View *) panel->contentView, resize, &size);

	$((View *) panel, sizeToFit);

	$(self->view, addSubview, (View *) panel);
	release(panel);
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
	.superclass = &_NavigationViewController,
	.instanceSize = sizeof(MainViewController),
	.interfaceOffset = offsetof(MainViewController, interface),
	.interfaceSize = sizeof(MainViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

