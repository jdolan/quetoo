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

/**
 * @brief glDrawArrays commands.
 */
typedef struct {
	/**
	 * @brief The mode, e.g. GL_LINE_STRIP
	 */
	GLenum mode;

	/**
	 * @brief The first vertex.
	 */
	GLint first_vertex;

	/**
	 * @brief The number of vertexes.
	 */
	GLsizei num_vertexes;
} r_draw_3d_arrays_t;

#define MAX_DRAW_3D_ARRAYS 0x100000

/**
 * @brief 3D vertex struct.
 */
typedef struct {
	/**
	 * @brief The vertex position.
	 */
	vec3_t position;

	/**
	 * @brief The vertex color.
	 */
	color32_t color;
} r_draw_3d_vertex_t;

#define MAX_DRAW_3D_VERTEXES (MAX_DRAW_3D_ARRAYS * 2)

/**
 * @brief 3D draw struct.
 */
static struct {

	// accumulated draw arrays to draw for this frame
	r_draw_3d_arrays_t draw_arrays[MAX_DRAW_3D_ARRAYS];
	int32_t num_draw_arrays;

	// accumulated vertexes backing the draw arrays
	r_draw_3d_vertex_t vertexes[MAX_DRAW_3D_VERTEXES];
	int32_t num_vertexes;

	// the vertex array
	GLuint vertex_array;

	// the vertex buffer
	GLuint vertex_buffer;

} r_draw_3d;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_color;
} r_draw_3d_program;

/**
 * @brief
 */
static void R_AddDraw3DArrays(const r_draw_3d_arrays_t *draw) {

	if (r_draw_3d.num_draw_arrays == MAX_DRAW_3D_ARRAYS) {
		Com_Warn("MAX_DRAW_3D_ARRAYS\n");
		return;
	}

	if (draw->num_vertexes == 0) {
		return;
	}

	r_draw_3d.draw_arrays[r_draw_3d.num_draw_arrays] = *draw;
	r_draw_3d.num_draw_arrays++;
}

/**
 * @brief
 */
static void R_AddDraw3DVertex(const r_draw_3d_vertex_t *v) {

	if (r_draw_3d.num_vertexes == MAX_DRAW_3D_VERTEXES) {
		Com_Warn("MAX_DRAW_3D_VERTEXES\n");
		return;
	}

	r_draw_3d.vertexes[r_draw_3d.num_vertexes] = *v;
	r_draw_3d.num_vertexes++;
}

/**
 * @brief Draws line strips in 3D space.
 */
void R_Draw3DLines(const vec3_t *points, size_t count, const color_t color) {

	r_draw_3d_arrays_t draw = {
		.mode = GL_LINE_STRIP,
		.first_vertex = r_draw_3d.num_vertexes,
		.num_vertexes = (GLsizei) count
	};

	r_draw_3d_vertex_t *out = r_draw_3d.vertexes + r_draw_3d.num_vertexes;

	const vec3_t *in = points;
	for (size_t i = 0; i < count; i++, in++, out++) {
		R_AddDraw3DVertex(&(const r_draw_3d_vertex_t) {
			.position = *in,
			.color = Color_Color32(color)
		});
	}

	R_AddDraw3DArrays(&draw);
}

/**
 * @brief Draws the bounding box using line strips in 3D space.
 */
void R_Draw3DBox(const box3_t bounds, const color_t color, const bool solid) {
	vec3_t points[8];

	Box3_ToPoints(bounds, points);

	if (!solid) {
		R_Draw3DLines((const vec3_t []) {
			points[0],
			points[1],
			points[3],
			points[2],
			points[0],
		}, 5, color);

		R_Draw3DLines((const vec3_t []) {
			points[4],
			points[5],
			points[7],
			points[6],
			points[4],
		}, 5, color);

		R_Draw3DLines((const vec3_t []) {
			points[0],
			points[4],
		}, 2, color);

		R_Draw3DLines((const vec3_t []) {
			points[2],
			points[6],
		}, 2, color);

		R_Draw3DLines((const vec3_t []) {
			points[3],
			points[7],
		}, 2, color);

		R_Draw3DLines((const vec3_t []) {
			points[1],
			points[5],
		}, 2, color);
	} else {
		const GLuint elements[] = {
			// bottom
			0, 1, 3, 0, 3, 2,
			// top
			6, 7, 4, 7, 5, 4,
			// front
			4, 5, 0, 5, 1, 0,
			// back
			7, 6, 3, 6, 2, 3,
			// left
			6, 4, 2, 4, 0, 2,
			// right
			5, 7, 1, 7, 3, 1,
		};

		r_draw_3d_arrays_t draw = {
			.mode = GL_TRIANGLES,
			.first_vertex = r_draw_3d.num_vertexes,
			.num_vertexes = (GLsizei) lengthof(elements)
		};

		for (int32_t i = 0; i < draw.num_vertexes; i++) {
			R_AddDraw3DVertex(&(const r_draw_3d_vertex_t) {
				.position = points[elements[i]],
				.color = Color_Color32(color)
			});
		}

		R_AddDraw3DArrays(&draw);
	}
}

/**
 * @brief Draw all geometry accumulated for the current frame.
 */
void R_Draw3D(void) {

	if (r_draw_3d.num_draw_arrays == 0) {
		return;
	}

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_draw_3d_program.name);

	glBindVertexArray(r_draw_3d.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_draw_3d.vertex_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, r_draw_3d.num_vertexes * sizeof(r_draw_3d_vertex_t), r_draw_3d.vertexes);

	glEnableVertexAttribArray(r_draw_3d_program.in_position);
	glEnableVertexAttribArray(r_draw_3d_program.in_color);

	const r_draw_3d_arrays_t *draw = r_draw_3d.draw_arrays;
	for (int32_t i = 0; i < r_draw_3d.num_draw_arrays; i++, draw++) {
		glDrawArrays(draw->mode, draw->first_vertex, draw->num_vertexes);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	r_draw_3d.num_draw_arrays = 0;
	r_draw_3d.num_vertexes = 0;

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitDraw3DProgram(void) {

	memset(&r_draw_3d_program, 0, sizeof(r_draw_3d_program));

	r_draw_3d_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "draw_3d_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "draw_3d_fs.glsl", NULL),
			NULL);

	glUseProgram(r_draw_3d_program.name);

	r_draw_3d_program.uniforms_block = glGetUniformBlockIndex(r_draw_3d_program.name, "uniforms_block");
	glUniformBlockBinding(r_draw_3d_program.name, r_draw_3d_program.uniforms_block, 0);

	r_draw_3d_program.in_position = glGetAttribLocation(r_draw_3d_program.name, "in_position");
	r_draw_3d_program.in_color = glGetAttribLocation(r_draw_3d_program.name, "in_color");

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitDraw3D(void) {

	memset(&r_draw_3d, 0, sizeof(r_draw_3d));

	glGenVertexArrays(1, &r_draw_3d.vertex_array);
	glBindVertexArray(r_draw_3d.vertex_array);

	glGenBuffers(1, &r_draw_3d.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_draw_3d.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(r_draw_3d.vertexes), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_draw_3d_vertex_t), (void *) offsetof(r_draw_3d_vertex_t, position));
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_draw_3d_vertex_t), (void *) offsetof(r_draw_3d_vertex_t, color));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitDraw3DProgram();
}

/**
 * @brief
 */
static void R_ShutdownDraw3DProgram(void) {

	glDeleteProgram(r_draw_3d_program.name);

	r_draw_3d_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownDraw3D(void) {

	glDeleteVertexArrays(1, &r_draw_3d.vertex_array);
	glDeleteBuffers(1, &r_draw_3d.vertex_buffer);

	R_ShutdownDraw3DProgram();
}
