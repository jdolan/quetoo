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

/*
 * Arrays are "lazily" managed to reduce glArrayPointer calls. Drawing routines
 * should call R_SetArrayState or R_ResetArrayState somewhat early-on.
 */

typedef struct r_array_state_s {
	const r_model_t *model;
	uint32_t arrays;
} r_array_state_t;

static r_array_state_t r_array_state;

/**
 * @brief Returns a bitmask representing the arrays which should be enabled according
 * to r_state. This function is consulted to determine whether or not array
 * bindings are up to date.
 */
static int32_t R_ArraysMask(void) {
	uint32_t mask = R_ARRAY_VERTEX;

	if (r_state.color_array_enabled)
		mask |= R_ARRAY_COLOR;

	if (r_state.lighting_enabled) {
		mask |= R_ARRAY_NORMAL;

		if (r_bumpmap->value)
			mask |= R_ARRAY_TANGENT;
	}

	if (texunit_diffuse.enabled)
		mask |= R_ARRAY_TEX_DIFFUSE;

	if (texunit_lightmap.enabled)
		mask |= R_ARRAY_TEX_LIGHTMAP;

	return mask;
}

/**
 * @brief
 */
static void R_SetVertexArrayState(const r_model_t *mod, uint32_t mask) {

	// vertex array
	if (mask & R_ARRAY_VERTEX)
		R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->verts);

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_NORMAL)
			R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->normals);

		if (r_bumpmap->value) {

			if ((mask & R_ARRAY_TANGENT) && mod->tangents)
				R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangents);
		}
	}

	// diffuse texcoords
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_TEX_DIFFUSE)
			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoords);
	}

	// lightmap coords
	if (texunit_lightmap.enabled) {

		if (mask & R_ARRAY_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);

			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lightmap_texcoords);

			R_SelectTexture(&texunit_diffuse);
		}
	}
}

/**
 * @brief
 */
static void R_SetVertexBufferState(const r_model_t *mod, uint32_t mask) {

	// vertex array
	if (mask & R_ARRAY_VERTEX)
		R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->vertex_buffer);

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_NORMAL)
			R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->normal_buffer);

		if (r_bumpmap->value) {

			if ((mask & R_ARRAY_TANGENT) && mod->tangent_buffer)
				R_BindBuffer(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangent_buffer);
		}
	}

	// diffuse texcoords
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_TEX_DIFFUSE)
			R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoord_buffer);
	}

	// lightmap texcoords
	if (texunit_lightmap.enabled) {

		if (mask & R_ARRAY_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);

			R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lightmap_texcoord_buffer);

			R_SelectTexture(&texunit_diffuse);
		}
	}
}

/**
 * @brief
 */
void R_SetArrayState(const r_model_t *mod) {

	if (r_vertex_buffers->modified) { // force a full re-bind
		r_array_state.model = NULL;
		r_array_state.arrays = 0xffff;
	}

	r_vertex_buffers->modified = false;

	uint32_t mask = 0xffff, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == mod) { // try to save some binds

		const uint32_t xor = r_array_state.arrays ^ arrays;

		if (!xor) // no changes, we're done
			return;

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	if (r_state.active_program) // cull anything the program doesn't use
		mask &= r_state.active_program->arrays_mask;

	if (r_vertex_buffers->value && qglGenBuffers) // use vbo
		R_SetVertexBufferState(mod, mask);
	else
		// or arrays
		R_SetVertexArrayState(mod, mask);

	r_array_state.model = mod;
	r_array_state.arrays = arrays;
}

/**
 * @brief
 */
void R_ResetArrayState(void) {

	uint32_t mask = 0xffff, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == NULL) {
		const uint32_t xor = r_array_state.arrays ^ arrays;

		if (!xor) // no changes, we're done
			return;

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	// reset vbo
	R_BindBuffer(0, 0, 0);

	// vertex array
	if (mask & R_ARRAY_VERTEX)
		R_BindDefaultArray(GL_VERTEX_ARRAY);

	// color array
	if (r_state.color_array_enabled) {
		if (mask & R_ARRAY_COLOR)
			R_BindDefaultArray(GL_COLOR_ARRAY);
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_NORMAL)
			R_BindDefaultArray(GL_NORMAL_ARRAY);

		if (r_bumpmap->value) {

			if (mask & R_ARRAY_TANGENT)
				R_BindDefaultArray(GL_TANGENT_ARRAY);
		}
	}

	// diffuse texcoords
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_TEX_DIFFUSE)
			R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	}

	// lightmap texcoords
	if (texunit_lightmap.enabled) {

		if (mask & R_ARRAY_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);

			R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

			R_SelectTexture(&texunit_diffuse);
		}
	}

	r_array_state.model = NULL;
	r_array_state.arrays = arrays;
}

/**
 * @brief
 */
void R_DrawArrays(GLenum type, GLint start, GLsizei count) {

	R_UseMatrices();

	glDrawArrays(type, start, count);
}