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
 * @brief
 */
GLuint R_LoadShader(const r_shader_descriptor_t *desc) {

	GLuint shader = glCreateShader(desc->type);
	if (shader) {

		void *source[lengthof(desc->filenames) + 1] = {
			"#version 330",
			NULL
		};

		GLsizei count = 0;
		while (count < (GLsizei) lengthof(desc->filenames)) {

			const char *filename = desc->filenames[count];
			if (filename) {
				const ssize_t length = Fs_Load(va("shaders/%s", filename), &source[count + 1]);
				if (length == -1) {
					Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
				}
				count++;
			} else {
				break;
			}
		}
		
		glShaderSource(shader, count + 1, (const GLchar **) source, NULL);

		while (count--) {
			Fs_Free(source[count + 1]);
			source[count + 1] = NULL;
		}
		
		glCompileShader(shader);

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

		if (status == GL_FALSE) {
			GLint log_length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

			GLchar log[log_length];
			glGetShaderInfoLog(shader, log_length, NULL, log);

			Com_Error(ERROR_FATAL, "%s\n", log);
		}
	}

	return shader;
}

/**
 * @brief
 */
GLuint R_LoadProgram(const r_shader_descriptor_t *desc, ...) {

	assert(desc);

	GLuint program = glCreateProgram();
	if (program) {

		va_list args;
		va_start(args, desc);

		GLuint shaders[MAX_SHADER_DESCRIPTOR_FILENAMES];
		int32_t i = 0;

		while (desc) {

			shaders[i] = R_LoadShader(desc);
			if (shaders[i] == 0) {
				glDeleteProgram(program);
				program = 0;
				break;
			}

			glAttachShader(program, shaders[i++]);

			desc = va_arg(args, const r_shader_descriptor_t *);
		}

		va_end(args);

		glLinkProgram(program);

		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);

		if (status == GL_FALSE) {
			GLint log_length;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

			GLchar log[log_length];
			glGetProgramInfoLog(program, log_length, NULL, log);

			Com_Error(ERROR_FATAL, "%s\n", log);
		} else {
			while (--i > 0) {
				glDetachShader(program, shaders[i]);
				glDeleteShader(shaders[i]);
			}
		}
	}

	R_GetError(NULL);

	return program;
}
