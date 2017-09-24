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

#include "ShootViewController.h"

#include "CvarSelect.h"
#include "Theme.h"

#define _Class _ShootViewController

#pragma mark - Actions and delegates

/**
 * @brief TextViewDelegate callback for binding keys.
 */
static void didBindKey(TextView *textView) {

	const ViewController *this = textView->delegate.self;

	$(this->view, updateBindings);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Shoot");

	ShootViewController *this = (ShootViewController *) self;

	TextViewDelegate delegate = {
		.self = this,
		.didEndEditing = didBindKey
	};

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Combat");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, bindTextView, "Attack", "+attack", &delegate);
		$(theme, bindTextView, "Next weapon", "cg_weapon_next", &delegate);
		$(theme, bindTextView, "Previous weapon", "cg_weapon_previous", &delegate);
		$(theme, bindTextView, "Best Weapon", "cg_weapon_best", &delegate);

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Grapple hook");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, bindTextView, "Hook", "+hook", &delegate);

		CvarSelect *hookStyle = $(alloc(CvarSelect), initWithVariableName, "hook_style");
		assert(hookStyle);

		hookStyle->expectsStringValue = true;

		$((Select *) hookStyle, addOption, "pull", (ident) HOOK_PULL);
		$((Select *) hookStyle, addOption, "swing", (ident) HOOK_SWING);

		$(theme, control, "Hook style", hookStyle);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Weapons");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, bindTextView, "Blaster", "use blaster", &delegate);
		$(theme, bindTextView, "Shotgun", "use shotgun", &delegate);
		$(theme, bindTextView, "Super shotgun", "use super shotgun", &delegate);
		$(theme, bindTextView, "Machinegun", "use machinegun", &delegate);
		$(theme, bindTextView, "Hand grenades", "use hand grenades", &delegate);
		$(theme, bindTextView, "Grenade launcher", "use grenade launcher", &delegate);
		$(theme, bindTextView, "Rocket launcher", "use rocket launcher", &delegate);
		$(theme, bindTextView, "Hyperblaster", "use hyperblaster", &delegate);
		$(theme, bindTextView, "Lightning", "use lightning gun", &delegate);
		$(theme, bindTextView, "Railgun", "use railgun", &delegate);
		$(theme, bindTextView, "BFG-10K", "use bfg10k", &delegate);

		release(box);
	}

	release(columns);
	release(container);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *ShootViewController::_ShootViewController(void)
 * @memberof ShootViewController
 */
Class *_ShootViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "ShootViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(ShootViewController);
		clazz.interfaceOffset = offsetof(ShootViewController, interface);
		clazz.interfaceSize = sizeof(ShootViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
