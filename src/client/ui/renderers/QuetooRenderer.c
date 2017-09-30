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

#include "src/client/renderer/r_gl.h"
#include "QuetooRenderer.h"

#include "ui_local.h"

#define _Class _QuetooRenderer

#pragma mark - QuetooRenderer

/**
 * @see Renderer::beginFrame(Renderer *self)
 * @memberof QuetooRenderer
 */
static void beginFrame(Renderer *self) {

	R_Color(NULL);
}

/**
 * @see Renderer::drawLine(const Renderer *self, const SDL_Point *points, size_t count)
 */
static void drawLine(const Renderer *self, const SDL_Point *points) {

	assert(points);

	R_DrawLinesUI(points, 2, false);
}

/**
 * @see Renderer::drawLines(const Renderer *self, const SDL_Point *points, size_t count)
 */
static void drawLines(const Renderer *self, const SDL_Point *points, size_t count) {
	assert(points);

	R_DrawLinesUI(points, count, false);
}

/**
 * @fn uint32_t Renderer::createTexture(const Renderer *self, const SDL_Surface *surface)
 * @memberof Renderer
 */
static uint32_t createTexture(const Renderer *self, const SDL_Surface *surface) {

	assert(surface);

	GLenum format;
	switch (surface->format->BytesPerPixel) {
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			MVC_LogError("Invalid surface format: %s\n", SDL_GetPixelFormatName(surface->format->format));
			return 0;
	}

	uint32_t texture;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

	return texture;
}

/**
 * @see Renderer::drawRect(const Renderer *self, const SDL_Rect *rect)
 */
static void drawRect(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	const SDL_Point points[] = {
		{ rect->x,					rect->y },
		{ rect->x + rect->w - 1,	rect->y },
		{ rect->x + rect->w - 1,	rect->y + rect->h - 1 },
		{ rect->x,					rect->y + rect->h - 1 }
	};

	R_DrawLinesUI(points, 4, true);
}

/**
 * @see Renderer::drawRectFilled(const Renderer *self, const SDL_Rect *rect)
 */
static void drawRectFilled(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	R_DrawFillUI(rect);
}

/**
 * @see Renderer::drawTexture(const Renderer *self, uint32_t texture, const SDL_Rect *dest)
 */
static void drawTexture(const Renderer *self, uint32_t texture, const SDL_Rect *rect) {

	assert(rect);

	const r_image_t image = { .texnum = texture };

	R_DrawImageResized(rect->x, rect->y, rect->w, rect->h, &image);
}

/**
 * @see Renderer::endFrame(QuetooRenderer *self)
 */
static void endFrame(Renderer *self) {

	$(self, setClippingFrame, NULL);
}

/**
 * @see Renderer::setDrawColor(Renderer *self, const SDL_Color *color)
 */
static void setDrawColor(Renderer *self, const SDL_Color *color) {

	if (color) {
		R_Color((const vec4_t) {
			color->r / 255.0f,
			      color->g / 255.0f,
			      color->b / 255.0f,
			      color->a / 255.0f
		});
	} else {
		R_Color(NULL);
	}
}

/**
 * @see Renderer::setClippingFrame(Renderer *self, const SDL_Rect *frame)
 */
static void setClippingFrame(Renderer *self, const SDL_Rect *frame) {

	if (!frame) {
		R_EnableScissor(NULL);
		return;
	}

	SDL_Window *window = SDL_GL_GetCurrentWindow();
	const SDL_Rect scissor = MVC_TransformToWindow(window, frame);

	R_EnableScissor(&scissor);
}

/**
 * @fn QuetooRenderer *QuetooRenderer::init(QuetooRenderer *self)
 * @memberof QuetooRenderer
 */
static QuetooRenderer *init(QuetooRenderer *self) {
	return (QuetooRenderer *) super(Renderer, self, init);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((RendererInterface *) clazz->def->interface)->beginFrame = beginFrame;
	((RendererInterface *) clazz->def->interface)->drawLine = drawLine;
	((RendererInterface *) clazz->def->interface)->drawLines = drawLines;
	((RendererInterface *) clazz->def->interface)->createTexture = createTexture;
	((RendererInterface *) clazz->def->interface)->drawRect = drawRect;
	((RendererInterface *) clazz->def->interface)->drawRectFilled = drawRectFilled;
	((RendererInterface *) clazz->def->interface)->drawTexture = drawTexture;
	((RendererInterface *) clazz->def->interface)->endFrame = endFrame;
	((RendererInterface *) clazz->def->interface)->setDrawColor = setDrawColor;
	((RendererInterface *) clazz->def->interface)->setClippingFrame = setClippingFrame;

	((QuetooRendererInterface *) clazz->def->interface)->init = init;
}

/**
 * @fn Class *QuetooRenderer::_QuetooRenderer(void)
 * @memberof QuetooRenderer
 */
Class *_QuetooRenderer(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "QuetooRenderer";
		clazz.superclass = _Renderer();
		clazz.instanceSize = sizeof(QuetooRenderer);
		clazz.interfaceOffset = offsetof(QuetooRenderer, interface);
		clazz.interfaceSize = sizeof(QuetooRendererInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

