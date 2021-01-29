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

#include "cl_local.h"

/**
 * @brief Restarts the renderer subsystem.
 */
void Cl_R_Restart_f(void) {

	if (cls.state == CL_CONNECTING ||
		cls.state == CL_CONNECTED ||
		cls.state == CL_LOADING) {
		return;
	}

	Ui_HandleEvent(&(const SDL_Event) {
		.window.type = SDL_WINDOWEVENT,
		.window.event = SDL_WINDOWEVENT_CLOSE
	});

	R_Shutdown();

	R_Init();

	if (cls.state == CL_ACTIVE) {
		Cl_LoadMedia();
	}
}

/**
 * @brief Toggles fullscreen vs windowed mode.
 */
void Cl_R_ToggleFullscreen_f(void) {

	Cvar_Toggle("r_fullscreen");

	Cl_R_Restart_f();
}
