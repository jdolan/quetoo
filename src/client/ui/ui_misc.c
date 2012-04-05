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

/**
 * Ui_ToggleBar
 *
 * Toggles the visibility of the TwBar specified by name in data.
 */
void TW_CALL Ui_ToggleBar(void *data) {
	const char *name = (const char *) data;
	TwBar *bar;
	int i, visible = 0;

	// first hide all other bars
	for (i = 0; i < TwGetBarCount(); i++) {

		bar = TwGetBarByIndex(i);

		const char *n = TwGetBarName(bar);

		if (strcmp(n, name)) {
			TwSetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);
		}
	}

	if (!strcmp(name, "*")) // special case for hiding all bars
		return;

	// then toggle the one we're interested in
	bar = TwGetBarByName(name);

	if (!bar) {
		Com_Warn("Ui_ToggleBar: No TwBar: %s\n", name);
		return;
	}

	TwGetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);

	visible = !visible;

	TwSetParam(bar, NULL, "visible", TW_PARAM_INT32, 1, &visible);

	if (visible) {
		Ui_CenterBar((void *) name);
	}
}

/**
 * Ui_CenterBar
 *
 * Centers the TwBar by the specified name.
 */
void TW_CALL Ui_CenterBar(void *data) {
	const char *name = (const char *) data;
	TwBar *bar;

	bar = TwGetBarByName(name);

	if (!bar) {
		Com_Warn("Ui_ToggleBar: No TwBar: %s\n", name);
		return;
	}

	double size[2], position[2];
	TwGetParam(bar, NULL, "size", TW_PARAM_DOUBLE, 2, size);

	position[0] = (r_pixel_t) ((r_context.width - size[0]) / 2.0);
	position[1] = (r_pixel_t) ((r_context.height - size[1]) / 2.0);

	Com_Debug("Ui_CenterBar: %s: %4f, %4f\n", name, position[0], position[1]);

	TwSetParam(bar, NULL, "position", TW_PARAM_DOUBLE, 2, position);
}
