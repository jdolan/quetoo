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

#include "ControlsViewController.h"

#include "LookViewController.h"
#include "MiscViewController.h"
#include "MoveViewController.h"
#include "ShootViewController.h"

#include "Theme.h"

#define _Class _ControlsViewController

#pragma mark - Object

static void dealloc(Object *self) {

	ControlsViewController *this = (ControlsViewController *) self;

	release(this->tabViewController);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	ControlsViewController *this = (ControlsViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	Panel *panel = $(theme, panel);

	$(theme, attach, panel);
	$(theme, target, panel->contentView);

	this->tabViewController = $(alloc(TabViewController), init);
	assert(this->tabViewController);

	ViewController *viewController, *tabViewController = (ViewController *) this->tabViewController;

	viewController = $((ViewController *) alloc(LookViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(MoveViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(ShootViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(MiscViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	$(self, addChildViewController, tabViewController);
	$(theme, attach, tabViewController->view);

	release(panel);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *ControlsViewController::_ControlsViewController(void)
 * @memberof ControlsViewController
 */
Class *_ControlsViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "ControlsViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(ControlsViewController);
		clazz.interfaceOffset = offsetof(ControlsViewController, interface);
		clazz.interfaceSize = sizeof(ControlsViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
