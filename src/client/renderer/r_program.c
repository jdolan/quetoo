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
	} else {
		glUseProgram(0);
	}

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

	if (!variable || variable->location == -1) {
		Com_Warn("NULL or invalid variable\n");
		return;
	}

	if (variable->value.i == value) {
		return;
	}

	variable->value.i = value;
	glUniform1i(variable->location, variable->value.i);

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter1f(r_uniform1f_t *variable, const GLfloat value) {

	if (!variable || variable->location == -1) {
		Com_Warn("NULL or invalid variable\n");
		return;
	}

	if (variable->value.f == value) {
		return;
	}

	variable->value.f = value;
	glUniform1f(variable->location, variable->value.f);

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter3fv(r_uniform3fv_t *variable, const GLfloat *value) {

	if (!variable || variable->location == -1) {
		Com_Warn("NULL or invalid variable\n");
		return;
	}

	if (VectorCompare(variable->value.vec3, value)) {
		return;
	}

	VectorCopy(value, variable->value.vec3);
	glUniform3fv(variable->location, 1, variable->value.vec3);

	R_GetError(variable->name);
}

/**
 * @brief
 */
void R_ProgramParameter4fv(r_uniform4fv_t *variable, const GLfloat *value) {

	if (!variable || variable->location == -1) {
		Com_Warn("NULL or invalid variable\n");
		return;
	}

	if (Vector4Compare(variable->value.vec4, value)) {
		return;
	}

	Vector4Copy(value, variable->value.vec4);
	glUniform4fv(variable->location, 1, variable->value.vec4);

	R_GetError(variable->name);
}

/**
 * @brief
 */
_Bool R_ProgramParameterMatrix4fv(r_uniform_matrix4fv_t *variable, const GLfloat *value) {

	if (!variable || variable->location == -1) {
		Com_Warn("NULL or invalid variable\n");
		return false;
	}

	if (memcmp(&variable->value.mat4, value, sizeof(variable->value.mat4)) == 0) {
		return false;
	}

	memcpy(&variable->value.mat4, value, sizeof(variable->value.mat4));
	glUniformMatrix4fv(variable->location, 1, false, (GLfloat *) variable->value.mat4.m);

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

/**
 * @brief
 */
void R_AttributePointer(const r_attribute_id_t attribute, GLuint size, const r_buffer_t *buffer, const GLvoid *offset) {

	R_EnableAttribute(attribute);

	// only set the ptr if it hasn't changed.
	if (r_state.attributes[attribute].constant == true ||
	        r_state.attributes[attribute].value.buffer != buffer ||
	        r_state.attributes[attribute].size != size ||
	        r_state.attributes[attribute].offset != offset) {

		R_BindBuffer(buffer);

		glVertexAttribPointer(attribute, size, GL_FLOAT, GL_FALSE, 0, offset);

		r_state.attributes[attribute].value.buffer = buffer;
		r_state.attributes[attribute].size = size;
		r_state.attributes[attribute].offset = offset;
		r_state.attributes[attribute].constant = false;

		R_GetError(r_state.active_program->attributes[attribute].name);
	}
}

/**
 * @brief
 */
void R_AttributeConstant4fv(const r_attribute_id_t attribute, const GLfloat *value) {

	R_DisableAttribute(attribute);

	if (r_state.attributes[attribute].constant == true && Vector4Compare(r_state.attributes[attribute].value.vec4, value)) {
		return;
	}

	Vector4Copy(value, r_state.attributes[attribute].value.vec4);
	glVertexAttrib4fv(attribute, r_state.attributes[attribute].value.vec4);

	r_state.attributes[attribute].constant = true;

	R_GetError(r_state.active_program->attributes[attribute].name);
}

/**
 * @brief
 */
void R_EnableAttribute(const r_attribute_id_t attribute) {

	if (attribute >= R_ARRAY_MAX_ATTRIBS ||
	        r_state.active_program->attributes[attribute].location == -1) {
		Com_Warn("Invalid attribute\n");
		return;
	}

	if (r_state.attributes[attribute].enabled != true) {
		glEnableVertexAttribArray(attribute);
		r_state.attributes[attribute].enabled = true;

		R_GetError(r_state.active_program->attributes[attribute].name);
	}
}

/**
 * @brief
 */
void R_DisableAttribute(const r_attribute_id_t attribute) {

	if (attribute >= R_ARRAY_MAX_ATTRIBS ||
	        r_state.active_program->attributes[attribute].location == -1) {
		Com_Warn("Invalid attribute\n");
		return;
	}

	if (r_state.attributes[attribute].enabled != false) {
		glDisableVertexAttribArray(attribute);
		r_state.attributes[attribute].enabled = false;

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
                                        gpointer data __attribute((unused))) {
	const gchar *name = g_match_info_fetch(match_info, 1);
	gchar path[MAX_OS_PATH];
	int64_t len;
	void *buf;

	g_snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = Fs_Load(path, &buf)) == -1) {
		Com_Warn("Failed to load %s\n", name);
		return true;
	}

	gchar *processed = R_PreprocessShader((const char *) buf, (uint32_t) len);
	g_string_append(result, processed);
	g_free(processed);

	Fs_Free(buf);

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
	int32_t mask = R_ArraysMask();

	if (p->arrays_mask & R_ARRAY_MASK_VERTEX) {

		if (mask & R_ARRAY_MASK_VERTEX) {

			R_AttributePointer(R_ARRAY_VERTEX, 3, r_state.array_buffers[R_ARRAY_VERTEX], NULL);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_VERTEX) {

				if ((mask & R_ARRAY_MASK_NEXT_VERTEX) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_VERTEX])) {
					R_AttributePointer(R_ARRAY_NEXT_VERTEX, 3, r_state.array_buffers[R_ARRAY_NEXT_VERTEX], NULL);
				} else {
					R_DisableAttribute(R_ARRAY_NEXT_VERTEX);
				}
			}
		} else {

			R_DisableAttribute(R_ARRAY_VERTEX);
			R_DisableAttribute(R_ARRAY_NEXT_VERTEX);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_COLOR) {

		if (mask & R_ARRAY_MASK_COLOR) {
			R_AttributePointer(R_ARRAY_COLOR, 4, r_state.array_buffers[R_ARRAY_COLOR], NULL);
		} else {
			R_AttributeConstant4fv(R_ARRAY_COLOR, r_state.current_color);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_TEX_DIFFUSE) {

		if (mask & R_ARRAY_MASK_TEX_DIFFUSE) {
			R_AttributePointer(R_ARRAY_TEX_DIFFUSE, 2, r_state.array_buffers[R_ARRAY_TEX_DIFFUSE], NULL);
		} else {
			R_DisableAttribute(R_ARRAY_TEX_DIFFUSE);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_TEX_LIGHTMAP) {

		if (mask & R_ARRAY_MASK_TEX_LIGHTMAP) {
			R_AttributePointer(R_ARRAY_TEX_LIGHTMAP, 2, r_state.array_buffers[R_ARRAY_TEX_LIGHTMAP], NULL);
		} else {
			R_DisableAttribute(R_ARRAY_TEX_LIGHTMAP);
		}
	}

	if (p->arrays_mask & R_ARRAY_MASK_NORMAL) {

		if (mask & R_ARRAY_MASK_NORMAL) {

			R_AttributePointer(R_ARRAY_NORMAL, 3, r_state.array_buffers[R_ARRAY_NORMAL], NULL);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_NORMAL) {

				if ((mask & R_ARRAY_MASK_NEXT_NORMAL) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_NORMAL])) {
					R_AttributePointer(R_ARRAY_NEXT_NORMAL, 3, r_state.array_buffers[R_ARRAY_NEXT_NORMAL], NULL);
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

			R_AttributePointer(R_ARRAY_TANGENT, 4, r_state.array_buffers[R_ARRAY_TANGENT], NULL);

			if (p->arrays_mask & R_ARRAY_MASK_NEXT_TANGENT) {

				if ((mask & R_ARRAY_MASK_NEXT_TANGENT) && R_ValidBuffer(r_state.array_buffers[R_ARRAY_NEXT_TANGENT])) {
					R_AttributePointer(R_ARRAY_NEXT_TANGENT, 4, r_state.array_buffers[R_ARRAY_NEXT_TANGENT], NULL);
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
		r_state.shadow_program->arrays_mask = R_ARRAY_MASK_VERTEX | R_ARRAY_MASK_NEXT_VERTEX;
	}

	if ((r_state.shell_program = R_LoadProgram("shell", R_InitProgram_shell, R_PreLink_shell))) {
		r_state.shell_program->Use = R_UseProgram_shell;
		r_state.shell_program->UseMatrices = R_UseMatrices_shell;
		r_state.shell_program->UseCurrentColor = R_UseCurrentColor_shell;
		r_state.shell_program->UseInterpolation = R_UseInterpolation_shell;
		r_state.shell_program->arrays_mask = R_ARRAY_MASK_VERTEX | R_ARRAY_MASK_TEX_DIFFUSE | R_ARRAY_MASK_NEXT_VERTEX;
	}

	if ((r_state.warp_program = R_LoadProgram("warp", R_InitProgram_warp, R_PreLink_warp))) {
		r_state.warp_program->Use = R_UseProgram_warp;
		r_state.warp_program->UseFog = R_UseFog_warp;
		r_state.warp_program->UseMatrices = R_UseMatrices_warp;
		r_state.warp_program->UseCurrentColor = R_UseCurrentColor_warp;
		r_state.warp_program->arrays_mask = R_ARRAY_MASK_VERTEX | R_ARRAY_MASK_TEX_DIFFUSE;
	}

	if ((r_state.null_program = R_LoadProgram("null", R_InitProgram_null, R_PreLink_null))) {
		r_state.null_program->UseFog = R_UseFog_null;
		r_state.null_program->UseMatrices = R_UseMatrices_null;
		r_state.null_program->UseCurrentColor = R_UseCurrentColor_null;
		r_state.null_program->UseInterpolation = R_UseInterpolation_null;
		r_state.null_program->arrays_mask = R_ARRAY_MASK_VERTEX | R_ARRAY_MASK_NEXT_VERTEX | R_ARRAY_MASK_TEX_DIFFUSE |
		                                    R_ARRAY_MASK_COLOR;
	}

	if ((r_state.corona_program = R_LoadProgram("corona", R_InitProgram_corona, R_PreLink_corona))) {
		r_state.corona_program->UseFog = R_UseFog_corona;
		r_state.corona_program->UseMatrices = R_UseMatrices_corona;
		r_state.corona_program->arrays_mask = R_ARRAY_MASK_VERTEX | R_ARRAY_MASK_TEX_DIFFUSE | R_ARRAY_MASK_COLOR;
	}

	R_UseProgram(r_state.null_program);
}

