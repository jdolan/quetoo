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

#include "DialogView.h"
#include "QuetooTheme.h"

#define _Class _DialogView

#pragma mark - Object

static void dealloc(Object *self) {

	DialogView *this = (DialogView *) self;

	release(this->message);
	release(this->okButton);
	release(this->cancelButton);

	super(Object, self, dealloc);
}

#pragma mark - DialogView

/**
 * @see DialogView::init(DialogView *self)
 */
static DialogView *init(DialogView *self) {

	self = (DialogView *) super(Panel, self, initWithFrame, NULL);
	if (self) {

		View *this = (View *) self;

		self->panel.isDraggable = false;
		self->panel.isResizable = false;

		QuetooTheme *theme = $(alloc(QuetooTheme), init);
		assert(theme);

		this->alignment = ViewAlignmentMiddleCenter;
		this->autoresizingMask |= ViewAutoresizingWidth;
		this->borderColor = theme->colors.lightBorder;

		self->panel.stackView->view.alignment = ViewAlignmentMiddleCenter;

		self->panel.contentView->view.alignment = ViewAlignmentMiddleCenter;

		self->message = $(alloc(Label), initWithText, NULL, NULL);

		self->message->view.alignment = ViewAlignmentMiddleCenter;
		self->message->view.autoresizingMask = ViewAutoresizingContain;

		$((View *) self->panel.contentView, addSubview, (View *) self->message);

		self->panel.accessoryView->view.alignment = ViewAlignmentBottomCenter;
		self->panel.accessoryView->view.hidden = false;
		
		self->cancelButton = $(alloc(Button), initWithFrame, NULL);
		assert(self->cancelButton);

		$(self->cancelButton->title, setText, "Cancel");

		self->cancelButton->control.view.backgroundColor = theme->colors.dark;

		$((View *) self->panel.accessoryView, addSubview, (View *) self->cancelButton);

		self->okButton = $(alloc(Button), initWithFrame, NULL);
		assert(self->okButton);

		$(self->okButton->title, setText, "Ok");

		self->okButton->control.view.backgroundColor = theme->colors.light;

		$((View *) self->panel.accessoryView, addSubview, (View *) self->okButton);

		release(theme);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((DialogViewInterface *) clazz->def->interface)->init = init;
}

/**
 * @fn Class *DialogView::_DialogView(void)
 * @memberof DialogView
 */
Class *_DialogView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "DialogView";
		clazz.superclass = _Panel();
		clazz.instanceSize = sizeof(DialogView);
		clazz.interfaceOffset = offsetof(DialogView, interface);
		clazz.interfaceSize = sizeof(DialogViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
