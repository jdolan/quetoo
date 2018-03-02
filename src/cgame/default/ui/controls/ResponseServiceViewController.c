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

#include "ResponseServiceViewController.h"

#define _Class _ResponseServiceViewController

#pragma mark - Crosshair selection

/**
 * @brief Fs_EnumerateFunc for crosshair selection.
 */
static void enumerateCrosshairs(const char *path, void *data) {
	char name[MAX_QPATH];

	StripExtension(Basename(path), name);

	intptr_t value = strtol(name + strlen("ch"), NULL, 10);
	assert(value);

	$((Select *) data, addOption, name, (ident) value);
}

/**
 * @brief Alphabetical sorting.
 */
static Order sortAlphabetical(const ident id1, const ident id2) {

	Option *opt1 = (Option *) id1;
	Option *opt2 = (Option *) id2;

	int sort = g_strcmp0(opt1->title->text, opt2->title->text);

	if (sort <= 0) {
		return OrderAscending;
	} else {
		return OrderDescending;
	}
}

/**
 * @brief SelectDelegate callback for crosshair selection.
 */
static void didSelectCrosshair(Select *select, Option *option) {

	ResponseServiceViewController *this = (ResponseServiceViewController *) select->delegate.self;

	$((View *) select, updateBindings);
	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief HueColorPickerDelegate callback for crosshair color selection.
 */
static void didPickCrosshairColor(HueColorPicker *hueColorPicker, double hue, double saturation, double value) {

	ResponseServiceViewController *this = (ResponseServiceViewController *) hueColorPicker->delegate.self;

	if (hue < 1.0) {
		cgi.SetCvarString(cg_draw_crosshair_color->name, "default");

		hueColorPicker->colorView->backgroundColor = Colors.Charcoal;

		$(hueColorPicker->hueSlider->label, setText, "");
	} else {
		const SDL_Color color = $(hueColorPicker, rgbColor);
		cgi.SetCvarString(cg_draw_crosshair_color->name, MVC_RGBToHex(&color));
	}

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief SliderDelegate callback for crosshair scale.
 */
static void didSetCrosshairScale(Slider *slider, double value) {

	ResponseServiceViewController *this = (ResponseServiceViewController *) slider->delegate.self;

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief SliderDelegate callback for crosshair opcaity.
 */
static void didSetCrosshairAlpha(Slider *slider, double value) {

	ResponseServiceViewController *this = (ResponseServiceViewController *) slider->delegate.self;

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief SelectDelegate callback for crosshair health style.
 */
static void didSelectCrosshairHealth(Select *select, Option *option) {

	ResponseServiceViewController *this = (ResponseServiceViewController *) select->delegate.self;

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief TextViewDelegate callback for binding keys.
 */
static void didBindKey(TextView *textView) {

	const ViewController *this = textView->delegate.self;

	$(this->view, updateBindings);
}

/**
 * @brief ViewEnumerator for setting the TextViewDelegate.
 */
static void setDelegate(View *view, ident data) {

	((TextView *) view)->delegate = (TextViewDelegate) {
		.self = data,
		.didEndEditing = didBindKey
	};
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	$(self->view, awakeWithResourceName, "ui/controls/ResponseServiceViewController.json");

	ResponseServiceViewController *this = (ResponseServiceViewController *) self;

	Slider *crosshairScale;
	Slider *crosshairAlpha;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("crosshair", &this->crosshair),
		MakeOutlet("crosshairColor", &this->crosshairColorPicker),
		MakeOutlet("crosshairAlpha", &crosshairAlpha),
		MakeOutlet("crosshairScale", &crosshairScale),
		MakeOutlet("crosshairHealth", &this->crosshairHealth),
		MakeOutlet("crosshairView", &this->crosshairView)
	);

	$(self->view, resolve, outlets);

	self->view->stylesheet = $(alloc(Stylesheet), initWithResourceName, "ui/controls/ResponseServiceViewController.css");
	assert(self->view->stylesheet);

	$(self->view, enumerateSelection, "BindTextView", setDelegate, self);

	this->crosshair->comparator = sortAlphabetical;

	$(this->crosshair, addOption, "", (ident) 0);
	cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, this->crosshair);

	this->crosshair->delegate.self = this;
	this->crosshair->delegate.didSelectOption = didSelectCrosshair;

	crosshairScale->delegate.self = this;
	crosshairScale->delegate.didSetValue = didSetCrosshairScale;

	crosshairAlpha->delegate.self = this;
	crosshairAlpha->delegate.didSetValue = didSetCrosshairAlpha;

	$(this->crosshairHealth, addOption, "None", (ident) 0); // Add options as needed; this is ugly but functional for now
	$(this->crosshairHealth, addOption, "1", (ident) 1);
	$(this->crosshairHealth, addOption, "2", (ident) 2);
	$(this->crosshairHealth, addOption, "3", (ident) 3);
	$(this->crosshairHealth, addOption, "4", (ident) 4);
	$(this->crosshairHealth, addOption, "5", (ident) 5);

	this->crosshairHealth->delegate.self = this;
	this->crosshairHealth->delegate.didSelectOption = didSelectCrosshairHealth;

	this->crosshairColorPicker->delegate.self = this;
	this->crosshairColorPicker->delegate.didPickColor = didPickCrosshairColor;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	ResponseServiceViewController *this = (ResponseServiceViewController *) self;

	const SDL_Color color = MVC_HexToRGBA(cg_draw_crosshair_color->string);
	if (color.r || color.g || color.b) {
		$(this->crosshairColorPicker, setRGBColor, &color);
	} else {
		$(this->crosshairColorPicker, setColor, 0.0, 1.0, 1.0);
	}

	$(this->crosshair, selectOptionWithValue, (ident) ((size_t) cg_draw_crosshair->integer)); // Double cast to silence warnings
	$(this->crosshairHealth, selectOptionWithValue, (ident) ((size_t) cg_draw_crosshair_health->integer));
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
 * @fn Class *ResponseServiceViewController::_ResponseServiceViewController(void)
 * @memberof ResponseServiceViewController
 */
Class *_ResponseServiceViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "ResponseServiceViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(ResponseServiceViewController),
			.interfaceOffset = offsetof(ResponseServiceViewController, interface),
			.interfaceSize = sizeof(ResponseServiceViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}
#undef _Class
