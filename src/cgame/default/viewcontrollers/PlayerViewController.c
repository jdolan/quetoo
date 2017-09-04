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

#pragma mark - Actions & delegates

/**
 * @brief ColorPickerDelegate callback for tint selection.
 */
static void didPickColor(ColorPicker *colorPicker, SDL_Color *color) {

	PlayerViewController *this = colorPicker->delegate.self;

	if (colorPicker == this->tintGColorPicker) {

	} else if (colorPicker == this->tintRColorPicker) {

	} else {
		assert(false);
	}

//	ColorPicker *colorSelect = (ColorPicker *) this->tintRColorPicker;
//
//	color_t color = ColorFromRGB(colorSelect->color.r, colorSelect->color.g, colorSelect->color.b);
//	char hexColor[COLOR_MAX_LENGTH];
//
//	if (color.r == 0 && color.g == 0 && color.b == 0) {
//		cgi.CvarSet(cg_tint_r->name, "default");
//	} else {
//		ColorToHex(color, hexColor, sizeof(hexColor));
//
//		cgi.CvarSet(cg_tint_r->name, hexColor);
//	}

	if (this->playerModelView) {
		$((View *) this->playerModelView, updateBindings);
	}
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	PlayerViewController *this = (PlayerViewController *) self;

	release(this->tintRColorPicker);
	release(this->tintGColorPicker);

	super(Object, self, dealloc);
}
#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	PlayerViewController *this = (PlayerViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->spacing = DEFAULT_PANEL_SPACING;

	columns->axis = StackViewAxisHorizontal;
	columns->distribution = StackViewDistributionFill;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "Profile");

			box->view.autoresizingMask |= ViewAutoresizingWidth;

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			// Player name

			Cgui_CvarTextView((View *) stackView, "Name", "name");

			// Player model

			Control *skinSelect = (Control *) $(alloc(SkinSelect), initWithFrame, NULL, ControlStyleDefault);

			$(skinSelect, addActionForEventType, SDL_MOUSEBUTTONUP, selectSkin, self, NULL);

			Cgui_Input((View *) stackView, "Player skin", skinSelect);
			release(skinSelect);

			// Hook style

			CvarSelect *hookSelect = (CvarSelect *) $(alloc(CvarSelect), initWithVariable, cg_hook_style);
			hookSelect->expectsStringValue = true;

			$((Select *) hookSelect, addOption, "pull", (ident) HOOK_PULL);
			$((Select *) hookSelect, addOption, "swing", (ident) HOOK_SWING);

			g_hook_style_t hook_style = HOOK_PULL;

			if (!g_strcmp0(cg_hook_style->string, "swing")) {
				hook_style = HOOK_SWING;
			}

			$((Select *) hookSelect, selectOptionWithValue, (ident) (intptr_t) hook_style);

			Cgui_Input((View *) stackView, "Hook style", (Control *) hookSelect);

			release(hookSelect);

			// Handicap

			Cgui_CvarSliderInput((View *) stackView, "Handicap", cg_handicap->name, 50.0, 100.0, 5.0);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "Colors");

			box->view.autoresizingMask |= ViewAutoresizingWidth;

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			// Effect color

			double hue = -1;
			if (g_strcmp0(cg_color->string, "default")) {
				hue = cg_color->integer;
			}

			Slider *hueSlider = (Slider *) $(alloc(HueSlider), initWithVariable, hue, selectEffectColor);

			Cgui_Input((View *) stackView, "Effects", (Control *) hueSlider);

			// Shirt color

			const SDL_Rect colorFrame = MakeRect(0, 0, 200, 96); // Used for both shirt and pants
			color_t color;

			this->tintRColorPicker = (ColorPicker *) $(alloc(ColorPicker), initWithFrame, &colorFrame);

			this->tintRColorPicker->delegate.self = self;
			this->tintRColorPicker->delegate.didPickColor= didPickColor;

			const char *tintR = cg_tint_r->string;

			if (!g_ascii_strcasecmp(tintR, "default")) {
				color.r = color.g = color.b = 0;
			} else {
				ColorParseHex(tintR, &color);
			}

			$(this->tintRColorPicker, setColor, (SDL_Color) { .r = color.r, .g = color.g, .b = color.b });

			Cgui_Input((View *) stackView, "Shirt", (Control *) this->tintRColorPicker);

			// Pants color

			this->tintGColorPicker = (ColorPicker *) $(alloc(ColorPicker), initWithFrame, &colorFrame);

			this->tintGColorPicker->delegate.self = self;
			this->tintGColorPicker->delegate.didPickColor = didPickColor;

			const char *tintG = cg_tint_g->string;

			if (!g_ascii_strcasecmp(tintG, "default")) {
				color.r = color.g = color.b = 0;
			} else {
				ColorParseHex(tintG, &color);
			}

			$(this->tintGColorPicker, setColor, (SDL_Color) { .r = color.r, .g = color.g, .b = color.b });

			Cgui_Input((View *) stackView, "Pants", (Control *) this->tintGColorPicker);

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
			this->playerModelView = $(alloc(PlayerModelView), initWithFrame, &frame, ControlStyleCustom);

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

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

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
