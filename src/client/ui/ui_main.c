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

#include "viewcontrollers/ControlsViewController.h"
#include "viewcontrollers/MainViewController.h"
#include "viewcontrollers/MenuViewController.h"

#include "client.h"

extern cl_static_t cls;

static WindowController *windowController;
static ViewController *mainViewController;

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
 * @brief Renders the user interface to a texture in a reserved OpenGL context, then
 * blits it back to the screen in the default context. A separate OpenGL context is
 * used to avoid OpenGL state pollution.
 */
void Ui_Draw(void) {

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	SDL_GL_MakeCurrent(r_context.window, ui_context.context);

	SDL_SetRenderTarget(ui_context.renderer, ui_context.texture);

	SDL_SetRenderDrawColor(ui_context.renderer, 0, 0, 0, 0);

	SDL_SetRenderDrawBlendMode(ui_context.renderer, SDL_BLENDMODE_BLEND);

	SDL_RenderClear(ui_context.renderer);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, r_context.window_width, 0, r_context.window_height, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	$(windowController, render, ui_context.renderer);

	glFinish(); // <-- well fuck my ass

	SDL_SetRenderTarget(ui_context.renderer, NULL);

	SDL_GL_MakeCurrent(r_context.window, r_context.context);

	R_DrawImage(0, 0, 1.0, &ui_context.image);
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	mainViewController = $((ViewController *) alloc(ControlsViewController), init);

	$(windowController, setViewController, mainViewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	Ui_ShutdownContext();

	release(mainViewController);
	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
