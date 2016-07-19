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

ui_t ui;

extern cl_static_t cls;

/**
 * @brief Activates the menu system when ESC is pressed.
 */
static _Bool Ui_HandleKeyEvent(const SDL_Event *event) {

	switch (event->key.keysym.sym) {

		case SDLK_ESCAPE: {
			int32_t visible;

			TwBar *bar = cl_editor->value ? ui.editor : ui.root;
			TwGetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);

			if (!visible) {
				if (cl_editor->value)
					Ui_ShowBar("Editor");
				else
					Ui_ShowBar("Quetoo");

				return true;
			}
		}
			break;
	}

	return false;
}

/**
 * @brief Informs AntTweakBar of window events.
 */
static _Bool Ui_HandleWindowEvent(const SDL_Event *event) {

	switch (event->window.event) {

		case SDL_WINDOWEVENT_SHOWN:
		case SDL_WINDOWEVENT_SIZE_CHANGED:

			TwDefine("GLOBAL fontresizable=false fontstyle=fixed ");

			if (ui.top && ui.top != ui.editor) {
				Ui_CenterBar((void *) TwGetBarName(ui.top));
			}

			break;
	}

	return false;
}

/**
 * @brief Handles input events to the user interface, returning true if the
 * event was swallowed. While in focus, AntTweakBar receives all events. While
 * not in focus, we still pass SDL_WINDOWEVENTs to it.
 */
_Bool Ui_HandleEvent(const SDL_Event *event) {

	if (cls.key_state.dest == KEY_UI || event->type == SDL_WINDOWEVENT) {

		if (TwEventSDL(event, SDL_MAJOR_VERSION, SDL_MINOR_VERSION))
			return true;

		switch (event->type) {
			case SDL_KEYDOWN:
				return Ui_HandleKeyEvent(event);

			case SDL_WINDOWEVENT:
				return Ui_HandleWindowEvent(event);
		}
	}

	return false;
}

/**
 * @brief Draws any active TwBar components.
 */
void Ui_Draw(void) {

	if (cls.key_state.dest != KEY_UI)
		return;

	TwDraw();
}

/**
 * @brief Defines the root TwBar.
 */
static TwBar *Ui_Root(void) {
	TwBar *bar = TwNewBar("Quetoo");

	TwAddButton(bar, "Join Server", Ui_ShowBar, "Join Server", NULL);
	TwAddButton(bar, "Create Server", Ui_ShowBar, "Create Server", NULL);

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Controls", Ui_ShowBar, "Controls", NULL);
	TwAddButton(bar, "Player Setup", Ui_ShowBar, "Player Setup", NULL);
	TwAddButton(bar, "System", Ui_ShowBar, "System", NULL);

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Credits", Ui_ShowBar, "Credits", NULL);
	TwAddButton(bar, "Quit", Ui_Command, "quit\n", NULL);

	TwDefine("Quetoo size='300 180' alpha=200 iconifiable=false");

	Ui_ShowBar("Quetoo");

	return bar;
}

/**
 * @brief
 */
static void Ui_Restart_f(void) {

	Ui_Shutdown();

	Ui_Init();
}

/**
 * @brief
 */
void Ui_Init(void) {

	const TwEnumVal OffOrOn[] = {
		{ 0, "Off" },
		{ 1, "On" }
	};

	const TwEnumVal OffLowMediumHigh[] = {
		{ 0, "Off" },
		{ 1, "Low" },
		{ 2, "Medium" },
		{ 3, "High" }
	};

	memset(&ui, 0, sizeof(ui));

	TwInit(TW_OPENGL, NULL);

	ui.OffOrOn = TwDefineEnum("OnOrOff", OffOrOn, lengthof(OffOrOn));
	ui.OffLowMediumHigh = TwDefineEnum("OffLowMediumHigh", OffLowMediumHigh, lengthof(OffLowMediumHigh));

	ui.root = Ui_Root();
	ui.servers = Ui_Servers();
	ui.create_server = Ui_CreateServer();
	ui.controls = Ui_Controls();
	ui.player = Ui_Player();
	ui.system = Ui_System();
	ui.credits = Ui_Credits();
	ui.editor = Ui_Editor();

	Ui_ShowBar("Quetoo");

	Cmd_Add("ui_restart", Ui_Restart_f, CMD_UI, "Restarts the menus subsystem");
}

/**
 * @brief
 */
void Ui_Shutdown(void) {

	TwTerminate();

	Cmd_RemoveAll(CMD_UI);
}
