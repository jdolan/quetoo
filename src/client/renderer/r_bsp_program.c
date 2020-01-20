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

r_bsp_program_t r_bsp_program;

#define GET_ATTRIBUTE_LOCATION(prog, attr) \
	prog->attributes.attr = glGetAttribLocation(prog->name, #attr)

#define GET_UNIFORM_LOCATION(prog, uniform) \
	prog->uniforms.uniform = glGetUniformLocation(prog->name, #uniform)

/**
 * @brief
 */
void R_InitBspProgram(void) {

	r_bsp_program_t *prog = &r_bsp_program;

	memset(prog, 0, sizeof(*prog));

	prog->name = R_LoadProgram(
		&MakeShaderDescriptor(GL_VERTEX_SHADER, "bsp_vs.glsl"),
		&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "bsp_fs.glsl"),
		NULL);

	glUseProgram(prog->name);

	GET_ATTRIBUTE_LOCATION(prog, in_position);
	GET_ATTRIBUTE_LOCATION(prog, in_normal);
	GET_ATTRIBUTE_LOCATION(prog, in_tangent);
	GET_ATTRIBUTE_LOCATION(prog, in_bitangent);
	GET_ATTRIBUTE_LOCATION(prog, in_diffuse);
	GET_ATTRIBUTE_LOCATION(prog, in_lightmap);
	GET_ATTRIBUTE_LOCATION(prog, in_color);

	GET_UNIFORM_LOCATION(prog, projection);
	GET_UNIFORM_LOCATION(prog, model_view);
	GET_UNIFORM_LOCATION(prog, normal);

	GET_UNIFORM_LOCATION(prog, textures);
	GET_UNIFORM_LOCATION(prog, texture_diffuse);
	GET_UNIFORM_LOCATION(prog, texture_normalmap);
	GET_UNIFORM_LOCATION(prog, texture_glossmap);
	GET_UNIFORM_LOCATION(prog, texture_lightmap);

	GET_UNIFORM_LOCATION(prog, contents);

	GET_UNIFORM_LOCATION(prog, color);
	GET_UNIFORM_LOCATION(prog, alpha_threshold);

	GET_UNIFORM_LOCATION(prog, brightness);
	GET_UNIFORM_LOCATION(prog, contrast);
	GET_UNIFORM_LOCATION(prog, saturation);
	GET_UNIFORM_LOCATION(prog, modulate);

	GET_UNIFORM_LOCATION(prog, bump);
	GET_UNIFORM_LOCATION(prog, parallax);
	GET_UNIFORM_LOCATION(prog, hardness);
	GET_UNIFORM_LOCATION(prog, specular);

	GET_UNIFORM_LOCATION(prog, light_positions[0]);
	GET_UNIFORM_LOCATION(prog, light_positions[1]);
	GET_UNIFORM_LOCATION(prog, light_positions[2]);
	GET_UNIFORM_LOCATION(prog, light_positions[3]);
	GET_UNIFORM_LOCATION(prog, light_positions[4]);
	GET_UNIFORM_LOCATION(prog, light_positions[5]);
	GET_UNIFORM_LOCATION(prog, light_positions[6]);
	GET_UNIFORM_LOCATION(prog, light_positions[7]);
	GET_UNIFORM_LOCATION(prog, light_positions[8]);
	GET_UNIFORM_LOCATION(prog, light_positions[9]);

	GET_UNIFORM_LOCATION(prog, light_colors[0]);
	GET_UNIFORM_LOCATION(prog, light_colors[1]);
	GET_UNIFORM_LOCATION(prog, light_colors[2]);
	GET_UNIFORM_LOCATION(prog, light_colors[3]);
	GET_UNIFORM_LOCATION(prog, light_colors[4]);
	GET_UNIFORM_LOCATION(prog, light_colors[5]);
	GET_UNIFORM_LOCATION(prog, light_colors[6]);
	GET_UNIFORM_LOCATION(prog, light_colors[7]);
	GET_UNIFORM_LOCATION(prog, light_colors[8]);
	GET_UNIFORM_LOCATION(prog, light_colors[9]);

	GET_UNIFORM_LOCATION(prog, fog_parameters);
	GET_UNIFORM_LOCATION(prog, fog_color);

	GET_UNIFORM_LOCATION(prog, caustics);

	R_GetError(NULL);

	glUniform1i(prog->uniforms.texture_diffuse, BSP_TEXTURE_DIFFUSE);
	glUniform1i(prog->uniforms.texture_normalmap, BSP_TEXTURE_NORMALMAP);
	glUniform1i(prog->uniforms.texture_glossmap, BSP_TEXTURE_GLOSSMAP);
	glUniform1i(prog->uniforms.texture_lightmap, BSP_TEXTURE_LIGHTMAP);

	glUniform4f(prog->uniforms.color, 1.f, 1.f, 1.f, 1.f);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownBspProgram(void) {

	glDeleteProgram(r_bsp_program.name);

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));
}



