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

#define MAX_SHADER_DESCRIPTOR_FILENAMES 8

/**
 * @brief Shader descriptors are used to load GLSL shaders from disk.
 */
typedef struct {
	GLenum type;
	const char *filenames[MAX_SHADER_DESCRIPTOR_FILENAMES + 1];
} r_shader_descriptor_t;

/**
 * @brief Constructs a shader descriptor with the given type and filenames.
 */
#define MakeShaderDescriptor(_type, ...) \
	(r_shader_descriptor_t) { \
		.type = _type, \
		.filenames = { "version.glsl", "uniforms.glsl", __VA_ARGS__, NULL } \
	}

GLuint R_LoadShader(const r_shader_descriptor_t *desc);
GLuint R_LoadProgram(const r_shader_descriptor_t *desc, ...);
#endif /* __R_LOCAL_H__ */
