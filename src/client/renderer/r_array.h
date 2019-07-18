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

void R_BindBuffer(const r_buffer_t *buffer);
void R_UnbindBuffer(const r_buffer_type_t type);
void R_UploadToBuffer(r_buffer_t *buffer, const size_t size, const void *data);
void R_UploadToSubBuffer(r_buffer_t *buffer, const size_t start, const size_t size, const void *data,
                         const _Bool data_offset);

void R_CreateBuffer(r_buffer_t *buffer, const r_create_buffer_t *arguments);
void R_CreateInterleaveBuffer(r_buffer_t *buffer, const r_create_interleave_t *arguments);
void R_CreateDataBuffer(r_buffer_t *buffer, const r_create_buffer_t *arguments);
void R_CreateElementBuffer(r_buffer_t *buffer, const r_create_element_t *arguments);

void R_DestroyBuffer(r_buffer_t *buffer);
_Bool R_ValidateBuffer(const r_buffer_t *buffer);

void R_BindAttributeBufferOffset(const r_attribute_id_t target, const r_buffer_t *buffer, const GLsizei offset);
void R_BindAttributeInterleaveBufferOffset(const r_buffer_t *buffer, const r_attribute_mask_t mask,
        const GLsizei offset);

#define R_BindAttributeBuffer(target, buffer) R_BindAttributeBufferOffset(target, buffer, 0)
#define R_BindAttributeInterleaveBuffer(buffer, mask) R_BindAttributeInterleaveBufferOffset(buffer, mask, 0)

#define R_UnbindAttributeBuffer(target) R_BindAttributeBuffer(target, NULL)
#define R_UnbindAttributeBuffers() R_UnbindAttributeBuffer(R_ATTRIB_ALL)

void R_SetArrayState(const r_model_t *mod);
void R_ResetArrayState(void);
void R_DrawArrays(GLenum type, GLint start, GLsizei count);
r_attribute_mask_t R_AttributeMask(void);
#endif /* __R_LOCAL_H__ */
