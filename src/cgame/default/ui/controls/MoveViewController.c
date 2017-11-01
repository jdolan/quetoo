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

#include "MoveViewController.h"
#include "CvarSelect.h"
#include "QuetooTheme.h"

#define _Class _MoveViewController

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
	self->view->identifier = strdup("Move");

	MoveViewController *this = (MoveViewController *) self;

	TextViewDelegate delegate = {
		.self = this,
		.didEndEditing = didBindKey
	};

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 1);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Movement");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, bindTextView, "Forward", "+forward", &delegate);
		$(theme, bindTextView, "Back", "+back", &delegate);
		$(theme, bindTextView, "Move left", "+move_left", &delegate);
		$(theme, bindTextView, "Move right", "+move_right", &delegate);
		$(theme, bindTextView, "Jump", "+move_up", &delegate);
		$(theme, bindTextView, "Crouch", "+move_down", &delegate);
		$(theme, bindTextView, "Turn left", "+left", &delegate);
		$(theme, bindTextView, "Turn right", "+right", &delegate);
		$(theme, bindTextView, "Center view", "center_view", &delegate);
		$(theme, bindTextView, "Run / walk", "+speed", &delegate);

		$(theme, checkbox, "Always Run", "cg_run");

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
 * @fn Class *MoveViewController::_MoveViewController(void)
 * @memberof MoveViewController
 */
Class *_MoveViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MoveViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(MoveViewController);
		clazz.interfaceOffset = offsetof(MoveViewController, interface);
		clazz.interfaceSize = sizeof(MoveViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
