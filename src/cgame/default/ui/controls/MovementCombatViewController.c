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

#include "MovementCombatViewController.h"

#define _Class _MovementCombatViewController

#pragma mark - Actions and delegates

/**
 * @brief TextViewDelegate callback for binding keys.
 */
static void didBindKey(TextView *textView) {

	const ViewController *this = textView->delegate.self;

	$(this->view, updateBindings);
}

/**
 * @brief ViewEnumerator for setting the TextViewDelegate on BindTextViews.
 */
static void setDelegate(View *view, ident data) {

	((TextView *) view)->delegate = (TextViewDelegate) {
		.self = data,
		.didEndEditing = didBindKey
	};
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Select *hookStyle;
	Outlet outlets[] = MakeOutlets(
		MakeOutlet("hookStyle", &hookStyle)
	);

	$(self->view, awakeWithResourceName, "ui/controls/MovementCombatViewController.json");
	$(self->view, resolve, outlets);
	
	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/controls/MovementCombatViewController.css");
	assert(self->view->stylesheet);
	
	$(self->view, enumerateSelection, "BindTextView", setDelegate, self);

	$(hookStyle, addOption, "pull", NULL);
	$(hookStyle, addOption, "swing", NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *MovementCombatViewController::_MovementCombatViewController(void)
 * @memberof MovementCombatViewController
 */
Class *_MovementCombatViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "MovementCombatViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(MovementCombatViewController),
			.interfaceOffset = offsetof(MovementCombatViewController, interface),
			.interfaceSize = sizeof(MovementCombatViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
