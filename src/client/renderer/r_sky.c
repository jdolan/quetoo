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
	vec2_t diffuse;
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

	GLint in_position;
	GLint in_diffuse;

	GLint projection;
	GLint model_view;

	GLint texture_diffuse;

	GLint brightness;
	GLint contrast;
	GLint saturation;
	GLint gamma;
} r_sky_program;

/**
 * @brief
 */
void R_DrawSkyBox(void) {

	if (r_clear->value && r_draw_wireframe->value) {
		return;
	}

	glEnable(GL_DEPTH_TEST);

	glUseProgram(r_sky_program.name);

	glUniformMatrix4fv(r_sky_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_sky_program.model_view, 1, GL_FALSE, (GLfloat *) r_locals.model_view.m);

	glUniform1f(r_sky_program.brightness, r_brightness->value);
	glUniform1f(r_sky_program.contrast, r_contrast->value);
	glUniform1f(r_sky_program.saturation, r_saturation->value);
	glUniform1f(r_sky_program.gamma, r_gamma->value);

	glBindVertexArray(r_sky.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_sky.vertex_buffer);

	glEnableVertexAttribArray(r_sky_program.in_position);
	glEnableVertexAttribArray(r_sky_program.in_diffuse);

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

	glDisable(GL_DEPTH_TEST);
}

/**
 * @brief
 */
static void R_InitSkyProgram(void) {

	memset(&r_sky_program, 0, sizeof(r_sky_program));

	r_sky_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "sky_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "color_filter.glsl", "sky_fs.glsl"),
			NULL);

	r_sky_program.in_position = glGetAttribLocation(r_sky_program.name, "in_position");
	r_sky_program.in_diffuse = glGetAttribLocation(r_sky_program.name, "in_diffuse");

	r_sky_program.projection = glGetUniformLocation(r_sky_program.name, "projection");
	r_sky_program.model_view = glGetUniformLocation(r_sky_program.name, "model_view");

	r_sky_program.texture_diffuse = glGetUniformLocation(r_sky_program.name, "texture_diffuse");

	r_sky_program.brightness = glGetUniformLocation(r_sky_program.name, "brightness");
	r_sky_program.contrast = glGetUniformLocation(r_sky_program.name, "contrast");
	r_sky_program.saturation = glGetUniformLocation(r_sky_program.name, "saturation");
	r_sky_program.gamma = glGetUniformLocation(r_sky_program.name, "gamma");

	glUniform1i(r_sky_program.texture_diffuse, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSky(void) {

	memset(&r_sky, 0, sizeof(r_sky));

	const r_sky_vertex_t vertexes[] = {
		// +z (top)
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },

		// -z (bottom)
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },

		// +x (right)
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },

		// -x (left)
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },

		// +y (back)
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { -SKY_DISTANCE, SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },

		// -y (front)
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MIN } },
		{ .position = { -SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MAX, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, -SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MAX } },
		{ .position = { SKY_DISTANCE, -SKY_DISTANCE, SKY_DISTANCE },
			.diffuse = { SKY_ST_MIN, SKY_ST_MIN } },
	};

	glGenVertexArrays(1, &r_sky.vertex_array);
	glBindVertexArray(r_sky.vertex_array);

	glGenBuffers(1, &r_sky.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_sky.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, lengthof(vertexes) * sizeof(r_sky_vertex_t), vertexes, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sky_vertex_t), (void *) offsetof(r_sky_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_sky_vertex_t), (void *) offsetof(r_sky_vertex_t, diffuse));

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
	uint32_t i;

	for (i = 0; i < lengthof(suf); i++) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "env/%s%s", name, suf[i]);
		r_sky.images[i] = R_LoadImage(path, IT_SKY);

		if (r_sky.images[i]->type == IT_NULL) { // try unit1_
			if (g_strcmp0(name, "unit1_")) {
				R_SetSky("unit1_");
				return;
			}
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
