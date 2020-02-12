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

static cvar_t *cl_view_size;

/**
 * @brief Clears all volatile view members so that a new scene may be populated.
 */
void Cl_ClearView(void) {

	r_view.num_entities = 0;
	r_view.num_lights = 0;
	r_view.num_particles = 0;
	r_view.num_stains = 0;
}

/**
 * @brief
 */
static void Cl_UpdateViewSize(void) {

	if (!cl_view_size->modified && !r_view.update) {
		return;
	}

	Cvar_SetValue(cl_view_size->name, clampf(cl_view_size->value, 40.0, 100.0));

	r_view.viewport.w = r_context.width * cl_view_size->value / 100.0;
	r_view.viewport.h = r_context.height * cl_view_size->value / 100.0;

	r_view.viewport.x = (r_context.width - r_view.viewport.w) / 2.0;
	r_view.viewport.y = (r_context.height - r_view.viewport.h) / 2.0;

	cl_view_size->modified = false;
}

/**
 * @brief Updates the view definition for the pending render frame.
 */
void Cl_UpdateView(void) {

	r_view.ticks = cl.unclamped_time;
	r_view.area_bits = cl.frame.area_bits;

	Cl_UpdateViewSize();

	cls.cgame->UpdateView(&cl.frame);
}

/**
 * @brief
 */
static void Cl_ViewSizeUp_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer + 10);
}

/**
 * @brief
 */
static void Cl_ViewSizeDown_f(void) {
	Cvar_SetValue("cl_view_size", cl_view_size->integer - 10);
}

/**
 * @brief
 */
void Cl_InitView(void) {

	cl_view_size = Cvar_Add("cl_view_size", "100.0", CVAR_ARCHIVE, NULL);

	Cmd_Add("cl_view_size_up", Cl_ViewSizeUp_f, CMD_CLIENT, NULL);
	Cmd_Add("cl_view_size_down", Cl_ViewSizeDown_f, CMD_CLIENT, NULL);
}
