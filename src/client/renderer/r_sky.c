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

#define SKY_DISTANCE (MAX_WORLD_COORD)

#define SKY_ST_EPSILON (0.00175)

#define SKY_ST_MIN   (0.0 + SKY_ST_EPSILON)
#define SKY_ST_MAX   (1.0 - SKY_ST_EPSILON)

/**
 * @brief
 */
static struct {
	/**
	 * @brief The sky cubemap.
	 */
	r_image_t *image;

	/**
	 * @brief The sky matrix, which includes Quake rotation, but not view rotation.
	 */
	mat4_t cubemap_matrix;
} r_sky;

/**
 * @brief The sky program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_diffusemap;

	GLint texture_cubemap;
	GLint texture_lightgrid_fog;

	GLint cube;
} r_sky_program;

/**
 * @brief
 */
void R_DrawSky(const r_view_t *view) {

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glUseProgram(r_sky_program.name);

	Matrix4x4_CreateIdentity(&r_sky.cubemap_matrix);

	Matrix4x4_ConcatRotate(&r_sky.cubemap_matrix, -90.f, 1.f, 0.f, 0.f); // put Z going up
	Matrix4x4_ConcatRotate(&r_sky.cubemap_matrix,  90.f, 0.f, 0.f, 1.f); // put Z going up

	Matrix4x4_ConcatTranslate(&r_sky.cubemap_matrix, -view->origin.x, -view->origin.y, -view->origin.z);

	glUniformMatrix4fv(r_sky_program.cube, 1, GL_FALSE, (GLfloat *) r_sky.cubemap_matrix.m);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	glEnableVertexAttribArray(r_sky_program.in_position);
	glEnableVertexAttribArray(r_sky_program.in_diffusemap);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
	glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, r_sky.image->texnum);

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (draw->texinfo->flags & SURF_SKY) {
			glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}

/**
 * @brief
 */
static void R_InitSkyProgram(void) {

	memset(&r_sky_program, 0, sizeof(r_sky_program));

	r_sky_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "sky_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "sky_fs.glsl", NULL),
			NULL);

	glUseProgram(r_sky_program.name);

	r_sky_program.uniforms_block = glGetUniformBlockIndex(r_sky_program.name, "uniforms_block");
	glUniformBlockBinding(r_sky_program.name, r_sky_program.uniforms_block, 0);

	r_sky_program.in_position = glGetAttribLocation(r_sky_program.name, "in_position");
	r_sky_program.in_diffusemap = glGetAttribLocation(r_sky_program.name, "in_diffusemap");

	r_sky_program.texture_cubemap = glGetUniformLocation(r_sky_program.name, "texture_cubemap");
	r_sky_program.texture_lightgrid_fog = glGetUniformLocation(r_sky_program.name, "texture_lightgrid_fog");

	r_sky_program.cube = glGetUniformLocation(r_sky_program.name, "cube");

	glUniform1i(r_sky_program.texture_cubemap, TEXTURE_DIFFUSEMAP);
	glUniform1i(r_sky_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSky(void) {

	memset(&r_sky, 0, sizeof(r_sky));

	R_InitSkyProgram();
}

/**
 * @brief
 */
static void R_ShutdownSkyProgram(void) {

	glDeleteProgram(r_sky_program.name);

	r_sky_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownSky(void) {

	R_ShutdownSkyProgram();
}

/**
 * @brief Sets the sky to the specified environment map.
 */
void R_LoadSky(const char *name) {

	r_sky.image = R_LoadImage(va("env/%s", name), IT_CUBEMAP);
	if (r_sky.image == NULL) {

		Com_Warn("Failed to load sky env/%s\n", name);

		r_sky.image = R_LoadImage("env/edge/dragonheart_", IT_CUBEMAP);
		if (r_sky.image == NULL) {

			Com_Error(ERROR_DROP, "Failed to load default sky\n");
		}
	}
}

/**
 * @brief
 */
void R_Sky_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <basename>\n", Cmd_Argv(0));
		return;
	}

	R_LoadSky(Cmd_Argv(1));
}
