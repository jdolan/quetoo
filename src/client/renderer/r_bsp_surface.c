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
#include "r_gl.h"

/**
 * @brief
 */
static void R_SetBspSurfaceState_default(const r_bsp_surface_t *surf) {

	if (r_state.blend_enabled) { // alpha blend
		vec4_t color = { 1.0, 1.0, 1.0, 1.0 };

		switch (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			case SURF_BLEND_33:
				color[3] = 0.40;
				break;
			case SURF_BLEND_66:
				color[3] = 0.80;
				break;
			default: // both flags mean use the texture's alpha channel
				color[3] = 1.0;
				break;
		}

		R_Color(color);
	}

	if (texunit_diffuse->enabled) { // diffuse texture
		R_BindDiffuseTexture(surf->texinfo->material->diffuse->texnum);
	}

	if (texunit_lightmap->enabled) { // lightmap texture

		const r_bsp_lightmap_t *atlas = surf->lightmap.atlas;

		R_BindLightmapTexture(atlas->lightmaps->texnum);

		if (texunit_stainmap->enabled) {
			R_BindStainmapTexture(atlas->stainmaps->texnum);
		}
	}

	if (r_state.lighting_enabled) { // hardware lighting

		if (surf->light_frame == r_locals.light_frame) { // dynamic light sources
			R_EnableLights(surf->light_mask);
		} else {
			R_EnableLights(0);
		}

		R_EnableCaustic(surf->flags & R_SURF_IN_LIQUID);
	} else {
		R_EnableCaustic(false);
	}

	R_UseMaterial(surf->texinfo->material);

	if (r_state.stencil_test_enabled) { // write to stencil buffer to clip shadows
		if (r_model_state.world->bsp->plane_shadows[surf->plane->num]) {
			R_StencilFunc(GL_ALWAYS, R_STENCIL_REF(surf->plane->num), ~0);
		} else {
			R_StencilFunc(GL_ALWAYS, 0, 0);
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspSurface_default(const r_bsp_surface_t *surf) {

	R_DrawArrays(GL_TRIANGLES, surf->element, surf->num_elements);

	r_view.num_bsp_surfaces++;
}

/**
 * @brief
 */
static void R_DrawBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	R_EnableTexture(texunit_diffuse, true);

	R_SetArrayState(r_model_state.world);

	// draw the surfaces
	for (size_t i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->texinfo->flags & SURF_MATERIAL) {
			continue;
		}

		if (surfs->surfaces[i]->frame != r_locals.frame) {
			continue;
		}

		R_SetBspSurfaceState_default(surfs->surfaces[i]);

		R_DrawBspSurface_default(surfs->surfaces[i]);
	}

	// reset state
	if (r_state.lighting_enabled) {

		R_EnableLights(0);

		R_EnableCaustic(false);
	}

	R_UseMaterial(NULL);

	R_Color(NULL);
}

/**
 * @brief
 */
static void R_DrawBspSurfacesLines_default(const r_bsp_surfaces_t *surfs) {

	R_EnableTexture(texunit_diffuse, false);

	R_BindDiffuseTexture(r_image_state.null->texnum);

	R_SetArrayState(r_model_state.world);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (size_t i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->frame != r_locals.frame) {
			continue;
		}

		R_DrawBspSurface_default(surfs->surfaces[i]);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_EnableTexture(texunit_diffuse, true);
}

/**
 * @brief
 */
void R_DrawOpaqueBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count) {
		return;
	}

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawBspSurfacesLines_default(surfs);
		return;
	}

	if (r_draw_bsp_lightmaps->value) {
		R_EnableTexture(texunit_diffuse, false);

		R_BindDiffuseTexture(r_image_state.null->texnum);
	}

	R_EnableTexture(texunit_lightmap, true);

	if (r_deluxemap->value) {
		R_EnableTexture(texunit_deluxemap, true);
	}

	if (r_stainmaps->value) {
		R_EnableTexture(texunit_stainmap, true);
	}

	R_EnableLighting(program_default, true);

	if (r_shadows->value) {
		R_EnableStencilTest(GL_REPLACE, true);
	}

	R_DrawBspSurfaces_default(surfs);

	if (r_shadows->value) {
		R_EnableStencilTest(GL_KEEP, false);
	}

	R_EnableLighting(NULL, false);

	R_EnableTexture(texunit_lightmap, false);
	R_EnableTexture(texunit_deluxemap, false);
	R_EnableTexture(texunit_stainmap, false);

	if (r_draw_bsp_lightmaps->value) {
		R_EnableTexture(texunit_diffuse, true);
	}

#if 0
	if (r_clear->value) {
		byte data[r_context.width * r_context.height];

		glReadPixels(0, 0, r_context.width, r_context.height, GL_UNSIGNED_BYTE, GL_STENCIL_INDEX, data);

		FILE *f = fopen("/tmp/q2w.pgm", "w");
		fprintf(f, "P2 %d %d 255\n", r_context.width, r_context.height);
		for (r_pixel_t i = 0; i < r_context.height; i++) {
			for (r_pixel_t j = 0; j < r_context.width; j++) {
				fprintf(f, "%03d ", data[i * r_context.width + j]);
			}
			fprintf(f, "\n");
		}
		fclose(f);
	}
#endif
}

/**
 * @brief
 */
void R_DrawOpaqueWarpBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count) {
		return;
	}

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawBspSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(program_warp, true);

	R_DrawBspSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}

/**
 * @brief
 */
void R_DrawAlphaTestBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count) {
		return;
	}

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawBspSurfacesLines_default(surfs);
		return;
	}

	R_EnableAlphaTest(ALPHA_TEST_ENABLED_THRESHOLD);

	R_EnableTexture(texunit_lightmap, true);

	if (r_deluxemap->value) {
		R_EnableTexture(texunit_deluxemap, true);
	}

	if (r_stainmaps->value) {
		R_EnableTexture(texunit_stainmap, true);
	}

	R_EnableLighting(program_default, true);

	R_DrawBspSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableTexture(texunit_lightmap, false);
	R_EnableTexture(texunit_deluxemap, false);
	R_EnableTexture(texunit_stainmap, false);

	R_EnableAlphaTest(ALPHA_TEST_DISABLED_THRESHOLD);
}

/**
 * @brief
 */
void R_DrawBlendBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count) {
		return;
	}

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawBspSurfacesLines_default(surfs);
		return;
	}

	if (r_draw_bsp_lightmaps->value) {
		R_EnableTexture(texunit_diffuse, false);

		R_BindDiffuseTexture(r_image_state.null->texnum);
	}

	// blend is already enabled when this is called

	R_EnableTexture(texunit_lightmap, true);

	if (r_deluxemap->value) {
		R_EnableTexture(texunit_deluxemap, true);
	}

	if (r_stainmaps->value) {
		R_EnableTexture(texunit_stainmap, true);
	}

	R_EnableLighting(program_default, true);

	R_DrawBspSurfaces_default(surfs);

	R_EnableLighting(NULL, false);

	R_EnableTexture(texunit_lightmap, false);
	R_EnableTexture(texunit_deluxemap, false);
	R_EnableTexture(texunit_stainmap, false);

	if (r_draw_bsp_lightmaps->value) {
		R_EnableTexture(texunit_diffuse, true);
	}
}

/**
 * @brief
 */
void R_DrawBlendWarpBspSurfaces_default(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count) {
		return;
	}

	if (r_draw_wireframe->value) { // surface outlines
		R_DrawBspSurfacesLines_default(surfs);
		return;
	}

	R_EnableWarp(program_warp, true);

	R_DrawBspSurfaces_default(surfs);

	R_EnableWarp(NULL, false);
}
