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

#include "MiscViewController.h"
#include "CvarSelect.h"
#include "QuetooTheme.h"

#define _Class _MiscViewController

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
	self->view->identifier = strdup("Misc");

	MiscViewController *this = (MiscViewController *) self;

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
		Box *box = $(theme, box, "Communication");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, bindTextView, "Say", "cl_message_mode", &delegate);
		$(theme, bindTextView, "Say team", "cl_message_mode_2", &delegate);
		$(theme, bindTextView, "Show score", "+score", &delegate);
		$(theme, bindTextView, "Take screenshot", "r_screenshot", &delegate);

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
 * @fn Class *MiscViewController::_MiscViewController(void)
 * @memberof MiscViewController
 */
Class *_MiscViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MiscViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(MiscViewController);
		clazz.interfaceOffset = offsetof(MiscViewController, interface);
		clazz.interfaceSize = sizeof(MiscViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
