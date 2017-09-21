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
#include "Theme.h"

#define _Class _PlayerViewController

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

	PlayerViewController *self = (PlayerViewController *) select->delegate.self;

	cgi.CvarSet(cg_skin->name, option->title->text);

	$((View *) self->playerModelView, updateBindings);
}

/**
 * @brief HSVColorPickerDelegate callback for tint selection.
 */
static void didPickColor(HSVColorPicker *hsvColorPicker, double hue, double saturation, double value) {

	PlayerViewController *this = hsvColorPicker->delegate.self;

	if (hsvColorPicker == this->effectColorPicker) {

	} else if (hsvColorPicker == this->pantsColorPicker) {

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

	PlayerViewController *this = (PlayerViewController *) self;

	release(this->effectColorPicker);
	release(this->hookStyleSelect);
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

	PlayerViewController *this = (PlayerViewController *) self;

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);

	Panel *panel = $(theme, panel);

	$(theme, attach, panel);
	$(theme, target, panel->contentView);

	StackView *columns = $(theme, columns, 2);
	$(theme, attach, columns);

	{
		$(theme, targetSubview, columns, 0);

		{
			Box *box = $(theme, box, "Profile");

			$(theme, attach, box);
			$(theme, target, box->contentView);

			$(theme, textView, "Name", "name");

			this->skinSelect = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			this->skinSelect->comparator = sortSkins;
			this->skinSelect->delegate.self = this;
			this->skinSelect->delegate.didSelectOption = didSelectSkin;

			cgi.EnumerateFiles("players/*", enumerateModels, this->skinSelect);

			$(theme, control, "Player model", this->skinSelect);

			this->hookStyleSelect = (Select *) $(alloc(CvarSelect), initWithVariable, cg_hook_style);
			((CvarSelect *) this->hookStyleSelect)->expectsStringValue = true;

			$(this->hookStyleSelect, addOption, "pull", (ident) HOOK_PULL);
			$(this->hookStyleSelect, addOption, "swing", (ident) HOOK_SWING);

			const g_hook_style_t style = g_strcmp0(cg_hook_style->string, "swing") ? HOOK_PULL : HOOK_SWING;

			$(this->hookStyleSelect, selectOptionWithValue, (ident) (intptr_t) style);

			$(theme, control, "Hook style", this->hookStyleSelect);

			$(theme, slider, "Handicap", cg_handicap->name, 50.0, 100.0, 5.0);

			release(box);
		}

		$(theme, targetSubview, columns, 0);

		{
			Box *box = $(theme, box, "Colors");

			$(theme, attach, box);
			$(theme, target, box->contentView);

			this->effectColorPicker = $(alloc(HSVColorPicker), initWithFrame, NULL);
			assert(this->effectColorPicker);

			this->effectColorPicker->delegate.self = this;
			this->effectColorPicker->delegate.didPickColor = didPickColor;

			this->effectColorPicker->saturationInput->stackView.view.hidden = true;
			this->effectColorPicker->valueInput->stackView.view.hidden = true;

			$(theme, control, "Effects", this->effectColorPicker);

			this->shirtColorPicker = $(alloc(HSVColorPicker), initWithFrame, NULL);
			assert(this->shirtColorPicker);

			this->shirtColorPicker->colorView->hidden = true;

			this->shirtColorPicker->delegate.self = self;
			this->shirtColorPicker->delegate.didPickColor = didPickColor;

			this->shirtColorPicker->valueSlider->min = 0.5;

			$(theme, control, "Shirt", this->shirtColorPicker);

			this->pantsColorPicker = $(alloc(HSVColorPicker), initWithFrame, NULL);
			assert(this->pantsColorPicker);

			this->pantsColorPicker->colorView->hidden = true;

			this->pantsColorPicker->delegate.self = self;
			this->pantsColorPicker->delegate.didPickColor = didPickColor;

			this->pantsColorPicker->valueSlider->min = 0.5;

			$(theme, control, "Pants", this->pantsColorPicker);

			// TODO data binding color, cg_tint_r, cg_tint_b (TODO: rename these cvars)

			release(box);
		}
	}

	{
		$(theme, targetSubview, columns, 1);

		{
			const SDL_Rect frame = { .w = 400, .h = 500 };
			this->playerModelView = $(alloc(PlayerModelView), initWithFrame, &frame, ControlStyleCustom);

			$(theme, attach, this->playerModelView);
		}
	}

	release(columns);
	release(panel);
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
 * @fn Class *PlayerViewController::_PlayerViewController(void)
 * @memberof PlayerViewController
 */
Class *_PlayerViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayerViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(PlayerViewController);
		clazz.interfaceOffset = offsetof(PlayerViewController, interface);
		clazz.interfaceSize = sizeof(PlayerViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
