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

#include "HomeViewController.h"

#include "Theme.h"

#define _Class _HomeViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	Panel *panel = $(theme, panel);

	$(theme, attach, panel);
	$(theme, target, panel->contentView);

	{
		Box *box = $(theme, box, "News");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Label *label = $(alloc(Label), initWithText, "Message of the day", NULL);

		$(theme, attach, label);

		release(label);
		release(box);
	}

	release(panel);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @memberof HomeViewController
 */
Class *_HomeViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "HomeViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(HomeViewController);
		clazz.interfaceOffset = offsetof(HomeViewController, interface);
		clazz.interfaceSize = sizeof(HomeViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
