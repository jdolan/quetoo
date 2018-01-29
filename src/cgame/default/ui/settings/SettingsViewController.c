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

#include "SettingsViewController.h"

#include "MoveViewController.h"
#include "LookViewController.h"
#include "OptionsViewController.h"
#include "SystemViewController.h"

#include "QuetooTheme.h"

#define _Class _SettingsViewController

#pragma mark - Object

static void dealloc(Object *self) {

	SettingsViewController *this = (SettingsViewController *) self;

	release(this->tabViewController);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	SettingsViewController *this = (SettingsViewController *) self;

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);
	assert(theme);

	Panel *panel = $(theme, panel);

	$(theme, attach, panel);
	$(theme, target, panel->contentView);

	this->tabViewController = $(alloc(TabViewController), init);
	assert(this->tabViewController);

	ViewController *viewController, *tabViewController = (ViewController *) this->tabViewController;

	viewController = $((ViewController *) alloc(SystemViewController), init);
	$(tabViewController, addChildViewController, viewController);
	release(viewController);

	viewController = $((ViewController *) alloc(OptionsViewController), init);
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
 * @fn Class *SettingsViewController::_SettingsViewController(void)
 * @memberof SettingsViewController
 */
Class *_SettingsViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "SettingsViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(SettingsViewController);
		clazz.interfaceOffset = offsetof(SettingsViewController, interface);
		clazz.interfaceSize = sizeof(SettingsViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
