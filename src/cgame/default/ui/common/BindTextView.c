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
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

	super(View, self, awakeWithDictionary, dictionary);

	BindTextView *this = (BindTextView *) self;

	const Inlet inlets[] = MakeInlets(
		MakeInlet("bind", InletTypeCharacters, &this->bind, NULL)
	);

	$(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
	return (View *) $((BindTextView *) self, initWithBind, NULL);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	BindTextView *this = (BindTextView *) self;
	if (this->bind) {

		MutableArray *keys = $$(MutableArray, array);
		SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
		while (true) {
			key = cgi.KeyForBind(key, ((BindTextView *) this)->bind);
			if (key == SDL_SCANCODE_UNKNOWN) {
				break;
			}

			$(keys, addObject, str(cgi.KeyName(key)));
		}

		String *keyNames = $((Array *) keys, componentsJoinedByCharacters, ", ");

		$((TextView *) self, setDefaultText, keyNames->chars);

		release(keyNames);
		release(keys);

	} else {
		$((TextView *) self, setDefaultText, NULL);
	}
}

#pragma mark - Control

/**
 * @see Control::captureEvent(Control *, const SDL_Event *)
 */
static bool captureEvent(Control *self, const SDL_Event *event) {

	if (self->state & ControlStateFocused) {

		if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {

			SDL_Scancode key;
			if (event->type == SDL_KEYDOWN) {
				key = event->key.keysym.scancode;
			} else {
				key = SDL_SCANCODE_MOUSE1 + (event->button.button - 1);
			}

			BindTextView *this = (BindTextView *) self;

			if (key != SDL_SCANCODE_ESCAPE) {
				
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

	self = (BindTextView *) super(TextView, self, initWithFrame, NULL);
	if (self) {

		self->bind = strdup(bind ?: "");
		assert(self->bind);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->interface)->captureEvent = captureEvent;

	((BindTextViewInterface *) clazz->interface)->initWithBind = initWithBind;
}

/**
 * @fn Class *BindTextView::_BindTextView(void)
 * @memberof BindTextView
 */
Class *_BindTextView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "BindTextView",
			.superclass = _TextView(),
			.instanceSize = sizeof(BindTextView),
			.interfaceOffset = offsetof(BindTextView, interface),
			.interfaceSize = sizeof(BindTextViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
