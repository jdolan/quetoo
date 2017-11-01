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

#include "PrimaryButton.h"
#include "QuetooTheme.h"

#define _Class _PrimaryButton

#pragma mark - PrimaryButton

/**
 * @fn PrimaryButton *PrimaryButton::initWithTitle(PrimaryButton *self, const char *title)
 *
 * @memberof PrimaryButton
 */
static PrimaryButton *initWithTitle(PrimaryButton *self, const char *title) {

	const SDL_Rect frame = MakeRect(0, 0, DEFAULT_PRIMARY_BUTTON_WIDTH, DEFAULT_PRIMARY_BUTTON_HEIGHT);

	self = (PrimaryButton *) super(Button, self, initWithFrame, &frame, ControlStyleCustom);
	if (self) {

		$(self->button.title, setFont, $$(Font, defaultFont, FontCategoryPrimaryResponder));
		$(self->button.title, setText, title);

		View *this = (View *) self;

		this->autoresizingMask = ViewAutoresizingNone;

		QuetooTheme *theme = $(alloc(QuetooTheme), init);
		assert(theme);

		this->backgroundColor = theme->colors.dark;
		this->borderColor = theme->colors.lightBorder;

		release(theme);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((PrimaryButtonInterface *) clazz->def->interface)->initWithTitle = initWithTitle;
}

/**
 * @fn Class *PrimaryButton::_PrimaryButton(void)
 * @memberof PrimaryButton
 */
Class *_PrimaryButton(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PrimaryButton";
		clazz.superclass = _Button();
		clazz.instanceSize = sizeof(PrimaryButton);
		clazz.interfaceOffset = offsetof(PrimaryButton, interface);
		clazz.interfaceSize = sizeof(PrimaryButtonInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
