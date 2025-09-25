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

#ifdef __R_LOCAL_H__

/**
 * @brief The BSP program.
 */
typedef struct {
	GLuint name;

	GLuint uniforms_block;
	GLuint lights_block;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffusemap;
	GLint in_color;

	GLint active_lights;

	GLint model;

	GLint texture_material;
	GLint texture_stage;
	GLint texture_warp;

	GLint texture_voxel_diffuse;
	GLint texture_voxel_caustics;
	GLint texture_voxel_fog;
	GLint texture_voxel_stains;

	GLint texture_sky;

	GLint texture_shadow_cubemap_array0;
	GLint texture_shadow_cubemap_array1;
	GLint texture_shadow_cubemap_array2;
	GLint texture_shadow_cubemap_array3;

	GLint alpha_test;

	struct {
		GLint alpha_test;
		GLint roughness;
		GLint hardness;
		GLint specularity;
		GLint parallax;
		GLint bloom;
	} material;

	struct {
		GLint flags;
		GLint color;
		GLint pulse;
		GLint st_origin;
		GLint stretch;
		GLint rotate;
		GLint scroll;
		GLint scale;
		GLint terrain;
		GLint dirtmap;
		GLint warp;
	} stage;

	r_image_t *warp_image;
} r_bsp_program_t;

extern r_bsp_program_t r_bsp_program;

void R_InitBspProgram(void);
void R_ShutdownBspProgram(void);
void R_DrawOpaqueBspInlineEntities(const r_view_t *view);
void R_DrawBlendBspInlineEntities(const r_view_t *view);
void R_AddBspVoxelSprites(r_view_t *view);
#endif
