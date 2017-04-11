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

#pragma mark - Tab enumeration

/**
 * @brief Disable selection on a single TabButton if applicable
 */
void enumerateTabs(const Array *array, ident obj, ident data) {

	TabButton *self = (TabButton *) obj;

	if ($((Object *) self, isKindOfClass, _TabButton())) {
		if (data == obj) {
			self->isSelected = true;
		} else {
			self->isSelected = false;
		}

		$((View *) self, updateBindings);
	}
}

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

	this->selectionImage->view.hidden = !this->isSelected;

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

		// Button

		$(self->button.title, setFont, $$(Font, defaultFont, FontCategoryPrimaryResponder));

		self->button.control.bevel = ControlBevelTypeNone;

		self->button.control.view.borderWidth = 1;

		self->button.control.view.padding.top = 0;
		self->button.control.view.padding.right = 0;
		self->button.control.view.padding.bottom = 0;
		self->button.control.view.padding.left = 0;

		// Selection

		self->isSelected = false;

		self->selectionImage = $(alloc(ImageView), initWithFrame, NULL);
		assert(self->selectionImage);

		self->selectionImage->view.autoresizingMask = ViewAutoresizingFill;

		$((View *) self, addSubview, (View *) self->selectionImage);

		SDL_Surface *surface;

		if (cgi.LoadSurface("ui/pics/select_highlight", &surface)) {
			$(self->selectionImage, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		}

		$((View *) self, updateBindings);
	}

	return self;
}

/**
 * @fn void TabButton::selectTab(TabButton *self)
 *
 * @memberof TabButton
 */
static void selectTab(TabButton *self) {

	assert(self);
	assert(((View *) self)->superview);

	const Array *array = (Array *) (((View *) self)->superview->subviews);

	$(array, enumerateObjects, enumerateTabs, self);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->respondToEvent = respondToEvent;
	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((TabButtonInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
	((TabButtonInterface *) clazz->def->interface)->selectTab = selectTab;
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
