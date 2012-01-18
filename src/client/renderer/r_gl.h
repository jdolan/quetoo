/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __R_GL_H__
#define __R_GL_H__

#include "r_types.h"

#ifndef GLchar
# define GLchar char
#endif

#ifdef __R_LOCAL_H__

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_FRAGMENT_SHADER
# define GL_FRAGMENT_SHADER 0x8B30
# define GL_VERTEX_SHADER 0x8B31
# define GL_COMPILE_STATUS 0x8B81
# define GL_LINK_STATUS 0x8B82
#endif

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define  GL_MAX_TEXTURE_UNITS 0x84E2
#endif

// internally defined convenience constant
#define GL_TANGENT_ARRAY -1

// multitexture
extern void (APIENTRY *qglActiveTexture)(GLenum texture);
extern void (APIENTRY *qglClientActiveTexture)(GLenum texture);

// vertex buffer objects
extern void (APIENTRY *qglGenBuffers)(GLuint count, GLuint *id);
extern void (APIENTRY *qglDeleteBuffers)(GLuint count, GLuint *id);
extern void (APIENTRY *qglBindBuffer)(GLenum target, GLuint id);
extern void (APIENTRY *qglBufferData)(GLenum target, GLsizei size, const GLvoid *data, GLenum usage);

// vertex attribute arrays
extern void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
extern void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
extern void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type,
		GLboolean normalized, GLsizei stride, const GLvoid *pointer);

// glsl vertex and fragment shaders and programs
extern GLuint (APIENTRY *qglCreateShader)(GLenum type);
extern void (APIENTRY *qglDeleteShader)(GLuint id);
extern void (APIENTRY *qglShaderSource)(GLuint id, GLuint count, GLchar **sources, GLuint *len);
extern void (APIENTRY *qglCompileShader)(GLuint id);
extern void (APIENTRY *qglGetShaderiv)(GLuint id, GLenum field, GLuint *dest);
extern void (APIENTRY *qglGetShaderInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
extern GLuint (APIENTRY *qglCreateProgram)(void);
extern void (APIENTRY *qglDeleteProgram)(GLuint id);
extern void (APIENTRY *qglAttachShader)(GLuint prog, GLuint shader);
extern void (APIENTRY *qglDetachShader)(GLuint prog, GLuint shader);
extern void (APIENTRY *qglLinkProgram)(GLuint id);
extern void (APIENTRY *qglUseProgram)(GLuint id);
extern void (APIENTRY *qglGetProgramiv)(GLuint id, GLenum field, GLuint *dest);
extern void (APIENTRY *qglGetProgramInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
extern GLint (APIENTRY *qglGetUniformLocation)(GLuint id, const GLchar *name);
extern void (APIENTRY *qglUniform1i)(GLint location, GLint i);
extern void (APIENTRY *qglUniform1f)(GLint location, GLfloat f);
extern void (APIENTRY *qglUniform3fv)(GLint location, int count, GLfloat *f);
extern void (APIENTRY *qglUniform4fv)(GLint location, int count, GLfloat *f);
extern GLint (APIENTRY *qglGetAttribLocation)(GLuint id, const GLchar *name);

void R_EnforceGlVersion(void);
boolean_t R_InitGlExtensions(void);

#endif /* __R_LOCAL_H__ */

#endif /* __R_GL_H__ */
