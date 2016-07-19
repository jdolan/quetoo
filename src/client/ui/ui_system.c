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

/**
 * @brief A NULL-terminated array of TwEnumVal for available display modes.
 */
static TwEnumVal *Ui_DisplayModes(void) {

	const int32_t display = SDL_GetWindowDisplayIndex(r_context.window);

	GList *list = NULL;

	for (int32_t i = 0; i < SDL_GetNumDisplayModes(display); i++) {
		SDL_DisplayMode *mode = g_new(SDL_DisplayMode, 1);
		SDL_GetDisplayMode(display, i, mode);

		if (SDL_BITSPERPIXEL(mode->format) < 32) {
			break;
		}

		list = g_list_append(list, mode);
	}

	const size_t count = g_list_length(list);

	TwEnumVal *modes = Mem_TagMalloc((count + 2) * sizeof(TwEnumVal), MEM_TAG_UI);

	for (size_t i = 0; i < count; i++) {
		SDL_DisplayMode *mode = g_list_nth_data(list, i);
		char *label = Mem_CopyString(va("%dx%d", mode->w, mode->h));

		modes[i].Label = Mem_Link(label, modes);
		modes[i].Value = i;
	}

	modes[count].Label = "Custom";
	modes[count].Value = -1;

	g_list_free_full(list, g_free);

	return modes;
}

/**
 * @brief TwSetVarCallback for display mode.
 */
static void TW_CALL Ui_System_SetDisplayMode(const void *value, void *data __attribute__((unused))) {

	const int32_t index = *(int32_t *) value;
	if (index > -1) {
		const int32_t display = SDL_GetWindowDisplayIndex(r_context.window);

		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(display, index, &mode) == 0) {
			if (r_fullscreen->value) {
				Cvar_SetValue(r_width->name, mode.w);
				Cvar_SetValue(r_height->name, mode.h);
			} else {
				Cvar_SetValue(r_windowed_width->name, mode.w);
				Cvar_SetValue(r_windowed_height->name, mode.h);
			}
		} else {
			Com_Warn("Failed to get video mode: %s\n", SDL_GetError());
		}
	}
}

/**
 * @brief TwGetVarCallback for display mode.
 */
static void TW_CALL Ui_System_GetDisplayMode(void *value, void *data __attribute__((unused))) {

	const int32_t display = SDL_GetWindowDisplayIndex(r_context.window);

	int32_t index = -1;

	for (int32_t i = 0; i < SDL_GetNumDisplayModes(display); i++) {
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(display, i, &mode);

		if (SDL_BITSPERPIXEL(mode.format) < 32) {
			break;
		}

		if (r_fullscreen->value) {
			if (mode.w == r_width->integer && mode.h == r_height->integer) {
				index = i;
				break;
			}
		} else {
			if (mode.w == r_windowed_width->integer && mode.h == r_windowed_height->integer) {
				index = i;
				break;
			}
		}
	}

	*(int32_t *) value = index;
}

/**
 * @brief
 */
TwBar *Ui_System(void) {

	const TwEnumVal *DisplayModesEnum = Ui_DisplayModes();
	TwType DisplayModes = TwDefineEnum("Display Modes", DisplayModesEnum, Ui_EnumLength(DisplayModesEnum));

	const TwEnumVal RateEnum[] = {
		{ 16384, "DSL" },
		{ 131072, "Cable" },
		{ 0, "LAN" },
	};

	TwType Rate = TwDefineEnum("Rate", RateEnum, lengthof(RateEnum));

	TwBar *bar = TwNewBar("System");

	TwAddVarCB(bar, "Mode", DisplayModes, Ui_System_SetDisplayMode, Ui_System_GetDisplayMode, NULL, "group=Video");
	Ui_CvarEnum(bar, "Fullscreen", r_fullscreen, ui.OffOrOn, "group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarDecimal(bar, "Brightness", r_brightness, "min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Contrast", r_contrast, "min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Saturation", r_saturation, "min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Modulate", r_modulate, "min=0.1 max=5.0 step=0.5 group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarEnum(bar, "Shaders", r_programs, ui.OffOrOn, "group=Video");
	Ui_CvarEnum(bar, "Shadows", r_shadows, ui.OffLowMediumHigh, "group=Video");
	Ui_CvarEnum(bar, "Anisotropy", r_anisotropy, ui.OffOrOn, "group=Video");
	Ui_CvarEnum(bar, "Multisampling", r_multisample, ui.OffLowMediumHigh, "group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarEnum(bar, "Vertical sync", r_swap_interval, ui.OffOrOn, "group=Video");
	Ui_CvarInteger(bar, "Framerate", cl_max_fps, "min=0 max=250 step=5 group=Video");

	Ui_CvarDecimal(bar, "Volume", s_volume, "min=0.0 max=1.0 step=0.05 group=Audio");
	Ui_CvarDecimal(bar, "Music volume", s_music_volume, "min=0.0 max=1.0 step=0.05 group=Audio");

	Ui_CvarEnum(bar, "Connection", rate, Rate, "group=Network");

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Apply", Ui_Command, "r_restart; s_restart;", NULL);

	TwDefine("System size='400 400' alpha=200 iconifiable=false valueswidth=100 visible=false");

	return bar;
}
