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
#include "Theme.h"

#define _Class _DialogView

#pragma mark - Object

static void dealloc(Object *self) {

	DialogView *this = (DialogView *) self;

	release(this->label);

	release(this->cancelButton);
	release(this->okButton);

	super(Object, self, dealloc);
}

#pragma mark - Actions

/**
 * @brief ActionFunction for clicking Ok
 */
static void okAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	DialogView *this = (DialogView *) sender;

	if (this->okFunction) {
		this->okFunction();
	}

	this->panel.stackView.view.hidden = true;
}

/**
 * @brief ActionFunction for clicking Cancel
 */
static void cancelAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	DialogView *this = (DialogView *) sender;

	this->panel.stackView.view.hidden = true;
}

#pragma mark - DialogView

/**
 * @see DialogView::init(DialogView *self)
 */
static DialogView *init(DialogView *self) {

	self = (DialogView *) super(Panel, self, initWithFrame, NULL);
	if (self) {

		Panel *this = (Panel *) self;

		this->isDraggable = false;
		this->isResizable = false;

		this->stackView.view.hidden = true; // Don't appear until opened
		this->stackView.view.zIndex = 1000; // We is always on top suckas

		Theme *theme = $(alloc(Theme), init);
		assert(theme);

		this->stackView.view.backgroundColor = theme->colors.dialog;
		this->stackView.view.borderColor = theme->colors.lightBorder;

		release(theme);

		this->stackView.view.alignment = ViewAlignmentMiddleLeft;
		this->stackView.view.autoresizingMask = ViewAutoresizingWidth | ViewAutoresizingContain;

		this->contentView->view.alignment = ViewAlignmentTopCenter;
		this->contentView->view.autoresizingMask = ViewAutoresizingContain;

		this->accessoryView->view.hidden = false;

		this->accessoryView->view.alignment = ViewAlignmentBottomCenter;
		this->accessoryView->view.autoresizingMask = ViewAutoresizingContain;

		{
			// Label

			self->label = $(alloc(Label), initWithText, "No description given", $$(Font, defaultFont, FontCategoryPrimaryResponder));

			self->label->view.alignment = ViewAlignmentTopCenter;
			self->label->view.autoresizingMask = ViewAutoresizingContain;

			$((View *) this->contentView, addSubview, (View *) self->label);
		}

		{
			// Cancel button

			self->cancelButton = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

			$(self->cancelButton->title, setText, "Cancel");

			self->cancelButton->control.view.alignment = ViewAlignmentBottomCenter;
			self->cancelButton->control.view.autoresizingMask = ViewAutoresizingContain;
			self->cancelButton->control.view.backgroundColor = theme->colors.dark;

			$((Control *) self->cancelButton, addActionForEventType, SDL_MOUSEBUTTONUP, cancelAction, self, NULL);

			$((View *) this->accessoryView, addSubview, (View *) self->cancelButton);

			// Ok button

			self->okButton = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

			$(self->okButton->title, setText, "Ok");

			self->okButton->control.view.alignment = ViewAlignmentBottomCenter;
			self->okButton->control.view.autoresizingMask = ViewAutoresizingContain;
			self->okButton->control.view.backgroundColor = theme->colors.light;

			self->okFunction = NULL;

			$((Control *) self->okButton, addActionForEventType, SDL_MOUSEBUTTONUP, okAction, self, NULL);

			$((View *) this->accessoryView, addSubview, (View *) self->okButton);
		}
	}

	return self;
}

/**
 * @see DialogView::showDialog(DialogView *self, const char *text, void (*okFunction)(void))
 */
static void showDialog(DialogView *self, const char *text, const char *cancelText, const char *okText, void (*okFunction)(void)) {

	// Description and the Ok text need to exist, the Cancel button can be disabled by passing NULL for the text

	assert(text);
	assert(okText);

	self->okFunction = okFunction;

	self->panel.stackView.view.hidden = false;
	self->panel.stackView.view.needsLayout = true;

	self->cancelButton->control.view.needsLayout = true;
	self->okButton->control.view.needsLayout = true;

	$(self->label->text, setText, text);

	if (cancelText) {
		$(self->cancelButton->title, setText, cancelText);
	} else {
		self->cancelButton->control.view.hidden = true;
	}

	$(self->okButton->title, setText, okText);
}

/**
 * @see DialogView::hideDialog(DialogView *self)
 */
static void hideDialog(DialogView *self) {

	self->panel.stackView.view.hidden = true;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((DialogViewInterface *) clazz->def->interface)->init = init;
	((DialogViewInterface *) clazz->def->interface)->showDialog = showDialog;
	((DialogViewInterface *) clazz->def->interface)->hideDialog = hideDialog;
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
