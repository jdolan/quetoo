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

#include "VariableSliderInput.h"

#define _Class _VariableSliderInput

#pragma mark - Object

static void dealloc(Object *self) {

	VariableSliderInput *this = (VariableSliderInput *) self;

	release(this->output);

	super(Object, self, dealloc);
}

#pragma mark - View

void respondToEvent(View *self, const SDL_Event *event) {

	VariableSliderInput *this = (VariableSliderInput *) self;

	Slider *slider = (Slider *) this->input.control;

	$(slider, setValue, this->var->value);

	super(View, self, respondToEvent, event);
}

#pragma mark - VariableSliderInput

/**
 * @brief SliderDelegate callback.
 */
static void didSetValue(Slider *slider) {

	const VariableSliderInput *this = (VariableSliderInput *) slider->delegate.data;

	$(this->output, setText, va("%.1f", slider->value));

	Cvar_SetValue(this->var->name, slider->value);
}

/**
 * @fn VariableSliderInput *VariableSliderInput::initWithVariable(VariableSliderInput *self, cvar_t *var, const char *name, double min, double max, double step)
 *
 * @memberof VariableSliderInput
 */
static VariableSliderInput *initWithVariable(VariableSliderInput *self, cvar_t *var, const char *name, double min, double max, double step) {

	Control *control = (Control *) $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
	control->view.frame.w = VARIABLE_SLIDER_INPUT_SLIDER_WIDTH;

	Label *label = $(alloc(Label), initWithText, name, NULL);
	label->view.frame.w = VARIABLE_SLIDER_INPUT_LABEL_WIDTH;

	self = (VariableSliderInput *) super(Input, self, initWithOrientation, InputOrientationLeft, control, label);
	if (self) {

		self->name = name;
		assert(self->name);

		self->var = var;
		assert(self->var);

		self->output = $(alloc(Label), initWithText, "", NULL);
		assert(self->output);

		self->output->view.alignment = ViewAlignmentMiddleLeft;
		self->output->view.frame.w = VARIABLE_SLIDER_INPUT_OUTPUT_WIDTH;

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

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

	((VariableSliderInputInterface *) clazz->interface)->initWithVariable = initWithVariable;
}

Class _VariableSliderInput = {
	.name = "VariableSliderInput",
	.superclass = &_Input,
	.instanceSize = sizeof(VariableSliderInput),
	.interfaceOffset = offsetof(VariableSliderInput, interface),
	.interfaceSize = sizeof(VariableSliderInputInterface),
	.initialize = initialize,
};

#undef _Class

