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

#include "TabButton.h"

#define _Class _TabButton

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

	super(View, self, respondToEvent, event);

	((Control *) self)->bevel = ControlBevelTypeNone;
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	TabButton *this = (TabButton *) self;

	SDL_Surface *surface;

	if (this->selected && cgi.LoadSurface("ui/pics/select_highlight", &surface)) {
		$(this->selectionImage, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(this->selectionImage, setImage, NULL);
	}

	self->needsLayout = true;
}
#pragma mark - TabButton

/**
 * @fn TabButton *TabButton::init(TabButton *self)
 *
 * @memberof TabButton
 */
static TabButton *initWithFrame(TabButton *self, const SDL_Rect *frame, ControlStyle style) {

	self = (TabButton *) super(Button, self, initWithFrame, frame, style);
	if (self) {

		$(self->button.title, setFont, $$(Font, defaultFont, FontCategoryPrimaryResponder));

		self->button.control.bevel = ControlBevelTypeNone;

		self->button.control.view.borderWidth = 1;

		// Selection

		self->isSelected = false;

		self->selectionImage = $(alloc(ImageView), initWithFrame, NULL);
		assert(self->selectionImage);

		self->selectionImage->view.autoresizingMask = ViewAutoresizingFill;

		$((View *) self, addSubview, (View *) self->imageView);

		$((View *) self, updateBindings);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->respondToEvent = respondToEvent;
	((ViewInterface *) clazz->def->interface)->updateBindigns = updateBindings;

	((TabButtonInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *TabButton::_TabButton(void)
 * @memberof TabButton
 */
Class *_TabButton(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "TabButton";
		clazz.superclass = _Button();
		clazz.instanceSize = sizeof(TabButton);
		clazz.interfaceOffset = offsetof(TabButton, interface);
		clazz.interfaceSize = sizeof(TabButtonInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
