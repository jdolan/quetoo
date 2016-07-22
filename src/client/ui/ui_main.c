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
#include "client.h"

ui_context_t ui_context;

extern cl_static_t cls;

static ViewController *mainViewController;

/**
 * @return True if the user interface handled the event, false otherwise.
 */
void Ui_HandleEvent(const SDL_Event *event) {

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	$(mainViewController, respondToEvent, event);
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

	$(mainViewController, drawView, ui_context.renderer);

	SDL_SetRenderTarget(ui_context.renderer, NULL);

	SDL_GL_MakeCurrent(r_context.window, r_context.context);

	R_DrawImage(0, 0, 1.0, &ui_context.image);
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

	memset(&ui_context, 0, sizeof(ui_context));

	if ((ui_context.renderer = SDL_CreateRenderer(r_context.window, 0,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to create SDL Renderer: %s\n", SDL_GetError());
	}

	if ((ui_context.texture = SDL_CreateTexture(ui_context.renderer,
			SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
			r_context.width, r_context.height)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to create SDL_Texture target: %s\n", SDL_GetError());
	}

	SDL_QueryTexture(ui_context.texture, NULL, NULL,
			(int32_t *) &ui_context.image.width, (int32_t *) &ui_context.image.height);

	SDL_GL_BindTexture(ui_context.texture, NULL, NULL);

	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &ui_context.image.texnum);

	SDL_GL_UnbindTexture(ui_context.texture);

	if ((ui_context.context = SDL_GL_CreateContext(r_context.window)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to create OpenGL context: %s\n", SDL_GetError());
	}

	SDL_GL_MakeCurrent(r_context.window, r_context.context);

	mainViewController = $((ViewController *) alloc(MainViewController), initRootViewController, r_context.window);
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	if (ui_context.texture) {
		SDL_DestroyTexture(ui_context.texture);
		ui_context.texture = NULL;
	}

	if (ui_context.renderer) {
		SDL_DestroyRenderer(ui_context.renderer);
		ui_context.renderer = NULL;
	}

	if (ui_context.context) {
		SDL_GL_DeleteContext(ui_context.context);
		ui_context.context = NULL;
	}

	release(mainViewController);

	Mem_FreeTag(MEM_TAG_UI);
}
