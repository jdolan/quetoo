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

#include "PlayerViewController.h"

#include "CvarSelect.h"
#include "HueSlider.h"
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

/**
 * @brief ActionFunction for effect hue selection.
 */
static void selectEffectColor(double hue) {

	if (hue < 0.0) {
		cgi.CvarSet(cg_color->name, "default");

		return;
	}

	cgi.CvarSetValue(cg_color->name, (vec_t) floor(hue));
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	PlayerViewController *this = (PlayerViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Profile");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			// player name

			Cg_CvarTextView((View *) stackView, "Name", "name");

			// player model

			Control *skinSelect = (Control *) $(alloc(SkinSelect), initWithFrame, NULL, ControlStyleDefault);

			$(skinSelect, addActionForEventType, SDL_MOUSEBUTTONUP, selectSkin, self, NULL);

			Cg_Input((View *) stackView, "Player skin", skinSelect);
			release(skinSelect);

			// effect color

			double hue = -1;
			if (g_strcmp0(cg_color->string, "default")) {
				hue = cg_color->integer;
			}

			Slider *hueSlider = (Slider *) $(alloc(HueSlider), initWithVariable, hue, selectEffectColor);

			Cg_Input((View *) stackView, "Effect color", (Control *) hueSlider);

			release(hueSlider);

			// hook style

			CvarSelect *hookSelect = (CvarSelect *) $(alloc(CvarSelect), initWithVariable, cg_hook_style);
			hookSelect->expectsStringValue = true;

			$((Select *) hookSelect, addOption, "pull", (ident) HOOK_PULL);
			$((Select *) hookSelect, addOption, "swing", (ident) HOOK_SWING);

			g_hook_style_t hook_style = HOOK_PULL;

			if (!g_strcmp0(cg_hook_style->string, "swing")) {
				hook_style = HOOK_SWING;
			}

			$((Select *) hookSelect, selectOptionWithValue, (ident) (intptr_t) hook_style);

			Cg_Input((View *) stackView, "Hook style", (Control *) hookSelect);

			release(hookSelect);

			// handicap

			Cg_CvarSliderInput((View *) stackView, "Handicap", cg_handicap->name, 50.0, 100.0, 5.0);

			// field of view

			Cg_CvarSliderInput((View *) stackView, "FOV", cg_fov->name, 80.0, 130.0, 5.0);

			// zoomed field of view

			Cg_CvarSliderInput((View *) stackView, "Zoom FOV", cg_fov_zoom->name, 20.0, 70.0, 5.0);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			const SDL_Rect frame = { .w = 400, .h = 500 };
			this->playerModelView = $(alloc(PlayerModelView), initWithFrame, &frame);

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

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *PlayerViewController::_PlayerViewController(void)
 * @memberof PlayerViewController
 */
Class *_PlayerViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayerViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(PlayerViewController);
		clazz.interfaceOffset = offsetof(PlayerViewController, interface);
		clazz.interfaceSize = sizeof(PlayerViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
