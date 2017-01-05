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

#include "CvarSlider.h"

#define _Class _CvarSlider

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	CvarSlider *this = (CvarSlider *) self;

	$((Slider *) this, setValue, this->var->value);
}

#pragma mark - CvarSlider

/**
 * @brief SliderDelegate callback.
 */
static void didSetValue(Slider *slider) {

	const CvarSlider *this = (CvarSlider *) slider;

	cgi.CvarSetValue(this->var->name, slider->value);
}

/**
 * @fn CvarSlider *CvarSlider::initWithVariable(CvarSlider *self, cvar_t *var, double min, double max, double step)
 *
 * @memberof CvarSlider
 */
static CvarSlider *initWithVariable(CvarSlider *self, cvar_t *var, double min, double max, double step) {

	self = (CvarSlider *) super(Slider, self, initWithFrame, NULL, ControlStyleDefault);
	if (self) {

		self->var = var;
		assert(self->var);

		Slider *this = (Slider *) self;

		this->delegate.didSetValue = didSetValue;

		this->min = min;
		this->max = max;
		this->step = step;

		$(this, setValue, var->value);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((CvarSliderInterface *) clazz->def->interface)->initWithVariable = initWithVariable;
}

Class *_CvarSlider(void) {
	static Class _class;
	
	if (!_class.name) {
		_class.name = "CvarSlider";
		_class.superclass = _Slider();
		_class.instanceSize = sizeof(CvarSlider);
		_class.interfaceOffset = offsetof(CvarSlider, interface);
		_class.interfaceSize = sizeof(CvarSliderInterface);
		_class.initialize = initialize;
	}

	return &_class;
}

#undef _Class

