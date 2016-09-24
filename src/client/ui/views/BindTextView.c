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

#include <Objectively/Value.h>

#include "BindTextView.h"

#include "client.h"

#define _Class _BindTextView

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	BindTextView *this = (BindTextView *) self;
	TextView *textView = (TextView *) this;

	free(textView->defaultText);
	textView->defaultText = NULL;

	MutableArray *keys = $$(MutableArray, array);
	SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
	while (true) {
		key = Cl_KeyForBind(key, this->bind);
		if (key == SDL_SCANCODE_UNKNOWN) {
			break;
		}

		$(keys, addObject, str(Cl_KeyName(key)));
	}

	if (keys->array.count) {
		String *keyNames = $((Array *) keys, componentsJoinedByCharacters, ", ");
		textView->defaultText = g_strdup(keyNames->chars);
		release(keyNames);
	}

	release(keys);
}

#pragma mark - Control

/**
 * @see Control::captureEvent(Control *, const SDL_Event *)
 */
static _Bool captureEvent(Control *self, const SDL_Event *event) {

	if (self->state & ControlStateFocused) {

		if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {

			SDL_Scancode key;
			if (event->type == SDL_KEYDOWN) {
				key = event->key.keysym.scancode;
			} else {
				key = SDL_SCANCODE_MOUSE1 + (event->button.button - 1);
			}

			BindTextView *this = (BindTextView *) self;

			
			if (key == SDL_SCANCODE_BACKSPACE) {
				key = SDL_SCANCODE_UNKNOWN;
				while (true) {
					key = Cl_KeyForBind(key, this->bind);
					if (key == SDL_SCANCODE_UNKNOWN) {
						break;
					}

					Cl_Bind(key, NULL);
				}
			} else {
				Cl_Bind(key, this->bind);
			}

			$((View *) self, updateBindings);

			self->state &= ~ControlStateFocused;
			return true;
		}
	}

	return super(Control, self, captureEvent, event);
}

#pragma mark - BindTextView

/**
 * @fn BindTextView *BindTextView::initWithBind(BindTextView *self, const char *bind)
 *
 * @memberof BindTextView
 */
static BindTextView *initWithBind(BindTextView *self, const char *bind) {

	self = (BindTextView *) super(TextView, self, initWithFrame, NULL, ControlStyleDefault);
	if (self) {

		self->bind = bind;
		assert(self->bind);

		self->textView.control.view.frame.w = BIND_WIDTH;

		$((View *) self, updateBindings);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->interface)->captureEvent = captureEvent;

	((BindTextViewInterface *) clazz->interface)->initWithBind = initWithBind;
}

Class _BindTextView = {
	.name = "BindTextView",
	.superclass = &_TextView,
	.instanceSize = sizeof(BindTextView),
	.interfaceOffset = offsetof(BindTextView, interface),
	.interfaceSize = sizeof(BindTextViewInterface),
	.initialize = initialize,
};

#undef _Class

