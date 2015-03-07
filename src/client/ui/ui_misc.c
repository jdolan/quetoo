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

/*
 * @brief Displays the TwBar specified by name in data, hiding all others.
 */
void TW_CALL Ui_ShowBar(void *data) {
	const char *name = (const char *) data;
	int32_t hidden = 0, visible = 1;

	Com_Debug("%s\n", name);

	if (ui.top) {
		TwSetParam(ui.top, NULL, "visible", TW_PARAM_INT32, 1, &hidden);
	}

	ui.top = TwGetBarByName(name);

	if (ui.top) {
		TwSetParam(ui.top, NULL, "visible", TW_PARAM_INT32, 1, &visible);
	}

	// then center the visible bar
	Ui_CenterBar((void *) name);
}

/*
 * @brief Centers the TwBar by the specified name.
 */
void TW_CALL Ui_CenterBar(void *data) {
	const char *name = (const char *) data;
	TwBar *bar = TwGetBarByName(name);

	if (bar) {
		dvec_t size[2], position[2];
		TwGetParam(bar, NULL, "size", TW_PARAM_DOUBLE, 2, size);

		position[0] = (r_context.width - size[0]) / 2.0;
		position[1] = (r_context.height - size[1]) / 2.0;

		if (position[0] < (r_pixel_t) 0)
			position[0] = (r_pixel_t) 0;

		if (position[1] < (r_pixel_t) 0)
			position[1] = (r_pixel_t) 0;

		position[0] = (r_pixel_t) position[0];
		position[1] = (r_pixel_t) position[1];

		// Com_Debug("%s: %4f, %4f\n", name, position[0], position[1]);

		TwSetParam(bar, NULL, "position", TW_PARAM_DOUBLE, 2, position);
	}
}
