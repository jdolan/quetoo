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

#include "InputViewController.h"
#include "BindTextView.h"
#include "Theme.h"

#define _Class _InputViewController

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
	self->view->identifier = strdup("Input");

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	InputViewController *this = (InputViewController *) self;

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Movement");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(this, bindTextView, "Forward", "+forward");
		$(this, bindTextView, "Back", "+back");
		$(this, bindTextView, "Move left", "+move_left");
		$(this, bindTextView, "Move right", "+move_right");
		$(this, bindTextView, "Jump", "+move_up");
		$(this, bindTextView, "Crouch", "+move_down");
		$(this, bindTextView, "Turn left", "+left");
		$(this, bindTextView, "Turn right", "+right");
		$(this, bindTextView, "Center view", "center_view");
		$(this, bindTextView, "Run / walk", "+speed");

		$(theme, checkbox, "Always Run", "cg_run");

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Communication");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(this, bindTextView, "Say", "cl_message_mode");
		$(this, bindTextView, "Say Team", "cl_message_mode_2");
		$(this, bindTextView, "Show score", "+score");
		$(this, bindTextView, "Take screenshot", "r_screenshot");

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Combat");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(this, bindTextView, "Attack", "+attack");
		$(this, bindTextView, "Grapple Hook", "+hook");
		$(this, bindTextView, "Next weapon", "cg_weapon_next");
		$(this, bindTextView, "Previous weapon", "cg_weapon_previous");
		$(this, bindTextView, "Zoom", "+ZOOM");

		$(this, bindTextView, "Blaster", "use blaster");
		$(this, bindTextView, "Shotgun", "use shotgun");
		$(this, bindTextView, "Super shotgun", "use super shotgun");
		$(this, bindTextView, "Machinegun", "use machinegun");
		$(this, bindTextView, "Hand grenades", "use hand grenades");
		$(this, bindTextView, "Grenade launcher", "use grenade launcher");
		$(this, bindTextView, "Rocket launcher", "use rocket launcher");
		$(this, bindTextView, "Hyperblaster", "use hyperblaster");
		$(this, bindTextView, "Lightning", "use lightning gun");
		$(this, bindTextView, "Railgun", "use railgun");
		$(this, bindTextView, "BFG-10K", "use bfg10k");

		release(box);
	}

	release(columns);
	release(container);
	release(theme);
}

#pragma mark - InputViewController

/**
 * @fn void InputViewController::bindTextView(InputViewController *self, const char *label, const char *bind)
 * @memberof InputViewController
 */
static void bindTextView(InputViewController *self, const char *label, const char *bind) {

	TextView *textView = (TextView *) $(alloc(BindTextView), initWithBind, bind);
	assert(textView);

	textView->delegate.self = self;
	textView->delegate.didEndEditing = didBindKey;

	$($$(Theme, sharedInstance), control, label, (Control *) textView);
}


#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((InputViewControllerInterface *) clazz->def->interface)->bindTextView = bindTextView;
}

/**
 * @fn Class *InputViewController::_InputViewController(void)
 * @memberof InputViewController
 */
Class *_InputViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "InputViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(InputViewController);
		clazz.interfaceOffset = offsetof(InputViewController, interface);
		clazz.interfaceSize = sizeof(InputViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
