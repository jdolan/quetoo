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
 * Arrays are "lazily" managed to reduce glArrayPointer calls.  Drawing routines
 * should call R_SetArrayState or R_ResetArrayState somewhat early-on.
 */

#define R_ARRAY_VERTEX			0x1
#define R_ARRAY_COLOR			0x2
#define R_ARRAY_NORMAL			0x4
#define R_ARRAY_TANGENT			0x8
#define R_ARRAY_TEX_DIFFUSE		0x10
#define R_ARRAY_TEX_LIGHTMAP	0x20

typedef struct r_array_state_s {
	const r_model_t *model;
	int arrays;
} r_array_state_t;

static r_array_state_t r_array_state;

/*
 * Returns a bitmask representing the arrays which should be enabled according
 * to r_state.  This function is consulted to determine whether or not array
 * bindings are up to date.
 */
static int R_ArraysMask(void) {
	int mask;

	mask = R_ARRAY_VERTEX;

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

/*
 * R_SetVertexArrayState
 */
static void R_SetVertexArrayState(const r_model_t *mod, int mask) {

	// vertex array
	if (mask & R_ARRAY_VERTEX)
		R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->verts);

	// color array
	if (r_state.color_array_enabled) {
		if (mask & R_ARRAY_COLOR)
			R_BindArray(GL_COLOR_ARRAY, GL_FLOAT, mod->colors);
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_NORMAL)
			R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->normals);

		if (r_bumpmap->value && r_state.active_program == r_state.world_program) {

			if (mask & R_ARRAY_TANGENT)
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

			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lmtexcoords);

			R_SelectTexture(&texunit_diffuse);
		}
	}
}

/*
 * R_SetVertexBufferState
 */
static void R_SetVertexBufferState(const r_model_t *mod, int mask) {

	// vertex array
	if (mask & R_ARRAY_VERTEX)
		R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->vertex_buffer);

	// color array
	if (r_state.color_array_enabled) {
		if (mask & R_ARRAY_COLOR)
			R_BindBuffer(GL_COLOR_ARRAY, GL_FLOAT, mod->color_buffer);
	}

	// normals and tangents
	if (r_state.lighting_enabled) {

		if (mask & R_ARRAY_NORMAL)
			R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->normal_buffer);

		if (r_bumpmap->value && mod->tangent_buffer && r_view.render_mode
				== render_mode_default) {

			if (mask & R_ARRAY_TANGENT)
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

			R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT,
					mod->lmtexcoord_buffer);

			R_SelectTexture(&texunit_diffuse);
		}
	}
}

/*
 * R_SetArrayState
 */
void R_SetArrayState(const r_model_t *mod) {
	int arrays, mask;

	if (r_vertex_buffers->modified) { // force a full re-bind
		r_array_state.model = NULL;
		r_array_state.arrays = 0xffff;
	}

	r_vertex_buffers->modified = false;

	mask = 0xffff, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == mod) { // try to save some binds

		const int xor = r_array_state.arrays ^ arrays;

		if (!xor) // no changes, we're done
			return;

		// resolve what's left to turn on
		mask = arrays & xor;
	}

	if (r_vertex_buffers->value && qglGenBuffers) // use vbo
		R_SetVertexBufferState(mod, mask);
	else
		// or arrays
		R_SetVertexArrayState(mod, mask);

	r_array_state.model = mod;
	r_array_state.arrays = arrays;
}

/*
 * R_ResetArrayState
 */
void R_ResetArrayState(void) {
	int arrays, mask;

	mask = 0xffff, arrays = R_ArraysMask(); // resolve the desired arrays mask

	if (r_array_state.model == NULL) {
		const int xor = r_array_state.arrays ^ arrays;

		if (!xor) // no changes, we're done
			return;

		// resolve what's left to turn on
		mask = arrays & xor;
	} else
		// vbo
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

		if (r_bumpmap->value && r_state.active_program == r_state.world_program) {

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

