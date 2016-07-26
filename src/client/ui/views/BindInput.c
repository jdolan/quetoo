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

#include <ObjectivelyMVC/TextView.h>

#include "BindInput.h"

#include "client.h"

#define _Class _BindInput

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

	BindInput *this = (BindInput *) self;

	Control *control = this->input.control;
	if (control->state & ControlStateFocused) {

		if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {

			SDL_Scancode key;
			if (event->type == SDL_KEYDOWN) {
				key = event->key.keysym.scancode;
			} else {
				key = SDL_SCANCODE_MOUSE1 + (event->button.button - 1);
			}

			if (key == SDL_SCANCODE_ESCAPE) {
				key = Cl_KeyForBind(this->bind);
				Cl_Bind(key, NULL);
			} else {
				Cl_Bind(key, this->bind);
			}

			TextView *textView = (TextView *) control;
			key = Cl_KeyForBind(this->bind);
			if (key != SDL_SCANCODE_UNKNOWN) {
				textView->defaultText = Cl_KeyName(key);
			} else {
				textView->defaultText = NULL;
			}

			control->state &= ~ControlStateFocused;
			return;
		}
	}

	super(View, self, respondToEvent, event);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	BindInput *this = (BindInput *) self;

	TextView *textView = (TextView *) this->input.control;
	const SDL_Scancode key = Cl_KeyForBind(this->bind);
	if (key != SDL_SCANCODE_UNKNOWN) {
		textView->defaultText = Cl_KeyName(key);
	} else {
		textView->defaultText = NULL;
	}
}

#pragma mark - BindInput

/**
 * @fn BindInput *BindInput::initWithBind(BindInput *self, const char *bind, const char *name)
 *
 * @memberof BindInput
 */
static BindInput *initWithBind(BindInput *self, const char *bind, const char *name) {

	Control *control = (Control *) $(alloc(TextView), initWithFrame, NULL, ControlStyleDefault);
	control->view.frame.w = BIND_INPUT_CONTROL_WIDTH;

	Label *label = $(alloc(Label), initWithText, name, NULL);
	label->view.frame.w = BIND_INPUT_LABEL_WIDTH;

	self = (BindInput *) super(Input, self, initWithOrientation, InputOrientationLeft, control, label);
	if (self) {

		self->bind = bind;
		assert(self->bind);

		$((View *) self, updateBindings);
	}

	release(control);
	release(label);
	
	return self;
}

/**
 * @fn void BindInput::input(View *view, const char *bind, const char *name)
 *
 * @memberof BindInput
 */
static void input(View *view, const char *bind, const char *name) {

	assert(view);

	BindInput *input = $(alloc(BindInput), initWithBind, bind, name);
	assert(input);

	$(view, addSubview, (View *) input);
	release(input);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((BindInputInterface *) clazz->interface)->initWithBind = initWithBind;

	((BindInputInterface *) clazz->interface)->input = input;
}

Class _BindInput = {
	.name = "BindInput",
	.superclass = &_Input,
	.instanceSize = sizeof(BindInput),
	.interfaceOffset = offsetof(BindInput, interface),
	.interfaceSize = sizeof(BindInputInterface),
	.initialize = initialize,
};

#undef _Class

