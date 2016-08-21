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

#include <assert.h>

#include "CreateServerViewController.h"

#include "ui_data.h"

#define _Class _CreateServerViewController

#pragma mark - ViewController

static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "CREATE SERVER");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
		stackView->spacing = DEFAULT_PANEL_STACK_VIEW_SPACING;

		extern cvar_t *sv_hostname;
		Ui_CvarTextView((View *) stackView, "Hostname", sv_hostname);

		extern cvar_t *sv_max_clients;
		Ui_CvarTextView((View *) stackView, "Clients", sv_max_clients);

		extern cvar_t *sv_public;
		Ui_CvarCheckbox((View *) stackView, "Public", sv_public);

		extern cvar_t *password;
		Ui_CvarTextView((View *) stackView, "Password", password);

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) this->panel->contentView, addSubview, (View *) box);
		release(box);
	}

	$((View *) this->panel->contentView, sizeToFit);
	$((View *) this->panel, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _CreateServerViewController = {
	.name = "CreateServerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(CreateServerViewController),
	.interfaceOffset = offsetof(CreateServerViewController, interface),
	.interfaceSize = sizeof(CreateServerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

