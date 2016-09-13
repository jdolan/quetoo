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

void (APIENTRY *qglActiveTexture)(GLenum texture);
void (APIENTRY *qglClientActiveTexture)(GLenum texture);

void (APIENTRY *qglGenBuffers)(GLuint count, GLuint *ids);
void (APIENTRY *qglDeleteBuffers)(GLuint count, GLuint *ids);
void (APIENTRY *qglBindBuffer)(GLenum target, GLuint id);
void (APIENTRY *qglBufferData)(GLenum target, GLsizei size, const GLvoid *data, GLenum usage);

void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
										GLsizei stride, const GLvoid *pointer);

GLuint (APIENTRY *qglCreateShader)(GLenum type);
void (APIENTRY *qglDeleteShader)(GLuint id);
void (APIENTRY *qglShaderSource)(GLuint id, GLuint count, GLchar **sources, GLuint *len);
void (APIENTRY *qglCompileShader)(GLuint id);
void (APIENTRY *qglGetShaderiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetShaderInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLuint (APIENTRY *qglCreateProgram)(void);
void (APIENTRY *qglDeleteProgram)(GLuint id);
void (APIENTRY *qglAttachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglDetachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglLinkProgram)(GLuint id);
void (APIENTRY *qglUseProgram)(GLuint id);
void (APIENTRY *qglGetProgramiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetProgramInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLint (APIENTRY *qglGetUniformLocation)(GLuint id, const GLchar *name);
void (APIENTRY *qglUniform1i)(GLint location, GLint i);
void (APIENTRY *qglUniform1f)(GLint location, GLfloat f);
void (APIENTRY *qglUniform3fv)(GLint location, int32_t count, GLfloat *f);
void (APIENTRY *qglUniform4fv)(GLint location, int32_t count, GLfloat *f);
void (APIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
									 const GLfloat *value);
GLint (APIENTRY *qglGetAttribLocation)(GLuint id, const GLchar *name);

/**
 * @brief
 */
void R_EnforceGlVersion(void) {
	const char *s = r_config.version_string;
	int32_t maj, min;

	sscanf(s, "%d.%d", &maj, &min);

	if (maj > 1)
		return;

	if (min > 2)
		return;

	Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.3\n", s);
}

/**
 * @brief
 */
void R_InitGlExtensions(void) {

	// multitexture
	if (strstr(r_config.extensions_string, "GL_ARB_multitexture")) {
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
	} else
		Com_Error(ERR_FATAL, "GL_ARB_multitexture not found\n");

	// vertex buffer objects
	if (strstr(r_config.extensions_string, "GL_ARB_vertex_buffer_object")) {
		qglGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
		qglDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
		qglBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
		qglBufferData = SDL_GL_GetProcAddress("glBufferData");
	} else
		Com_Warn("GL_ARB_vertex_buffer_object not found\n");

	// glsl vertex and fragment shaders and programs
	if (strstr(r_config.extensions_string, "GL_ARB_fragment_shader")) {
		qglCreateShader = SDL_GL_GetProcAddress("glCreateShader");
		qglDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
		qglShaderSource = SDL_GL_GetProcAddress("glShaderSource");
		qglCompileShader = SDL_GL_GetProcAddress("glCompileShader");
		qglGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
		qglCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
		qglDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
		qglAttachShader = SDL_GL_GetProcAddress("glAttachShader");
		qglDetachShader = SDL_GL_GetProcAddress("glDetachShader");
		qglLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
		qglUseProgram = SDL_GL_GetProcAddress("glUseProgram");
		qglGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
		qglGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
		qglUniform1i = SDL_GL_GetProcAddress("glUniform1i");
		qglUniform1f = SDL_GL_GetProcAddress("glUniform1f");
		qglUniform3fv = SDL_GL_GetProcAddress("glUniform3fv");
		qglUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");
		qglUniformMatrix4fv = SDL_GL_GetProcAddress("glUniformMatrix4fv");
		qglGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");

		// vertex attribute arrays
		qglEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
		qglVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
	} else
		Com_Warn("GL_ARB_fragment_shader not found\n");
}
