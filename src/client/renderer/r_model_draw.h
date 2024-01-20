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
 * @brief The model program.
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
	GLint in_lightmap;
	GLint in_color;

	GLint in_next_position;
	GLint in_next_normal;
	GLint in_next_tangent;
	GLint in_next_bitangent;

	GLint model_type;
	GLint model;
	GLint lerp;

	GLint texture_material;
	GLint texture_lightmap_ambient;
	GLint texture_lightmap_diffuse;
	GLint texture_lightmap_direction;
	GLint texture_lightmap_caustics;
	GLint texture_lightmap_stains;
	GLint texture_stage;
	GLint texture_warp;
	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
	GLint texture_lightgrid_direction;
	GLint texture_lightgrid_caustics;
	GLint texture_lightgrid_fog;
	GLint texture_shadowmap;
	GLint texture_shadowmap_cube;

	GLint alpha_test;

	struct {
		GLint alpha_test;
		GLint roughness;
		GLint hardness;
		GLint specularity;
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
} r_model_program_t;

extern r_model_program_t r_model_program;

extern void R_InitModelProgram(void);
extern void R_ShutdownModelProgram(void);

#endif
