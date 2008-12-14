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
R_SetVertexArrayState_pro
*/
static void R_SetVertexArrayState_pro(const model_t *mod){

	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->verts);

	if(r_state.color_array_enabled)
		R_BindArray(GL_COLOR_ARRAY, GL_FLOAT, mod->colors);

	if(r_state.lighting_enabled)  // normal vectors for lighting
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->normals);
}


/*
R_SetVertexBufferState_pro
*/
static void R_SetVertexBufferState_pro(const model_t *mod){

	R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->vertex_buffer);

	if(r_state.color_array_enabled)
		R_BindBuffer(GL_COLOR_ARRAY, GL_FLOAT, mod->color_buffer);

	if(r_state.lighting_enabled)  // normal vectors for lighting
		R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->normal_buffer);
}


/*
R_SetArrayState_pro
*/
static void R_SetArrayState_pro(const model_t *mod){
	int vbo;

	vbo = mod->type == mod_bsp ? 1 : 2;

	if((int)r_vertexbuffers->value & vbo)
		R_SetVertexBufferState_pro(mod);
	else
		R_SetVertexArrayState_pro(mod);
}


/*
R_ResetArrayState_pro
*/
static void R_ResetArrayState_pro(void){

	R_BindBuffer(0, 0, 0);

	R_BindDefaultArray(GL_VERTEX_ARRAY);

	if(r_state.color_array_enabled)
		R_BindDefaultArray(GL_COLOR_ARRAY);

	if(r_state.lighting_enabled)
		R_BindDefaultArray(GL_NORMAL_ARRAY);
}


/*
R_SetSurfaceState_pro
*/
static void R_SetSurfaceState_pro(msurface_t *surf){

	if(r_state.lighting_enabled){

		if(surf->lightframe == r_locals.lightframe)  // dynamic light sources
			R_EnableLights(surf->lights);
		else
			R_EnableLights(0);
	}
}


/*
R_DrawSurface_pro
*/
static void R_DrawSurface_pro(const msurface_t *surf){

	glDrawArrays(GL_POLYGON, surf->index, surf->numedges);

	r_view.bsp_polys++;
}


/*
R_DrawSurfaces_pro
*/
static void R_DrawSurfaces_pro(const msurfaces_t *surfs){
	int i;

	R_SetArrayState_pro(r_worldmodel);

	// draw the surfaces
	for(i = 0; i < surfs->count; i++){

		if(surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_SetSurfaceState_pro(surfs->surfaces[i]);

		R_DrawSurface_pro(surfs->surfaces[i]);
	}

	R_ResetArrayState_pro();
}


/*
R_DrawOpaqueSurfaces_pro
*/
void R_DrawOpaqueSurfaces_pro(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_EnableLighting(r_state.pro_program, true);

	R_DrawSurfaces_pro(surfs);

	R_EnableLighting(NULL, false);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}


/*
R_DrawAlphaTestSurfaces_pro
*/
void R_DrawAlphaTestSurfaces_pro(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_EnableBlend(true);

	R_DrawSurfaces_pro(surfs);

	R_EnableBlend(false);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}


/*
R_DrawBlendSurfaces_pro
*/
void R_DrawBlendSurfaces_pro(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// blend is already enabled when this is called
	R_DrawSurfaces_pro(surfs);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}


/*
R_DrawBackSurfaces_pro
*/
void R_DrawBackSurfaces_pro(msurfaces_t *surfs){
	int i;

	if(!r_lines->value || !surfs->count)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_SetArrayState_pro(r_worldmodel);

	glPolygonMode(GL_FRONT, GL_LINE);

	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(1.0, 1.0);

	glEnable(GL_LINE_SMOOTH);

	glLineWidth(r_linewidth->value);
	glColor4f(0.0, 0.0, 0.0, r_lines->value);

	// draw the surfaces
	for(i = 0; i < surfs->count; i++){

		if(surfs->surfaces[i]->backframe != r_locals.frame)
			continue;

		if(surfs->surfaces[i]->texinfo->flags & (SURF_WARP | SURF_SKY))
			continue;

		R_DrawSurface_pro(surfs->surfaces[i]);
	}

	glLineWidth(1.0);
	glColor4ubv(color_white);

	glDisable(GL_LINE_SMOOTH);

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_LINE);

	glPolygonMode(GL_FRONT, GL_FILL);

	R_ResetArrayState_pro();

	R_EnableTexture(&texunit_diffuse, true);
}
