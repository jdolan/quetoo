/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

/*
 * Ui_System
 */
TwBar *Ui_System(void) {

	TwBar *bar = TwNewBar("System");

	Ui_CvarInteger(bar, "Width", r_width,
			"min=1024 max=4096 step=128 group=Video");
	Ui_CvarInteger(bar, "Height", r_height,
			"min=768 max=4096 step=128 group=Video");
	Ui_CvarInteger(bar, "Windowed width", r_windowed_width,
			"min=1024 max=4096 step=128 group=Video");
	Ui_CvarInteger(bar, "Windowed height", r_windowed_height,
			"min=768 max=4096 step=128 group=Video");
	Ui_CvarEnum(bar, "Fullscreen", r_fullscreen, ui.OffOrOn, "group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarDecimal(bar, "Brightness", r_brightness,
			"min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Contrast", r_contrast,
			"min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Saturation", r_saturation,
			"min=0.1 max=3.0 step=0.1 group=Video");
	Ui_CvarDecimal(bar, "Modulate", r_modulate,
			"min=0.1 max=5.0 step=0.5 group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarEnum(bar, "Shaders", r_programs, ui.OffOrOn, "group=Video");
	Ui_CvarEnum(bar, "Anisotropy", r_anisotropy, ui.OffOrOn, "group=Video");
	Ui_CvarEnum(bar, "Anti-aliasing", r_multisample, ui.OffLowMediumHigh,
			"group=Video");

	TwAddSeparator(bar, NULL, "group=Video");

	Ui_CvarEnum(bar, "Vertical sync", r_swap_interval, ui.OffOrOn,
			"group=Video");
	Ui_CvarInteger(bar, "Framerate", cl_max_fps,
			"min=0 max=240 step=5 group=Video");

	Ui_CvarDecimal(bar, "Volume", s_volume,
			"min=0.0 max=1.0 step=0.1 group=Audio");
	Ui_CvarDecimal(bar, "Music volume", s_music_volume,
			"min=0.0 max=1.0 step=0.1 group=Audio");

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Apply", Ui_Command, "r_restart; s_restart", NULL);

	TwDefine("System size='240 380' iconifiable=false visible=false");

	return bar;
}
