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

#include "r_local.h"

/**
 * @brief
 */
void R_EnforceGlVersion(void) {
	const char *s = r_config.version_string;
	int32_t maj, min;

	sscanf(s, "%d.%d", &maj, &min);

	if (maj > 2)
		return;

	if (min > 1)
		return;

	Com_Error(ERR_FATAL, "OpenGL version %s is less than 2.1\n", s);
}

/**
 * @brief
 */
void R_InitGlPointers(void) {

	gladLoadGL();
}
