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

#include "ui_local.h"

#include "viewcontrollers/MainViewController.h"

#include "client.h"

extern cl_static_t cls;

static WindowController *windowController;
static ViewController *viewController;

/**
 * @brief Dispatch events to the user interface. Filter most common event types for
 * performance consideration.
 */
void Ui_HandleEvent(const SDL_Event *event) {

	if (cls.key_state.dest != KEY_UI) {

		switch (event->type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
			case SDL_MOUSEMOTION:
			case SDL_TEXTINPUT:
			case SDL_TEXTEDITING:
				return;
		}
	}

	$(windowController, respondToEvent, event);
}

/**
 * @brief TODO: Make this an ImageView within MainViewController's View?
 */
static void Ui_DrawBackground(void) {

	if (cls.state != CL_ACTIVE) {

		const r_image_t *background = R_LoadImage("ui/background", IT_UI);
		if (background->type != IT_NULL) {

			const vec_t x_scale = r_context.width / (vec_t) background->width;
			const vec_t y_scale = r_context.height / (vec_t) background->height;

			const vec_t scale = MAX(x_scale, y_scale);

			R_DrawImage(0, 0, scale, background);
		}
	}
}

/**
 * @brief Renders the user interface to a texture in a reserved OpenGL context, then
 * blits it back to the screen in the default context. A separate OpenGL context is
 * used to avoid OpenGL state pollution.
 */
void Ui_Draw(void) {
	extern void R_BindDefaultArray(GLenum type);

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	Ui_DrawBackground();

	GLint texnum;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texnum);

	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, r_context.window_width, r_context.window_height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	$(windowController, render);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, texnum);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	glOrtho(0, r_context.width, r_context.height, 0, -1, 1);
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

#if defined(__APPLE__)
	const char *path = Fs_BaseDir();
	if (path) {

		char fonts[MAX_OS_PATH];
		g_snprintf(fonts, sizeof(fonts), "%s/Contents/MacOS/etc/fonts", path);

		setenv("FONTCONFIG_PATH", fonts, 0);
	}
#endif

	windowController = alloc(WindowController, initWithWindow, r_context.window);

	viewController = (ViewController *) $((ViewController *) _alloc(&_MainViewController), init);

	$(windowController, setViewController, viewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	release(viewController);
	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
