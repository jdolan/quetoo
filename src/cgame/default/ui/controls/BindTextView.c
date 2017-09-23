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

#include "BindTextView.h"

#define _Class _BindTextView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	BindTextView *this = (BindTextView *) self;

	free(this->bind);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	TextView *this = (TextView *) self;

	free(this->defaultText);
	this->defaultText = NULL;

	MutableArray *keys = $$(MutableArray, array);
	SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
	while (true) {
		key = cgi.KeyForBind(key, ((BindTextView *) this)->bind);
		if (key == SDL_SCANCODE_UNKNOWN) {
			break;
		}

		$(keys, addObject, str(cgi.KeyName(key)));
	}

	if (keys->array.count) {
		String *keyNames = $((Array *) keys, componentsJoinedByCharacters, ", ");
		this->defaultText = strdup(keyNames->chars);
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
					key = cgi.KeyForBind(key, this->bind);
					if (key == SDL_SCANCODE_UNKNOWN) {
						break;
					}

					cgi.BindKey(key, NULL);
				}
			} else {
				cgi.BindKey(key, this->bind);
			}

			if (this->textView.delegate.didEndEditing) {
				this->textView.delegate.didEndEditing(&this->textView);
			}

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

		self->bind = strdup(bind);
		assert(self->bind);

		self->textView.control.view.frame.w = BIND_TEXTVIEW_WIDTH;

		$((View *) self, updateBindings);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->def->interface)->captureEvent = captureEvent;

	((BindTextViewInterface *) clazz->def->interface)->initWithBind = initWithBind;
}

/**
 * @fn Class *BindTextView::_BindTextView(void)
 * @memberof BindTextView
 */
Class *_BindTextView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "BindTextView";
		clazz.superclass = _TextView();
		clazz.instanceSize = sizeof(BindTextView);
		clazz.interfaceOffset = offsetof(BindTextView, interface);
		clazz.interfaceSize = sizeof(BindTextViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
