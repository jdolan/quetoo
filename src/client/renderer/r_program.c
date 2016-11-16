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
void R_UseProgram(const r_program_t *prog) {

	if (r_state.active_program == prog) {
		return;
	}

	r_state.active_program = prog;

	if (prog) {
		glUseProgram(prog->id);

		if (prog->Use) { // invoke use function
			prog->Use();
		}

		// FIXME: required?
		r_state.array_buffers_dirty |= prog->arrays_mask;
	} else {
		glUseProgram(0);
	}

	r_view.num_state_changes[R_STATE_PROGRAM]++;

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ProgramVariable(r_variable_t *variable, const GLenum type, const char *name) {

	memset(variable, 0, sizeof(*variable));
	variable->location = -1;

	if (!r_state.active_program) {
		Com_Warn("No program currently bound\n");
		return;
	}

	switch (type) {
		case R_ATTRIBUTE:
			variable->location = glGetAttribLocation(r_state.active_program->id, name);
			break;
		default:
			memset(&variable->value, 0xff, sizeof(variable->value));
			variable->location = glGetUniformLocation(r_state.active_program->id, name);
			break;
	}

	if (variable->location == -1) {
		Com_Warn("Failed to resolve variable %s in program %s\n", name,
		         r_state.active_program->name);
		return;
	}

	variable->type = type;
	g_strlcpy(variable->name, name, sizeof(variable->name));
}

/**
 * @brief
 */
void R_ProgramParameter1i(r_uniform1i_t *variable, const GLint value) {

	assert(variable && variable->location != -1);

	if (variable->value.i == value) {
		return;
	}

	variable->value.i = value;
	glUniform1i(variable->location, variable->value.i);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter1f(r_uniform1f_t *variable, const GLfloat value) {

	assert(variable && variable->location != -1);

	if (variable->value.f == value) {
		return;
	}

	variable->value.f = value;
	glUniform1f(variable->location, variable->value.f);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter3fv(r_uniform3fv_t *variable, const GLfloat *value) {

	assert(variable && variable->location != -1);

	if (VectorCompare(variable->value.vec3, value)) {
		return;
	}

	VectorCopy(value, variable->value.vec3);
	glUniform3fv(variable->location, 1, variable->value.vec3);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter4fv(r_uniform4fv_t *variable, const GLfloat *value) {

	assert(variable && variable->location != -1);

	if (Vector4Compare(variable->value.vec4, value)) {
		return;
	}

	Vector4Copy(value, variable->value.vec4);
	glUniform4fv(variable->location, 1, variable->value.vec4);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter4ubv(r_uniform4fv_t *variable, const GLubyte *value) {

	assert(variable && variable->location != -1);

	if (Vector4Compare(variable->value.u8vec4, value)) {
		return;
	}

	Vector4Copy(value, variable->value.u8vec4);

	const vec4_t floats = { value[0] / 255.0, value[1] / 255.0, value[2] / 255.0, value[3] / 255.0 };

	glUniform4fv(variable->location, 1, floats);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
}

/**
 * @brief
 */
_Bool R_ProgramParameterMatrix4fv(r_uniform_matrix4fv_t *variable, const GLfloat *value) {

	assert(variable && variable->location != -1);

	if (memcmp(&variable->value.mat4, value, sizeof(variable->value.mat4)) == 0) {
		return false;
	}

	memcpy(&variable->value.mat4, value, sizeof(variable->value.mat4));
	glUniformMatrix4fv(variable->location, 1, false, (GLfloat *) variable->value.mat4.m);
	r_view.num_state_changes[R_STATE_PROGRAM_PARAMETER]++;

	R_GetError(variable->name);
	return true;
}

/**
 * @brief
 */
void R_BindAttributeLocation(const r_program_t *prog, const char *name, const GLuint location) {

	glBindAttribLocation(prog->id, location, name);

	R_GetError(name);
}

GLenum r_attrib_type_to_gl_type[R_ATTRIB_TOTAL_TYPES] = {
	GL_FLOAT,
	GL_BYTE,
	GL_UNSIGNED_BYTE,
	GL_SHORT,
	GL_UNSIGNED_SHORT,
	GL_INT,
	GL_UNSIGNED_INT
};

/**
 * @brief
 */
static void R_AttributePointer(const r_attribute_id_t attribute) {

	const r_attribute_mask_t mask = 1 << attribute;

	if (!(r_state.array_buffers_dirty & mask)) {
		return;
	}

	r_state.array_buffers_dirty &= ~mask;

	const r_buffer_t *buffer = r_state.array_buffers[attribute];

	if (!buffer) {
		R_DisableAttribute(attribute);
		return;
	}

	R_EnableAttribute(attribute);

	GLsizei stride = 0;
	GLsizeiptr offset = r_state.array_buffer_offsets[attribute];
	const r_attrib_type_state_t *type = &buffer->element_type;

	if (buffer->interleave) {
		r_attribute_id_t real_attrib = attribute;

		// check to see if we need to point to the right attrib
		// for NEXT_*
		if (buffer->interleave_attribs[attribute] == NULL) {

			switch (attribute) {
				case R_ARRAY_NEXT_POSITION:
					real_attrib = R_ARRAY_POSITION;
					break;
				case R_ARRAY_NEXT_NORMAL:
					real_attrib = R_ARRAY_NORMAL;
					break;
				case R_ARRAY_NEXT_TANGENT:
					real_attrib = R_ARRAY_TANGENT;
					break;
				default:
					break;
			}
		}

		assert(buffer->interleave_attribs[real_attrib]);

		type = &buffer->interleave_attribs[real_attrib]->_type_state;
		offset += type->offset;
		stride = buffer->element_type.stride;
	}

	r_attrib_state_t *attrib = &r_state.attributes[attribute];

	// only set the ptr if it hasn't changed.
	if (attrib->type != type ||
	        attrib->value.buffer != buffer ||
	        attrib->offset != offset) {

		R_BindBuffer(buffer);

		glVertexAttribPointer(attribute, type->count, r_attrib_type_to_gl_type[type->type], type->normalized, stride,
		                      (const GLvoid *) offset);
		r_view.num_state_changes[R_STATE_PROGRAM_ATTRIB_POINTER]++;

		attrib->value.buffer = buffer;
		attrib->type = type;
		attrib->offset = offset;

		R_GetError(r_state.active_program->attributes[attribute].name);
	}
}

/**
 * @brief
 */
void R_AttributeConstant4fv(const r_attribute_id_t attribute, const GLfloat *value) {

	R_DisableAttribute(attribute);

	if (r_state.attributes[attribute].type == NULL && Vector4Compare(r_state.attributes[attribute].value.vec4, value)) {
		return;
	}

	Vector4Copy(value, r_state.attributes[attribute].value.vec4);
	glVertexAttrib4fv(attribute, r_state.attributes[attribute].value.vec4);
	r_view.num_state_changes[R_STATE_PROGRAM_ATTRIB_CONSTANT]++;

	r_state.attributes[attribute].type = NULL;

	R_GetError(r_state.active_program->attributes[attribute].name);
}

/**
 * @brief
 */
void R_AttributeConstant4ubv(const r_attribute_id_t attribute, const GLubyte *value) {

	R_DisableAttribute(attribute);

	if (r_state.attributes[attribute].type == NULL &&
	        Vector4Compare(r_state.attributes[attribute].value.u8vec4, value)) {
		return;
	}

	Vector4Copy(value, r_state.attributes[attribute].value.u8vec4);
	glVertexAttrib4ubv(attribute, r_state.attributes[attribute].value.u8vec4);
	r_view.num_state_changes[R_STATE_PROGRAM_ATTRIB_CONSTANT]++;

	r_state.attributes[attribute].type = NULL;

	R_GetError(r_state.active_program->attributes[attribute].name);
}

/**
 * @brief
 */
void R_EnableAttribute(const r_attribute_id_t attribute) {

	assert(attribute < R_ARRAY_MAX_ATTRIBS && r_state.active_program->attributes[attribute].location != -1);

	if (r_state.attributes[attribute].enabled != true) {

		glEnableVertexAttribArray(attribute);
		r_state.attributes[attribute].enabled = true;

		r_view.num_state_changes[R_STATE_PROGRAM_ATTRIB_TOGGLE]++;

		R_GetError(r_state.active_program->attributes[attribute].name);
	}
}

/**
 * @brief
 */
void R_DisableAttribute(const r_attribute_id_t attribute) {

	assert(attribute < R_ARRAY_MAX_ATTRIBS && r_state.active_program->attributes[attribute].location != -1);

	if (r_state.attributes[attribute].enabled != false) {

		glDisableVertexAttribArray(attribute);
		r_state.attributes[attribute].enabled = false;

		r_view.num_state_changes[R_STATE_PROGRAM_ATTRIB_TOGGLE]++;

		R_GetError(r_state.active_program->attributes[attribute].name);
	}
}

/**
 * @brief
 */
static void R_ShutdownShader(r_shader_t *sh) {

	glDeleteShader(sh->id);
	memset(sh, 0, sizeof(r_shader_t));
}

/**
 * @brief
 */
static void R_ShutdownProgram(r_program_t *prog) {

	if (prog->Shutdown) {
		prog->Shutdown();
	}

	if (prog->v) {
		R_ShutdownShader(prog->v);
	}
	if (prog->f) {
		R_ShutdownShader(prog->f);
	}

	glDeleteProgram(prog->id);

	R_GetError(prog->name);

	memset(prog, 0, sizeof(r_program_t));
}

/**
 * @brief
 */
void R_ShutdownPrograms(void) {

	R_UseProgram(NULL);

	for (int32_t i = 0; i < MAX_PROGRAMS; i++) {

		if (!r_state.programs[i].id) {
			continue;
		}

		R_ShutdownProgram(&r_state.programs[i]);
	}
}

static gchar *R_PreprocessShader(const char *input, const uint32_t length);

/**
 * @brief
 */
static gboolean R_PreprocessShader_eval(const GMatchInfo *match_info, GString *result,
                                        gpointer data __attribute__((unused))) {
	gchar *name = g_match_info_fetch(match_info, 1);
	gchar path[MAX_OS_PATH];
	int64_t len;
	void *buf;

	g_snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = Fs_Load(path, &buf)) == -1) {
		Com_Warn("Failed to load %s\n", name);
		g_free(name);
		return true;
	}

	gchar *processed = R_PreprocessShader((const char *) buf, (uint32_t) len);
	g_string_append(result, processed);
	g_free(processed);

	Fs_Free(buf);
	g_free(name);

	return false;
}

static GRegex *shader_preprocess_regex = NULL;

/**
 * @brief
 */
static gchar *R_PreprocessShader(const char *input, const uint32_t length) {
	GString *emplaced = NULL;
	_Bool had_replacements = Cvar_ExpandString(input, length, &emplaced);

	if (!had_replacements) {
		emplaced = g_string_new_len(input, length);
	}

	GError *error = NULL;
	gchar *output = g_regex_replace_eval(shader_preprocess_regex, emplaced->str, emplaced->len, 0, 0,
	                                     R_PreprocessShader_eval, NULL, &error);

	if (error) {
		Com_Warn("Error preprocessing shader: %s", error->message);
	}

	return output;
}

/**
 * @brief
 */
static r_shader_t *R_LoadShader(GLenum type, const char *name) {
	r_shader_t *sh;
	char path[MAX_QPATH], log[MAX_STRING_CHARS];
	void *buf;
	int32_t e, i;
	int64_t len;

	g_snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = Fs_Load(path, &buf)) == -1) {
		Com_Warn("Failed to load %s\n", name);
		return NULL;
	}

	for (i = 0; i < MAX_SHADERS; i++) {
		sh = &r_state.shaders[i];

		if (!sh->id) {
			break;
		}
	}

	if (i == MAX_SHADERS) {
		Com_Warn("MAX_SHADERS reached\n");
		Fs_Free(buf);
		return NULL;
	}

	g_strlcpy(sh->name, name, sizeof(sh->name));

	sh->type = type;

	sh->id = glCreateShader(sh->type);
	if (!sh->id) {
		Fs_Free(buf);
		return NULL;
	}

	// run shader source through cvar parser
	gchar *parsed = R_PreprocessShader((const char *) buf, (uint32_t) len);
	const GLchar *src[] = { parsed };
	GLint length = (GLint) strlen(parsed);

	// upload the shader source
	glShaderSource(sh->id, 1, src, &length);

	g_free(parsed);

	// compile it and check for errors
	glCompileShader(sh->id);

	glGetShaderiv(sh->id, GL_COMPILE_STATUS, &e);
	if (!e) {
		glGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
		Com_Warn("%s: %s\n", sh->name, log);

		glDeleteShader(sh->id);
		memset(sh, 0, sizeof(*sh));

		Fs_Free(buf);
		return NULL;
	}

	Fs_Free(buf);
	return sh;
}

/**
 * @brief
 */
static r_program_t *R_LoadProgram(const char *name, void (*Init)(r_program_t *program),
                                  void (*PreLink)(const r_program_t *program)) {

	r_program_t *prog;
	char log[MAX_STRING_CHARS];
	int32_t i, e;

	for (i = 0; i < MAX_PROGRAMS; i++) {
		prog = &r_state.programs[i];

		if (!prog->id) {
			break;
		}
	}

	if (i == MAX_PROGRAMS) {
		Com_Warn("MAX_PROGRAMS reached\n");
		return NULL;
	}

	g_strlcpy(prog->name, name, sizeof(prog->name));

	memset(prog->attributes, -1, sizeof(prog->attributes));

	prog->id = glCreateProgram();

	prog->v = R_LoadShader(GL_VERTEX_SHADER, va("%s_vs.glsl", name));
	prog->f = R_LoadShader(GL_FRAGMENT_SHADER, va("%s_fs.glsl", name));

	if (prog->v) {
		glAttachShader(prog->id, prog->v->id);
	}
	if (prog->f) {
		glAttachShader(prog->id, prog->f->id);
	}

	if (PreLink) {
		PreLink(prog);
	}

	glLinkProgram(prog->id);

	glGetProgramiv(prog->id, GL_LINK_STATUS, &e);
	if (!e) {
		glGetProgramInfoLog(prog->id, sizeof(log) - 1, NULL, log);
		Com_Warn("%s: %s\n", prog->name, log);

		R_ShutdownProgram(prog);
		return NULL;
	}

	if (Init) { // invoke initialization function
		R_UseProgram(prog);

		Init(prog);

		R_UseProgram(NULL);
	}

	R_GetError(prog->name);

	return prog;
}

/**
 * @brief Sets up attributes for the currently bound program based on the active
 * array mask.
 */
void R_SetupAttributes(void) {

	const r_program_t *p = (r_program_t *) r_state.active_program;
	r_attribute_mask_t mask = R_ArraysMask();

	if (p->arrays_mask & R_ARRAY_MASK_POSITION) {

		if (mask & R_ARRAY_MASK_POSITION) {

			R_AttributePointer(R_ARRAY_POSITION);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_POSITION) {

				if ((mask & R_ARRAY_MASK_NEXT_POSITION) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_POSITION])) {
					R_AttributePointer(R_ARRAY_NEXT_POSITION);
				} else {
					R_DisableAttribute(R_ARRAY_NEXT_POSITION);
				}
			}
		} else {

			R_DisableAttribute(R_ARRAY_POSITION);
			R_DisableAttribute(R_ARRAY_NEXT_POSITION);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_COLOR) {

		if (mask & R_ARRAY_MASK_COLOR) {
			R_AttributePointer(R_ARRAY_COLOR);
		} else {
			R_AttributeConstant4fv(R_ARRAY_COLOR, r_state.current_color);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_DIFFUSE) {

		if (mask & R_ARRAY_MASK_DIFFUSE) {
			R_AttributePointer(R_ARRAY_DIFFUSE);
		} else {
			R_DisableAttribute(R_ARRAY_DIFFUSE);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_LIGHTMAP) {

		if (mask & R_ARRAY_MASK_LIGHTMAP) {
			R_AttributePointer(R_ARRAY_LIGHTMAP);
		} else {
			R_DisableAttribute(R_ARRAY_LIGHTMAP);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_NORMAL) {

		if (mask & R_ARRAY_MASK_NORMAL) {

			R_AttributePointer(R_ARRAY_NORMAL);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_NORMAL) {

				if ((mask & R_ARRAY_MASK_NEXT_NORMAL) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_NORMAL])) {
					R_AttributePointer(R_ARRAY_NEXT_NORMAL);
				} else {
					R_DisableAttribute(R_ARRAY_NEXT_NORMAL);
				}
			}
		} else {

			R_DisableAttribute(R_ARRAY_NORMAL);
			R_DisableAttribute(R_ARRAY_NEXT_NORMAL);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_TANGENT) {

		if (mask & R_ARRAY_MASK_TANGENT) {

			R_AttributePointer(R_ARRAY_TANGENT);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_TANGENT) {

				if ((mask & R_ARRAY_MASK_NEXT_TANGENT) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_TANGENT])) {
					R_AttributePointer(R_ARRAY_NEXT_TANGENT);
				} else {
					R_DisableAttribute(R_ARRAY_NEXT_TANGENT);
				}
			}
		} else {

			R_DisableAttribute(R_ARRAY_TANGENT);
			R_DisableAttribute(R_ARRAY_NEXT_TANGENT);
		}
	}
}

/**
 * @brief
 */
void R_InitPrograms(void) {

	// this only needs to be done once
	if (!shader_preprocess_regex) {
		GError *error = NULL;
		shader_preprocess_regex = g_regex_new("#include [\"\']([a-z0-9_]+\\.glsl)[\"\']",
		                                      G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, &error);

		if (error) {
			Com_Warn("Error compiling regex: %s", error->message);
		}
	}

	memset(r_state.shaders, 0, sizeof(r_state.shaders));
	memset(r_state.programs, 0, sizeof(r_state.programs));

	if ((r_state.default_program = R_LoadProgram("default", R_InitProgram_default, R_PreLink_default))) {
		r_state.default_program->Shutdown = R_Shutdown_default;
		r_state.default_program->Use = R_UseProgram_default;
		r_state.default_program->UseMaterial = R_UseMaterial_default;
		r_state.default_program->UseFog = R_UseFog_default;
		r_state.default_program->UseLight = R_UseLight_default;
		r_state.default_program->UseMatrices = R_UseMatrices_default;
		r_state.default_program->UseAlphaTest = R_UseAlphaTest_default;
		r_state.default_program->UseInterpolation = R_UseInterpolation_default;
		r_state.default_program->arrays_mask = R_ARRAY_MASK_ALL;
	}

	if ((r_state.shadow_program = R_LoadProgram("shadow", R_InitProgram_shadow, R_PreLink_shadow))) {
		r_state.shadow_program->UseFog = R_UseFog_shadow;
		r_state.shadow_program->UseMatrices = R_UseMatrices_shadow;
		r_state.shadow_program->UseCurrentColor = R_UseCurrentColor_shadow;
		r_state.shadow_program->UseInterpolation = R_UseInterpolation_shadow;
		r_state.shadow_program->arrays_mask = R_ARRAY_MASK_POSITION | R_ARRAY_MASK_NEXT_POSITION;
	}

	if ((r_state.shell_program = R_LoadProgram("shell", R_InitProgram_shell, R_PreLink_shell))) {
		r_state.shell_program->Use = R_UseProgram_shell;
		r_state.shell_program->UseMatrices = R_UseMatrices_shell;
		r_state.shell_program->UseCurrentColor = R_UseCurrentColor_shell;
		r_state.shell_program->UseInterpolation = R_UseInterpolation_shell;
		r_state.shell_program->arrays_mask = R_ARRAY_MASK_POSITION | R_ARRAY_MASK_NEXT_POSITION | R_ARRAY_MASK_DIFFUSE |
		                                     R_ARRAY_MASK_NORMAL | R_ARRAY_MASK_NEXT_NORMAL;
	}

	if ((r_state.warp_program = R_LoadProgram("warp", R_InitProgram_warp, R_PreLink_warp))) {
		r_state.warp_program->Use = R_UseProgram_warp;
		r_state.warp_program->UseFog = R_UseFog_warp;
		r_state.warp_program->UseMatrices = R_UseMatrices_warp;
		r_state.warp_program->UseCurrentColor = R_UseCurrentColor_warp;
		r_state.warp_program->arrays_mask = R_ARRAY_MASK_POSITION | R_ARRAY_MASK_DIFFUSE;
	}

	if ((r_state.null_program = R_LoadProgram("null", R_InitProgram_null, R_PreLink_null))) {
		r_state.null_program->UseFog = R_UseFog_null;
		r_state.null_program->UseMatrices = R_UseMatrices_null;
		r_state.null_program->UseCurrentColor = R_UseCurrentColor_null;
		r_state.null_program->UseInterpolation = R_UseInterpolation_null;
		r_state.null_program->arrays_mask = R_ARRAY_MASK_POSITION | R_ARRAY_MASK_NEXT_POSITION | R_ARRAY_MASK_DIFFUSE |
		                                    R_ARRAY_MASK_COLOR;
	}

	if ((r_state.corona_program = R_LoadProgram("corona", R_InitProgram_corona, R_PreLink_corona))) {
		r_state.corona_program->UseFog = R_UseFog_corona;
		r_state.corona_program->UseMatrices = R_UseMatrices_corona;
		r_state.corona_program->arrays_mask = R_ARRAY_MASK_POSITION | R_ARRAY_MASK_DIFFUSE | R_ARRAY_MASK_COLOR;
	}

	R_UseProgram(r_state.null_program);
}

