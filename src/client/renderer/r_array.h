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

#ifndef __R_ARRAY_H__
#define __R_ARRAY_H__

#include "r_types.h"

#ifdef __R_LOCAL_H__

	void R_BindBuffer(const r_buffer_t *buffer);
	void R_UnbindBuffer(const r_buffer_type_t type);
	void R_UploadToBuffer(r_buffer_t *buffer, const size_t size, const void *data);
	void R_UploadToSubBuffer(r_buffer_t *buffer, const size_t start, const size_t size, const void *data,
							 const _Bool data_offset);

	void R_CreateBuffer(r_buffer_t *buffer, const r_attrib_type_t element_type, const GLubyte element_count,
						const _Bool element_normalized, const GLenum hint, const r_buffer_type_t type,
						const size_t size, const void *data, const char *func);

	#define R_CreateDataBuffer(buffer, element_type, element_count, element_normalized, hint, size, data) \
			R_CreateBuffer(buffer, element_type, element_count, element_normalized, hint, R_BUFFER_DATA, size, data, __func__)

	void R_CreateInterleaveBuffer_(r_buffer_t *buffer, const GLubyte struct_size, const r_buffer_layout_t *layout,
								   const GLenum hint, const size_t size, const void *data, const char *func);

	#define R_CreateInterleaveBuffer(buffer, struct_size, layout, hint, size, data) \
			R_CreateInterleaveBuffer_(buffer, struct_size, layout, hint, size, data, __func__)

	#define R_CreateElementBuffer(buffer, element_type, hint, size, data) \
			R_CreateBuffer(buffer, element_type, 1, false, hint, R_BUFFER_ELEMENT, size, data, __func__)

	void R_DestroyBuffer(r_buffer_t *buffer);
	_Bool R_ValidBuffer(const r_buffer_t *buffer);

	void R_BindAttributeBuffer(const r_attribute_id_t target, const r_buffer_t *buffer);
	void R_BindAttributeBufferOffset(const r_attribute_id_t target, const r_buffer_t *buffer, const GLsizei offset);
	void R_BindAttributeInterleaveBuffer(const r_buffer_t *buffer, const r_attribute_mask_t mask);
	void R_BindAttributeInterleaveBufferOffset(const r_buffer_t *buffer, const GLsizei offset, const r_attribute_mask_t mask);
	#define R_UnbindAttributeBuffer(target) R_BindAttributeBuffer(target, NULL)
	#define R_UnbindAttributeBuffers() R_UnbindAttributeBuffer(R_ARRAY_ALL)

	void R_SetArrayState(const r_model_t *mod);
	void R_ResetArrayState(void);
	void R_DrawArrays(GLenum type, GLint start, GLsizei count);
	r_attribute_mask_t R_ArraysMask(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_ARRAY_H__ */
