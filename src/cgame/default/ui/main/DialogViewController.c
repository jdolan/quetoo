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

#include "DialogViewController.h"

#define _Class _DialogViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for clicking Ok
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	DialogViewController *this = (DialogViewController *) sender;

	if (control == (Control *) this->dialogView->okButton) {
		if (this->dialog.okFunction) {
			this->dialog.okFunction(this->dialog.data);
		} else {
			cgi.Warn("okFunction was NULL\n");
		}
	} else if (control == (Control *) this->dialogView->cancelButton) {
		if (this->dialog.cancelFunction) {
			this->dialog.cancelFunction(this->dialog.data);
		}
	} else {
		assert(false);
	}

	$((ViewController *) this, removeFromParentViewController);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	DialogViewController *this = (DialogViewController *) self;

	release(this->dialogView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	DialogViewController *this = (DialogViewController *) self;

	this->dialogView = $(alloc(DialogView), init);
	assert(this->dialogView);

	$(this->dialogView->message->text, setText, this->dialog.message);

	$(this->dialogView->okButton->title, setText, this->dialog.ok);
	$(this->dialogView->cancelButton->title, setText, this->dialog.cancel);

	Control *ok = (Control *) this->dialogView->okButton;
	$(ok, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);

	Control *cancel = (Control *) this->dialogView->cancelButton;
	$(cancel, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);

	$(self->view, addSubview, (View *) this->dialogView);
}

#pragma mark - DialogViewController

/**
 * @fn DialogViewController *DialogViewController::initWithDialog(DialogViewController *self, const Dialog *dialog)
 * @memberof DialogViewController
 */
static DialogViewController *initWithDialog(DialogViewController *self, const Dialog *dialog) {

	self = (DialogViewController *) super(ViewController, self, init);
	if (self) {
		assert(dialog);
		self->dialog = *dialog;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((DialogViewControllerInterface *) clazz->def->interface)->initWithDialog = initWithDialog;
}

/**
 * @fn Class *DialogViewController::____FILEBASENAMEASIDENTIFIER___(void)
 * @memberof DialogViewController
 */
Class *_DialogViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "DialogViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(DialogViewController);
		clazz.interfaceOffset = offsetof(DialogViewController, interface);
		clazz.interfaceSize = sizeof(DialogViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
