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
#include "r_gl.h"

cvar_t *r_geometry_shaders;

// glsl vertex and fragment shaders
typedef struct {
	GLenum type;
	GLuint id;
	char name[MAX_QPATH];
} r_shader_t;

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
void R_ProgramVariable(r_variable_t *variable, const GLenum type, const char *name, const _Bool required) {

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
		if (required)
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

	if (memcmp(&variable->value.mat4, value, sizeof(variable->value.mat4)) == 0) {
		return false;
	}

	memcpy(&variable->value.mat4, value, sizeof(variable->value.mat4));
	glUniformMatrix4fv(variable->location, 1, false, (GLfloat *) value);
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
	GLenum gl_type = buffer->element_gl_type;

	if (buffer->interleave) {
		r_attribute_id_t real_attrib = attribute;

		// check to see if we need to point to the right attrib
		// for NEXT_*
		if (buffer->interleave_attribs[attribute] == NULL) {

			switch (attribute) {
				case R_ATTRIB_NEXT_POSITION:
					real_attrib = R_ATTRIB_POSITION;
					break;
				case R_ATTRIB_NEXT_NORMAL:
					real_attrib = R_ATTRIB_NORMAL;
					break;
				case R_ATTRIB_NEXT_TANGENT:
					real_attrib = R_ATTRIB_TANGENT;
					break;
				default:
					break;
			}
		}

		assert(buffer->interleave_attribs[real_attrib]);

		type = &buffer->interleave_attribs[real_attrib]->_type_state;
		offset += type->offset;
		stride = buffer->element_type.stride;
		gl_type = buffer->interleave_attribs[real_attrib]->gl_type;
	}

	r_attrib_state_t *attrib = &r_state.attributes[attribute];

	// only set the ptr if it hasn't changed.
	if (attrib->type != type ||
	        attrib->value.buffer != buffer ||
	        attrib->offset != offset) {

		R_BindBuffer(buffer);

		if (type->integer) {
			glVertexAttribIPointer(attribute, type->count, gl_type, stride, (const GLvoid *) offset);
		} else {
			glVertexAttribPointer(attribute, type->count, gl_type, type->normalized, stride,
								  (const GLvoid *) offset);
		}

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
static void R_AttributeConstant4fv(const r_attribute_id_t attribute, const GLfloat *value) {

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
void R_EnableAttribute(const r_attribute_id_t attribute) {

	assert(attribute < R_ATTRIB_ALL && r_state.active_program->attributes[attribute].location != -1);

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

	assert(attribute < R_ATTRIB_ALL && r_state.active_program->attributes[attribute].location != -1);

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
static void R_ShutdownProgram(r_program_t *prog) {

	if (prog->Shutdown) {
		prog->Shutdown();
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

	for (r_program_id_t i = R_PROGRAM_DEFAULT; i < R_PROGRAM_TOTAL; i++) {

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
		Com_Warn("Failed to load %s\n", path);
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

	g_string_free(emplaced, true);

	return output;
}

/**
 * @brief
 */
static void R_LoadShader(GLenum type, const char *name, r_shader_t *out_shader) {
	char path[MAX_QPATH];
	void *buf;
	int32_t e;
	int64_t len;

	if (out_shader->id != 0) {
		Com_Error(ERROR_FATAL, "Memory corruption\n");
		return;
	}

	g_snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = Fs_Load(path, &buf)) == -1) {
		Com_Warn("Failed to load %s\n", name);
		return;
	}

	g_strlcpy(out_shader->name, name, sizeof(out_shader->name));

	out_shader->type = type;

	out_shader->id = glCreateShader(out_shader->type);
	if (!out_shader->id) {
		Fs_Free(buf);
		return;
	}

	// run shader source through cvar parser
	gchar *parsed = R_PreprocessShader((const char *) buf, (uint32_t) len);

	// upload the shader source
	glShaderSource(out_shader->id, 1, (const GLchar *const *) &parsed, NULL);

	// compile it and check for errors
	glCompileShader(out_shader->id);

	glGetShaderiv(out_shader->id, GL_COMPILE_STATUS, &e);

	if (!e) {
		glGetShaderiv(out_shader->id, GL_INFO_LOG_LENGTH, &e);

		char log[e];

		glGetShaderInfoLog(out_shader->id, e, NULL, log);
		Com_Warn("%s: %s\n", out_shader->name, log);
		
		glDeleteShader(out_shader->id);
		memset(out_shader, 0, sizeof(*out_shader));
	}
	
	g_free(parsed);

	Fs_Free(buf);
}

/**
 * @brief
 */
static void R_InitProgramUniforms(r_program_t *program) {

	for (r_matrix_id_t i = R_MATRIX_PROJECTION; i < R_MATRIX_TOTAL; i++) {

		// initial dirtiness
		program->matrix_dirty[i] = true;
	}

	// hook up UBO
	GLuint block_index = glGetUniformBlockIndex(program->id, "SharedData");

	if (block_index != GL_INVALID_INDEX) {
		glUniformBlockBinding(program->id, block_index, 8);
	}
}

// some temporary storage, since only one program can be made at a time
static GLuint shaders_attached[32];
static size_t cur_shader = 0;

/**
 * @brief
 */
static void R_CreateProgram(const char *name, r_program_t *out_program) {
	g_strlcpy(out_program->name, name, sizeof(out_program->name));

	memset(out_program->attributes, -1, sizeof(out_program->attributes));

	out_program->id = glCreateProgram();
	out_program->program_id = (r_program_id_t) (ptrdiff_t) (out_program - r_state.programs);
}

/**
 * @brief
 */
static void R_AttachShader(r_program_t *out_program, const GLenum type, const char *name) {
	static r_shader_t shader;
	shader.id = 0;

	R_LoadShader(type, va("%s_%ss.glsl", name, type == GL_VERTEX_SHADER ? "v" : type == GL_FRAGMENT_SHADER ? "f" : "g"), &shader);

	if (shader.id) {
		glAttachShader(out_program->id, shader.id);
	
		shaders_attached[cur_shader++] = shader.id;
	}
}

/**
 * @brief
 */
static _Bool R_LinkProgram(r_program_t *out_program, void (*Init)(r_program_t *program)) {
	int32_t e;

	glLinkProgram(out_program->id);

	glGetProgramiv(out_program->id, GL_LINK_STATUS, &e);

	if (!e) {
		glGetProgramiv(out_program->id, GL_INFO_LOG_LENGTH, &e);

		char log[e];

		glGetProgramInfoLog(out_program->id, e, NULL, log);
		Com_Warn("%s: %s\n", out_program->name, log);

		R_ShutdownProgram(out_program);
		return false;
	}
	
	if (Init) { // invoke initialization function
		R_UseProgram(out_program);

		Init(out_program);

		R_UseProgram(NULL);
	}

	for (size_t i = 0; i < cur_shader; i++) {
		glDeleteShader(shaders_attached[i]);
	}

	cur_shader = 0;

	R_UseProgram(out_program);

	R_InitProgramUniforms(out_program);

	R_GetError(out_program->name);

	return true;
}

/**
 * @brief Sets up attributes for the currently bound program based on the active
 * array mask.
 */
void R_SetupAttributes(void) {

	const r_program_t *p = (const r_program_t *) r_state.active_program;
	const r_attribute_mask_t mask = R_ArraysMask();

	if (p->arrays_mask & R_ATTRIB_MASK_POSITION) {

		if (mask & R_ATTRIB_MASK_POSITION) {

			R_AttributePointer(R_ATTRIB_POSITION);

			if (p->arrays_mask & R_ATTRIB_MASK_NEXT_POSITION) {

				if ((mask & R_ATTRIB_MASK_NEXT_POSITION) && R_ValidBuffer(r_state.array_buffers[R_ATTRIB_NEXT_POSITION])) {
					R_AttributePointer(R_ATTRIB_NEXT_POSITION);
				} else {
					R_DisableAttribute(R_ATTRIB_NEXT_POSITION);
				}
			}
		} else {

			R_DisableAttribute(R_ATTRIB_POSITION);
			R_DisableAttribute(R_ATTRIB_NEXT_POSITION);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_COLOR) {

		if (mask & R_ATTRIB_MASK_COLOR) {
			R_AttributePointer(R_ATTRIB_COLOR);
		} else if (r_state.color_array_enabled) {
			R_AttributeConstant4fv(R_ATTRIB_COLOR, r_state.uniforms.global_color);
		} else {
			const vec_t white[] = { 1.0, 1.0, 1.0, 1.0 };
			R_AttributeConstant4fv(R_ATTRIB_COLOR, white);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_DIFFUSE) {

		if (mask & R_ATTRIB_MASK_DIFFUSE) {
			R_AttributePointer(R_ATTRIB_DIFFUSE);
		} else {
			R_DisableAttribute(R_ATTRIB_DIFFUSE);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_LIGHTMAP) {

		if (mask & R_ATTRIB_MASK_LIGHTMAP) {
			R_AttributePointer(R_ATTRIB_LIGHTMAP);
		} else {
			R_DisableAttribute(R_ATTRIB_LIGHTMAP);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_NORMAL) {

		if (mask & R_ATTRIB_MASK_NORMAL) {

			R_AttributePointer(R_ATTRIB_NORMAL);

			if (p->arrays_mask & R_ATTRIB_MASK_NEXT_NORMAL) {

				if ((mask & R_ATTRIB_MASK_NEXT_NORMAL) && R_ValidBuffer(r_state.array_buffers[R_ATTRIB_NEXT_NORMAL])) {
					R_AttributePointer(R_ATTRIB_NEXT_NORMAL);
				} else {
					R_DisableAttribute(R_ATTRIB_NEXT_NORMAL);
				}
			}
		} else {

			R_DisableAttribute(R_ATTRIB_NORMAL);
			R_DisableAttribute(R_ATTRIB_NEXT_NORMAL);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_TANGENT) {

		if (mask & R_ATTRIB_MASK_TANGENT) {

			R_AttributePointer(R_ATTRIB_TANGENT);

			if (p->arrays_mask & R_ATTRIB_MASK_NEXT_TANGENT) {

				if ((mask & R_ATTRIB_MASK_NEXT_TANGENT) && R_ValidBuffer(r_state.array_buffers[R_ATTRIB_NEXT_TANGENT])) {
					R_AttributePointer(R_ATTRIB_NEXT_TANGENT);
				} else {
					R_DisableAttribute(R_ATTRIB_NEXT_TANGENT);
				}
			}
		} else {

			R_DisableAttribute(R_ATTRIB_TANGENT);
			R_DisableAttribute(R_ATTRIB_NEXT_TANGENT);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_SCALE) {

		if (mask & R_ATTRIB_MASK_SCALE) {
			R_AttributePointer(R_ATTRIB_SCALE);
		} else {
			R_DisableAttribute(R_ATTRIB_SCALE);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_ROLL) {

		if (mask & R_ATTRIB_MASK_ROLL) {
			R_AttributePointer(R_ATTRIB_ROLL);
		} else {
			R_DisableAttribute(R_ATTRIB_ROLL);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_END) {

		if (mask & R_ATTRIB_MASK_END) {
			R_AttributePointer(R_ATTRIB_END);
		} else {
			R_DisableAttribute(R_ATTRIB_END);
		}
	}

	if (p->arrays_mask & R_ATTRIB_MASK_TYPE) {

		if (mask & R_ATTRIB_MASK_TYPE) {
			R_AttributePointer(R_ATTRIB_TYPE);
		} else {
			R_DisableAttribute(R_ATTRIB_TYPE);
		}
	}
}

/**
 * @brief Upload the shared uniform data block if it's changed
 */
void R_SetupUniforms(void) {
	
	// update per-frame uniforms
	if (r_state.uniforms.time != r_view.ticks) {

		r_state.uniforms.time = r_view.ticks;
		R_EXPAND_BOUNDS(r_state.uniforms_dirty, time);
	}

	if (r_state.uniforms_dirty.end == 0) {
		return;
	}
	
	R_UploadToSubBuffer(&r_state.uniforms_buffer, r_state.uniforms_dirty.begin, r_state.uniforms_dirty.end - r_state.uniforms_dirty.begin, &r_state.uniforms, true);

	r_state.uniforms_dirty.begin = (size_t) -1;
	r_state.uniforms_dirty.end = 0;
}

/**
 * @brief
 */
static _Bool R_LoadSimpleProgram(const char *name, void (*Init)(r_program_t *program),
                           void (*PreLink)(const r_program_t *program),
                           r_program_t *out_program) {

	R_CreateProgram(name, out_program);
	R_AttachShader(out_program, GL_VERTEX_SHADER, name);
	R_AttachShader(out_program, GL_FRAGMENT_SHADER, name);
	PreLink(out_program);
	return R_LinkProgram(out_program, Init);
}

/**
 * @brief
 */
void R_InitPrograms(void) {

	// this only needs to be done once
	if (!shader_preprocess_regex) {
		GError *error = NULL;
		shader_preprocess_regex = g_regex_new("#include [\"\']([a-z0-9_/]+\\.glsl)[\"\']",
		                                      G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, &error);

		if (error) {
			Com_Warn("Error compiling regex: %s", error->message);
		}
	}

	memset(r_state.programs, 0, sizeof(r_state.programs));
	
	r_geometry_shaders = Cvar_Add("r_geometry_shaders", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Whether geometry shaders are enabled or not, if supported.");

	if (R_LoadSimpleProgram("default", R_InitProgram_default, R_PreLink_default, program_default)) {
		program_default->Shutdown = R_Shutdown_default;
		program_default->Use = R_UseProgram_default;
		program_default->UseMaterial = R_UseMaterial_default;
		program_default->UseFog = R_UseFog_default;
		program_default->UseLight = R_UseLight_default;
		program_default->UseCaustic = R_UseCaustic_default;
		program_default->MatricesChanged = R_MatricesChanged_default;
		program_default->UseAlphaTest = R_UseAlphaTest_default;
		program_default->UseTints = R_UseTints_default;
		program_default->arrays_mask = R_ATTRIB_MASK_ALL & ~R_ATTRIB_GEOMETRY_MASK;
	}

	if (R_LoadSimpleProgram("shadow", R_InitProgram_shadow, R_PreLink_shadow, program_shadow)) {
		program_shadow->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_NEXT_POSITION;
	}

	if (R_LoadSimpleProgram("shell", R_InitProgram_shell, R_PreLink_shell, program_shell)) {
		program_shell->Use = R_UseProgram_shell;
		program_shell->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_NEXT_POSITION | R_ATTRIB_MASK_DIFFUSE |
		                             R_ATTRIB_MASK_NORMAL | R_ATTRIB_MASK_NEXT_NORMAL;
	}

	if (R_LoadSimpleProgram("warp", R_InitProgram_warp, R_PreLink_warp, program_warp)) {
		program_warp->Use = R_UseProgram_warp;
		program_warp->UseFog = R_UseFog_warp;
		program_warp->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_DIFFUSE;
	}

	if (R_LoadSimpleProgram("null", R_InitProgram_null, R_PreLink_null, program_null)) {
		program_null->UseFog = R_UseFog_null;
		program_null->UseMaterial = R_UseMaterial_null;
		program_null->UseTints = R_UseTints_null;
		program_null->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_NEXT_POSITION | R_ATTRIB_MASK_DIFFUSE |
		                            R_ATTRIB_MASK_COLOR;
	}

	if (R_LoadSimpleProgram("corona", R_InitProgram_corona, R_PreLink_corona, program_corona)) {
		program_corona->UseFog = R_UseFog_corona;
		program_corona->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_DIFFUSE | R_ATTRIB_MASK_COLOR;
	}

	if (R_LoadSimpleProgram("stain", R_InitProgram_stain, R_PreLink_stain, program_stain)) {
		program_stain->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_DIFFUSE | R_ATTRIB_MASK_COLOR;
	}

	if (r_geometry_shaders->integer) {
		R_CreateProgram("particle", program_particle);
		R_AttachShader(program_particle, GL_VERTEX_SHADER, "particle");
		R_AttachShader(program_particle, GL_GEOMETRY_SHADER, "particle");
		R_AttachShader(program_particle, GL_FRAGMENT_SHADER, "particle");
		R_PreLink_particle(program_particle);

		if (R_LinkProgram(program_particle, R_InitProgram_particle)) {
			program_particle->UseFog = R_UseFog_particle;
			program_particle->arrays_mask = R_ATTRIB_MASK_POSITION | R_ATTRIB_MASK_DIFFUSE |
											R_ATTRIB_MASK_COLOR | R_ATTRIB_MASK_LIGHTMAP | R_ATTRIB_MASK_SCALE |
											R_ATTRIB_MASK_ROLL | R_ATTRIB_MASK_END | R_ATTRIB_MASK_TYPE;
	
			r_state.particle_program = program_particle;
		} else {
			r_state.particle_program = program_null;
		}

		R_CreateProgram("particle_corona", program_particle_corona);
		R_AttachShader(program_particle_corona, GL_VERTEX_SHADER, "particle_corona");
		R_AttachShader(program_particle_corona, GL_GEOMETRY_SHADER, "particle");
		R_AttachShader(program_particle_corona, GL_FRAGMENT_SHADER, "corona");
		R_PreLink_particle(program_particle_corona);

		if (R_LinkProgram(program_particle_corona, R_InitProgram_particle_corona)) {
			program_particle_corona->UseFog = R_UseFog_particle_corona;
			program_particle_corona->arrays_mask =	R_ATTRIB_MASK_POSITION |
													R_ATTRIB_MASK_COLOR | R_ATTRIB_MASK_SCALE;
	
			r_state.corona_program = program_particle_corona;
		} else {
			r_state.corona_program = program_corona;
		}
	} else {
		r_state.particle_program = program_null;
		r_state.corona_program = program_corona;
	}

	R_UseProgram(program_null);
}
