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
 * @brief Binds the specified buffer for the given attribute target with an offset.
 */
void R_BindAttributeBufferOffset(const r_attribute_id_t target, const r_buffer_t *buffer, const GLsizei offset) {

	assert(!buffer || ((buffer->type == R_BUFFER_DATA) == (target != R_ARRAY_ELEMENTS)));

	if (target == R_ARRAY_ALL) {
		for (r_attribute_id_t id = 0; id < R_ARRAY_MAX_ATTRIBS; id++) {
			R_BindAttributeBufferOffset(id, buffer, offset);
		}

		R_BindAttributeBufferOffset(R_ARRAY_ELEMENTS, buffer, offset);
	} else if (target == R_ARRAY_ELEMENTS) {
		r_state.element_buffer = buffer;
	} else {
		r_state.array_buffers[target] = buffer;
		r_state.array_buffer_offsets[target] = offset;
		r_state.array_buffers_dirty |= 1 << target;
	}
}

/**
 * @brief Binds an interleave array to all of its appropriate endpoints with an offset.
 */
void R_BindAttributeInterleaveBufferOffset(const r_buffer_t *buffer, const r_attribute_mask_t mask,
        const GLsizei offset) {

	if (!mask) {
		return;
	}

	for (r_attribute_id_t attrib = R_ARRAY_POSITION; attrib < R_ARRAY_MAX_ATTRIBS; attrib++) {

		r_attribute_mask_t this_mask = (1 << attrib);

		if (!(mask & this_mask) ||
		        !(buffer->attrib_mask & this_mask)) {
			continue;
		}

		assert(buffer->interleave_attribs[attrib]);

		R_BindAttributeBufferOffset(attrib, buffer, offset);
	}
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
static GLenum R_BufferTypeToTarget(int type) {

	return (type == R_BUFFER_ELEMENT) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
}

/**
 * @brief
 */
void R_BindBuffer(const r_buffer_t *buffer) {

	assert(buffer->bufnum != 0);

	if (r_state.active_buffers[buffer->type] == buffer->bufnum) {
		return;
	}

	r_state.active_buffers[buffer->type] = buffer->bufnum;

	glBindBuffer(buffer->target, buffer->bufnum);

	r_view.num_state_changes[R_STATE_BIND_BUFFER]++;

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_UnbindBuffer(const r_buffer_type_t type) {

	if (!r_state.active_buffers[type]) {
		return;
	}

	r_state.active_buffers[type] = 0;

	glBindBuffer(R_BufferTypeToTarget(type), 0);

	r_view.num_state_changes[R_STATE_BIND_BUFFER]++;

	R_GetError(NULL);
}

/**
 * @brief Upload data to an already-created buffer. You could also use this function to
 * resize an existing buffer without uploading data, although it won't make the
 * buffer smaller.
 */
void R_UploadToBuffer(r_buffer_t *buffer, const size_t size, const void *data) {

	assert(buffer->bufnum != 0);

	// Check size. This is benign really, but it's usually a bug.
	if (!size) {
		Com_Warn("Attempted to upload 0 bytes to GPU");
		return;
	}

	R_BindBuffer(buffer);

	// if the buffer isn't big enough to hold what we had already,
	// we have to resize the buffer

	if (size > buffer->size) {
		r_state.buffers_total_bytes -= buffer->size;
		r_state.buffers_total_bytes += size;

		glBufferData(buffer->target, size, data, buffer->hint);
		R_GetError("Full resize");
		r_view.num_buffer_full_uploads++;

		buffer->size = size;
	} else {
		// just update the range we specified
		if (data) {
			glBufferSubData(buffer->target, 0, size, data);
			r_view.num_buffer_partial_uploads++;

			R_GetError("Updating existing buffer");
		}
	}

	r_view.size_buffer_uploads += size;
}

/**
 * @brief Upload data to a sub-position in already-created buffer. You could also use this function to
 * resize an existing buffer without uploading data, although it won't make the
 * buffer smaller.
 * @param data_offset Whether the data pointer should be offset by start or not.
 */
void R_UploadToSubBuffer(r_buffer_t *buffer, const size_t start, const size_t size, const void *data,
                         const _Bool data_offset) {

	assert(buffer->bufnum != 0);

	// Check size. This is benign really, but it's usually a bug.
	if (!size) {
		Com_Warn("Attempted to upload 0 bytes to GPU\n");
		return;
	}

	// Don't allow null ptrs since bufferSubData does not allow it.
	if (!data) {
		Com_Error(ERROR_DROP, "Fatal: attempted to upload null to GPU. bufferSubData does not allow this.\n");
	}

	// offset ptr if requested
	if (start && data && data_offset) {
		data += start;
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
			r_view.num_buffer_full_uploads++;
			glBufferSubData(buffer->target, start, size, data);
			R_GetError("Partial update");
			r_view.num_buffer_partial_uploads++;
		} else {
			glBufferData(buffer->target, total_size, data, buffer->hint);
			R_GetError("Full resize");
			r_view.num_buffer_full_uploads++;
		}

		r_state.buffers_total_bytes -= buffer->size;
		r_state.buffers_total_bytes += total_size;

		buffer->size = total_size;
	} else {
		// just update the range we specified
		glBufferSubData(buffer->target, start, size, data);
		R_GetError("Updating existing buffer");
		r_view.num_buffer_partial_uploads++;
	}

	r_view.size_buffer_uploads += size;
}

/**
 * @brief
 */
static GLubyte R_GetElementSize(const GLenum type) {

	switch (type) {
		case R_ATTRIB_BYTE:
		case R_ATTRIB_UNSIGNED_BYTE:
			return 1;
		case R_ATTRIB_SHORT:
		case R_ATTRIB_UNSIGNED_SHORT:
			return 2;
		case R_ATTRIB_INT:
		case R_ATTRIB_UNSIGNED_INT:
		case R_ATTRIB_FLOAT:
			return 4;
		default:
			Com_Error(ERROR_DROP, "Bad GL type");
	}
}

/**
 * @brief Get the GL_ type from an R_ATTRIB_ type.
 */
static GLenum R_GetGLTypeFromAttribType(const r_attrib_type_t type) {

	switch (type) {
		case R_ATTRIB_FLOAT:
			return GL_FLOAT;
		case R_ATTRIB_BYTE:
			return GL_BYTE;
		case R_ATTRIB_UNSIGNED_BYTE:
			return GL_UNSIGNED_BYTE;
		case R_ATTRIB_SHORT:
			return GL_SHORT;
		case R_ATTRIB_UNSIGNED_SHORT:
			return GL_UNSIGNED_SHORT;
		case R_ATTRIB_INT:
			return GL_INT;
		case R_ATTRIB_UNSIGNED_INT:
			return GL_UNSIGNED_INT;
		default:
			Com_Error(ERROR_FATAL, "Invalid R_ATTRIB_* type\n");
			return GL_INVALID_ENUM;
	}
}

/**
 * @brief
 */
uint32_t R_GetNumAllocatedBuffers(void) {
	return r_state.buffers_total;
}

/**
 * @brief
 */
uint32_t R_GetNumAllocatedBufferBytes(void) {
	return r_state.buffers_total_bytes;
}

/**
 * @brief Allocate a GPU buffer of the specified size.
 * Optionally upload the data immediately too.
 */
void R_CreateBuffer(r_buffer_t *buffer, const r_attrib_type_t element_type, const GLubyte element_count,
                    const _Bool element_normalized, const _Bool element_integer, const GLenum hint,
                    const r_buffer_type_t type, const size_t size,
                    const void *data, const char *func) {

	if (buffer->bufnum != 0) {

		// this one is actually warning since this is a memory leak!!
		Com_Warn("Attempting to reclaim non-empty buffer: %s", buffer->func);
		R_DestroyBuffer(buffer);
	}

	memset(buffer, 0, sizeof(*buffer));

	glGenBuffers(1, &buffer->bufnum);

	buffer->type = type & R_BUFFER_TYPE_MASK;
	buffer->target = R_BufferTypeToTarget(buffer->type);
	buffer->hint = hint;
	buffer->element_type.type = element_type;
	buffer->func = func;

	if (element_type != R_ATTRIB_TOTAL_TYPES) {
		buffer->element_type.count = element_count;
		buffer->element_type.stride = R_GetElementSize(buffer->element_type.type);
		buffer->element_gl_type = R_GetGLTypeFromAttribType(buffer->element_type.type);
		buffer->element_type.normalized = element_normalized;
		buffer->element_type.integer = element_integer;
	} else {
		buffer->element_type.stride = element_count;
	}

	if (type & R_BUFFER_INTERLEAVE) {
		buffer->interleave = true;
	}

	if (size) {
		R_UploadToBuffer(buffer, size, data);
	}

	r_state.buffers_total++;
	g_hash_table_add(r_state.buffers_list, buffer);
}

/**
 * @brief Allocate an interleaved GPU buffer of the specified size.
 * Optionally upload the data immediately too.
 */
void R_CreateInterleaveBuffer_(r_buffer_t *buffer, const GLubyte struct_size, const r_buffer_layout_t *layout,
                               const GLenum hint, const size_t size, const void *data, const char *func) {

	if ((struct_size % 4) != 0) {
		Com_Warn("Buffer struct not aligned to 4, might be an error\n");
	}

	R_CreateBuffer(buffer, R_ATTRIB_TOTAL_TYPES, struct_size, false, false, hint, R_BUFFER_DATA | R_BUFFER_INTERLEAVE, size, data,
	               func);

	GLsizei stride = 0, offset = 0;

	for (; layout->attribute != -1; layout++) {
		stride += layout->size;
		buffer->interleave_attribs[layout->attribute] = layout;
		buffer->attrib_mask |= 1 << layout->attribute;

		// init type pack state once
		if (!layout->_type_state.packed) {
			r_buffer_layout_t *temp = (r_buffer_layout_t *) layout;

			temp->_type_state.type = layout->type;
			temp->_type_state.stride = buffer->element_type.stride;
			temp->_type_state.offset = offset;
			temp->_type_state.count = layout->count;
			temp->_type_state.normalized = layout->normalized;
			temp->_type_state.integer = layout->integral;
			temp->gl_type = R_GetGLTypeFromAttribType(layout->type);

			offset += layout->size;
		}
	}

	if (stride != struct_size) {
		Com_Error(ERROR_DROP, "Buffer interleave size doesn't match layout size\n");
	}
}

/**
 * @brief Destroy a GPU-allocated buffer.
 */
void R_DestroyBuffer(r_buffer_t *buffer) {

	if (buffer->bufnum == 0) {
		Com_Debug(DEBUG_RENDERER, "Attempting to free empty buffer");
		return;
	}

	// if this buffer is currently bound, unbind it.
	if (r_state.active_buffers[buffer->type] == buffer->bufnum) {
		R_UnbindBuffer(buffer->type);
	}

	// if the buffer is attached to any active attribs, remove that ptr too
	for (r_attribute_id_t i = R_ARRAY_POSITION; i < R_ARRAY_MAX_ATTRIBS; ++i) {

		if (r_state.attributes[i].type != NULL &&
		        r_state.attributes[i].value.buffer == buffer) {
			r_state.attributes[i].type = NULL;
			r_state.attributes[i].value.buffer = NULL;
		}
	}

	glDeleteBuffers(1, &buffer->bufnum);

	r_state.buffers_total_bytes -= buffer->size;
	r_state.buffers_total--;

	memset(buffer, 0, sizeof(r_buffer_t));
	g_hash_table_remove(r_state.buffers_list, buffer);

	R_GetError(NULL);
}

typedef struct r_array_state_s {
	const r_model_t *model;
	uint32_t arrays;
	_Bool shell;
} r_array_state_t;

static r_array_state_t r_array_state;

/**
 * @brief Determines whether or not this entity with this specific model
 * should be interpolated.
 */
static _Bool R_IsEntityInterpolatable(const r_entity_t *e, const r_model_t *mod) {

	if (e == NULL) {
		return NULL;
	} else if (mod == NULL) {
		mod = e->model;
	}

	return  IS_MESH_MODEL(mod) &&
	        mod->mesh->num_frames > 1 &&
	        e->old_frame != e->frame;
}

/**
 * @brief Returns a bitmask representing the arrays which should be enabled according
 * to r_state. This function is consulted to determine whether or not array
 * bindings are up to date.
 */
r_attribute_mask_t R_ArraysMask(void) {

	r_attribute_mask_t mask = R_ARRAY_MASK_POSITION;
	_Bool do_interpolation = R_IsEntityInterpolatable(r_view.current_entity, NULL);

	if (do_interpolation) {
		mask |= R_ARRAY_MASK_NEXT_POSITION;
	}

	if (r_state.color_array_enabled) {
		mask |= R_ARRAY_MASK_COLOR;
	}

	if (r_state.lighting_enabled || r_state.shell_enabled) {
		mask |= R_ARRAY_MASK_NORMAL;

		if (do_interpolation) {
			mask |= R_ARRAY_MASK_NEXT_NORMAL;
		}
	}

	if (r_state.lighting_enabled) {
		if (r_bumpmap->value) {
			mask |= R_ARRAY_MASK_TANGENT;

			if (do_interpolation) {
				mask |= R_ARRAY_MASK_NEXT_TANGENT;
			}
		}
	}

	if (texunit_diffuse->enabled) {
		mask |= R_ARRAY_MASK_DIFFUSE;
	}

	if (texunit_lightmap->enabled) {
		mask |= R_ARRAY_MASK_LIGHTMAP;
	}

	mask |= R_ARRAY_GEOMETRY_MASK;

	return mask;
}

/**
 * @brief
 */
static void R_SetArrayStateBSP(const r_model_t *mod, r_attribute_mask_t mask, r_attribute_mask_t attribs) {

	if (r_array_state.model == mod) { // try to save some binds

		const r_attribute_mask_t xor = r_array_state.arrays ^ attribs;

		if (!xor) { // no changes, we're done
			return;
		}

		// resolve what's left to turn on
		mask = attribs & xor;
	}

	R_BindAttributeInterleaveBuffer(&mod->bsp->vertex_buffer, mask);

	R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &mod->bsp->element_buffer);
}

/**
 * @brief
 */
static void R_SetArrayStateMesh(const r_model_t *mod, r_attribute_mask_t mask, r_attribute_mask_t attribs) {

	_Bool use_shell_model = r_state.shell_enabled && R_ValidBuffer(&mod->mesh->shell_vertex_buffer);

	if (r_array_state.model == mod) { // try to save some binds

		r_attribute_mask_t xor = r_array_state.arrays ^ attribs;

		if (r_array_state.shell != use_shell_model) {
			xor |= R_ARRAY_MASK_POSITION |
			       R_ARRAY_MASK_NEXT_POSITION |
			       R_ARRAY_MASK_NORMAL |
			       R_ARRAY_MASK_NEXT_NORMAL;

			r_array_state.shell = use_shell_model;
		} else if (!xor) { // no changes, we're done
			return;
		}

		// resolve what's left to turn on
		mask = attribs & xor;
	} else {
		r_array_state.shell = false;
	}

	if (use_shell_model) {

		R_BindAttributeInterleaveBuffer(&mod->mesh->shell_vertex_buffer, mask);

		// elements
		R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &mod->mesh->shell_element_buffer);
	} else {

		// see if we can do interpolation
		_Bool do_interpolation = false;
		uint16_t old_frame = 0;
		uint16_t frame = 0;

		if (r_view.current_entity) {

			old_frame = r_view.current_entity->old_frame;
			do_interpolation = R_IsEntityInterpolatable(r_view.current_entity, mod);

			if (do_interpolation) {
				frame = r_view.current_entity->frame;
			}
		}

		const uint32_t stride = (mod->num_verts * mod->mesh->vertex_buffer.element_type.stride);

		R_BindAttributeInterleaveBufferOffset(&mod->mesh->vertex_buffer,
		                                      mask & ~(R_ARRAY_MASK_NEXT_POSITION | R_ARRAY_MASK_NEXT_NORMAL | R_ARRAY_MASK_NEXT_TANGENT),
		                                      stride * old_frame);

		if (do_interpolation) {

			const uint32_t offset = stride * frame;

			if (mask & R_ARRAY_MASK_NEXT_POSITION) {
				R_BindAttributeBufferOffset(R_ARRAY_NEXT_POSITION, &mod->mesh->vertex_buffer, offset);
			}
			if (mask & R_ARRAY_MASK_NEXT_NORMAL) {
				R_BindAttributeBufferOffset(R_ARRAY_NEXT_NORMAL, &mod->mesh->vertex_buffer, offset);
			}
			if (mask & R_ARRAY_MASK_NEXT_TANGENT) {
				R_BindAttributeBufferOffset(R_ARRAY_NEXT_TANGENT, &mod->mesh->vertex_buffer, offset);
			}
		}

		// diffuse texcoords
		if (mask & R_ARRAY_MASK_DIFFUSE) {

			if (R_ValidBuffer(&mod->mesh->texcoord_buffer)) {
				R_BindAttributeBuffer(R_ARRAY_DIFFUSE, &mod->mesh->texcoord_buffer);
			} else {
				R_BindAttributeBuffer(R_ARRAY_DIFFUSE, &mod->mesh->vertex_buffer);
			}
		}

		// elements
		R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &mod->mesh->element_buffer);
	}
}

/**
 * @brief
 */
void R_SetArrayState(const r_model_t *mod) {

	r_attribute_mask_t mask = r_state.active_program->arrays_mask; // start with what the program has
	r_attribute_mask_t arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (IS_BSP_MODEL(mod)) {
		R_SetArrayStateBSP(mod, mask, arrays);
	} else if (IS_MESH_MODEL(mod)) {
		R_SetArrayStateMesh(mod, mask, arrays);
	}

	r_array_state.model = mod;
	r_array_state.arrays = arrays;
}

/**
 * @brief
 */
void R_ResetArrayState(void) {

	r_attribute_mask_t mask = R_ARRAY_MASK_ALL, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == NULL) {
		const uint32_t xor = r_array_state.arrays ^ arrays;

		if (!xor) { // no changes, we're done
			return;
		}

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	// vertex array
	if (mask & R_ARRAY_MASK_POSITION) {
		R_UnbindAttributeBuffer(R_ARRAY_POSITION);

		if (mask & R_ARRAY_MASK_NEXT_POSITION) {
			R_UnbindAttributeBuffer(R_ARRAY_NEXT_POSITION);
		}
	}

	// color array
	if (r_state.color_array_enabled) {

		if (mask & R_ARRAY_MASK_COLOR) {
			R_UnbindAttributeBuffer(R_ARRAY_COLOR);
		}
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_MASK_NORMAL) {
			R_UnbindAttributeBuffer(R_ARRAY_NORMAL);

			if (mask & R_ARRAY_MASK_NEXT_NORMAL) {
				R_UnbindAttributeBuffer(R_ARRAY_NEXT_NORMAL);
			}
		}

		if (r_bumpmap->value) {

			if (mask & R_ARRAY_MASK_TANGENT) {
				R_UnbindAttributeBuffer(R_ARRAY_TANGENT);

				if (mask & R_ARRAY_MASK_NEXT_TANGENT) {
					R_UnbindAttributeBuffer(R_ARRAY_NEXT_TANGENT);
				}
			}
		}
	}

	// diffuse texcoords
	if (texunit_diffuse->enabled) {
		if (mask & R_ARRAY_MASK_DIFFUSE) {
			R_UnbindAttributeBuffer(R_ARRAY_DIFFUSE);
		}
	}

	// lightmap texcoords
	if (texunit_lightmap->enabled) {

		if (mask & R_ARRAY_MASK_LIGHTMAP) {
			R_UnbindAttributeBuffer(R_ARRAY_LIGHTMAP);
		}
	}

	// elements
	R_UnbindAttributeBuffer(R_ARRAY_ELEMENTS);

	r_array_state.model = NULL;
	r_array_state.arrays = arrays;
	r_array_state.shell = false;
}

/**
 * @brief
 */
static void R_PrepareProgram() {

	// upload state data that needs to be synced up to current program
	R_SetupAttributes();

	R_UseMatrices();

	R_UseAlphaTest();

	R_UseCurrentColor();

	R_UseFog();

	R_UseCaustic();
}

/**
 * @brief
 */
void R_DrawArrays(GLenum type, GLint start, GLsizei count) {

	assert(r_state.array_buffers[R_ARRAY_POSITION] != NULL);

	R_PrepareProgram();

	if (r_state.element_buffer) {

		R_BindBuffer(r_state.element_buffer);

		const void *offset = (const void *) (ptrdiff_t) (start * r_state.element_buffer->element_type.stride);

		glDrawElements(type, count, r_state.element_buffer->element_gl_type, offset);
		r_view.num_draw_elements++;
		r_view.num_draw_element_count += count;
	} else {
		glDrawArrays(type, start, count);
		r_view.num_draw_arrays++;
		r_view.num_draw_array_count += count;
	}

	R_GetError(NULL);
}
