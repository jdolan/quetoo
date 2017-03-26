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

#include "cg_local.h"

#include "MainViewController.h"

#include "CreateServerViewController.h"
#include "MultiplayerViewController.h"
#include "PlayViewController.h"
#include "PlayerViewController.h"
#include "SettingsViewController.h"

#include "PrimaryButton.h"

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
		cgi.Cbuf("quit\n");
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);

	((View *) panel)->backgroundColor = QColors.Main;

	panel->isDraggable = false;
	panel->isResizable = false;

	panel->stackView.distribution = StackViewAxisHorizontal;

	panel->stackView.view.alignment = ViewAlignmentTopLeft;
	panel->stackView.view.autoresizingMask = ViewAutoresizingNone;

	{
		panel->contentView->axis = StackViewAxisHorizontal;
		panel->contentView->distribution = StackViewDistributionDefault;

		panel->contentView->view.alignment = ViewAlignmentTopLeft;

		char name[MAX_STRING_CHARS];
		StripColors(cgi.CvarGet("name")->string, name);

		Cg_PrimaryButton((View *) panel->contentView, name, ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayerViewController());
	}

	{
		panel->accessoryView->axis = StackViewAxisHorizontal;
		panel->accessoryView->distribution = StackViewDistributionDefault;

		panel->accessoryView->view.alignment = ViewAlignmentTopRight;

		Cg_PrimaryButton((View *) panel->accessoryView, "PLAY", ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayViewController());

		Cg_PrimaryIcon((View *) panel->accessoryView, "ui/pics/settings", ViewAlignmentTopRight, QColors.Border, action, self, _SettingsViewController());
		Cg_PrimaryIcon((View *) panel->accessoryView, "ui/pics/quit", ViewAlignmentTopRight,  QColors.Border,action, self, NULL);
	}

	SDL_Size size = MakeSize(cgi.context->window_width, 36);
	$((View *) (StackView *) panel, resize, &size);

	panel->accessoryView->view.hidden = false;

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

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((MainViewControllerInterface *) clazz->def->interface)->init = init;
}

/**
 * @fn Class *MainViewController::_MainViewController(void)
 * @memberof MainViewController
 */
Class *_MainViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MainViewController";
		clazz.superclass = _NavigationViewController();
		clazz.instanceSize = sizeof(MainViewController);
		clazz.interfaceOffset = offsetof(MainViewController, interface);
		clazz.interfaceSize = sizeof(MainViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
