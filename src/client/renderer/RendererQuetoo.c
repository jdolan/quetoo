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

#include "r_local.h"

#include <ObjectivelyMVC/Image.h>
#include <ObjectivelyMVC/Log.h>
#include <ObjectivelyMVC/Types.h>
#include "RendererQuetoo.h"
#include <ObjectivelyMVC/View.h>

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

	R_UseProgram(r_state.null_program);

	R_EnableBlend(true);
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	super(Renderer, self, beginFrame);
}

/**
 * @fn void RendererQuetoo::createTexture(const Renderer *self, const SDL_Surface *surface)
 * @memberof RendererQuetoo
 */
static ident createTexture(const Renderer *self, const SDL_Surface *surface) {

	assert(surface);

	static uint32_t count;
	static char name[MAX_QPATH];

	g_snprintf(name, sizeof(name), "mvc_%u", count++);

	r_image_t *image = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t));

	image->type = IT_EXPLICIT;
	image->width = surface->w;
	image->height = surface->h;

	GLenum format;
	switch (surface->format->BytesPerPixel) {
		case 1:
			format = GL_LUMINANCE;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			MVC_LogError("Invalid surface format: %s\n",
					 SDL_GetPixelFormatName(surface->format->format));
			return NULL;
	}

	R_UploadImage(image, format, surface->pixels);

	return (ident)image;
}

/**
 * @fn void RendererQuetoo::destroyTexture(const Renderer *self, ident texture)
 * @memberof RendererQuetoo
 */
static void destroyTexture(const Renderer *self, ident texture) {

	r_image_t *image = (r_image_t *) texture;

	glDeleteTextures(1, &image->texnum);
	Mem_Free(image);
}

/**
 * @fn void RendererQuetoo::drawLine(const Renderer *self, const SDL_Point *points, size_t count)
 * @memberof RendererQuetoo
 */
static void drawLine(const Renderer *self, const SDL_Point *points) {
	
	RendererQuetoo *this = (RendererQuetoo *) self;

	assert(points);
	
	R_DrawLine(points[0].x, points[0].y, points[1].x, points[1].y, this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawLines(const Renderer *self, const SDL_Point *points, size_t count)
 * @memberof RendererQuetoo
 */
static void drawLines(const Renderer *self, const SDL_Point *points, size_t count) {
	
	RendererQuetoo *this = (RendererQuetoo *) self;

	assert(points);
	
	for (size_t i = 0; i < count - 1; ++i)
	{
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

	RendererQuetoo *this = (RendererQuetoo *) self;

	assert(rect);

	GLint verts[8];

	verts[0] = rect->x;
	verts[1] = rect->y;

	verts[2] = rect->x + rect->w;
	verts[3] = rect->y;

	verts[4] = rect->x + rect->w;
	verts[5] = rect->y + rect->h;

	verts[6] = rect->x;
	verts[7] = rect->y + rect->h;

	R_DrawLine(rect->x, rect->y, rect->x + rect->w, rect->y, this->currentColor.c, -1.0);
	R_DrawLine(rect->x + rect->w, rect->y, rect->x + rect->w, rect->y + rect->h, this->currentColor.c, -1.0);
	R_DrawLine(rect->x + rect->w, rect->y + rect->h, rect->x, rect->y + rect->h, this->currentColor.c, -1.0);
	R_DrawLine(rect->x, rect->y + rect->h, rect->x, rect->y, this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawRectFilled(const Renderer *self, const SDL_Rect *rect)
 * @memberof RendererQuetoo
 */
static void drawRectFilled(const Renderer *self, const SDL_Rect *rect) {

	RendererQuetoo *this = (RendererQuetoo *) self;

	assert(rect);

	R_DrawFill(rect->x, rect->y, rect->w, rect->h, this->currentColor.c, -1.0);
}

/**
 * @fn void RendererQuetoo::drawTexture(const Renderer *self, ident texture, const SDL_Rect *dest)
 * @memberof RendererQuetoo
 */
static void drawTexture(const Renderer *self, ident texture, const SDL_Rect *rect) {

	assert(rect);

	R_DrawImage(rect->x, rect->y, 1.0f, (const r_image_t *) texture);
}

/**
 * @fn void RendererQuetoo::endFrame(RendererQuetoo *self)
 * @memberof RendererQuetoo
 */
static void endFrame(Renderer *self) {
	
	$(self, setScissor, NULL);

	const GLenum err = glGetError();
	if (err) {
		MVC_LogError("GL error: %d\n", err);
	}
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
 * @fn void RendererQuetoo::setScissor(Renderer *self, const SDL_Rect *rect)
 * @memberof RendererQuetoo
 */
static void setScissor(Renderer *self, const SDL_Rect *rect) {
	R_EnableScissor((const GLint *) rect);
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

#include <ObjectivelyMVC/Program.h>

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((RendererInterface *) clazz->def->interface)->beginFrame = beginFrame;
	((RendererInterface *) clazz->def->interface)->createTexture = createTexture;
	((RendererInterface *) clazz->def->interface)->destroyTexture = destroyTexture;
	((RendererInterface *) clazz->def->interface)->drawLine = drawLine;
	((RendererInterface *) clazz->def->interface)->drawLines = drawLines;
	((RendererInterface *) clazz->def->interface)->drawRect = drawRect;
	((RendererInterface *) clazz->def->interface)->drawRectFilled = drawRectFilled;
	((RendererInterface *) clazz->def->interface)->drawTexture = drawTexture;
	((RendererInterface *) clazz->def->interface)->endFrame = endFrame;
	((RendererInterface *) clazz->def->interface)->setDrawColor = setDrawColor;
	((RendererInterface *) clazz->def->interface)->setScissor = setScissor;

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

