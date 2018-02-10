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

#include "ControlsViewController.h"
#include "HomeViewController.h"
#include "PlayViewController.h"
#include "SettingsViewController.h"

#include "DialogViewController.h"

#include "QuetooTheme.h"

#define _Class _MainViewController

#pragma mark - Actions

/**
 * @brief Quit the game.
 */
static void quit(ident data) {
	cgi.Cbuf("quit\n");
}

/**
 * @brief Disconnect from the current game.
 */
static void disconnect(ident data) {
	cgi.Cbuf("disconnect\n");
}

/**
 * @brief ActionFunction for main menu primary items.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	MainViewController *this = (MainViewController *) sender;

	Class *clazz = (Class *) data;

	if (clazz) {
		ViewController *topViewController = $(this->navigationViewController, topViewController);
		if (topViewController && $((Object *) topViewController, isKindOfClass, clazz)) {
			return;
		}

		$(this->navigationViewController, popToRootViewController);
		$(this->navigationViewController, popViewController);

		ViewController *viewController = $((ViewController *) _alloc(clazz), init);
		$(this->navigationViewController, pushViewController, viewController);
		release(viewController);
	} else {

		Dialog dialog;
		if (*cgi.state >= CL_CONNECTED) {
			dialog = (const Dialog) {
				.message = "Are you sure you want to disconnect?",
				.ok = "Yes",
				.cancel = "No",
				.okFunction = disconnect
			};
		} else {
			dialog = (const Dialog) {
				.message = "Are you sure you want to quit?",
				.ok = "Yes",
				.cancel = "No",
				.okFunction = quit
			};
		}

		ViewController *viewController = (ViewController *) $(alloc(DialogViewController), initWithDialog, &dialog);
		$((ViewController *) this, addChildViewController, viewController);
	}
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	MainViewController *this = (MainViewController *) self;

	release(this->mainView);
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

	this->mainView = $(alloc(MainView), initWithFrame, NULL);
	assert(this->mainView);

	$(self, setView, (View *) this->mainView);

	$(this, primaryButton, "Home", _HomeViewController());
	$(this, primaryButton, "Play", _PlayViewController());
	$(this, primaryButton, "Controls", _ControlsViewController());
	$(this, primaryButton, "Settings", _SettingsViewController());

	$(this, primaryButton, "Quit", NULL);

	$(this, secondaryButton, "Join", _HomeViewController()); // TODO
	$(this, secondaryButton, "Votes", _HomeViewController()); // TODO

	$(self, addChildViewController, (ViewController *) this->navigationViewController);
	$(this->mainView->contentView, addSubview, this->navigationViewController->viewController.view);

	action(NULL, NULL, this, _HomeViewController());
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
 * @fn void MainViewController::primaryButton(MainViewController *self, const char *title, Class *clazz)
 * @memberof MainViewController
 */
static void primaryButton(MainViewController *self, const char *title, Class *clazz) {

	Button *button = $(alloc(Button), initWithTitle, title);
	assert(button);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, clazz);

	$((View *) self->mainView->primaryMenu, addSubview, (View *) button);
	release(button);
}

/**
 * @fn void MainViewController::secondaryButton(MainViewController *self, const char *title, Class *clazz)
 * @memberof MainViewController
 */
static void secondaryButton(MainViewController *self, const char *title, Class *clazz) {

	Button *button = $(alloc(Button), initWithTitle, title);
	assert(button);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, clazz);

	$((View *) self->mainView->secondaryMenu, addSubview, (View *) button);
	release(button);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((MainViewControllerInterface *) clazz->interface)->init = init;
	((MainViewControllerInterface *) clazz->interface)->primaryButton = primaryButton;
	((MainViewControllerInterface *) clazz->interface)->secondaryButton = secondaryButton;
}

/**
 * @fn Class *MainViewController::_MainViewController(void)
 * @memberof MainViewController
 */
Class *_MainViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "MainViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(MainViewController),
			.interfaceOffset = offsetof(MainViewController, interface),
			.interfaceSize = sizeof(MainViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
