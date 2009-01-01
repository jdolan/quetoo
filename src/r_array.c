/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "renderer.h"


/*
 * Arrays are "lazily" managed to reduce glArrayPointer calls.  Drawing routines
 * should call R_SetArrayState or R_ResetArrayState somewhat early-on.
 */

/*
 * R_SetVertexArrayState
 */
static void R_SetVertexArrayState(const model_t *mod){

	// vertex array
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->verts);

	// color array
	if(r_state.color_array_enabled)
		R_BindArray(GL_COLOR_ARRAY, GL_FLOAT, mod->colors);

	// normals and tangents
	if(r_state.lighting_enabled){
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->normals);

		if(r_bumpmap->value && mod->tangents)
			R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangents);
	}

	// diffuse texcoords
	if(texunit_diffuse.enabled)
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoords);

	// lightmap coords
	if(texunit_lightmap.enabled){
		R_SelectTexture(&texunit_lightmap);

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lmtexcoords);

		R_SelectTexture(&texunit_diffuse);
	}
}


/*
 * R_SetVertexBufferState
 */
static void R_SetVertexBufferState(const model_t *mod){

	// vertex array
	R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->vertex_buffer);

	// color array
	if(r_state.color_array_enabled)
		R_BindBuffer(GL_COLOR_ARRAY, GL_FLOAT, mod->color_buffer);

	// normals and tangents
	if(r_state.lighting_enabled){
		R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->normal_buffer);

		if(r_bumpmap->value && mod->tangent_buffer)
			R_BindBuffer(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangent_buffer);
	}

	// diffuse texcoords
	if(texunit_diffuse.enabled)
		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoord_buffer);

	// lightmap texcoords
	if(texunit_lightmap.enabled){
		R_SelectTexture(&texunit_lightmap);

		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lmtexcoord_buffer);

		R_SelectTexture(&texunit_diffuse);
	}
}


/*
 * R_SetArrayState
 */
void R_SetArrayState(const model_t *mod){
	int mask;

	if(r_vertexbuffers->modified)  // force a re-bind
		r_locals.model = NULL;

	r_vertexbuffers->modified = false;

	if(r_locals.model == mod)  // save binds
		return;

	r_locals.model = (model_t *)mod;

	// use of vertex buffer objects depends on cvar bitmask
	if(mod->type == mod_bsp || mod->type == mod_bsp_submodel)
		mask = 1;
	else
		mask = 2;

	if(((int)r_vertexbuffers->value) & mask)  // use vbo
		R_SetVertexBufferState(mod);
	else  // or arrays
		R_SetVertexArrayState(mod);
}


/*
 * R_ResetArrayState
 */
void R_ResetArrayState(void){

	r_locals.model = NULL;

	// vbo
	R_BindBuffer(0, 0, 0);

	// vertex array
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	// color array
	if(r_state.color_array_enabled)
		R_BindDefaultArray(GL_COLOR_ARRAY);

	// normals and tangents
	if(r_state.lighting_enabled){
		R_BindDefaultArray(GL_NORMAL_ARRAY);

		if(r_bumpmap->value)
			R_BindDefaultArray(GL_TANGENT_ARRAY);
	}

	// diffuse texcoords
	if(texunit_diffuse.enabled)
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	// lightmap texcoords
	if(texunit_lightmap.enabled){
		R_SelectTexture(&texunit_lightmap);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		R_SelectTexture(&texunit_diffuse);
	}
}

