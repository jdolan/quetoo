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

#include <ObjectivelyMVC/View.h>

#include "MainViewController.h"

#include "HomeViewController.h"
#include "InfoViewController.h"
#include "PlayViewController.h"
#include "PlayerViewController.h"
#include "SettingsViewController.h"

#include "MainView.h"

#define _Class _MainViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MainViewController *this = (MainViewController *) self;

	release(this->mainView);

	super(Object, self, dealloc);
}

#pragma mark - Actions

/**
 * @brief Quit the game.
 */
static void quit(void) {
	cgi.Cbuf("quit\n");
}

/**
 * @brief Disconnect from the current game.
 */
static void disconnect(void) {
	cgi.Cbuf("disconnect\n");
}

/**
 * @brief ActionFunction for main menu PrimaryButtons.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	MainViewController *self = (MainViewController *) sender;

	Class *clazz = (Class *) data;

	if (clazz) {
		$(self->navigationViewController, popToRootViewController);
		$(self->navigationViewController, popViewController);

		ViewController *viewController = $((ViewController *) _alloc(clazz), init);

		$(self->navigationViewController, pushViewController, viewController);

		release(viewController);
	} else {
		if (self->state >= CL_CONNECTED) {
			$(self->mainView->dialog, showDialog, "Are you sure you want to disconnect?", "No", "Yes", disconnect);
		} else {
			$(self->mainView->dialog, showDialog, "Are you sure you want to quit?", "No", "Yes", quit);
		}
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MainViewController *this = (MainViewController *) self;

	this->mainView = $(alloc(MainView), initWithFrame, NULL);
	assert(self->view);

	View *topBar = (View *) this->mainView->topBar;

	Cgui_PrimaryButton(topBar, "HOME", QColors.Dark, action, self, _HomeViewController());
	Cgui_PrimaryButton(topBar, "PROFILE", QColors.Dark, action, self, _PlayerViewController());
	Cgui_PrimaryButton(topBar, "PLAY", QColors.Theme, action, self, _PlayViewController());

	Cgui_PrimaryIcon(topBar, "ui/pics/settings", QColors.Dark, action, self, _SettingsViewController());
	Cgui_PrimaryIcon(topBar, "ui/pics/info", QColors.Dark, action, self, _InfoViewController());
	Cgui_PrimaryIcon(topBar, "ui/pics/quit",  QColors.Dark, action, self, NULL);

	View *bottomBar = (View *) this->mainView->bottomBar;

	Cgui_PrimaryButton(bottomBar, "JOIN", QColors.Dark, action, self, _HomeViewController()); // TODO
	Cgui_PrimaryButton(bottomBar, "VOTES", QColors.Dark, action, self, _HomeViewController()); // TODO

	$(self->view, addSubview, (View *) this->mainView);

	$(self, addChildViewController, (ViewController *) this->navigationViewController);

	View *content = this->navigationViewController->viewController.view;

	content->padding.top = 80;
	content->padding.bottom = 80;
	content->zIndex = 1;

	action(NULL, NULL, this, _HomeViewController());
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	MainViewController *this = (MainViewController *) self;

	this->mainView->background->view.hidden = this->state == CL_ACTIVE;
	this->mainView->logo->view.hidden = this->state == CL_ACTIVE;
	this->mainView->version->view.hidden = this->state == CL_ACTIVE;

	this->mainView->bottomBar->view.hidden = this->state != CL_ACTIVE;
}

#pragma mark - MainViewController

/**
 * @fn MainViewController *MainViewController::init(MainViewController *self)
 *
 * @memberof MainViewController
 */
static MainViewController *init(MainViewController *self) {

	self = (MainViewController *) super(ViewController, self, init);
	if (self) {
		self->navigationViewController = $(alloc(NavigationViewController), init);
		assert(self->navigationViewController);
	}
	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->def->interface)->viewWillAppear = viewWillAppear;

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
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(MainViewController);
		clazz.interfaceOffset = offsetof(MainViewController, interface);
		clazz.interfaceSize = sizeof(MainViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
