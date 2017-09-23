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
#include "CrosshairView.h"
#include "CvarSelect.h"
#include "CvarSlider.h"

#include "Theme.h"

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
static void didPickCrosshairColor(HueColorPicker *colorPicker, double hue, double saturation, double value) {

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

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	LookViewController *this = (LookViewController *) self;

	release(this->crosshairView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Look");

	LookViewController *this = (LookViewController *) self;

	TextViewDelegate delegate = {
		.self = this,
		.didEndEditing = didBindKey
	};

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);
	$(theme, attach, columns);

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Response");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "Sensitivity", "m_sensitivity", 0.1, 6.0, 0.1, NULL);
		$(theme, slider, "Zoom Sensitivity", "m_sensitivity_zoom", 0.1, 6.0, 0.1, NULL);
		$(theme, checkbox, "Invert mouse", "m_invert");
		$(theme, checkbox, "Smooth mouse", "m_interpolate");

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Field of view");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "FOV", cg_fov->name, 80.0, 130.0, 5.0, NULL);
		$(theme, slider, "Zoom FOV", cg_fov_zoom->name, 20.0, 70.0, 5.0, NULL);
		$(theme, bindTextView, "Zoom", "+ZOOM", &delegate);
		$(theme, slider, "Zoom speed", cg_fov_interpolate->name, 0.0, 2.0, 0.1, NULL);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Crosshair");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		Select *crosshair = (Select *) $(alloc(CvarSelect), initWithVariable, cg_draw_crosshair);
		assert(crosshair);

		$(crosshair, addOption, "", NULL);
		cgi.EnumerateFiles("pics/ch*", enumerateCrosshairs, crosshair);

		crosshair->delegate.self = this;
		crosshair->delegate.didSelectOption = didSelectCrosshair;

		$(theme, control, "Crosshair", crosshair);

		this->crosshairView = $(alloc(CrosshairView), initWithFrame, &MakeRect(0, 0, 72, 72));
		assert(this->crosshairView);

		this->crosshairView->view.alignment = ViewAlignmentMiddleCenter;

		$((View *) this->crosshairView, updateBindings);

		$(theme, attach, this->crosshairView);

		HueColorPicker *crosshairColor = $(alloc(HueColorPicker), initWithFrame, NULL, ControlStyleDefault);
		assert(crosshairColor);

		crosshairColor->colorView->hidden = true;

		crosshairColor->delegate.self = this;
		crosshairColor->delegate.didPickColor = didPickCrosshairColor;

		$(theme, control, "Color", crosshairColor);

		SliderDelegate crosshairScaleDelegate = {
			.self = this,
			.didSetValue = didSetCrosshairScale
		};

		$(theme, slider, "Scale", cg_draw_crosshair_scale->name, 0.1, 2.0, 0.1, &crosshairScaleDelegate);

		$(theme, checkbox, "Pulse on Pickup", cg_draw_crosshair_pulse->name);

		release(box);
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
 * @fn Class *LookViewController::_LookViewController(void)
 * @memberof LookViewController
 */
Class *_LookViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "LookViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(LookViewController);
		clazz.interfaceOffset = offsetof(LookViewController, interface);
		clazz.interfaceSize = sizeof(LookViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

