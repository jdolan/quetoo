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

static cvar_t *r_get_error;

r_state_t r_state;

const vec_t default_texcoords[] = { // useful for particles, pics, etc..
	0.0, 0.0,
	1.0, 0.0,
	1.0, 1.0,
	0.0, 1.0
};

/**
 * @brief Queries OpenGL for any errors and prints them as warnings.
 */
void R_GetError_(const char *function, const char *msg) {
	GLenum err;
	char *s;

	if (!r_get_error->value)
		return;

	while (true) {

		if ((err = glGetError()) == GL_NO_ERROR)
			return;

		switch (err) {
			case GL_INVALID_ENUM:
				s = "GL_INVALID_ENUM";
				break;
			case GL_INVALID_VALUE:
				s = "GL_INVALID_VALUE";
				break;
			case GL_INVALID_OPERATION:
				s = "GL_INVALID_OPERATION";
				break;
			case GL_OUT_OF_MEMORY:
				s = "GL_OUT_OF_MEMORY";
				break;
			default:
				s = va("%" PRIx32, err);
				break;
		}

		Sys_Backtrace();

		Com_Warn("%s threw %s: %s.\n", function, s, msg);
	}
}

/**
 * @brief Sets the current color. Pass NULL to reset to default.
 */
void R_Color(const vec4_t color) {
	static const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color) {
		Vector4Copy(color, r_state.current_color);
	} else {
		Vector4Copy(white, r_state.current_color);
	}
}

/**
 * @brief Set the active texture unit.
 */
void R_SelectTexture(r_texunit_t *texunit) {

	if (texunit == r_state.active_texunit)
		return;

	r_state.active_texunit = texunit;

	glActiveTexture(texunit->texture);
}

/**
 * @brief Request that a texnum be bound to the specified texture unit.
 * returns true if it was indeed bound (for statistical analysis)
 */
_Bool R_BindUnitTexture(r_texunit_t *texunit, GLuint texnum) {

	if (texnum == texunit->texnum)
		return false;

	// save old texunit so we go back to it after
	// this bind
	r_texunit_t *old_unit = r_state.active_texunit;

	R_SelectTexture(texunit);

	// bind the texture
	glBindTexture(GL_TEXTURE_2D, texnum);

	texunit->texnum = texnum;

	r_view.num_bind_texture++;

	// restore old texunit
	R_SelectTexture(old_unit);

	return true;
}

/**
 * @brief Request that a texnum be bound to the active texture unit.
 */
void R_BindTexture(GLuint texnum) {

	R_BindUnitTexture(r_state.active_texunit, texnum);
}

/**
 * @brief Binds the specified texture for the lightmap texture unit.
 */
void R_BindLightmapTexture(GLuint texnum) {

	if (R_BindUnitTexture(&texunit_lightmap, texnum))
		r_view.num_bind_lightmap++;
}

/**
 * @brief Binds the specified texture for the deluxemap texture unit.
 */
void R_BindDeluxemapTexture(GLuint texnum) {

	if (R_BindUnitTexture(&texunit_deluxemap, texnum))
		r_view.num_bind_deluxemap++;
}

/**
 * @brief Binds the specified texture for the normalmap texture unit.
 */
void R_BindNormalmapTexture(GLuint texnum) {

	if (R_BindUnitTexture(&texunit_normalmap, texnum))
		r_view.num_bind_normalmap++;
}

/**
 * @brief Binds the specified texture for the glossmap texture unit.
 */
void R_BindSpecularmapTexture(GLuint texnum) {

	if (R_BindUnitTexture(&texunit_specularmap, texnum))
		r_view.num_bind_specularmap++;
}

/**
 * @brief Binds the specified buffer for the given target.
 */
void R_BindArray(int target, const r_buffer_t *buffer) {

	assert(!buffer || ((buffer->type == R_BUFFER_DATA) == (target != R_ARRAY_ELEMENTS)));

	if (target == R_ARRAY_ELEMENTS)
		r_state.element_buffer = buffer;
	else
		r_state.array_buffers[target] = buffer;
}

/**
 * @brief Returns whether or not a buffer is "finished".
 * Doesn't care if data is actually in the buffer.
 */
_Bool R_ValidBuffer(const r_buffer_t *buffer) {

	return buffer && buffer->bufnum && buffer->size;
}

/**
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray(int target) {

	switch (target) {
		case R_ARRAY_VERTEX:
			R_BindArray(target, &r_state.buffer_vertex_array);
			break;
		case R_ARRAY_TEX_DIFFUSE:
			R_BindArray(target, &texunit_diffuse.buffer_texcoord_array);
			break;
		case R_ARRAY_TEX_LIGHTMAP:
			R_BindArray(target, &texunit_lightmap.buffer_texcoord_array);
			break;
		case R_ARRAY_COLOR:
			R_BindArray(target, &r_state.buffer_color_array);
			break;
		case R_ARRAY_NORMAL:
			R_BindArray(target, &r_state.buffer_normal_array);
			break;
		case R_ARRAY_TANGENT:
			R_BindArray(target, &r_state.buffer_tangent_array);
			break;
		default:
			R_BindArray(target, NULL);
			break;
	}
}

/**
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
static GLenum R_BufferTypeToTarget(int type) {

	return (type == R_BUFFER_ELEMENT) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
}

/**
 * @brief
 */
void R_BindBuffer(const r_buffer_t *buffer) {

	assert(buffer->bufnum != 0);

	if (r_state.active_buffers[buffer->type] == buffer->bufnum)
		return;

	r_state.active_buffers[buffer->type] = buffer->bufnum;

	glBindBuffer(buffer->target, buffer->bufnum);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_UnbindBuffer(const int type) {

	if (!r_state.active_buffers[type])
		return;

	r_state.active_buffers[type] = 0;

	glBindBuffer(R_BufferTypeToTarget(type), 0);

	R_GetError(NULL);
}

/**
 * @brief Upload data to an already-created buffer.
 * Note that this function assumes "data" is already offset to
 * where you want to start reading from. You could also use this function to
 * resize an existing buffer without uploading data, although it can't make the
 * buffer smaller.
 */
void R_UploadToBuffer(r_buffer_t *buffer, const size_t start, const size_t size, const void *data) {

	assert(buffer->bufnum != 0);

	// Check size. This is benign really, but it's usually a bug.
	if (!size) {
		Com_Warn("Attempted to upload 0 bytes to GPU");
		return;
	}

	R_BindBuffer(buffer);

	// if the buffer isn't big enough to hold what we had already,
	// we have to resize the buffer
	const size_t total_size = start + size;

	if (total_size > buffer->size) {
		// if we passed a "start", the data is offset, so
		// just reset to null. This is an odd edge case and
		// it's fairly rare you'll be editing at the end first,
		// but who knows.
		if (start) {
			glBufferData(buffer->target, total_size, NULL, buffer->hint);
			R_GetError("Partial resize");
			glBufferSubData(buffer->target, start, size, data);
			R_GetError("Partial update");
		}
		else
		{
			glBufferData(buffer->target, total_size, data, buffer->hint);
			R_GetError("Full resize");
		}

		buffer->size = total_size;
	}
	else {
		// just update the range we specified
		glBufferSubData(buffer->target, start, size, data);

		R_GetError("Updating existing buffer");
	}
}

/**
 * @brief Allocate a GPU buffer of the specified size.
 * Optionally upload the data immediately too.
 */
void R_CreateBuffer(r_buffer_t *buffer, const GLenum hint, const int type, const size_t size, const void *data) {

	assert(buffer->bufnum == 0);

	glGenBuffers(1, &buffer->bufnum);

	buffer->type = type;
	buffer->target = R_BufferTypeToTarget(buffer->type);
	buffer->hint = hint;

	R_UploadToBuffer(buffer, 0, size, data);
}

/**
 * @brief Destroy a GPU-allocated buffer.
 */
void R_DestroyBuffer(r_buffer_t *buffer)
{
	assert(buffer->bufnum != 0);

	// if this buffer is currently bound, unbind it.
	if (r_state.active_buffers[buffer->type] == buffer->bufnum)
		R_UnbindBuffer(buffer->type);

	// if the buffer is attached to any active attribs, remove that ptr too
	for (r_attribute_id_t i = 0; i < R_ARRAY_MAX_ATTRIBS; ++i) {

		if (r_state.attributes[i].constant == false &&
			r_state.attributes[i].value.buffer == buffer)
			r_state.attributes[i].value.buffer = NULL;
	}

	glDeleteBuffers(1, &buffer->bufnum);

	memset(buffer, 0, sizeof(r_buffer_t));

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_BlendFunc(GLenum src, GLenum dest) {

	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

/**
 * @brief
 */
void R_EnableDepthMask(_Bool enable) {
	
	if (r_state.depth_mask_enabled == enable)
		return;

	r_state.depth_mask_enabled = enable;

	if (enable) {
		glDepthMask(GL_TRUE);
	} else {
		glDepthMask(GL_FALSE);
	}
}

/**
 * @brief
 */
void R_EnableBlend(_Bool enable) {

	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}
}

/**
 * @brief
 */
void R_EnableAlphaTest(float threshold) {

	if (r_state.alpha_threshold == threshold)
		return;

	r_state.alpha_threshold = threshold;
}

/**
 * @brief Enables the stencil test for e.g. rendering shadow volumes.
 */
void R_EnableStencilTest(GLenum pass, _Bool enable) {

	if (r_state.stencil_test_enabled == enable)
		return;

	r_state.stencil_test_enabled = enable;

	if (enable)
		glEnable(GL_STENCIL_TEST);
	else
		glDisable(GL_STENCIL_TEST);

	if (r_state.stencil_op_pass != pass) {

		glStencilOp(GL_KEEP, GL_KEEP, pass);
		r_state.stencil_op_pass = pass;
	}
}

/**
 * @brief Sets stencil func parameters.
 */
void R_StencilFunc(GLenum func, GLint ref, GLuint mask) {
	
	if (r_state.stencil_func_func == func &&
		r_state.stencil_func_ref == ref &&
		r_state.stencil_func_mask == mask)
		return;
	
	r_state.stencil_func_func = func;
	r_state.stencil_func_ref = ref;
	r_state.stencil_func_mask = mask;

	glStencilFunc(func, ref, mask);
}

/**
 * @brief
 */
void R_PolygonOffset(GLfloat factor, GLfloat units) {

	if (r_state.polygon_offset_factor == factor &&
		r_state.polygon_offset_units == units)
		return;

	glPolygonOffset(factor, units);

	r_state.polygon_offset_factor = factor;
	r_state.polygon_offset_units = units;
}

/**
 * @brief Enables polygon offset fill for decals, etc.
 */
void R_EnablePolygonOffset(GLenum mode, _Bool enable) {

	if (r_state.polygon_offset_enabled == enable)
		return;

	r_state.polygon_offset_enabled = enable;

	if (enable)
		glEnable(mode);
	else
		glDisable(mode);

	R_PolygonOffset(-1.0, 1.0);
}

/**
 * @brief Enable the specified texture unit for multi-texture operations. This is not
 * necessary for texture units only accessed by GLSL shaders.
 */
void R_EnableTexture(r_texunit_t *texunit, _Bool enable) {

	if (enable == texunit->enabled)
		return;

	texunit->enabled = enable;

	GLuint texnum;

	if (enable) { // activate texture unit

		texnum = texunit->texnum;
	} else { // or deactivate it

		texnum = r_image_state.null ? r_image_state.null->texnum : 0;
	}

	R_BindUnitTexture(texunit, texnum);
}

/**
 * @brief
 */
void R_EnableColorArray(_Bool enable) {

	if (r_state.color_array_enabled == enable)
		return;

	r_state.color_array_enabled = enable;
}

/**
 * @brief Enables hardware-accelerated lighting with the specified program. This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
void R_EnableLighting(const r_program_t *program, _Bool enable) {

	if (program == NULL)
		program = r_state.null_program;

	// if the program can't use lights, lighting can't really
	// be enabled on it.
	if (enable && !program->UseLight) {
		Com_Warn("Attempted to use lights on a non-lightable program\n");
		return;
	}

	// use the program here. this is done regardless
	// of if lighting is supported.
	R_UseProgram(program);
	
	// enable fog if supported by the program.
	R_EnableFog(enable);
	
	R_GetError(NULL);

	// if we don't actually have lighting support,
	// don't bother turning on the lights.
	if (!r_lighting->value || r_state.lighting_enabled == enable)
		return;

	r_state.lighting_enabled = enable;

	// set up all the lights if we're enabling
	if (enable) {
		R_ResetLights();
		R_GetError(NULL);
	}
}

/**
 * @brief Enables alpha-blended, stencil-test shadows.
 */
void R_EnableShadow(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id))
		return;

	if (!r_shadows->value || r_state.shadow_enabled == enable)
		return;

	r_state.shadow_enabled = enable;

	if (enable)
		R_UseProgram(program);
	else
		R_UseProgram(r_state.null_program);

	R_EnableFog(enable);

	R_GetError(NULL);
}

/**
 * @brief Enables the warp shader for drawing liquids and other effects.
 */
void R_EnableWarp(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id))
		return;

	if (!r_warp->value || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		R_BindTexture(r_image_state.warp->texnum);

		R_UseProgram(program);
	} else {
		R_UseProgram(r_state.null_program);
	}

	R_EnableFog(enable);

	R_SelectTexture(&texunit_diffuse);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_EnableShell(const r_program_t *program, _Bool enable) {

	if (enable && (!program || !program->id))
		return;

	if (!r_shell->value || r_state.shell_enabled == enable)
		return;

	r_state.shell_enabled = enable;

	if (enable) {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		R_BindTexture(r_image_state.shell->texnum);

		R_UseProgram(program);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		R_UseProgram(r_state.null_program);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_EnableFog(_Bool enable) {
	
	if (!r_state.active_program)
		return;

	if (!r_fog->value || r_state.fog_enabled == enable || !r_state.active_program->UseFog)
		return;

	r_state.fog_enabled = false;

	if (enable) {
		if ((r_view.weather & WEATHER_FOG) || r_fog->integer == 2) {

			r_state.fog_enabled = true;
			
			r_state.active_fog_parameters.start = FOG_START;
			r_state.active_fog_parameters.end = FOG_END;
			VectorCopy(r_view.fog_color, r_state.active_fog_parameters.color);
			r_state.active_fog_parameters.density = 1.0;
		}
		else {
			r_state.active_fog_parameters.density = 0.0;
		}
	} else {
		r_state.active_fog_parameters.density = 0.0;
	}
}

/**
 * @brief Setup the GLSL program for the specified material. If no program is
 * bound, this function simply returns.
 */
void R_UseMaterial(const r_material_t *material) {

	if (!r_state.active_program)
		return;

	if (r_state.active_material == material)
		return;

	r_state.active_material = material;

	if (r_state.active_program->UseMaterial)
		r_state.active_program->UseMaterial(material);

	R_GetError(material ? material->diffuse->media.name : r_state.active_program->name);
}

/**
 * @brief Push the active matrix into the stack
 */
void R_PushMatrix(const r_matrix_id_t id) {

	if (r_state.matrix_stacks[id].depth == MAX_MATRIX_STACK)
		Com_Error(ERR_DROP, "Matrix stack overflow");

	Matrix4x4_Copy(&r_state.matrix_stacks[id].matrices[r_state.matrix_stacks[id].depth++], &r_view.active_matrices[id]);
}

/**
 * @brief Pop a saved matrix from the stack
 */
void R_PopMatrix(const r_matrix_id_t id) {
	
	if (r_state.matrix_stacks[id].depth == 0)
		Com_Error(ERR_DROP, "Matrix stack underflow");

	Matrix4x4_Copy(&r_view.active_matrices[id], &r_state.matrix_stacks[id].matrices[--r_state.matrix_stacks[id].depth]);
}

/**
 * @brief Uploads matrices to the currently loaded program.
 */
void R_UseMatrices(void) {

	if (r_state.active_program->UseMatrices)
		r_state.active_program->UseMatrices((const matrix4x4_t *) r_view.active_matrices);
}

/**
 * @brief Uploads the alpha threshold to the currently loaded program.
 */
void R_UseAlphaTest(void) {

	if (r_state.active_program->UseAlphaTest)
		r_state.active_program->UseAlphaTest(r_state.alpha_threshold);
}

/**
 * @brief Uploads the current global color to the currently loaded program.
 */
void R_UseCurrentColor(void) {

	if (r_state.active_program->UseCurrentColor)
		r_state.active_program->UseCurrentColor(r_state.current_color);
}

/**
 * @brief Uploads the current fog data to the currently loaded program.
 */
void R_UseFog(void) {

	if (r_state.active_program->UseFog)
		r_state.active_program->UseFog(&r_state.active_fog_parameters);
}

/**
 * @brief Uploads the interpolation value to the currently loaded program.
 */
void R_UseInterpolation(const float lerp) {

	if (r_state.active_program->UseInterpolation)
		r_state.active_program->UseInterpolation(lerp);
}

/**
 * @brief Change the rendering viewport.
 */
void R_SetViewport(GLint x, GLint y, GLsizei width, GLsizei height) {

	if (r_state.current_viewport.x == x &&
		r_state.current_viewport.y == y &&
		r_state.current_viewport.w == width &&
		r_state.current_viewport.h == height)
		return;
	
	r_state.current_viewport.x = x;
	r_state.current_viewport.y = y;
	r_state.current_viewport.w = width;
	r_state.current_viewport.h = height;

	glViewport(x, y, width, height);
}

#define NEAR_Z 4.0
#define FAR_Z  (MAX_WORLD_COORD * 4.0)

/**
 * @brief Prepare OpenGL for drawing the 3D scene. Update the view-port definition
 * and load our projection and model-view matrices.
 */
void R_Setup3D(void) {

	if (!r_context.context)
		return;

	const SDL_Rect *viewport = &r_view.viewport;
	R_SetViewport(viewport->x, viewport->y, viewport->w, viewport->h);

	// set up projection matrix
	const vec_t aspect = (vec_t) viewport->w / (vec_t) viewport->h;

	const vec_t ymax = NEAR_Z * tan(Radians(r_view.fov[1]));
	const vec_t ymin = -ymax;

	const vec_t xmin = ymin * aspect;
	const vec_t xmax = ymax * aspect;

	Matrix4x4_FromFrustum(&projection_matrix, xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);

	// setup the model-view matrix
	Matrix4x4_CreateIdentity(&r_view.matrix);
	
	Matrix4x4_ConcatRotate(&r_view.matrix, -90.0, 1.0, 0.0, 0.0);	 // put Z going up
	Matrix4x4_ConcatRotate(&r_view.matrix,  90.0, 0.0, 0.0, 1.0);	 // put Z going up

	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[ROLL],  1.0, 0.0, 0.0);
	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[PITCH], 0.0, 1.0, 0.0);
	Matrix4x4_ConcatRotate(&r_view.matrix, -r_view.angles[YAW],   0.0, 0.0, 1.0);

	Matrix4x4_ConcatTranslate(&r_view.matrix, -r_view.origin[0], -r_view.origin[1], -r_view.origin[2]);

	Matrix4x4_Copy(&modelview_matrix, &r_view.matrix);
	Matrix4x4_Invert_Simple(&r_view.inverse_matrix, &r_view.matrix);

	// bind default vertex array
	R_BindDefaultArray(R_ARRAY_VERTEX);

	R_EnableBlend(false);

	R_EnableDepthTest(true);
}

/**
 * @brief Toggle the state of depth testing.
 */
void R_EnableDepthTest(_Bool enable) {

	if (r_state.depth_test_enabled == enable)
		return;

	r_state.depth_test_enabled = enable;

	if (enable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

/**
 * @brief Set the range of depth testing.
 */
void R_DepthRange(GLdouble znear, GLdouble zfar) {

	if (r_state.depth_near == znear &&
		r_state.depth_far == zfar)
		return;

	glDepthRange(znear, zfar);
	r_state.depth_near = znear;
	r_state.depth_far = zfar;
}

/**
 * @brief Shortcut to toggling texunits by ID, for cgame.
 */
void R_EnableTextureID(const uint8_t texunit_id, _Bool enable) {

	R_EnableTexture(&r_state.texunits[texunit_id], enable);
}

/**
 * @brief Set the current window scissor.
 */
void R_EnableScissor(const SDL_Rect *bounds) {

	if (!bounds) {
		if (!r_state.scissor_enabled) {
			return;
		}

		glDisable(GL_SCISSOR_TEST);
		r_state.scissor_enabled = false;
	
		return;
	}

	if (!r_state.scissor_enabled) {
		glEnable(GL_SCISSOR_TEST);
		r_state.scissor_enabled = true;
	}

	glScissor(bounds->x, bounds->y, bounds->w, bounds->h);
}

/**
 * @brief Prepare OpenGL for drawing the 2D overlay. Update the view-port definition
 * and reset the project and model-view matrices.
 */
void R_Setup2D(void) {

	R_SetViewport(0, 0, r_context.width, r_context.height);

	Matrix4x4_FromOrtho(&projection_matrix, 0.0, r_context.width, r_context.height, 0.0, -1.0, 1.0);

	Matrix4x4_CreateIdentity(&modelview_matrix);

	// bind default vertex array
	R_BindDefaultArray(R_ARRAY_VERTEX);

	// and set default texcoords for all 2d pics
	memcpy(texunit_diffuse.texcoord_array, default_texcoords, sizeof(vec2_t) * 4);

	R_UploadToBuffer(&texunit_diffuse.buffer_texcoord_array, 0, sizeof(vec2_t) * 4, default_texcoords);

	R_EnableBlend(true);

	R_EnableDepthTest(false);
}

/**
 * @brief Initializes the OpenGL state cache and sets OpenGL state parameters to
 * appropriate defaults.
 */
void R_InitState(void) {

	r_get_error = Cvar_Add("r_get_error", "0", 0, NULL);

	// See if we have any errors before state initialization.
	R_GetError("Pre-init");
	
	memset(&r_state, 0, sizeof(r_state));

	r_state.depth_mask_enabled = true;
	Vector4Set(r_state.current_color, 1.0, 1.0, 1.0, 1.0);

	// setup vertex array pointers
	R_CreateBuffer(&r_state.buffer_vertex_array, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_state.vertex_array), NULL);
	R_CreateBuffer(&r_state.buffer_color_array, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_state.color_array), NULL);
	R_CreateBuffer(&r_state.buffer_normal_array, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_state.normal_array), NULL);
	R_CreateBuffer(&r_state.buffer_tangent_array, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_state.tangent_array), NULL);
	R_CreateBuffer(&r_state.buffer_element_array, GL_DYNAMIC_DRAW, R_BUFFER_ELEMENT, sizeof(r_state.indice_array), NULL);
	
	R_UnbindBuffer(R_BUFFER_DATA);
	R_UnbindBuffer(R_BUFFER_ELEMENT);

	R_BindDefaultArray(R_ARRAY_VERTEX);
	R_BindDefaultArray(R_ARRAY_COLOR);
	R_BindDefaultArray(R_ARRAY_NORMAL);

	// setup texture units
	const size_t len = MAX_GL_ARRAY_LENGTH * sizeof(vec2_t);

	for (int32_t i = 0; i < MAX_GL_TEXUNITS; i++) {
		r_texunit_t *texunit = &r_state.texunits[i];
		
		if (i < r_config.max_texunits) {
			texunit->texture = GL_TEXTURE0 + i;

			texunit->texcoord_array = Mem_TagMalloc(len, MEM_TAG_RENDERER);
			R_CreateBuffer(&texunit->buffer_texcoord_array, GL_DYNAMIC_DRAW, R_BUFFER_DATA, len, NULL);

			R_EnableTexture(texunit, true);

			R_BindDefaultArray(R_ARRAY_TEX_DIFFUSE);

			R_EnableTexture(texunit, false);
		}
	}

	// default texture unit
	R_SelectTexture(&texunit_diffuse);

	R_EnableTexture(&texunit_diffuse, true);
	
	// polygon offset parameters
	R_PolygonOffset(-1.0, 1.0);

	// alpha blend parameters
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// set num lights
	r_state.max_active_lights = Clamp(r_max_lights->integer, 0, MAX_LIGHTS);

	// set default alpha threshold
	r_state.alpha_threshold = ALPHA_TEST_DISABLED_THRESHOLD;

	// set default stencil pass operation
	r_state.stencil_op_pass = GL_KEEP;

	// set default stencil func arguments
	r_state.stencil_func_mask = ~0;
	r_state.stencil_func_func = GL_ALWAYS;
	r_state.stencil_func_ref = 0;

	// default depth range
	r_state.depth_near = 0.0;
	r_state.depth_far = 1.0;

	R_GetError("Post-init");
}

/**
 * @brief
 */
void R_ShutdownState(void) {

	for (int32_t i = 0; i < MIN(r_config.max_texunits, MAX_GL_TEXUNITS); i++) {
		r_texunit_t *texunit = &r_state.texunits[i];

		if (texunit->texcoord_array)
		{
			Mem_Free(texunit->texcoord_array);
			R_DestroyBuffer(&texunit->buffer_texcoord_array);
		}
	}
	
	R_DestroyBuffer(&r_state.buffer_vertex_array);
	R_DestroyBuffer(&r_state.buffer_color_array);
	R_DestroyBuffer(&r_state.buffer_normal_array);
	R_DestroyBuffer(&r_state.buffer_tangent_array);
	R_DestroyBuffer(&r_state.buffer_element_array);

	memset(&r_state, 0, sizeof(r_state));
}
