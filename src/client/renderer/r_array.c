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

// macro for determining interpolation ability
#define IS_ENTITY_INTERPOLATABLE(e, mod) \
	(IS_MESH_MODEL(mod) && \
	 mod->mesh->num_frames > 1 && \
	 e->old_frame != e->frame)

/**
 * @brief Returns a bitmask representing the arrays which should be enabled according
 * to r_state. This function is consulted to determine whether or not array
 * bindings are up to date.
 */
int32_t R_ArraysMask(void) {
	uint32_t mask = R_ARRAY_MASK_VERTEX;

	_Bool do_interpolation = r_view.current_entity != NULL &&
	                         IS_ENTITY_INTERPOLATABLE(r_view.current_entity, r_view.current_entity->model);

	if (do_interpolation) {
		mask |= R_ARRAY_MASK_NEXT_VERTEX;
	}

	if (r_state.color_array_enabled) {
		mask |= R_ARRAY_MASK_COLOR;
	}

	if (r_state.lighting_enabled) {
		mask |= R_ARRAY_MASK_NORMAL;

		if (do_interpolation) {
			mask |= R_ARRAY_MASK_NEXT_NORMAL;
		}

		if (r_bumpmap->value) {
			mask |= R_ARRAY_MASK_TANGENT;

			if (do_interpolation) {
				mask |= R_ARRAY_MASK_NEXT_TANGENT;
			}
		}
	}

	if (texunit_diffuse.enabled) {
		mask |= R_ARRAY_MASK_TEX_DIFFUSE;
	}

	if (texunit_lightmap.enabled) {
		mask |= R_ARRAY_MASK_TEX_LIGHTMAP;
	}

	return mask;
}

/**
 * @brief
 */
void R_SetArrayState(const r_model_t *mod) {

	uint32_t mask = 0xffff, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == mod) { // try to save some binds

		const uint32_t xor = r_array_state.arrays ^ arrays;

		if (!xor) { // no changes, we're done
			return;
		}

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	if (r_state.active_program) { // cull anything the program doesn't use
		mask &= r_state.active_program->arrays_mask;
	}

	R_BindArray(R_ARRAY_COLOR, NULL);

	_Bool do_interpolation = false;
	uint16_t old_frame = 0;
	uint16_t frame = 0;

	// see if we can do interpolation
	if (r_view.current_entity) {
		old_frame = r_view.current_entity->old_frame;
		do_interpolation = IS_ENTITY_INTERPOLATABLE(r_view.current_entity, mod);

		if (do_interpolation) {
			frame = r_view.current_entity->frame;
		}
	}

	// vertex array
	if (mask & R_ARRAY_MASK_VERTEX) {

		R_BindArrayOffset(R_ARRAY_VERTEX, &mod->vertex_buffer, (mod->num_verts * sizeof(vec3_t)) * old_frame);

		// bind interpolation if we need it
		if ((mask & R_ARRAY_MASK_NEXT_VERTEX) && do_interpolation) {
			R_BindArrayOffset(R_ARRAY_NEXT_VERTEX, &mod->vertex_buffer, (mod->num_verts * sizeof(vec3_t)) * frame);
		}
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_MASK_NORMAL) {

			R_BindArrayOffset(R_ARRAY_NORMAL, &mod->normal_buffer, (mod->num_verts * sizeof(vec3_t)) * old_frame);

			// bind interpolation if we need it
			if ((mask & R_ARRAY_MASK_NEXT_NORMAL) && do_interpolation) {
				R_BindArrayOffset(R_ARRAY_NEXT_NORMAL, &mod->normal_buffer, (mod->num_verts * sizeof(vec3_t)) * frame);
			}
		}

		if (r_bumpmap->value) {

			if ((mask & R_ARRAY_MASK_TANGENT) && R_ValidBuffer(&mod->tangent_buffer)) {

				R_BindArrayOffset(R_ARRAY_TANGENT, &mod->tangent_buffer, (mod->num_verts * sizeof(vec4_t)) * old_frame);

				// bind interpolation if we need it
				if ((mask & R_ARRAY_MASK_NEXT_TANGENT) && do_interpolation) {
					R_BindArrayOffset(R_ARRAY_NEXT_TANGENT, &mod->tangent_buffer, (mod->num_verts * sizeof(vec4_t)) * frame);
				}
			}
		}
	}

	// diffuse texcoords
	if (texunit_diffuse.enabled) {

		if (mask & R_ARRAY_MASK_TEX_DIFFUSE) {
			R_BindArray(R_ARRAY_TEX_DIFFUSE, &mod->texcoord_buffer);
		}
	}

	// lightmap texcoords
	if (texunit_lightmap.enabled) {

		if (mask & R_ARRAY_MASK_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);

			R_BindArray(R_ARRAY_TEX_LIGHTMAP, &mod->lightmap_texcoord_buffer);

			R_SelectTexture(&texunit_diffuse);
		}
	}

	// elements
	if (R_ValidBuffer(&mod->element_buffer)) {
		R_BindArray(R_ARRAY_ELEMENTS, &mod->element_buffer);
	}

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

		if (!xor) { // no changes, we're done
			return;
		}

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	// vertex array
	if (mask & R_ARRAY_MASK_VERTEX) {
		R_BindDefaultArray(R_ARRAY_VERTEX);

		if (mask & R_ARRAY_MASK_NEXT_VERTEX) {
			R_BindDefaultArray(R_ARRAY_NEXT_VERTEX);
		}
	}

	// color array
	if (r_state.color_array_enabled) {
		if (mask & R_ARRAY_MASK_COLOR) {
			R_BindDefaultArray(R_ARRAY_COLOR);
		}
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_MASK_NORMAL) {
			R_BindDefaultArray(R_ARRAY_NORMAL);

			if (mask & R_ARRAY_MASK_NEXT_NORMAL) {
				R_BindDefaultArray(R_ARRAY_NEXT_NORMAL);
			}
		}

		if (r_bumpmap->value) {

			if (mask & R_ARRAY_MASK_TANGENT) {
				R_BindDefaultArray(R_ARRAY_TANGENT);

				if (mask & R_ARRAY_MASK_NEXT_TANGENT) {
					R_BindDefaultArray(R_ARRAY_NEXT_TANGENT);
				}
			}
		}
	}

	// diffuse texcoords
	if (texunit_diffuse.enabled) {
		if (mask & R_ARRAY_MASK_TEX_DIFFUSE) {
			R_BindDefaultArray(R_ARRAY_TEX_DIFFUSE);
		}
	}

	// lightmap texcoords
	if (texunit_lightmap.enabled) {

		if (mask & R_ARRAY_MASK_TEX_LIGHTMAP) {
			R_SelectTexture(&texunit_lightmap);

			R_BindDefaultArray(R_ARRAY_TEX_LIGHTMAP);

			R_SelectTexture(&texunit_diffuse);
		}
	}

	// elements
	R_BindArray(R_ARRAY_ELEMENTS, NULL);

	r_array_state.model = NULL;
	r_array_state.arrays = arrays;
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
}

/**
 * @brief
 */
void R_DrawArrays(GLenum type, GLint start, GLsizei count) {

	assert(r_state.array_buffers[R_ARRAY_VERTEX] != NULL);

	R_PrepareProgram();

	if (r_state.element_buffer) {

		R_BindBuffer(r_state.element_buffer);
		glDrawElements(type, count, GL_UNSIGNED_INT, (const void *)(ptrdiff_t)(start * sizeof(GLuint)));
		r_view.num_draw_elements++;
		r_view.num_draw_element_count += count;
	} else {
		glDrawArrays(type, start, count);
		r_view.num_draw_arrays++;
		r_view.num_draw_array_count += count;
	}


	R_GetError(NULL);
}