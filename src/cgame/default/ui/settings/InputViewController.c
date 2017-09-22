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

	InputViewController *this = (InputViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

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

		$(this, bindTextView, theme, "Forward", "+forward");
		$(this, bindTextView, theme, "Back", "+back");
		$(this, bindTextView, theme, "Move left", "+move_left");
		$(this, bindTextView, theme, "Move right", "+move_right");
		$(this, bindTextView, theme, "Jump", "+move_up");
		$(this, bindTextView, theme, "Crouch", "+move_down");
		$(this, bindTextView, theme, "Turn left", "+left");
		$(this, bindTextView, theme, "Turn right", "+right");
		$(this, bindTextView, theme, "Center view", "center_view");
		$(this, bindTextView, theme, "Run / walk", "+speed");

		$(theme, checkbox, "Always Run", "cg_run");

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Communication");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(this, bindTextView, theme, "Say", "cl_message_mode");
		$(this, bindTextView, theme, "Say Team", "cl_message_mode_2");
		$(this, bindTextView, theme, "Show score", "+score");
		$(this, bindTextView, theme, "Take screenshot", "r_screenshot");

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Combat");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(this, bindTextView, theme, "Attack", "+attack");
		$(this, bindTextView, theme, "Grapple Hook", "+hook");
		$(this, bindTextView, theme, "Next weapon", "cg_weapon_next");
		$(this, bindTextView, theme, "Previous weapon", "cg_weapon_previous");
		$(this, bindTextView, theme, "Zoom", "+ZOOM");

		$(this, bindTextView, theme, "Blaster", "use blaster");
		$(this, bindTextView, theme, "Shotgun", "use shotgun");
		$(this, bindTextView, theme, "Super shotgun", "use super shotgun");
		$(this, bindTextView, theme, "Machinegun", "use machinegun");
		$(this, bindTextView, theme, "Hand grenades", "use hand grenades");
		$(this, bindTextView, theme, "Grenade launcher", "use grenade launcher");
		$(this, bindTextView, theme, "Rocket launcher", "use rocket launcher");
		$(this, bindTextView, theme, "Hyperblaster", "use hyperblaster");
		$(this, bindTextView, theme, "Lightning", "use lightning gun");
		$(this, bindTextView, theme, "Railgun", "use railgun");
		$(this, bindTextView, theme, "BFG-10K", "use bfg10k");

		release(box);
	}

	release(columns);
	release(container);
}

#pragma mark - InputViewController

/**
 * @fn void InputViewController::bindTextView(InputViewController *self, Theme *theme, const char *label, const char *bind)
 * @memberof InputViewController
 */
static void bindTextView(InputViewController *self, Theme *theme, const char *label, const char *bind) {

	TextView *textView = (TextView *) $(alloc(BindTextView), initWithBind, bind);
	assert(textView);

	textView->delegate.self = self;
	textView->delegate.didEndEditing = didBindKey;

	$(theme, control, label, textView);
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
