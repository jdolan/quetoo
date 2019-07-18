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

#include "cg_local.h"

static r_atlas_t *cg_stain_atlas;

/**
 *
 */
r_media_t *Cg_LoadStain(const char *name, r_image_type_t image_type) {

	if (*name == '@') {
		return (r_media_t *) cgi.LoadAtlasImage(cg_stain_atlas, name + 1, image_type);
	} else {
		return (r_media_t *) cgi.LoadImage(name, image_type);
	}
}

/**
 * @brief Initializes stain subsystem
 */
void Cg_InitStains(void) {

	cg_stain_atlas = cgi.CreateAtlas("cg_stain_atlas");
}

/**
 * @brief Called when all stain images are done loading.
 */
void Cg_CompileStainAtlas(void) {

	cgi.CompileAtlas(cg_stain_atlas);
}
