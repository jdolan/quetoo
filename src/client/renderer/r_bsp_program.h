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

#define MAX_ACTIVE_LIGHTS 10

typedef enum {
	BSP_TEXTURE_DIFFUSE    = 1 << 0,
	BSP_TEXTURE_NORMALMAP  = 1 << 1,
	BSP_TEXTURE_GLOSSMAP   = 1 << 2,

	BSP_TEXTURE_LIGHTMAP   = 1 << 3,
	BSP_TEXTURE_DELUXEMAP  = 1 << 4,
	BSP_TEXTURE_STAINMAP   = 1 << 5,

	BSP_TEXTURE_ALL        = 0xff
} r_bsp_program_texture_t;

typedef struct r_bsp_program_s r_bsp_program_t;
struct r_bsp_program_s {
    GLuint name;

    struct {
        GLint in_position;
        GLint in_normal;
        GLint in_tangent;
        GLint in_bitangent;
        GLint in_diffuse;
        GLint in_lightmap;
		GLint in_color;
    } attributes;

    struct {
        GLint projection;
        GLint model_view;
        GLint normal;

		GLint textures;

        GLint texture_diffuse;
		GLint texture_normalmap;
		GLint texture_glossmap;
        GLint texture_lightmap;

		GLint contents;

		GLint color;
		GLint alpha_threshold;

		GLint brightness;
		GLint contrast;
		GLint saturation;
		GLint gamma;
		GLint modulate;

        GLint bump;
        GLint parallax;
        GLint hardness;
        GLint specular;

		GLint light_positions[MAX_ACTIVE_LIGHTS];
		GLint light_colors[MAX_ACTIVE_LIGHTS];

		GLint fog_parameters;
		GLint fog_color;

        GLint caustics;
    } uniforms;
};

extern r_bsp_program_t r_bsp_program;

extern void R_InitBspProgram(void);
extern void R_ShutdownBspProgram(void);

#endif /* __R_LOCAL_H__ */
