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
R_SetVertexArrayState_default
*/
static void R_SetVertexArrayState_default(const model_t *mod){

	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->verts);

	if(r_state.color_array_enabled)  // colors for r_showpolys
		R_BindArray(GL_COLOR_ARRAY, GL_FLOAT, mod->colors);

	if(r_state.lighting_enabled){  // normal vectors for lighting
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->normals);

		if(r_bumpmap->value && mod->tangents)  // tangent vectors for bump mapping
			R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangents);
	}

	if(texunit_diffuse.enabled)  // diffuse texcoords
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoords);

	if(texunit_lightmap.enabled){  // lightmap texcoords
		R_SelectTexture(&texunit_lightmap);

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lmtexcoords);

		R_SelectTexture(&texunit_diffuse);
	}
}


/*
R_SetVertexBufferState_default
*/
static void R_SetVertexBufferState_default(const model_t *mod){

	R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->vertex_buffer);

	if(r_state.color_array_enabled)  // colors for r_showpolys
		R_BindBuffer(GL_COLOR_ARRAY, GL_FLOAT, mod->color_buffer);

	if(r_state.lighting_enabled){  // normal vectors for lighting
		R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->normal_buffer);

		if(r_bumpmap->value && mod->tangent_buffer)  // tangent vectors for bump mapping
			R_BindBuffer(GL_TANGENT_ARRAY, GL_FLOAT, mod->tangent_buffer);
	}

	if(texunit_diffuse.enabled)  // diffuse texcoords
		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->texcoord_buffer);

	if(texunit_lightmap.enabled){  // lightmap texcoords
		R_SelectTexture(&texunit_lightmap);

		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->lmtexcoord_buffer);

		R_SelectTexture(&texunit_diffuse);
	}
}


/*
R_SetArrayState_default
*/
void R_SetArrayState_default(const model_t *mod){
	int vbo;

	vbo = mod->type == mod_bsp ? 1 : 2;

	if((int)r_vertexbuffers->value & vbo)
		R_SetVertexBufferState_default(mod);
	else
		R_SetVertexArrayState_default(mod);
}


/*
R_ResetArrayState
*/
void R_ResetArrayState_default(void){

	R_BindBuffer(0, 0, 0);

	R_BindDefaultArray(GL_VERTEX_ARRAY);

	if(r_state.color_array_enabled)
		R_BindDefaultArray(GL_COLOR_ARRAY);

	if(r_state.lighting_enabled){
		R_BindDefaultArray(GL_NORMAL_ARRAY);

		if(r_bumpmap->value)
			R_BindDefaultArray(GL_TANGENT_ARRAY);
	}

	if(texunit_diffuse.enabled)
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	if(texunit_lightmap.enabled){
		R_SelectTexture(&texunit_lightmap);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		R_SelectTexture(&texunit_diffuse);
	}
}


/*
R_SetSurfaceState_default
*/
static void R_SetSurfaceState_default(msurface_t *surf){
	image_t *image;
	float a;

	if(r_state.blend_enabled){  // alpha blend
		switch(surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)){
			case SURF_BLEND33:
				a = 0.20;
				break;
			case SURF_BLEND66:
				a = 0.40;
				break;
			default:  // both flags mean use the texture's alpha channel
				a = 1.0;
				break;
		}

		glColor4f(1.0, 1.0, 1.0, a);
	}

	image = surf->texinfo->image;

	if(texunit_diffuse.enabled)  // diffuse texture
		R_BindTexture(image->texnum);

	if(texunit_lightmap.enabled)  // lightmap texture
		R_BindLightmapTexture(surf->lightmap_texnum);

	if(r_state.lighting_enabled){  // hardware lighting

		if(r_bumpmap->value){  // bump mapping

			if(image->normalmap){
				R_BindDeluxemapTexture(surf->deluxemap_texnum);
				R_BindNormalmapTexture(image->normalmap->texnum);

				R_EnableBumpmap(true, &image->material);
			}
			else
				R_EnableBumpmap(false, NULL);
		}

		if(surf->lightframe == r_locals.lightframe)  // dynamic light sources
			R_EnableLights(surf->lights);
		else
			R_EnableLights(0);
	}
}


/*
R_DrawSurface_default
*/
static void R_DrawSurface_default(msurface_t *surf){

	glDrawArrays(GL_POLYGON, surf->index, surf->numedges);

	r_view.bsp_polys++;
}


/*
R_DrawSurfaces_default
*/
static void R_DrawSurfaces_default(msurfaces_t *surfs){
	int i;

	R_SetArrayState_default(r_worldmodel);

	// draw the surfaces
	for(i = 0; i < surfs->count; i++){

		if(surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_SetSurfaceState_default(surfs->surfaces[i]);

		R_DrawSurface_default(surfs->surfaces[i]);
	}

	// reset state
	if(r_state.lighting_enabled){

		if(r_state.bumpmap_enabled)
			R_EnableBumpmap(false, NULL);

		R_EnableLights(0);
	}

	R_ResetArrayState_default();

	glColor4ubv(color_white);
}


/*
R_DrawSurfacesLines_default
*/
static void R_DrawSurfacesLines_default(msurfaces_t *surfs){
	int i;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	R_SetArrayState_default(r_worldmodel);
	
	for(i = 0; i < surfs->count; i++){

		if(surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_DrawSurface_default(surfs->surfaces[i]);
	}

	R_ResetArrayState_default();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}


/*
R_DrawOpaqueSurfaces_default
*/
void R_DrawOpaqueSurfaces_default(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	if(r_showpolys->value){  // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableTexture(&texunit_lightmap, true);

	R_EnableLighting(r_state.default_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_lightmap, false);
}


/*
R_DrawOpaqueWarpSurfaces_default
*/
void R_DrawOpaqueWarpSurfaces_default(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	if(r_showpolys->value){  // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(r_state.warp_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}


/*
R_DrawAlphaTestSurfaces_default
*/
void R_DrawAlphaTestSurfaces_default(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	if(r_showpolys->value){  // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableAlphaTest(true);

	R_EnableLighting(r_state.default_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableAlphaTest(false);
}


/*
R_DrawBlendSurfaces_default
*/
void R_DrawBlendSurfaces_default(msurfaces_t *surfs){

	if(!surfs->count)
		return;
	
	if(r_showpolys->value){  // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableTexture(&texunit_lightmap, true);

	// blend is already enabled when this is called
	R_DrawSurfaces_default(surfs);

	R_EnableTexture(&texunit_lightmap, false);
}


/*
R_DrawBlendWarpSurfaces_default
*/
void R_DrawBlendWarpSurfaces_default(msurfaces_t *surfs){

	if(!surfs->count)
		return;

	if(r_showpolys->value){  // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(r_state.warp_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}


/*
R_DrawBackSurfaces_default
*/
void R_DrawBackSurfaces_default(msurfaces_t *surfs){}
