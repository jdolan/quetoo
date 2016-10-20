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

#include "ui_local.h"

#include "client.h"

#include "RendererQuetoo.h"

#define _Class _RendererQuetoo

#pragma mark - RendererQuetoo

/**
 * @fn void RendererQuetoo::beginFrame(Renderer *self)
 * @memberof RendererQuetoo
 */
static void beginFrame(Renderer *self) {

	RendererQuetoo *this = (RendererQuetoo *) self;

	// set color to white
	this->currentColor.c = -1;

	R_EnableBlend(true);
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	super(Renderer, self, beginFrame);
}

/**
 * @fn void RendererQuetoo::drawLine(const Renderer *self, const SDL_Point *points, size_t count)
 * @memberof RendererQuetoo
 */
static void drawLine(const Renderer *self, const SDL_Point *points) {

	assert(points);

	RendererQuetoo *this = (RendererQuetoo *) self;

	R_DrawLine(points[0].x, points[0].y, points[1].x, points[1].y, this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawLines(const Renderer *self, const SDL_Point *points, size_t count)
 * @memberof RendererQuetoo
 */
static void drawLines(const Renderer *self, const SDL_Point *points, size_t count) {

	assert(points);

	RendererQuetoo *this = (RendererQuetoo *) self;

	for (size_t i = 0; i < count - 1; ++i) {
		const SDL_Point *start = &points[i];
		const SDL_Point *end = &points[i + 1];

		R_DrawLine(start->x, start->y, end->x, end->y, this->currentColor.c, -1.0);
	}
}

/**
 * @fn void RendererQuetoo::drawRect(const Renderer *self, const SDL_Rect *rect)
 * @memberof RendererQuetoo
 */
static void drawRect(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	RendererQuetoo *this = (RendererQuetoo *) self;

	R_DrawLine(rect->x,					rect->y,				rect->x + rect->w - 1,			rect->y, this->currentColor.c, -1.0);
	R_DrawLine(rect->x + rect->w - 1,	rect->y,				rect->x + rect->w - 1,			rect->y + rect->h - 1, this->currentColor.c, -1.0);
	R_DrawLine(rect->x + rect->w - 1,	rect->y + rect->h - 1,	rect->x, rect->y + rect->h - 1,	this->currentColor.c, -1.0);
	R_DrawLine(rect->x,					rect->y + rect->h - 1,	rect->x, rect->y,				this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawRectFilled(const Renderer *self, const SDL_Rect *rect)
 * @memberof RendererQuetoo
 */
static void drawRectFilled(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	RendererQuetoo *this = (RendererQuetoo *) self;

	R_DrawFill(rect->x, rect->y, rect->w, rect->h, this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *dest)
 * @memberof RendererQuetoo
 */
static void drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *rect) {

	assert(rect);

	static r_image_t image;

	image.texnum = texture;

	R_DrawImageResized(rect->x, rect->y, rect->w, rect->h, &image);
}

/**
 * @fn void RendererQuetoo::endFrame(RendererQuetoo *self)
 * @memberof RendererQuetoo
 */
static void endFrame(Renderer *self) {
	
	$(self, setClippingFrame, NULL);
}

/**
 * @fn void RendererQuetoo::setDrawColor(Renderer *self, const SDL_Color *color)
 * @memberof RendererQuetoo
 */
static void setDrawColor(Renderer *self, const SDL_Color *color) {
	RendererQuetoo *this = (RendererQuetoo *) self;

	memcpy(&this->currentColor, color, sizeof(this->currentColor));
}

/**
 * @fn void RendererQuetoo::setClippingFrame(Renderer *self, const SDL_Rect *frame)
 * @memberof RendererQuetoo
 */
static void setClippingFrame(Renderer *self, const SDL_Rect *frame) {

	if (!frame)
	{
		R_EnableScissor(NULL);
		return;
	}

	SDL_Window *window = SDL_GL_GetCurrentWindow();
	const SDL_Rect scissor = MVC_TransformToWindow(window, frame);

	R_EnableScissor(&scissor);
}

/**
 * @fn RendererQuetoo *RendererQuetoo::init(RendererQuetoo *self)
 * @memberof RendererQuetoo
 */
static RendererQuetoo *init(RendererQuetoo *self) {
	
	self = (RendererQuetoo *) super(Renderer, self, init);

	if (self) {

	}
	
	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((RendererInterface *) clazz->def->interface)->beginFrame = beginFrame;
	((RendererInterface *) clazz->def->interface)->drawLine = drawLine;
	((RendererInterface *) clazz->def->interface)->drawLines = drawLines;
	((RendererInterface *) clazz->def->interface)->drawRect = drawRect;
	((RendererInterface *) clazz->def->interface)->drawRectFilled = drawRectFilled;
	((RendererInterface *) clazz->def->interface)->drawTexture = drawTexture;
	((RendererInterface *) clazz->def->interface)->endFrame = endFrame;
	((RendererInterface *) clazz->def->interface)->setDrawColor = setDrawColor;
	((RendererInterface *) clazz->def->interface)->setClippingFrame = setClippingFrame;

	((RendererQuetooInterface *) clazz->def->interface)->init = init;
}

Class _RendererQuetoo = {
	.name = "RendererQuetoo",
	.superclass = &_Renderer,
	.instanceSize = sizeof(RendererQuetoo),
	.interfaceOffset = offsetof(RendererQuetoo, interface),
	.interfaceSize = sizeof(RendererQuetooInterface),
	.initialize = initialize
};

#undef _Class

