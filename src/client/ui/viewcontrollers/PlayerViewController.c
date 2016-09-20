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

#include "PlayerViewController.h"

#include "ui_data.h"

#include "CvarSelect.h"
#include "SkinSelect.h"

#define _Class _PlayerViewController

#pragma mark - Skin selection

/**
 * @brief ActionFunction for skin selection.
 */
static void selectSkin(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerViewController *this = (PlayerViewController *) sender;

	$((View *) this->playerModelView, updateBindings);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	PlayerViewController *this = (PlayerViewController *) self;

	StackView *columns = alloc(StackView, initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "PLAYER SETUP");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			extern cvar_t *name;
			Ui_CvarTextView((View *) stackView, "Name", name);

			Control *skinSelect = (Control *) alloc(SkinSelect, initWithFrame, NULL, ControlStyleDefault);

			$(skinSelect, addActionForEventType, SDL_MOUSEBUTTONUP, selectSkin, self, NULL);

			Ui_Input((View *) stackView, "Player skin", skinSelect);
			release(skinSelect);

			extern cvar_t *color;
			Select *colorSelect = (Select *) alloc(CvarSelect, initWithVariable, color);

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

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			const SDL_Rect frame = { .w = 400, .h = 500 };
			this->playerModelView = alloc(PlayerModelView, initWithFrame, &frame);

			$((View *) column, addSubview, (View *) this->playerModelView);
			release(this->playerModelView);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->menuViewController.panel->contentView, addSubview, (View *) columns);
	release(columns);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _PlayerViewController = {
	.name = "PlayerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(PlayerViewController),
	.interfaceOffset = offsetof(PlayerViewController, interface),
	.interfaceSize = sizeof(PlayerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

