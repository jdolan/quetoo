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

#include "SystemViewController.h"

#include "CvarSelect.h"
#include "VideoModeSelect.h"

#include "client.h"

#define _Class _SystemViewController

#pragma mark - Actions & Delegates

/**
 * @brief ActionFunction for Apply Button.
 */
static void applyAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	Cbuf_AddText("r_restart; s_restart;\n");
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	StackView *columns = alloc(StackView, initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "VIDEO");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			Control *videoModeSelect = (Control *) alloc(VideoModeSelect, initWithFrame, NULL, ControlStyleDefault);

			Ui_Input((View *) stackView, "Video mode", videoModeSelect);
			release(videoModeSelect);

			Ui_CvarCheckbox((View *) stackView, "Fullscreen", r_fullscreen);
			Ui_CvarCheckbox((View *) stackView, "Vertical Sync", r_swap_interval);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "OPTIONS");

			box->view.autoresizingMask |= ViewAutoresizingWidth;

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			Select *anisoSelect = (Select *) alloc(CvarSelect, initWithVariable, r_anisotropy);

			$(anisoSelect, addOption, "16x", (ident) 16);
			$(anisoSelect, addOption, "8x", (ident) 8);
			$(anisoSelect, addOption, "4x", (ident) 4);
			$(anisoSelect, addOption, "Off", (ident) 0);

			Ui_Input((View *) stackView, "Anisotropy", (Control *) anisoSelect);
			release(anisoSelect);

			Select *multisampleSelect = (Select *) alloc(CvarSelect, initWithVariable, r_multisample);

			$(multisampleSelect, addOption, "8x", (ident) 4);
			$(multisampleSelect, addOption, "4x", (ident) 2);
			$(multisampleSelect, addOption, "2x", (ident) 1);
			$(multisampleSelect, addOption, "Off", (ident) 0);

			Ui_Input((View *) stackView, "Multisample", (Control *) multisampleSelect);
			release(multisampleSelect);

			Select *shadowsSelect = (Select *) alloc(CvarSelect, initWithVariable, r_shadows);

			$(shadowsSelect, addOption, "High", (ident) 2);
			$(shadowsSelect, addOption, "Low", (ident) 1);
			$(shadowsSelect, addOption, "Off", (ident) 0);

			Ui_Input((View *) stackView, "Shadows", (Control *) shadowsSelect);
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
		StackView *column = alloc(StackView, initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "PICTURE");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			Ui_CvarSlider((View *) stackView, "Brightness", r_brightness, 0.1, 2.0, 0.1);
			Ui_CvarSlider((View *) stackView, "Contrast", r_contrast, 0.1, 2.0, 0.1);
			Ui_CvarSlider((View *) stackView, "Gamma", r_gamma, 0.1, 2.0, 0.1);
			Ui_CvarSlider((View *) stackView, "Modulate", r_modulate, 0.1, 5.0, 0.1);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = alloc(Box, initWithFrame, NULL);
			$(box->label, setText, "SOUND");

			StackView *stackView = alloc(StackView, initWithFrame, NULL);

			Ui_CvarSlider((View *) stackView, "Volume", s_volume, 0.0, 1.0, 0.0);
			Ui_CvarSlider((View *) stackView, "Music Volume", s_music_volume, 0.0, 1.0, 0.0);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			this->panel->accessoryView->view.hidden = false;

			Ui_Button((View *) this->panel->accessoryView, "Apply", applyAction, self, NULL);
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

