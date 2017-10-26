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

#pragma once

#include "r_types.h"

r_atlas_t *R_CreateAtlas(const char *name);
void R_AddImageToAtlas(r_atlas_t *atlas, const r_image_t *image);
const r_atlas_image_t *R_GetAtlasImageFromAtlas(const r_atlas_t *atlas, const r_image_t *image);
void R_CompileAtlas(r_atlas_t *atlas);

#ifdef __R_LOCAL_H__

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t x;
	uint32_t y;

	uint32_t right;
	uint32_t down; // nodes to the right/below this one
} r_packer_node_t;

typedef struct {
	// required if only reading
	uint32_t num_nodes;
	r_packer_node_t *nodes;
	uint32_t root;

	// required for writing
	uint32_t num_alloc_nodes;
	_Bool keep_square;
	uint32_t max_width, max_height;
} r_packer_t;

void R_AtlasPacker_InitPacker(r_packer_t *packer, const uint32_t max_width, const uint32_t max_height,
                              const uint32_t root_width, const uint32_t root_height, const uint32_t initial_size);
void R_AtlasPacker_FreePacker(r_packer_t *packer);
r_packer_node_t *R_AtlasPacker_GrowNode(r_packer_t *packer, const uint32_t width, const uint32_t height);
r_packer_node_t *R_AtlasPacker_SplitNode(r_packer_t *packer, r_packer_node_t *node, const uint32_t width,
        const uint32_t height);
r_packer_node_t *R_AtlasPacker_FindNode(r_packer_t *packer, const uint32_t root, const uint32_t width,
                                        const uint32_t height);
void R_AtlasPacker_Serialize(const r_packer_t *packer, file_t *file);
_Bool R_AtlasPacker_Unserialize(file_t *file, r_packer_t *packer);

void R_InitAtlas(void);

#endif /* __R_LOCAL_H__ */
