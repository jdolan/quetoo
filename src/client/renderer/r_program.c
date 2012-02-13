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

#include "r_local.h"

/*
 * R_UseProgram
 */
void R_UseProgram(r_program_t *prog) {

	if (!qglUseProgram || r_state.active_program == prog)
		return;

	r_state.active_program = prog;

	if (prog) {
		qglUseProgram(prog->id);

		if (prog->Use) // invoke use function
			prog->Use();
	} else {
		qglUseProgram(0);
	}

	R_GetError(NULL);
}

/*
 * R_ProgramVariable
 */
void R_ProgramVariable(r_variable_t *variable, GLenum type, const char *name) {

	memset(variable, 0, sizeof(*variable));
	variable->location = -1;

	if (!r_state.active_program) {
		Com_Warn("R_ProgramVariable: No program currently bound\n");
		return;
	}

	switch (type) {
	case R_ATTRIBUTE:
		variable->location = qglGetAttribLocation(r_state.active_program->id, name);
		break;
	default:
		variable->location = qglGetUniformLocation(r_state.active_program->id, name);
		break;
	}

	if (variable->location == -1) {
		Com_Warn("R_ProgramVariable: Failed to resolve variable %s in program %s\n", name,
				r_state.active_program->name);
		return;
	}

	variable->type = type;
	strncpy(variable->name, name, sizeof(variable->name));
	memset(&variable->value, 0xff, sizeof(variable->value));
}

/*
 * R_ProgramParameter1i
 */
void R_ProgramParameter1i(r_uniform1i_t *variable, GLint value) {

	if (!variable || variable->location == -1) {
		Com_Warn("R_ProgramParameter1i: NULL or invalid variable\n");
		return;
	}

	if (variable->value.i == value)
		return;

	qglUniform1i(variable->location, value);
	variable->value.i = value;

	R_GetError(variable->name);
}

/*
 * R_ProgramParameter1f
 */
void R_ProgramParameter1f(r_uniform1f_t *variable, GLfloat value) {

	if (!variable || variable->location == -1) {
		Com_Warn("R_ProgramParameter1f: NULL or invalid variable\n");
		return;
	}

	if (variable->value.f == value)
		return;

	qglUniform1f(variable->location, value);
	variable->value.f = value;

	R_GetError(variable->name);
}

/*
 * R_ProgramParameter3fv
 */
void R_ProgramParameter3fv(r_uniform3fv_t *variable, GLfloat *value) {

	if (!variable || variable->location == -1) {
		Com_Warn("R_ProgramParameter3fv: NULL or invalid variable\n");
		return;
	}

	if (VectorCompare(variable->value.vec3, value))
		return;

	qglUniform3fv(variable->location, 1, value);
	VectorCopy(value, variable->value.vec3);

	R_GetError(variable->name);
}

/*
 * R_AttributePointer
 */
void R_AttributePointer(const char *name, GLuint size, GLvoid *array) {
	r_attribute_t attribute;

	R_ProgramVariable(&attribute, R_ATTRIBUTE, name);

	qglVertexAttribPointer(attribute.location, size, GL_FLOAT, GL_FALSE, 0, array);

	R_GetError(name);
}

/*
 * R_EnableAttribute
 */
void R_EnableAttribute(r_attribute_t *attribute) {

	if (!attribute || attribute->location == -1) {
		Com_Warn("R_EnableAttribute: NULL or invalid attribute\n");
		return;
	}

	if (attribute->value.i != 1) {
		qglEnableVertexAttribArray(attribute->location);
		attribute->value.i = 1;
	}

	R_GetError(attribute->name);
}

/*
 * R_DisableAttribute
 */
void R_DisableAttribute(r_attribute_t *attribute) {

	if (!attribute || attribute->location == -1) {
		Com_Warn("R_DisableAttribute: NULL or invalid attribute\n");
		return;
	}

	if (attribute->value.i != 0) {
		qglDisableVertexAttribArray(attribute->location);
		attribute->value.i = 0;
	}

	R_GetError(attribute->name);
}

/*
 * R_ShutdownShader
 */
static void R_ShutdownShader(r_shader_t *sh) {

	qglDeleteShader(sh->id);
	memset(sh, 0, sizeof(r_shader_t));
}

/*
 * R_ShutdownProgram
 */
static void R_ShutdownProgram(r_program_t *prog) {

	if (prog->v)
		R_ShutdownShader(prog->v);
	if (prog->f)
		R_ShutdownShader(prog->f);

	qglDeleteProgram(prog->id);

	R_GetError(prog->name);

	memset(prog, 0, sizeof(r_program_t));
}

/*
 * R_ShutdownPrograms
 */
void R_ShutdownPrograms(void) {
	int i;

	if (!qglDeleteProgram)
		return;

	for (i = 0; i < MAX_PROGRAMS; i++) {

		if (!r_state.programs[i].id)
			continue;

		R_ShutdownProgram(&r_state.programs[i]);
	}
}

/*
 * R_LoadShader
 */
static r_shader_t *R_LoadShader(GLenum type, const char *name) {
	r_shader_t *sh;
	char path[MAX_QPATH], *src[1], log[MAX_STRING_CHARS];
	unsigned e, length[1];
	void *buf;
	int i, len;

	snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = Fs_LoadFile(path, &buf)) == -1) {
		Com_Debug("R_LoadShader: Failed to load %s.\n", name);
		return NULL;
	}

	src[0] = (char *) buf;
	length[0] = len;

	for (i = 0; i < MAX_SHADERS; i++) {
		sh = &r_state.shaders[i];

		if (!sh->id)
			break;
	}

	if (i == MAX_SHADERS) {
		Com_Warn("R_LoadShader: MAX_SHADERS reached.\n");
		Fs_FreeFile(buf);
		return NULL;
	}

	strncpy(sh->name, name, sizeof(sh->name));

	sh->type = type;

	sh->id = qglCreateShader(sh->type);
	if (!sh->id)
		return NULL;

	// upload the shader source
	qglShaderSource(sh->id, 1, src, length);

	// compile it and check for errors
	qglCompileShader(sh->id);

	qglGetShaderiv(sh->id, GL_COMPILE_STATUS, &e);
	if (!e) {
		qglGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
		Com_Warn("R_LoadShader: %s: %s\n", sh->name, log);

		qglDeleteShader(sh->id);
		memset(sh, 0, sizeof(*sh));

		Fs_FreeFile(buf);
		return NULL;
	}

	Fs_FreeFile(buf);
	return sh;
}

/*
 * R_LoadProgram
 */
static r_program_t *R_LoadProgram(const char *name, void(*Init)(void)) {

	r_program_t *prog;
	char log[MAX_STRING_CHARS];
	unsigned e;
	int i;

	for (i = 0; i < MAX_PROGRAMS; i++) {
		prog = &r_state.programs[i];

		if (!prog->id)
			break;
	}

	if (i == MAX_PROGRAMS) {
		Com_Warn("R_LoadProgram: MAX_PROGRAMS reached.\n");
		return NULL;
	}

	strncpy(prog->name, name, sizeof(prog->name));

	prog->id = qglCreateProgram();

	prog->v = R_LoadShader(GL_VERTEX_SHADER, va("%s_vs.glsl", name));
	prog->f = R_LoadShader(GL_FRAGMENT_SHADER, va("%s_fs.glsl", name));

	if (prog->v)
		qglAttachShader(prog->id, prog->v->id);
	if (prog->f)
		qglAttachShader(prog->id, prog->f->id);

	qglLinkProgram(prog->id);

	qglGetProgramiv(prog->id, GL_LINK_STATUS, &e);
	if (!e) {
		qglGetProgramInfoLog(prog->id, sizeof(log) - 1, NULL, log);
		Com_Warn("R_LoadProgram: %s: %s\n", prog->name, log);

		R_ShutdownProgram(prog);
		return NULL;
	}

	prog->Init = Init;

	if (prog->Init) { // invoke initialization function
		R_UseProgram(prog);

		prog->Init();

		R_UseProgram(NULL);
	}

	R_GetError(prog->name);

	return prog;
}

/*
 * R_InitPrograms
 */
void R_InitPrograms(void) {

	if (!qglCreateProgram)
		return;

	memset(r_state.shaders, 0, sizeof(r_state.shaders));

	memset(r_state.programs, 0, sizeof(r_state.programs));

	if (!r_programs->value)
		return;

	r_state.default_program = R_LoadProgram("default", R_InitProgram_default);
	r_state.default_program->Use = R_UseProgram_default;
	r_state.default_program->UseMaterial = R_UseMaterial_default;
	r_state.default_program->arrays_mask = 0xff;

	r_state.warp_program = R_LoadProgram("warp", R_InitProgram_warp);
	r_state.warp_program->Use = R_UseProgram_warp;
	r_state.warp_program->arrays_mask = R_ARRAY_VERTEX | R_ARRAY_TEX_DIFFUSE;

	r_state.pro_program = R_LoadProgram("pro", NULL);
	r_state.pro_program->arrays_mask = R_ARRAY_VERTEX | R_ARRAY_COLOR | R_ARRAY_NORMAL;
}

