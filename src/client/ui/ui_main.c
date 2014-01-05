/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief Handles input events, returning true if the event was swallowed by TwBar.
 */
_Bool Ui_Event(SDL_Event *event) {
	_Bool handled;

	if (!(handled = TwEventSDL(event, SDL_MAJOR_VERSION, SDL_MINOR_VERSION))) {

		if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
			int32_t visible;

			TwBar *bar = cl_editor->value ? ui.editor : ui.root;
			TwGetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);

			if (!visible) {
				if (cl_editor->value)
					Ui_ShowBar("Editor");
				else
					Ui_ShowBar("Quake2World");
				handled = true;
			}
		}
	}

	return handled;
}

/*
 * @brief Draws any active TwBar components.
 */
void Ui_Draw(void) {
	static int32_t w, h;

	if (w != r_context.width || h != r_context.height || r_view.update) {

		w = r_context.width;
		h = r_context.height;

		TwWindowSize(w, h);
		TwDefine("GLOBAL fontresizable=false fontstyle=fixed ");

		if (ui.top && ui.top != ui.editor) {
			Ui_CenterBar((void *) TwGetBarName(ui.top));
		}
	}

	if (cls.key_state.dest != KEY_UI)
		return;

	TwDraw();
}

/*
 * @brief Defines the root TwBar.
 */
static TwBar *Ui_Root(void) {
	TwBar *bar = TwNewBar("Quake2World");

	TwAddButton(bar, "Servers", Ui_ShowBar, "Servers", NULL);
	TwAddButton(bar, "Controls", Ui_ShowBar, "Controls", NULL);
	TwAddButton(bar, "Player", Ui_ShowBar, "Player", NULL);
	TwAddButton(bar, "System", Ui_ShowBar, "System", NULL);
	TwAddButton(bar, "Credits", Ui_ShowBar, "Credits", NULL);

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Quit", Ui_Command, "quit\n", NULL);

	TwDefine("Quake2World size='240 150' alpha=200 iconifiable=false");

	Ui_ShowBar("Quake2World");

	return bar;
}

/*
 * @brief
 */
static void Ui_Restart_f(void) {

	Ui_Shutdown();

	Ui_Init();
}

/*
 * @brief
 */
void Ui_Init(void) {

	const TwEnumVal OffOrOn[] = { { 0, "Off" }, { 1, "On" } };

	const TwEnumVal OffLowMediumHigh[] = {
			{ 0, "Off" },
			{ 1, "Low" },
			{ 2, "Medium" },
			{ 3, "High" } };

	memset(&ui, 0, sizeof(ui));

	TwInit(TW_OPENGL, NULL);

	ui.OffOrOn = TwDefineEnum("OnOrOff", OffOrOn, lengthof(OffOrOn));

	ui.OffLowMediumHigh = TwDefineEnum("OffLowMediumHigh", OffLowMediumHigh,
			lengthof(OffLowMediumHigh));

	ui.root = Ui_Root();
	ui.servers = Ui_Servers();
	ui.controls = Ui_Controls();
	ui.player = Ui_Player();
	ui.system = Ui_System();
	ui.credits = Ui_Credits();
	ui.editor = Ui_Editor();

	Ui_ShowBar("Quake2World");

	Cmd_Add("ui_restart", Ui_Restart_f, CMD_UI, "Restarts the menus subsystem");
}

/*
 * @brief
 */
void Ui_Shutdown(void) {

	TwTerminate();

	Cmd_RemoveAll(CMD_UI);
}
