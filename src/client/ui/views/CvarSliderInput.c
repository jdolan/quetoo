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

#include <ObjectivelyMVC/Input.h>
#include <ObjectivelyMVC/Slider.h>

#include "CvarSliderInput.h"

#define _Class _CvarSliderInput

#pragma mark - Object

static void dealloc(Object *self) {

	CvarSliderInput *this = (CvarSliderInput *) self;

	release(this->output);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
void updateBindings(View *self) {

	super(View, self, updateBindings);

	CvarSliderInput *this = (CvarSliderInput *) self;

	Slider *slider = (Slider *) this->input.control;

	$(slider, setValue, this->var->value);
}

#pragma mark - CvarSliderInput

/**
 * @brief SliderDelegate callback.
 */
static void didSetValue(Slider *slider) {

	const CvarSliderInput *this = (CvarSliderInput *) slider->delegate.data;

	$(this->output, setText, va("%.1f", slider->value));

	Cvar_SetValue(this->var->name, slider->value);
}

/**
 * @fn CvarSliderInput *CvarSliderInput::initWithVariable(CvarSliderInput *self, cvar_t *var, const char *name, double min, double max, double step)
 *
 * @memberof CvarSliderInput
 */
static CvarSliderInput *initWithVariable(CvarSliderInput *self, cvar_t *var, const char *name, double min, double max, double step) {

	Control *control = (Control *) $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
	control->view.frame.w = CVAR_SLIDER_INPUT_SLIDER_WIDTH;

	Label *label = $(alloc(Label), initWithText, name, NULL);
	label->view.frame.w = CVAR_SLIDER_INPUT_LABEL_WIDTH;

	self = (CvarSliderInput *) super(Input, self, initWithOrientation, InputOrientationLeft, control, label);
	if (self) {

		self->var = var;
		assert(self->var);

		self->output = $(alloc(Label), initWithText, "", NULL);
		assert(self->output);

		self->output->view.alignment = ViewAlignmentMiddleLeft;
		self->output->view.frame.w = CVAR_SLIDER_INPUT_OUTPUT_WIDTH;

		$((View *) self, addSubview, (View *) self->output);
		$((View *) self, sizeToFit);
	}

	Slider *slider = (Slider *) control;

	slider->delegate.data = self;
	slider->delegate.didSetValue = didSetValue;

	slider->min = min, slider->max = max, slider->step = step;
	$(slider, setValue, var->value);

	return self;
}

/**
 * @fn void CvarSliderInput::input(View *view, cvar_t *var, const char *name, double min, double max, double step)
 *
 * @memberof CvarSliderInput
 */
static void input(View *view, cvar_t *var, const char *name, double min, double max, double step) {

	assert(view);

	CvarSliderInput *input = $(alloc(CvarSliderInput), initWithVariable, var, name, min, max, step);
	assert(input);

	$(view, addSubview, (View *) input);
	release(input);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((CvarSliderInputInterface *) clazz->interface)->initWithVariable = initWithVariable;
	((CvarSliderInputInterface *) clazz->interface)->input = input;
}

Class _CvarSliderInput = {
	.name = "CvarSliderInput",
	.superclass = &_Input,
	.instanceSize = sizeof(CvarSliderInput),
	.interfaceOffset = offsetof(CvarSliderInput, interface),
	.interfaceSize = sizeof(CvarSliderInputInterface),
	.initialize = initialize,
};

#undef _Class

