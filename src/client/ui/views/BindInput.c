/*
 * ObjectivelyMVC: MVC framework for OpenGL and SDL2 in c.
 * Copyright (C) 2014 Jay Dolan <jay@jaydolan.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
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

#pragma mark - BindInput

/**
 * @fn BindInput *BindInput::initWithBind(BindInput *self, const char *bind, const char *name)
 *
 * @memberof BindInput
 */
static BindInput *initWithBind(BindInput *self, const char *bind, const char *name) {

	TextView *textView = $(alloc(TextView), initWithFrame, NULL, ControlStyleDefault);
	textView->control.view.frame.w = BIND_INPUT_BIND_WIDTH;

	Label *label = $(alloc(Label), initWithText, name, NULL);
	label->view.frame.w = BIND_INPUT_NAME_WIDTH;

	self = (BindInput *) super(Input, self, initWithOrientation, InputOrientationLeft, (Control *) textView, label);
	if (self) {

		self->bind = bind;
		assert(self->bind);

		self->name = name;
		assert(self->name);

		SDL_Scancode key = Cl_KeyForBind(bind);
		if (key != SDL_SCANCODE_UNKNOWN) {
			textView->defaultText = Cl_KeyName(key);
		}
	}

	release(textView);
	release(label);
	
	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->respondToEvent = respondToEvent;

	((BindInputInterface *) clazz->interface)->initWithBind = initWithBind;
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
