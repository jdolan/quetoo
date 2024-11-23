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
 * @brief Retain callback for Ui sounds.
 */
static bool Ui_RetainSample(s_media_t *media) {
	return true;
}

/**
 * @brief Loads a sample for the Ui.
 */
static s_sample_t *Ui_LoadSample(const char *name) {

	s_sample_t *sample = S_LoadSample(name);

	if (sample) {
		sample->media.Retain = Ui_RetainSample;
	}

	return sample;
}

/**
 * @brief
 */
static void Ui_HandleViewEvent(const View *view, ViewEvent event) {

	const char *attr = NULL;

	switch (event) {
		case ViewEventClick:
			attr = "-sound-on-click";
			break;
		case ViewEventChange:
			attr = "-sound-on-change";
			break;
		case ViewEventKeyDown:
			attr = "-sound-on-key-down";
			break;
		case ViewEventKeyUp:
			attr = "-sound-on-key-up";
			break;
		case ViewEventMouseEnter:
			attr = "-sound-on-mouse-enter";
			break;
		case ViewEventMouseLeave:
			attr = "-sound-on-mouse-leave";
			break;
		case ViewEventFocus:
			attr = "-sound-on-focus";
			break;
		case ViewEventBlur:
			attr = "-sound-on-blur";
			break;
		default:
			return;
	}

	const String *sound = $(view->computedStyle, attributeValue, attr);
	if (sound) {
		S_AddSample(&cl_stage, &(s_play_sample_t) {
			.sample = Ui_LoadSample(sound->chars),
			.flags = S_PLAY_UI,
		});
	}
}

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

		if (event->type == MVC_VIEW_EVENT) {
			Ui_HandleViewEvent(event->user.data1, event->user.code);
		}

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
void Ui_ViewWillDisappear(void) {

	if (windowController) {
		$(windowController->viewController, viewWillDisappear);
	} else {
		Com_Warn("windowControler was NULL\n");
	}
}

/**
 * @brief
 */
void Ui_Draw(void) {

	assert(windowController);

	Ui_CheckEditor();

	$(windowController, render);
}

/**
 * @brief
 */
ViewController *Ui_TopViewController(void) {

	if (navigationViewController) {
		return $(navigationViewController, topViewController);
	} else {
		Com_Warn("navigationViewController was NULL\n");
		return NULL;
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

	$$(Resource, addResourceProvider, Ui_Data);

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	Renderer *renderer = (Renderer *) $(alloc(QuetooRenderer), init);

	$(windowController, setRenderer, renderer);

	navigationViewController = $(alloc(NavigationViewController), init);
	$(windowController, setViewController, (ViewController *) navigationViewController);

	Ui_LoadSample("ui/change");
	Ui_LoadSample("ui/click");
	Ui_LoadSample("ui/clack");

	Ui_InitEditor();
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	$$(Resource, removeResourceProvider, Ui_Data);

	Ui_ShutdownEditor();

	Ui_PopAllViewControllers();

	windowController = release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
