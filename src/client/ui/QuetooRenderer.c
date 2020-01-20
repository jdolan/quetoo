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
 * @fn GLuint Renderer::createTexture(const Renderer *self, const SDL_Surface *surface)
 * @memberof Renderer
 */
static GLuint createTexture(const Renderer *self, const SDL_Surface *surface) {

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

	GLuint texture;
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
 * @see Renderer::drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *dest)
 */
static void drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *rect) {

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
		glDisable(GL_SCISSOR_TEST);
		return;
	}

	SDL_Window *window = SDL_GL_GetCurrentWindow();
	const SDL_Rect scissor = MVC_TransformToWindow(window, frame);

	glEnable(GL_SCISSOR_TEST);
	glScissor(scissor.x, scissor.y, scissor.w, scissor.h);
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

	((RendererInterface *) clazz->interface)->beginFrame = beginFrame;
	((RendererInterface *) clazz->interface)->drawLine = drawLine;
	((RendererInterface *) clazz->interface)->drawLines = drawLines;
	((RendererInterface *) clazz->interface)->createTexture = createTexture;
	((RendererInterface *) clazz->interface)->drawRect = drawRect;
	((RendererInterface *) clazz->interface)->drawRectFilled = drawRectFilled;
	((RendererInterface *) clazz->interface)->drawTexture = drawTexture;
	((RendererInterface *) clazz->interface)->endFrame = endFrame;
	((RendererInterface *) clazz->interface)->setDrawColor = setDrawColor;
	((RendererInterface *) clazz->interface)->setClippingFrame = setClippingFrame;

	((QuetooRendererInterface *) clazz->interface)->init = init;
}

/**
 * @fn Class *QuetooRenderer::_QuetooRenderer(void)
 * @memberof QuetooRenderer
 */
Class *_QuetooRenderer(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "QuetooRenderer",
			.superclass = _Renderer(),
			.instanceSize = sizeof(QuetooRenderer),
			.interfaceOffset = offsetof(QuetooRenderer, interface),
			.interfaceSize = sizeof(QuetooRendererInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class

