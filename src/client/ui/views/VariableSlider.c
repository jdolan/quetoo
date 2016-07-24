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

#include "VariableSlider.h"

#define _Class _VariableSlider

#pragma mark - Object

static void dealloc(Object *self) {

	VariableSlider *this = (VariableSlider *) self;

	release(this->output);
	release(this->slider);

	super(Object, self, dealloc);
}

#pragma mark - View

void respondToEvent(View *self, const SDL_Event *event) {

	VariableSlider *this = (VariableSlider *) self;

	if (fabs(this->var->value - this->slider->value) > this->slider->step) {
		this->slider->value = this->var->value;

		$(this->output, setText, va("%.1f", this->slider->value));

		self->needsLayout = true;
	}

	super(View, self, respondToEvent, event);
}

#pragma mark - VariableSlider

/**
 * @brief ActionFunction for Slider.
 */
static void sliderAction(ident sender, const SDL_Event *event, ident data) {

	VariableSlider *this = (VariableSlider *) data;

	$(this->output, setText, va("%.1f", this->slider->value));

	Cvar_SetValue(this->var->name, this->slider->value);
}

/**
 * @fn VariableSlider *VariableSlider::initWithVariable(VariableSlider *self, cvar_t *var, const char *name)
 *
 * @memberof VariableSlider
 */
static VariableSlider *initWithVariable(VariableSlider *self, cvar_t *var, const char *name) {

	self = (VariableSlider *) super(StackView, self, initWithFrame, NULL);
	if (self) {

		self->stackView.axis = StackViewAxisHorizontal;

		self->name = name;
		assert(self->name);

		self->var = var;
		assert(self->var);

		self->slider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);
		assert(self->slider);

		$((Control *) self->slider, addActionForEventType, SDL_MOUSEMOTION, sliderAction, self);

		Label *label = $(alloc(Label), initWithText, name, NULL);
		assert(label);

		label->view.frame.w = VARIABLE_SLIDER_INPUT_NAME_WIDTH;
		self->slider->control.view.frame.w = VARIABLE_SLIDER_INPUT_SLIDER_WIDTH;

		Input *input = $(alloc(Input), initWithOrientation, InputOrientationLeft, (Control *) self->slider, label);
		assert(input);

		self->output = $(alloc(Label), initWithText, NULL, NULL);
		assert(self->output);

		$((View *) self, addSubview, (View *) input);
		$((View *) self, addSubview, (View *) self->output);

		$((View *) self, sizeToFit);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

	((VariableSliderInterface *) clazz->interface)->initWithVariable = initWithVariable;
}

Class _VariableSlider = {
	.name = "VariableSlider",
	.superclass = &_Input,
	.instanceSize = sizeof(VariableSlider),
	.interfaceOffset = offsetof(VariableSlider, interface),
	.interfaceSize = sizeof(VariableSliderInterface),
	.initialize = initialize,
};

#undef _Class

