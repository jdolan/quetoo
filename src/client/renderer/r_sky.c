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
typedef struct {
	vec3_t position;
	vec2_t diffusemap;
} r_sky_vertex_t;

/**
 * @brief
 */
static struct {
	r_image_t *images[6];

	GLuint vertex_buffer;
	GLuint vertex_array;
} r_sky;

/**
 * @brief The sky program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_diffusemap;

	GLint texture_diffusemap;
	GLint texture_lightgrid_fog;
} r_sky_program;

/**
 * @brief
 */
void R_DrawSky(void) {

	glEnable(GL_DEPTH_TEST);

	glUseProgram(r_sky_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_sky.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_sky.vertex_buffer);

	glEnableVertexAttribArray(r_sky_program.in_position);
	glEnableVertexAttribArray(r_sky_program.in_diffusemap);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
	glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[4]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[5]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 4, 4);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[0]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 8, 4);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[2]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[1]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 16, 4);

	glBindTexture(GL_TEXTURE_2D, r_sky.images[3]->texnum);
	glDrawArrays(GL_TRIANGLE_FAN, 20, 4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glDisable(GL_DEPTH_TEST);
}

/**
 * @brief
 */
static void R_InitSkyProgram(void) {

	memset(&r_sky_program, 0, sizeof(r_sky_program));

	r_sky_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "sky_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "sky_fs.glsl", NULL),
			NULL);

	glUseProgram(r_sky_program.name);

	r_sky_program.uniforms_block = glGetUniformBlockIndex(r_sky_program.name, "uniforms_block");
	glUniformBlockBinding(r_sky_program.name, r_sky_program.uniforms_block, 0);

	r_sky_program.in_position = glGetAttribLocation(r_sky_program.name, "in_position");
	r_sky_program.in_diffusemap = glGetAttribLocation(r_sky_program.name, "in_diffusemap");

	r_sky_program.texture_diffusemap = glGetUniformLocation(r_sky_program.name, "texture_diffusemap");
	r_sky_program.texture_lightgrid_fog = glGetUniformLocation(r_sky_program.name, "texture_lightgrid_fog");

	glUniform1i(r_sky_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);
	glUniform1i(r_sky_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSky(void) {

	memset(&r_sky, 0, sizeof(r_sky));

	const r_sky_vertex_t vertexes[] = {
		// +z (top)
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)
		},

		// -z (bottom)
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)
		},
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},

		// +x (right)
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)
		},

		// -x (left)
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)
		},

		// +y (back)
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(-SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)
		},

		// -y (front)
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MIN)
		},
		{
			.position = Vec3(-SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MAX, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MAX)
		},
		{
			.position = Vec3(SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE),
			.diffusemap = Vec2(SKY_ST_MIN, SKY_ST_MIN)

		},
	};

	glGenVertexArrays(1, &r_sky.vertex_array);
	glBindVertexArray(r_sky.vertex_array);

	glGenBuffers(1, &r_sky.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_sky.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, lengthof(vertexes) * sizeof(r_sky_vertex_t), vertexes, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sky_vertex_t), (void *) offsetof(r_sky_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_sky_vertex_t), (void *) offsetof(r_sky_vertex_t, diffusemap));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);

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

	glDeleteVertexArrays(1, &r_sky.vertex_array);

	glDeleteBuffers(1, &r_sky.vertex_buffer);

	R_ShutdownSkyProgram();
}

/**
 * @brief Sets the sky to the specified environment map.
 */
void R_SetSky(const char *name) {
	const char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

	r_image_t **out = r_sky.images;
	for (size_t i = 0; i < lengthof(suf); i++, out++) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "env/%s%s", name, suf[i]);
		*out = R_LoadImage(path, IT_MATERIAL);

		if (*out == NULL) {
			*out = R_LoadImage("textures/common/notex", IT_MATERIAL);
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

	R_SetSky(Cmd_Argv(1));
}
