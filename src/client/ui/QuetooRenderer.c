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

static color_t drawColor;

/**
 * @see Renderer::beginFrame(Renderer *self)
 * @memberof QuetooRenderer
 */
static void beginFrame(Renderer *self) {

	drawColor = color_white;
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
 * @see Renderer::drawLine(const Renderer *self, const SDL_Point *points, size_t count)
 */
static void drawLine(const Renderer *self, const SDL_Point *points) {

	assert(points);

	$(self, drawLines, points, 2);
}

/**
 * @see Renderer::drawLines(const Renderer *self, const SDL_Point *points, size_t count)
 */
static void drawLines(const Renderer *self, const SDL_Point *points, size_t count) {
	assert(points);

	GLint p[count][2];

	for (size_t i = 0; i < count; i++) {
		p[i][0] = points[i].x;
		p[i][1] = points[i].y;
	}

	R_Draw2DLines((GLint *) p, count, drawColor);
}


/**
 * @see Renderer::drawRect(const Renderer *self, const SDL_Rect *rect)
 */
static void drawRect(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	const SDL_Rect r = *rect;

	const GLint points[][2] = {
		{ r.x,			r.y },
		{ r.x + r.w,	r.y },
		{ r.x + r.w,	r.y + r.h },
		{ r.x,			r.y + r.h },
		{ r.x,			r.y },
	};

	R_Draw2DLines((GLint *) points, lengthof(points), drawColor);
}

/**
 * @see Renderer::drawRectFilled(const Renderer *self, const SDL_Rect *rect)
 */
static void drawRectFilled(const Renderer *self, const SDL_Rect *rect) {

	assert(rect);

	R_Draw2DFill(rect->x, rect->y, rect->w, rect->h, drawColor);
}

/**
 * @see Renderer::drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *dest)
 */
static void drawTexture(const Renderer *self, GLuint texture, const SDL_Rect *rect) {

	assert(rect);

	const r_image_t image = { .texnum = texture };

	R_Draw2DImage(rect->x, rect->y, rect->w, rect->h, &image, drawColor);
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
		drawColor = Color4b(color->r, color->g, color->b, color->a);
	} else {
		drawColor = color_white;
	}
}

/**
 * @see Renderer::setClippingFrame(Renderer *self, const SDL_Rect *frame)
 */
static void setClippingFrame(Renderer *self, const SDL_Rect *frame) {

	if (frame) {
		const SDL_Rect clip = MVC_TransformToWindow(r_context.window, frame);
		R_SetClippingFrame(clip.x, clip.y, clip.w, clip.h);
	} else {
		R_SetClippingFrame(0, 0, 0, 0);
	}
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

