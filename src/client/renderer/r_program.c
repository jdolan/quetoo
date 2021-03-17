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
r_shader_descriptor_t *R_ShaderDescriptor(GLenum type, ...) {

	r_shader_descriptor_t *desc = Mem_TagMalloc(sizeof(*desc), MEM_TAG_RENDERER);

	desc->type = type;

	size_t i = 0;

	desc->filenames[i++] = "version.glsl";
	desc->filenames[i++] = "uniforms.glsl";
	desc->filenames[i++] = "common.glsl";

	switch (type) {
		case GL_VERTEX_SHADER:
			desc->filenames[i++] = "common_vs.glsl";
			break;
		case GL_FRAGMENT_SHADER:
			desc->filenames[i++] = "common_fs.glsl";
			break;
		default:
			break;
	}

	va_list args;
	va_start(args, type);

	while (i < lengthof(desc->filenames)) {

		const char *source = va_arg(args, const char *);
		if (source) {
			desc->filenames[i++] = source;
		} else {
			break;
		}
	}

	va_end(args);
	return desc;
}

/**
 * @brief
 */
GLuint R_LoadShader(const r_shader_descriptor_t *desc) {

	GLuint shader = glCreateShader(desc->type);
	if (shader) {

		void *source[lengthof(desc->filenames)];

		GLsizei count = 0;
		while (count < (GLsizei) lengthof(desc->filenames)) {
			const char *filename = desc->filenames[count];
			if (filename) {
				const int64_t length = Fs_Load(va("shaders/%s", filename), &source[count]);
				if (length == -1) {
					Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
				}
				count++;
			} else {
				break;
			}
		}

		glShaderSource(shader, count, (const GLchar **) source, NULL);

		while (count--) {
			Fs_Free(source[count]);
		}
		
		glCompileShader(shader);

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

		if (status == GL_FALSE) {
			GLint log_length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

			GLchar log[log_length];
			log[0] = 0;
			glGetShaderInfoLog(shader, log_length, NULL, log);

			GLchar source_list[MAX_PRINT_MSG] = { 0 };

			for (GLsizei i = 0; i < (GLsizei) lengthof(desc->filenames); i++) {
				const char *filename = desc->filenames[i];

				if (!filename) {
					break;
				} else if (source_list[0] != 0) {
					g_strlcat(source_list, ", ", sizeof(source_list));
				}
				
				g_strlcat(source_list, filename, sizeof(source_list));
			}

			GLint src_length;
			glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &src_length);

			GLchar src[src_length];
			src[0] = 0;
			glGetShaderSource(shader, src_length, NULL, src);

			Com_LogString("Shader source:\n");
			Com_LogString(src);

			Com_Error(ERROR_FATAL, "Sources: %s\n%s\n", source_list, log);
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
