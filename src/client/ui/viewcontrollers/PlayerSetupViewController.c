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

#include "PlayerSetupViewController.h"

#include "ui_data.h"

#include "CvarSelect.h"
#include "PlayerModelView.h"
#include "SkinSelect.h"

#define _Class _PlayerSetupViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_STACK_VIEW_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_STACK_VIEW_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			box->view.autoresizingMask = ViewAutoresizingContain;

			$(box->label, setText, "PLAYER SETUP");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			extern cvar_t *name;
			Ui_CvarTextView((View *) stackView, "Name", name);

			Control *skinSelect = (Control *) $(alloc(SkinSelect), initWithFrame, NULL, ControlStyleDefault);

			Ui_Input((View *) stackView, "Player skin", skinSelect);
			release(skinSelect);

			extern cvar_t *color;
			Select *colorSelect = (Select *) $(alloc(CvarSelect), initWithVariable, color);

			$(colorSelect, addOption, "default", (ident) 0);
			$(colorSelect, addOption, "red", (ident) 232);
			$(colorSelect, addOption, "green", (ident) 201);
			$(colorSelect, addOption, "blue", (ident) 119);
			$(colorSelect, addOption, "yellow", (ident) 219);
			$(colorSelect, addOption, "orange", (ident) 225);
			$(colorSelect, addOption, "white", (ident) 216);
			$(colorSelect, addOption, "pink", (ident) 247);
			$(colorSelect, addOption, "purple", (ident) 187);

			$(colorSelect, selectOptionWithValue, (ident) (intptr_t) color->integer);

			Ui_Input((View *) stackView, "Effect color", (Control *) colorSelect);
			release(colorSelect);

			$((View *) stackView, sizeToFit);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) box, sizeToFit);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) column, sizeToFit);

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_STACK_VIEW_SPACING;

		{
			const SDL_Rect frame = { .w = 400, .h = 500 };
			PlayerModelView *mesh = $(alloc(PlayerModelView), initWithFrame, &frame);

			$((View *) column, addSubview, (View *) mesh);
			release(mesh);
		}

		$((View *) column, sizeToFit);

		$((View *) columns, addSubview, (View *) column);
		release(column);

	}

	$((View *) columns, sizeToFit);

	$((View *) this->panel->stackView, addSubview, (View *) columns);
	release(columns);

	$((View *) this->panel->stackView, sizeToFit);
	$((View *) this->panel, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _PlayerSetupViewController = {
	.name = "PlayerSetupViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(PlayerSetupViewController),
	.interfaceOffset = offsetof(PlayerSetupViewController, interface),
	.interfaceSize = sizeof(PlayerSetupViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

