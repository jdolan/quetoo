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

#include "renderers/QuetooRenderer.h"

extern cl_static_t cls;

static WindowController *windowController;

static NavigationViewController *navigationViewController;

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
 * @brief
 */
void Ui_ViewWillAppear(void) {

	if (windowController) {
		$(windowController->viewController, viewWillAppear);
		$(windowController->viewController->view, updateBindings);
	}
}

/**
 * @brief
 */
void Ui_Draw(void) {

	Ui_CheckEditor();

	// backup all of the matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PushMatrix(matrix);
	}

	R_SetMatrix(R_MATRIX_PROJECTION, &r_view.matrix_base_ui);

	$(windowController, render);

	// restore matrices
	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PopMatrix(matrix);
	}
}

/**
 * @brief
 */
void Ui_PushViewController(ViewController *viewController) {
	if (viewController) {
		$(navigationViewController, pushViewController, viewController);
	}
}

/**
 * @brief
 */
void Ui_PopToViewController(ViewController *viewController) {
	if (viewController) {
		$(navigationViewController, popToViewController, viewController);
	}
}

/**
 * @brief
 */
void Ui_PopViewController(void) {
	$(navigationViewController, popViewController);
}

/**
 * @brief
 */
void Ui_PopAllViewControllers(void) {
	$(navigationViewController, popToRootViewController);
}

/**
 * @brief
 */
static void Ui_SetDefaultFont(FontCategory category, const char *path, int32_t size, int32_t index) {

	void *buffer;
	const int64_t length = Fs_Load(path, &buffer);
	if (length != -1) {

		Data *data = $$(Data, dataWithBytes, buffer, length);
		assert(data);

		Font *font = $(alloc(Font), initWithData, data, size, index);
		assert(font);

		$$(Font, setDefaultFont, category, font);

		release(font);
	}

	Fs_Free(buffer);
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

	const char *coda = "fonts/coda.regular.ttf";
	const char *codaHeavy = "fonts/coda.heavy.ttf";

	Ui_SetDefaultFont(FontCategoryDefault, coda, 16, 0);
	Ui_SetDefaultFont(FontCategoryPrimaryLabel, coda, 16, 0);
	Ui_SetDefaultFont(FontCategoryPrimaryControl, coda, 16, 0);

	Ui_SetDefaultFont(FontCategorySecondaryLabel, coda, 14, 0);
	Ui_SetDefaultFont(FontCategorySecondaryControl, coda, 14, 0);

	Ui_SetDefaultFont(FontCategoryPrimaryResponder, codaHeavy, 18, 0);
	Ui_SetDefaultFont(FontCategorySecondaryResponder, codaHeavy, 18, 0);

	Renderer *renderer = (Renderer *) $(alloc(QuetooRenderer), init);

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	$(windowController, setRenderer, renderer);

	navigationViewController = $(alloc(NavigationViewController), init);

	$(windowController, setViewController, (ViewController *) navigationViewController);

	Ui_InitEditor();
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	Ui_ShutdownEditor();

	Ui_PopAllViewControllers();

	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
