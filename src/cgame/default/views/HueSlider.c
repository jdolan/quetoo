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

#include "HueSlider.h"

#define _Class _HueSlider

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	Slider *this = (Slider *) self;

	this->value = floor(this->value);

	$(this, setValue, this->value);
}

#pragma mark - HueSlider

/**
 * @brief SliderDelegate callback.
 */
static void didSetValue(Slider *self) {

	vec3_t hsv = { 0.0, 1.0, 1.0 };
	hsv[0] = Max(self->value, 0.0);

	color_t rgb = ColorFromHSV(hsv);

	if (self->value >= 0) {
		self->handle->view.backgroundColor = (SDL_Color) {
			.r = rgb.r,
			.g = rgb.g,
			.b = rgb.b,
			.a = 255
		};
	} else {
		$(self->label, setText, "+");

		self->handle->view.backgroundColor = QColors.Dark;
	}

	if (((HueSlider *) self)->changeFunction) {
		((HueSlider *) self)->changeFunction(self->value);
	}
}

/**
 * @fn HueSlider *HueSlider::initWithVariable(HueSlider *self, double hue)
 *
 * @memberof HueSlider
 */
static HueSlider *initWithVariable(HueSlider *self, double hue, void (*changeFunction)(double hue)) {

	self = (HueSlider *) super(Slider, self, initWithFrame, NULL, ControlStyleDefault);
	if (self) {

		self->changeFunction = changeFunction;
		assert(self->changeFunction);

		Slider *this = (Slider *) self;

		this->delegate.didSetValue = didSetValue;

		this->handle->bevel = ControlBevelTypeNone;

		this->min = -5.0;
		this->max = 360.0;
		this->step = 5.0;

		$(this, setLabelFormat, "%0.0f");

		this->snapToStep = true;

		$(this, setValue, hue);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((HueSliderInterface *) clazz->def->interface)->initWithVariable = initWithVariable;
}

/**
 * @fn Class *HueSlider::_HueSlider(void)
 * @memberof HueSlider
 */
Class *_HueSlider(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "HueSlider";
		clazz.superclass = _Slider();
		clazz.instanceSize = sizeof(HueSlider);
		clazz.interfaceOffset = offsetof(HueSlider, interface);
		clazz.interfaceSize = sizeof(HueSliderInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
