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

#include "LookViewController.h"

#define _Class _LookViewController

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
 * @brief SelectDelegate callback for crosshair selection.
 */
static void didSelectCrosshair(Select *select, Option *option) {

	LookViewController *this = (LookViewController *) select->delegate.self;

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief HueColorPickerDelegate callback for crosshair color selection.
 */
static void didPickCrosshairColor(HueColorPicker *hueColorPicker, double hue, double saturation, double value) {

	LookViewController *this = (LookViewController *) hueColorPicker->delegate.self;

	if (hue < 1.0) {
		cgi.CvarSet(cg_draw_crosshair_color->name, "default");

		hueColorPicker->colorView->backgroundColor = Colors.Charcoal;

		$(hueColorPicker->hueSlider->label, setText, "");
	} else {
		const SDL_Color color = $(hueColorPicker, rgbColor);
		cgi.CvarSet(cg_draw_crosshair_color->name, MVC_RGBToHex(&color));
	}

	$((View *) this->crosshairView, updateBindings);
}

/**
 * @brief SliderDelegate callback for crosshair scale.
 */
static void didSetCrosshairScale(Slider *slider) {

	LookViewController *this = (LookViewController *) slider->delegate.self;

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

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	LookViewController *this = (LookViewController *) self;

	release(this->crosshairColorPicker);
	release(this->crosshairView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	LookViewController *this = (LookViewController *) self;

	Select *crosshair;
	Slider *crosshairScale;
	Outlet outlets[] = MakeOutlets(
		MakeOutlet("crosshair", &crosshair),
		MakeOutlet("crosshairScale", &crosshairScale),
		MakeOutlet("crosshairColor", &this->crosshairColorPicker),
		MakeOutlet("crosshairView", &this->crosshairView)
	);

	cgi.WakeView(self->view, "ui/controls/LookViewController.json", outlets);

	$(self->view, enumerateSelection, "BindTextView", setDelegate, self);

	$(crosshair, addOption, "", NULL);
	cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshair);

	crosshair->delegate.self = this;
	crosshair->delegate.didSelectOption = didSelectCrosshair;

	crosshairScale->delegate.self = this;
	crosshairScale->delegate.didSetValue = didSetCrosshairScale;

	this->crosshairColorPicker->delegate.self = this;
	this->crosshairColorPicker->delegate.didPickColor = didPickCrosshairColor;
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	LookViewController *this = (LookViewController *) self;

	const SDL_Color color = MVC_HexToRGBA(cg_draw_crosshair_color->string);
	if (color.r || color.g || color.b) {
		$(this->crosshairColorPicker, setRGBColor, &color);
	} else {
		$(this->crosshairColorPicker, setColor, 0.0, 1.0, 1.0);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;
}

/**
 * @fn Class *LookViewController::_LookViewController(void)
 * @memberof LookViewController
 */
Class *_LookViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "LookViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(LookViewController),
			.interfaceOffset = offsetof(LookViewController, interface),
			.interfaceSize = sizeof(LookViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}
#undef _Class

