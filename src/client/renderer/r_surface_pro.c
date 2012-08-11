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
 * @brief
 */
static void R_SetSurfaceState_pro(const r_bsp_surface_t *surf) {

	if (r_state.lighting_enabled) {

		if (surf->light_frame == r_locals.light_frame) // dynamic light sources
			R_EnableLights(surf->lights);
		else
			R_EnableLights(0);
	}
}

/*
 * @brief
 */
static void R_DrawSurface_pro(const r_bsp_surface_t *surf) {

	glDrawArrays(GL_POLYGON, surf->index, surf->num_edges);

	r_view.bsp_polys++;
}

/*
 * @brief
 */
static void R_DrawSurfaces_pro(const r_bsp_surfaces_t *surfs) {
	uint32_t i;

	R_SetArrayState(r_world_model);

	// draw the surfaces
	for (i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->texinfo->flags & SURF_MATERIAL)
			continue;

		if (surfs->surfaces[i]->frame != r_locals.frame)
			continue;

		R_SetSurfaceState_pro(surfs->surfaces[i]);

		R_DrawSurface_pro(surfs->surfaces[i]);
	}
}

/*
 * @brief
 */
void R_DrawOpaqueSurfaces_pro(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
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
 * @brief
 */
void R_DrawAlphaTestSurfaces_pro(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
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
 * @brief
 */
void R_DrawBlendSurfaces_pro(const r_bsp_surfaces_t *surfs) {

	if (!surfs->count)
		return;

	// blend is already enabled when this is called

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	R_DrawSurfaces_pro(surfs);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);
}

/*
 * @brief
 */
void R_DrawBackSurfaces_pro(const r_bsp_surfaces_t *surfs) {
	uint32_t i;

	if (!r_line_alpha->value || !surfs->count)
		return;

	const vec4_t color = { 0.0, 0.0, 0.0, r_line_alpha->value };

	R_EnableTexture(&texunit_diffuse, false);

	R_SetArrayState(r_world_model);

	glPolygonMode(GL_FRONT, GL_LINE);

	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(1.0, -r_line_width->value * 2.0);

	if (!r_multisample->value)
		glEnable(GL_LINE_SMOOTH);

	glLineWidth(r_line_width->value);
	R_Color(color);

	// draw the surfaces
	for (i = 0; i < surfs->count; i++) {

		if (surfs->surfaces[i]->back_frame != r_locals.frame)
			continue;

		if (surfs->surfaces[i]->texinfo->flags & (SURF_WARP | SURF_SKY))
			continue;

		R_DrawSurface_pro(surfs->surfaces[i]);
	}

	glLineWidth(1.0);
	R_Color(NULL);

	if (!r_multisample->value)
		glDisable(GL_LINE_SMOOTH);

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_LINE);

	glPolygonMode(GL_FRONT, GL_FILL);

	R_EnableTexture(&texunit_diffuse, true);
}
