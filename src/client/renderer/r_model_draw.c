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

r_model_program_t r_model_program;

#define WARP_IMAGE_SIZE 16

/**
 * @brief
 */
void R_InitModelProgram(void) {

	memset(&r_model_program, 0, sizeof(r_model_program));

	r_model_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "material.glsl", "model_vs.glsl", NULL),
			R_ShaderDescriptor(GL_GEOMETRY_SHADER, "polylib.glsl", "model_gs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "material.glsl", "model_fs.glsl", NULL),
			NULL);

	glUseProgram(r_model_program.name);

	r_model_program.uniforms_block = glGetUniformBlockIndex(r_model_program.name, "uniforms_block");
	glUniformBlockBinding(r_model_program.name, r_model_program.uniforms_block, 0);

	r_model_program.lights_block = glGetUniformBlockIndex(r_model_program.name, "lights_block");
	glUniformBlockBinding(r_model_program.name, r_model_program.lights_block, 1);

	r_model_program.in_position = glGetAttribLocation(r_model_program.name, "in_position");
	r_model_program.in_normal = glGetAttribLocation(r_model_program.name, "in_normal");
	r_model_program.in_tangent = glGetAttribLocation(r_model_program.name, "in_tangent");
	r_model_program.in_bitangent = glGetAttribLocation(r_model_program.name, "in_bitangent");
	r_model_program.in_diffusemap = glGetAttribLocation(r_model_program.name, "in_diffusemap");
	r_model_program.in_lightmap = glGetAttribLocation(r_model_program.name, "in_lightmap");
	r_model_program.in_color = glGetAttribLocation(r_model_program.name, "in_color");

	r_model_program.in_next_position = glGetAttribLocation(r_model_program.name, "in_next_position");
	r_model_program.in_next_normal = glGetAttribLocation(r_model_program.name, "in_next_normal");
	r_model_program.in_next_tangent = glGetAttribLocation(r_model_program.name, "in_next_tangent");
	r_model_program.in_next_bitangent = glGetAttribLocation(r_model_program.name, "in_next_bitangent");

	r_model_program.model_type = glGetUniformLocation(r_model_program.name, "model_type");
	r_model_program.model = glGetUniformLocation(r_model_program.name, "model");
	r_model_program.lerp = glGetUniformLocation(r_model_program.name, "lerp");

	r_model_program.texture_material = glGetUniformLocation(r_model_program.name, "texture_material");
	r_model_program.texture_stage = glGetUniformLocation(r_model_program.name, "texture_stage");
	r_model_program.texture_warp = glGetUniformLocation(r_model_program.name, "texture_warp");

	r_model_program.texture_lightmap_ambient = glGetUniformLocation(r_model_program.name, "texture_lightmap_ambient");
	r_model_program.texture_lightmap_diffuse = glGetUniformLocation(r_model_program.name, "texture_lightmap_diffuse");
	r_model_program.texture_lightmap_direction = glGetUniformLocation(r_model_program.name, "texture_lightmap_direction");
	r_model_program.texture_lightmap_caustics = glGetUniformLocation(r_model_program.name, "texture_lightmap_caustics");
	r_model_program.texture_lightmap_stains = glGetUniformLocation(r_model_program.name, "texture_lightmap_stains");

	r_model_program.texture_lightgrid_ambient = glGetUniformLocation(r_model_program.name, "texture_lightgrid_ambient");
	r_model_program.texture_lightgrid_diffuse = glGetUniformLocation(r_model_program.name, "texture_lightgrid_diffuse");
	r_model_program.texture_lightgrid_direction = glGetUniformLocation(r_model_program.name, "texture_lightgrid_direction");
	r_model_program.texture_lightgrid_caustics = glGetUniformLocation(r_model_program.name, "texture_lightgrid_caustics");
	r_model_program.texture_lightgrid_fog = glGetUniformLocation(r_model_program.name, "texture_lightgrid_fog");

	r_model_program.texture_shadowmap = glGetUniformLocation(r_model_program.name, "texture_shadowmap");
	r_model_program.texture_shadowmap_cube = glGetUniformLocation(r_model_program.name, "texture_shadowmap_cube");

	r_model_program.material.alpha_test = glGetUniformLocation(r_model_program.name, "material.alpha_test");
	r_model_program.material.roughness = glGetUniformLocation(r_model_program.name, "material.roughness");
	r_model_program.material.hardness = glGetUniformLocation(r_model_program.name, "material.hardness");
	r_model_program.material.specularity = glGetUniformLocation(r_model_program.name, "material.specularity");
	r_model_program.material.bloom = glGetUniformLocation(r_model_program.name, "material.bloom");

	r_model_program.stage.flags = glGetUniformLocation(r_model_program.name, "stage.flags");
	r_model_program.stage.color = glGetUniformLocation(r_model_program.name, "stage.color");
	r_model_program.stage.pulse = glGetUniformLocation(r_model_program.name, "stage.pulse");
	r_model_program.stage.st_origin = glGetUniformLocation(r_model_program.name, "stage.st_origin");
	r_model_program.stage.stretch = glGetUniformLocation(r_model_program.name, "stage.stretch");
	r_model_program.stage.rotate = glGetUniformLocation(r_model_program.name, "stage.rotate");
	r_model_program.stage.scroll = glGetUniformLocation(r_model_program.name, "stage.scroll");
	r_model_program.stage.scale = glGetUniformLocation(r_model_program.name, "stage.scale");
	r_model_program.stage.terrain = glGetUniformLocation(r_model_program.name, "stage.terrain");
	r_model_program.stage.dirtmap = glGetUniformLocation(r_model_program.name, "stage.dirtmap");
	r_model_program.stage.warp = glGetUniformLocation(r_model_program.name, "stage.warp");

	glUniform1i(r_model_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_model_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_model_program.texture_warp, TEXTURE_WARP);
	glUniform1i(r_model_program.texture_lightmap_ambient, TEXTURE_LIGHTMAP_AMBIENT);
	glUniform1i(r_model_program.texture_lightmap_diffuse, TEXTURE_LIGHTMAP_DIFFUSE);
	glUniform1i(r_model_program.texture_lightmap_direction, TEXTURE_LIGHTMAP_DIRECTION);
	glUniform1i(r_model_program.texture_lightmap_caustics, TEXTURE_LIGHTMAP_CAUSTICS);
	glUniform1i(r_model_program.texture_lightmap_stains, TEXTURE_LIGHTMAP_STAINS);
	glUniform1i(r_model_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_model_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
	glUniform1i(r_model_program.texture_lightgrid_direction, TEXTURE_LIGHTGRID_DIRECTION);
	glUniform1i(r_model_program.texture_lightgrid_caustics, TEXTURE_LIGHTGRID_CAUSTICS);
	glUniform1i(r_model_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);
	glUniform1i(r_model_program.texture_shadowmap, TEXTURE_SHADOWMAP);
	glUniform1i(r_model_program.texture_shadowmap_cube, TEXTURE_SHADOWMAP_CUBE);

	r_model_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), R_MEDIA_IMAGE);
	r_model_program.warp_image->media.Retain = R_RetainImage;
	r_model_program.warp_image->media.Free = R_FreeImage;

	r_model_program.warp_image->width = r_model_program.warp_image->height = WARP_IMAGE_SIZE;
	r_model_program.warp_image->type = IMG_PROGRAM;
	r_model_program.warp_image->target = GL_TEXTURE_2D;
	r_model_program.warp_image->internal_format = GL_RGBA8;
	r_model_program.warp_image->format = GL_RGBA;
	r_model_program.warp_image->pixel_type = GL_UNSIGNED_BYTE;

	byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];

	for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
		for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
			data[i][j][0] = RandomRangeu(0, 48);
			data[i][j][1] = RandomRangeu(0, 48);
			data[i][j][2] = 0;
			data[i][j][3] = 255;
		}
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);

	R_UploadImage(r_model_program.warp_image, (byte *) data);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownModelProgram(void) {

	glDeleteProgram(r_model_program.name);

	r_model_program.name = 0;
}

