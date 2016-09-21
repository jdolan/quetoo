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

#include "CrosshairView.h"
#include "client.h"

#define _Class _CrosshairView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	CrosshairView *this = (CrosshairView *) self;

	release(this->imageView);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::layoutSubviews(View *)
 */
static void layoutSubviews(View *self) {

	CrosshairView *this = (CrosshairView *) self;
	if (this->imageView->image) {

		const float scale = Cvar_GetValue("cg_draw_crosshair_scale");

		const SDL_Size size = MakeSize(
			this->imageView->image->surface->w * scale,
			this->imageView->image->surface->h * scale
		);

		$((View *) this->imageView, resize, &size);
	}

	super(View, self, layoutSubviews);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	CrosshairView *this = (CrosshairView *) self;

	$(this->imageView, setImage, NULL);

	const int ch = Cvar_GetValue("cg_draw_crosshair");
	if (ch) {
		SDL_Surface *surface;
		if (Img_LoadImage(va("pics/ch%d", ch), &surface)) {

			$(this->imageView, setImageWithSurface, surface);
			SDL_FreeSurface(surface);

			SDL_Color color = Colors.White;

			const char *s = Cvar_GetString("cg_draw_crosshair_color");
			if (!strcmp(s, "red")) {
				color = Colors.Red;
			} else if (!strcmp(s, "green")) {
				color = Colors.Lime;
			} else if (!strcmp(s, "yellow")) {
				color = Colors.Yellow;
			} else if (!strcmp(s, "orange")) {
				color = Colors.Orange;
			}

			this->imageView->color = color;
		}
	}

	self->needsLayout = true;
}

#pragma mark - CrosshairView

/**
 * @fn CrosshairView *CrosshairView::initWithFrame(CrosshairView *self, const SDL_Rect *frame)
 *
 * @memberof CrosshairView
 */
static CrosshairView *initWithFrame(CrosshairView *self, const SDL_Rect *frame) {
	
	self = (CrosshairView *) super(View, self, initWithFrame, frame);
	if (self) {

		self->imageView = alloc(ImageView, initWithFrame, NULL);
		assert(self->imageView);

		self->imageView->view.alignment = ViewAlignmentMiddleCenter;

		$((View *) self, addSubview, (View *) self->imageView);

		self->view.backgroundColor = Colors.Black;
		self->view.backgroundColor.a = 48;

		$((View *) self, updateBindings);
	}
	
	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->layoutSubviews = layoutSubviews;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((CrosshairViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

Class _CrosshairView = {
	.name = "CrosshairView",
	.superclass = &_View,
	.instanceSize = sizeof(CrosshairView),
	.interfaceOffset = offsetof(CrosshairView, interface),
	.interfaceSize = sizeof(CrosshairViewInterface),
	.initialize = initialize,
};

#undef _Class

