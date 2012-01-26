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
 * R_SetSurfaceState_default
 */
static void R_SetSurfaceState_default(const r_bsp_surface_t *surf) {
	r_image_t *image;
	float a;

	if (r_state.blend_enabled) { // alpha blend
		switch (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
		case SURF_BLEND_33:
			a = 0.33;
			break;
		case SURF_BLEND_66:
			a = 0.66;
			break;
		default: // both flags mean use the texture's alpha channel
			a = 1.0;
			break;
		}

		glColor4f(1.0, 1.0, 1.0, a);
	}

	image = surf->texinfo->image;

	if (texunit_diffuse.enabled) // diffuse texture
		R_BindTexture(image->texnum);

	if (texunit_lightmap.enabled) // lightmap texture
		R_BindLightmapTexture(surf->lightmap_texnum);

	if (r_state.lighting_enabled) { // hardware lighting

		if (r_bumpmap->value) { // bump mapping

			if (image->normalmap) {
				R_BindDeluxemapTexture(surf->deluxemap_texnum);
				R_BindNormalmapTexture(image->normalmap->texnum);

				R_EnableBumpmap(&image->material, true);
			} else
				R_EnableBumpmap(NULL, false);
		}

		if (surf->light_frame == r_locals.light_frame) // dynamic light sources
			R_EnableLights(surf->lights);
		else
			R_EnableLights(0);
	}
}

/*
 * R_DrawSurface_default
 */
static void R_DrawSurface_default(const r_bsp_surface_t *surf) {

	glDrawArrays(GL_POLYGON, surf->index, surf->num_edges);

	r_view.bsp_polys++;
}

/*
 * R_DrawSurfaces_default
 */
static void R_DrawSurfaces_default(const r_bsp_surfaces_t *surfs) {
	unsigned int i;

	R_SetArrayState(r_world_model);

	// draw the surfaces
	for (i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_SetSurfaceState_default(surfs->surfaces[i]);

		R_DrawSurface_default(surfs->surfaces[i]);
	}

	// reset state
	if (r_state.lighting_enabled) {

		if (r_state.bumpmap_enabled)
			R_EnableBumpmap(NULL, false);

		R_EnableLights(0);
	}

	glColor4ubv(color_white);
}

/*
 * R_DrawSurfacesLines_default
 */
static void R_DrawSurfacesLines_default(const r_bsp_surfaces_t *surfs) {
	unsigned int i;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_SetArrayState(r_world_model);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_DrawSurface_default(surfs->surfaces[i]);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}

/*
 * R_DrawOpaqueSurfaces_default
 */
void R_DrawOpaqueSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableTexture(&texunit_lightmap, true);

	R_EnableLighting(r_state.world_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_lightmap, false);
}

/*
 * R_DrawOpaqueWarpSurfaces_default
 */
void R_DrawOpaqueWarpSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(r_state.warp_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}

/*
 * R_DrawAlphaTestSurfaces_default
 */
void R_DrawAlphaTestSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableAlphaTest(true);

	R_EnableTexture(&texunit_lightmap, true);

	R_EnableLighting(r_state.world_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_lightmap, false);

	R_EnableAlphaTest(false);
}

/*
 * R_DrawBlendSurfaces_default
 */
void R_DrawBlendSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	// blend is already enabled when this is called

	R_EnableTexture(&texunit_lightmap, true);

	R_DrawSurfaces_default(surfs);

	R_EnableTexture(&texunit_lightmap, false);
}

/*
 * R_DrawBlendWarpSurfaces_default
 */
void R_DrawBlendWarpSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(r_state.warp_program, true);

	R_DrawSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}

/*
 * R_DrawBackSurfaces_default
 */
void R_DrawBackSurfaces_default(const r_bsp_surfaces_t *surfs) {
	// no-op
}
