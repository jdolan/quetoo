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

	if (control == (Control *) this->okButton) {
		if (this->dialog.okFunction) {
			this->dialog.okFunction(this->dialog.data);
		} else {
			cgi.Warn("okFunction was NULL\n");
		}
	} else if (control == (Control *) this->cancelButton) {
		if (this->dialog.cancelFunction) {
			this->dialog.cancelFunction(this->dialog.data);
		}
	} else {
		assert(false);
	}

	$((ViewController *) this, removeFromParentViewController);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	DialogViewController *this = (DialogViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("message", &this->message),
		MakeOutlet("cancel", &this->cancelButton),
		MakeOutlet("ok", &this->okButton)
	);

	$(self->view, awakeWithResourceName, "ui/main/DialogViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/main/DialogViewController.css");
	assert(self->view->stylesheet);

	$(this->message->text, setText, this->dialog.message);
	$(this->cancelButton->title, setText, this->dialog.cancel);
	$(this->okButton->title, setText, this->dialog.ok);

	$((Control *) this->okButton, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);
	$((Control *) this->cancelButton, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);
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

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((DialogViewControllerInterface *) clazz->interface)->initWithDialog = initWithDialog;
}

/**
 * @fn Class *DialogViewController::_DialogViewController(void)
 * @memberof DialogViewController
 */
Class *_DialogViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "DialogViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(DialogViewController),
			.interfaceOffset = offsetof(DialogViewController, interface),
			.interfaceSize = sizeof(DialogViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
