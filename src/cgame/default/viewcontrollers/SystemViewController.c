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

#include "SystemViewController.h"

#include "CvarSelect.h"
#include "VideoModeSelect.h"

#define _Class _SystemViewController

#pragma mark - Actions & Delegates

/**
 * @brief ActionFunction for Apply Button.
 */
static void applyAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	cgi.Cbuf("r_restart\n");
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "VIDEO");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Control *videoModeSelect = (Control *) $(alloc(VideoModeSelect), initWithFrame, NULL, ControlStyleDefault);

			Cg_Input((View *) stackView, "Video mode", videoModeSelect);
			release(videoModeSelect);

			Cg_CvarCheckboxInput((View *) stackView, "Fullscreen", cgi.Cvar("r_fullscreen", NULL, 0, NULL));
			Cg_CvarCheckboxInput((View *) stackView, "Vertical Sync", cgi.Cvar("r_swap_interval", NULL, 0, NULL));

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "OPTIONS");

			box->view.autoresizingMask |= ViewAutoresizingWidth;

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Select *anisoSelect = (Select *) $(alloc(CvarSelect), initWithVariable, cgi.Cvar("r_anisotropy", NULL, 0, NULL));

			$(anisoSelect, addOption, "16x", (ident) 16);
			$(anisoSelect, addOption, "8x", (ident) 8);
			$(anisoSelect, addOption, "4x", (ident) 4);
			$(anisoSelect, addOption, "Off", (ident) 0);

			Cg_Input((View *) stackView, "Anisotropy", (Control *) anisoSelect);
			release(anisoSelect);

			Select *multisampleSelect = (Select *) $(alloc(CvarSelect), initWithVariable, cgi.Cvar("r_multisample", NULL, 0, NULL));

			$(multisampleSelect, addOption, "8x", (ident) 4);
			$(multisampleSelect, addOption, "4x", (ident) 2);
			$(multisampleSelect, addOption, "2x", (ident) 1);
			$(multisampleSelect, addOption, "Off", (ident) 0);

			Cg_Input((View *) stackView, "Multisample", (Control *) multisampleSelect);
			release(multisampleSelect);

			Select *shadowsSelect = (Select *) $(alloc(CvarSelect), initWithVariable, cgi.Cvar("r_shadows", NULL, 0, NULL));

			$(shadowsSelect, addOption, "High", (ident) 2);
			$(shadowsSelect, addOption, "Low", (ident) 1);
			$(shadowsSelect, addOption, "Off", (ident) 0);

			Cg_Input((View *) stackView, "Shadows", (Control *) shadowsSelect);
			release(shadowsSelect);

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
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "PICTURE");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarSliderInput((View *) stackView, "Brightness", cgi.Cvar("r_brightness", NULL, 0, NULL), 0.1, 2.0, 0.1);
			Cg_CvarSliderInput((View *) stackView, "Contrast", cgi.Cvar("r_contrast", NULL, 0, NULL), 0.1, 2.0, 0.1);
			Cg_CvarSliderInput((View *) stackView, "Gamma", cgi.Cvar("r_gamma", NULL, 0, NULL), 0.1, 2.0, 0.1);
			Cg_CvarSliderInput((View *) stackView, "Modulate", cgi.Cvar("r_modulate", NULL, 0, NULL), 0.1, 5.0, 0.1);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "SOUND");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarSliderInput((View *) stackView, "Volume", cgi.Cvar("s_volume", NULL, 0, NULL), 0.0, 1.0, 0.0);
			Cg_CvarSliderInput((View *) stackView, "Music Volume", cgi.Cvar("s_music_volume", NULL, 0, NULL), 0.0, 1.0, 0.0);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			this->panel->accessoryView->view.hidden = false;

			Cg_Button((View *) this->panel->accessoryView, "Apply", applyAction, self, NULL);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->panel->contentView, addSubview, (View *) columns);
	release(columns);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _SystemViewController = {
	.name = "SystemViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(SystemViewController),
	.interfaceOffset = offsetof(SystemViewController, interface),
	.interfaceSize = sizeof(SystemViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

