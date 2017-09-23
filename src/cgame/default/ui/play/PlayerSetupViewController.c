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

#include "PlayerSetupViewController.h"

#include "CvarSelect.h"
#include "Theme.h"

#define _Class _PlayerSetupViewController

#pragma mark - Actions & delegates

/**
 * @brief Comparator for SkinSelect.
 */
static Order sortSkins(const ident a, const ident b) {

	const char *c = ((const Option *) a)->title->text;
	const char *d = ((const Option *) b)->title->text;

	return g_strcmp0(c, d) < 0 ? OrderAscending : OrderDescending;
}

/**
 * @brief Fs_EnumerateFunc for resolving available skins for a give model.
 */
static void enumerateSkins(const char *path, void *data) {
	char name[MAX_QPATH];

	Select *select = (Select *) data;

	char *s = strstr(path, "players/");
	if (s) {
		StripExtension(s + strlen("players/"), name);

		if (g_str_has_suffix(name, "_i")) {
			name[strlen(name) - strlen("_i")] = '\0';

			Array *options = (Array *) select->options;
			for (size_t i = 0; i < options->count; i++) {

				const Option *option = $(options, objectAtIndex, i);
				if (g_strcmp0(option->title->text, name) == 0) {
					return;
				}
			}

			ident value = (ident) options->count;
			$(select, addOption, name, value);

			if (g_strcmp0(cg_skin->string, name) == 0) {
				$(select, selectOptionWithValue, value);
			}
		}
	}
}

/**
 * @brief Fs_EnumerateFunc for resolving available models.
 */
static void enumerateModels(const char *path, void *data) {
	cgi.EnumerateFiles(va("%s/*.tga", path), enumerateSkins, data);
}

/**
 * @brief ActionFunction for skin selection.
 */
static void didSelectSkin(Select *select, Option *option) {

	PlayerSetupViewController *self = (PlayerSetupViewController *) select->delegate.self;

	cgi.CvarSet(cg_skin->name, option->title->text);

	$((View *) self->playerModelView, updateBindings);
}

/**
 * @brief HueColorPickerDelegate callback for effect color selection.
 */
static void didPickEffectColor(HueColorPicker *hueColorPicker, double hue, double saturation, double value) {

	// TODO: Data binding to color cvar

}

/**
 * @brief HSVColorPickerDelegate callback for player color selection.
 */
static void didPickPlayerColor(HSVColorPicker *hsvColorPicker, double hue, double saturation, double value) {

	PlayerSetupViewController *this = hsvColorPicker->delegate.self;

	if (hsvColorPicker == this->pantsColorPicker) {

	} else if (hsvColorPicker == this->shirtColorPicker) {

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

	PlayerSetupViewController *this = (PlayerSetupViewController *) self;

	release(this->effectColorPicker);
	release(this->pantsColorPicker);
	release(this->shirtColorPicker);
	release(this->skinSelect);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Player setup");

	PlayerSetupViewController *this = (PlayerSetupViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);
	$(theme, attach, columns);

	{
		$(theme, targetSubview, columns, 0);

		{
			Box *box = $(theme, box, "Player");

			$(theme, attach, box);
			$(theme, target, box->contentView);

			$(theme, textView, "Name", "name");

			this->skinSelect = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			this->skinSelect->comparator = sortSkins;
			this->skinSelect->delegate.self = this;
			this->skinSelect->delegate.didSelectOption = didSelectSkin;

			cgi.EnumerateFiles("players/*", enumerateModels, this->skinSelect);

			$(theme, control, "Player model", this->skinSelect);

			$(theme, slider, "Handicap", cg_handicap->name, 50.0, 100.0, 5.0);

			release(box);
		}

		$(theme, targetSubview, columns, 0);

		{
			Box *box = $(theme, box, "Colors");

			$(theme, attach, box);
			$(theme, target, box->contentView);

			this->effectColorPicker = $(alloc(HueColorPicker), initWithFrame, NULL, ControlStyleDefault);
			assert(this->effectColorPicker);

			this->effectColorPicker->delegate.self = this;
			this->effectColorPicker->delegate.didPickColor = didPickEffectColor;

			$(theme, control, "Effects", this->effectColorPicker);

			this->shirtColorPicker = $(alloc(HSVColorPicker), initWithFrame, NULL, ControlStyleDefault);
			assert(this->shirtColorPicker);

			this->shirtColorPicker->colorView->hidden = true;

			this->shirtColorPicker->delegate.self = self;
			this->shirtColorPicker->delegate.didPickColor = didPickPlayerColor;

			this->shirtColorPicker->valueSlider->min = 0.5;

			$(theme, control, "Shirt", this->shirtColorPicker);

			this->pantsColorPicker = $(alloc(HSVColorPicker), initWithFrame, NULL, ControlStyleDefault);
			assert(this->pantsColorPicker);

			this->pantsColorPicker->colorView->hidden = true;

			this->pantsColorPicker->delegate.self = self;
			this->pantsColorPicker->delegate.didPickColor = didPickPlayerColor;

			this->pantsColorPicker->valueSlider->min = 0.5;

			$(theme, control, "Pants", this->pantsColorPicker);

			// TODO data binding color, cg_tint_r, cg_tint_b (TODO: rename these cvars)

			release(box);
		}
	}


	$(theme, targetSubview, columns, 1);

	{
		this->playerModelView = $(alloc(PlayerModelView), initWithFrame, &MakeRect(0, 0, 640, 480), ControlStyleDefault);
		assert(this->playerModelView);

		$(theme, attach, this->playerModelView);
	}

	release(columns);
	release(container);
	release(theme);
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
 * @fn Class *PlayerSetupViewController::_PlayerSetupViewController(void)
 * @memberof PlayerSetupViewController
 */
Class *_PlayerSetupViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayerSetupViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(PlayerSetupViewController);
		clazz.interfaceOffset = offsetof(PlayerSetupViewController, interface);
		clazz.interfaceSize = sizeof(PlayerSetupViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
