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

#include "PlayViewController.h"

#include "CreateServerViewController.h"
#include "JoinServerViewController.h"
#include "PlayerSetupViewController.h"

#define _Class _PlayViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	PlayViewController *this = (PlayViewController *) self;

	View *view = cgi.View("ui/play/PlayViewController.json", NULL);

	$(self, setView, view);

	release(view);

	this->tabViewController = $(alloc(TabViewController), init);
	assert(this->tabViewController);

	ViewController *viewController, *tabViewController = (ViewController *) this->tabViewController;

	viewController = $((ViewController *) alloc(JoinServerViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(CreateServerViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(PlayerSetupViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	$(self, addChildViewController, tabViewController);
	$((View *) ((Panel *) view)->contentView, addSubview, tabViewController->view);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *PlayViewController::_PlayViewController(void)
 * @memberof PlayViewController
 */
Class *_PlayViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "PlayViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(PlayViewController),
			.interfaceOffset = offsetof(PlayViewController, interface),
			.interfaceSize = sizeof(PlayViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
