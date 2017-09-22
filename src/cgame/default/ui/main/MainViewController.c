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
#include "MainView.h"

#include "PrimaryButton.h"
#include "PrimaryIcon.h"

#include "HomeViewController.h"
#include "InfoViewController.h"
#include "PlayViewController.h"
#include "SettingsViewController.h"

#include "Theme.h"

#define _Class _MainViewController

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

	MainViewController *this = (MainViewController *) sender;

	Class *clazz = (Class *) data;

	if (clazz) {
		$(this->navigationViewController, popToRootViewController);
		$(this->navigationViewController, popViewController);

		ViewController *viewController = $((ViewController *) _alloc(clazz), init);
		$(this->navigationViewController, pushViewController, viewController);
		release(viewController);
	} else {
		MainView *mainView = (MainView *) ((ViewController *) this)->view;

		if (*cgi.state >= CL_CONNECTED) {
			$(mainView->dialog, showDialog, "Are you sure you want to disconnect?", "No", "Yes", disconnect);
		} else {
			$(mainView->dialog, showDialog, "Are you sure you want to quit?", "No", "Yes", quit);
		}
	}
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	MainViewController *this = (MainViewController *) self;

	release(this->navigationViewController);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MainViewController *this = (MainViewController *) self;

	MainView *mainView = $(alloc(MainView), initWithFrame, NULL);
	assert(mainView);

	release(self->view);
	self->view = (View *) mainView;

	$(this, primaryButton, mainView->topBar, "Home", _HomeViewController());
	$(this, primaryButton, mainView->topBar, "Play", _PlayViewController());

	$(this, primaryIcon, mainView->topBar, "ui/pics/settings", _SettingsViewController());
	$(this, primaryIcon, mainView->topBar, "ui/pics/info", _InfoViewController());
	$(this, primaryIcon, mainView->topBar, "ui/pics/quit", NULL);

	$(this, primaryButton, mainView->bottomBar, "Join", _HomeViewController()); // TODO
	$(this, primaryButton, mainView->bottomBar, "Votes", _HomeViewController()); // TODO

	$(self, addChildViewController, (ViewController *) this->navigationViewController);
	$(mainView->contentView, addSubview, this->navigationViewController->viewController.view);

	action(NULL, NULL, this, _HomeViewController());
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	MainView *mainView = (MainView *) self->view;

	mainView->background->view.hidden = *cgi.state == CL_ACTIVE;
	mainView->logo->view.hidden = *cgi.state == CL_ACTIVE;
	mainView->version->view.hidden = *cgi.state == CL_ACTIVE;
	mainView->bottomBar->view.hidden = *cgi.state != CL_ACTIVE;
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

/**
 * @fn void MainViewController::primaryButton(MainViewController *self, ident target, const char *title, Class *clazz)
 * @memberof MainViewController
 */
static void primaryButton(MainViewController *self, ident target, const char *title, Class *clazz) {

	PrimaryButton *button = $(alloc(PrimaryButton), initWithTitle, title);
	assert(button);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, clazz);

	$((View *) target, addSubview, (View *) button);
	release(button);
}

/**
 * @fn void MainViewController::primaryIcon(MainViewController *self, ident target, const char *icon, Class *clazz)
 * @memberof MainViewController
 */
static void primaryIcon(MainViewController *self, ident target, const char *icon, Class *clazz) {

	PrimaryIcon *button = $(alloc(PrimaryIcon), initWithIcon, icon);
	assert(button);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, clazz);

	$((View *) target, addSubview, (View *) button);
	release(button);
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
	((MainViewControllerInterface *) clazz->def->interface)->primaryButton = primaryButton;
	((MainViewControllerInterface *) clazz->def->interface)->primaryIcon = primaryIcon;
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
