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

			if (g_strcmp0(cg_skin->string, name) == 0 && $(select, selectedOption) == NULL) {
				Option *option = $(select, optionWithValue, value);
				$(option, setSelected, true);
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

	PlayerSetupViewController *this = hueColorPicker->delegate.self;

	if (hue < 0.0) {
		cgi.CvarSet(cg_color->name, "default");

		this->effectsColorPicker->colorView->backgroundColor = Colors.Charcoal;

		$(this->effectsColorPicker->hueSlider->label, setText, "");
	} else {
		cgi.CvarSetValue(cg_color->name, (vec_t) hueColorPicker->hue);
	}
}

/**
 * @brief HSVColorPickerDelegate callback for player color selection.
 */
static void didPickPlayerColor(HSVColorPicker *hsvColorPicker, double hue, double saturation, double value) {

	PlayerSetupViewController *this = hsvColorPicker->delegate.self;

	cvar_t *var = NULL;
	if (hsvColorPicker == this->helmetColorPicker) {
		var = cg_helmet;
	} else if (hsvColorPicker == this->pantsColorPicker) {
		var = cg_pants;
	} else if (hsvColorPicker == this->shirtColorPicker) {
		var = cg_shirt;
	}
	assert(var);

	if (hue < 0.0) {
		cgi.CvarSet(var->name, "default");

		hsvColorPicker->colorView->backgroundColor = Colors.Charcoal;

		$(hsvColorPicker->hueSlider->label, setText, "");
	} else {
		const SDL_Color color = $(hsvColorPicker, rgbColor);
		cgi.CvarSet(var->name, MVC_RGBToHex(&color));
	}

	if (this->playerModelView) {
		$((View *) this->playerModelView, updateBindings);
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	PlayerSetupViewController *this = (PlayerSetupViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("skin", &this->skinSelect),
		MakeOutlet("effects", &this->effectsColorPicker),
		MakeOutlet("helmet", &this->helmetColorPicker),
		MakeOutlet("shirt", &this->shirtColorPicker),
		MakeOutlet("pants", &this->pantsColorPicker),
		MakeOutlet("player", &this->playerModelView)
	);

	$(self->view, awakeWithResourceName, "ui/play/PlayerSetupViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/PlayerSetupViewController.css");
	assert(self->view->stylesheet);

	this->skinSelect->comparator = sortSkins;
	this->skinSelect->delegate.self = this;
	this->skinSelect->delegate.didSelectOption = didSelectSkin;

	cgi.EnumerateFiles("players/*", enumerateModels, this->skinSelect);

	this->effectsColorPicker->delegate.self = this;
	this->effectsColorPicker->delegate.didPickColor = didPickEffectColor;

	this->helmetColorPicker->delegate.self = self;
	this->helmetColorPicker->delegate.didPickColor = didPickPlayerColor;

	this->shirtColorPicker->delegate.self = self;
	this->shirtColorPicker->delegate.didPickColor = didPickPlayerColor;

	this->pantsColorPicker->delegate.self = self;
	this->pantsColorPicker->delegate.didPickColor = didPickPlayerColor;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	PlayerSetupViewController *this = (PlayerSetupViewController *) self;

	if (g_strcmp0(cg_color->string, "default")) {
		$(this->effectsColorPicker, setColor, cg_color->integer, 1.0, 1.0);
	} else {
		$(this->effectsColorPicker, setColor, -1.0, 1.0, 1.0);
		didPickEffectColor(this->effectsColorPicker, -1.0, 1.0, 1.0);
	}

	const SDL_Color helmet = MVC_HexToRGBA(cg_helmet->string);
	if (helmet.r || helmet.g || helmet.b) {
		$(this->helmetColorPicker, setRGBColor, &helmet);
	} else {
		$(this->helmetColorPicker, setColor, -1.0, 1.0, 1.0);
		didPickPlayerColor(this->helmetColorPicker, -1.0, 1.0, 1.0);
	}

	const SDL_Color shirt = MVC_HexToRGBA(cg_shirt->string);
	if (shirt.r || shirt.g || shirt.b) {
		$(this->shirtColorPicker, setRGBColor, &shirt);
	} else {
		$(this->shirtColorPicker, setColor, -1.0, 1.0, 1.0);
		didPickPlayerColor(this->shirtColorPicker, -1.0, 1.0, 1.0);
	}

	const SDL_Color pants = MVC_HexToRGBA(cg_pants->string);
	if (pants.r || pants.g || pants.b) {
		$(this->pantsColorPicker, setRGBColor, &pants);
	} else {
		$(this->pantsColorPicker, setColor, -1.0, 1.0, 1.0);
		didPickPlayerColor(this->pantsColorPicker, -1.0, 1.0, 1.0);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *PlayerSetupViewController::_PlayerSetupViewController(void)
 * @memberof PlayerSetupViewController
 */
Class *_PlayerSetupViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "PlayerSetupViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(PlayerSetupViewController),
			.interfaceOffset = offsetof(PlayerSetupViewController, interface),
			.interfaceSize = sizeof(PlayerSetupViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
