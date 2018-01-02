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

#include "PrimaryIcon.h"
#include "QuetooTheme.h"

#define _Class _PrimaryIcon

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	PrimaryIcon *this = (PrimaryIcon *) self;

	release(this->imageView);

	super(Object, self, dealloc);
}

#pragma mark - PrimaryIcon

/**
 * @fn PrimaryIcon *PrimaryIcon::initWithIcon(PrimaryIcon *self, const char *icon)
 * @memberof PrimaryIcon
 */
static PrimaryIcon *initWithIcon(PrimaryIcon *self, const char *icon) {

	const SDL_Rect frame = MakeRect(0, 0, DEFAULT_PRIMARY_ICON_WIDTH, DEFAULT_PRIMARY_ICON_HEIGHT);

	self = (PrimaryIcon *) super(Button, self, initWithFrame, &frame);
	if (self) {

		View *this = (View *) self;

		QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, this);
		assert(theme);

		self->imageView = $(theme, image, icon, NULL);
		assert(self->imageView);

		self->imageView->view.autoresizingMask = ViewAutoresizingFill;

		$(theme, attach, self->imageView);

		this->autoresizingMask = ViewAutoresizingNone;

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

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;
	((PrimaryIconInterface *) clazz->def->interface)->initWithIcon = initWithIcon;
}

/**
 * @fn Class *PrimaryIcon::_PrimaryIcon(void)
 * @memberof PrimaryIcon
 */
Class *_PrimaryIcon(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PrimaryIcon";
		clazz.superclass = _Button();
		clazz.instanceSize = sizeof(PrimaryIcon);
		clazz.interfaceOffset = offsetof(PrimaryIcon, interface);
		clazz.interfaceSize = sizeof(PrimaryIconInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
