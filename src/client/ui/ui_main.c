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

#include "QuetooRenderer.h"

extern cl_static_t cls;

static WindowController *windowController;

static NavigationViewController *navigationViewController;

/**
 * @brief Dispatch events to the user interface.
 */
void Ui_HandleEvent(const SDL_Event *event) {

	if (windowController) {
		if (cls.key_state.dest != KEY_UI) {
			switch (event->type) {
				case SDL_WINDOWEVENT:
					break;
				default:
					return;
			}
		}

		$(windowController, respondToEvent, event);
	} else {
		Com_Warn("windowController was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_ViewWillAppear(void) {

	if (windowController) {
		$(windowController->viewController->view, updateBindings);
		$(windowController->viewController, viewWillAppear);
	} else {
		Com_Warn("windowController was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_Draw(void) {

	assert(windowController);

	Ui_CheckEditor();

	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PushMatrix(matrix);
	}

	R_SetMatrix(R_MATRIX_PROJECTION, &r_view.matrix_base_ui);

	$(windowController, render);

	for (r_matrix_id_t matrix = R_MATRIX_PROJECTION; matrix < R_MATRIX_TOTAL; ++matrix) {
		R_PopMatrix(matrix);
	}
}

/**
 * @brief
 */
void Ui_PushViewController(ViewController *viewController) {

	if (navigationViewController) {
		if (viewController) {
			$(navigationViewController, pushViewController, viewController);
		} else {
			Com_Warn("viewController was NULL\n");
		}
	} else {
		Com_Warn("navigationViewController was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_PopToViewController(ViewController *viewController) {

	if (navigationViewController) {
		if (viewController) {
			$(navigationViewController, popToViewController, viewController);
		} else {
			Com_Warn("viewController was NULL\n");
		}
	} else {
		Com_Warn("navigationViewController was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_PopViewController(void) {

	if (navigationViewController) {
		$(navigationViewController, popViewController);
	} else {
		Com_Warn("navigationViewController was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_PopAllViewControllers(void) {

	if (navigationViewController) {
		$(navigationViewController, popToRootViewController);
	} else {
		Com_Warn("navigationViewController was NULL\n");
	}
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

	MVC_LogSetPriority(SDL_LOG_PRIORITY_DEBUG);

	$$(Resource, setProvider, Ui_Data);

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

	windowController = release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
