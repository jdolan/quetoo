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
 * Ui_ToggleBar
 *
 * Toggles the visibility of the TwBar specified by name in data.
 */
void TW_CALL Ui_ToggleBar(void *data) {
	const char *name = (const char *) data;
	TwBar *bar;

	bar = TwGetBarByName(name);

	if (!bar) {
		Com_Warn("Ui_ToggleBar: No TwBar: %s\n", name);
		return;
	}

	int visible;

	TwGetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);

	visible = !visible;

	TwSetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);
}
