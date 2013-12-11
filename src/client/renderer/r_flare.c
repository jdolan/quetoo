/*
 * Copyright(c) 1997-2001 id Software, Inc.
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
#include "client.h"

/*
 * @brief Allocates an initializes the flare for the specified surface, if one
 * should be applied. The flare is linked to the provided BSP model and will
 * be freed automatically.
 */
void R_CreateBspSurfaceFlare(r_bsp_model_t *bsp, r_bsp_surface_t *surf) {
	r_material_t *m;
	vec3_t span;

	m = surf->texinfo->material;

	if (!(m->flags & STAGE_FLARE)) // surface is not flared
		return;

	surf->flare = Mem_LinkMalloc(sizeof(*surf->flare), bsp);

	// move the flare away from the surface, into the level
	VectorMA(surf->center, 2, surf->normal, surf->flare->origin);

	// calculate the flare radius based on surface size
	VectorSubtract(surf->maxs, surf->mins, span);
	surf->flare->radius = VectorLength(span);

	const r_stage_t *s = m->stages; // resolve the flare stage
	while (s) {
		if (s->flags & STAGE_FLARE)
			break;
		s = s->next;
	}

	if(!s) {
		return;
	}

	// resolve flare color
	if (s->flags & STAGE_COLOR)
		VectorCopy(s->color, surf->flare->color);
	else
		VectorCopy(surf->texinfo->material->diffuse->color, surf->flare->color);

	// and scaled radius
	if (s->flags & (STAGE_SCALE_S | STAGE_SCALE_T))
		surf->flare->radius *= (s->scale.s ? s->scale.s : s->scale.t);

	// and image
	surf->flare->image = s->image;
}

/*
 * @brief Flares are batched by their texture. Usually, this means one draw operation
 * for all flares in view. Flare visibility is calculated every few millis, and
 * flare alpha is ramped up or down depending on the results of the visibility
 * trace. Flares are also faded according to the angle of their surface to the
 * view origin.
 */
void R_DrawFlareBspSurfaces(const r_bsp_surfaces_t *surfs) {
	uint32_t i, j, k, l, m;
	vec3_t view, verts[4];
	vec3_t right, up, up_right, down_right;

	if (!r_flares->value)
		return;

	if (!surfs->count)
		return;

	R_EnableColorArray(true);

	R_ResetArrayState();

	glDisable(GL_DEPTH_TEST);

	const r_image_t *image = surfs->surfaces[0]->flare->image;
	R_BindTexture(image->texnum);

	j = k = l = 0;
	for (i = 0; i < surfs->count; i++) {
		const r_bsp_surface_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame)
			continue;

		r_bsp_flare_t *f = surf->flare;

		// bind the flare's texture
		if (f->image != image) {

			glDrawArrays(GL_QUADS, 0, l / 3);
			j = k = l = 0;

			image = f->image;
			R_BindTexture(image->texnum);
		}

		// periodically test visibility to ramp alpha
		if (r_view.time - f->time > 15) {

			if (r_view.time - f->time > 500) // reset old flares
				f->alpha = 0;

			cm_trace_t tr = Cl_Trace(r_view.origin, f->origin, NULL, NULL, 0, MASK_SHOT);

			f->alpha += (tr.fraction == 1.0) ? 0.03 : -0.15; // ramp
			f->alpha = Clamp(f->alpha, 0.0, 1.0); // clamp

			f->time = r_view.time;
		}

		VectorSubtract(f->origin, r_view.origin, view);
		const vec_t dist = VectorNormalize(view);

		// fade according to angle
		const vec_t cos = DotProduct(surf->normal, view);
		if (cos > 0.0)
			continue;

		vec_t alpha = 0.1 + -cos * r_flares->value;

		if (alpha > 1.0)
			alpha = 1.0;

		alpha = f->alpha * alpha;

		// scale according to distance
		const vec_t scale = f->radius + (f->radius * dist * .0005);

		VectorScale(r_view.right, scale, right);
		VectorScale(r_view.up, scale, up);

		VectorAdd(up, right, up_right);
		VectorSubtract(right, up, down_right);

		VectorSubtract(f->origin, down_right, verts[0]);
		VectorAdd(f->origin, up_right, verts[1]);
		VectorAdd(f->origin, down_right, verts[2]);
		VectorSubtract(f->origin, up_right, verts[3]);

		for (m = 0; m < 4; m++) { // duplicate color data to all 4 verts
			memcpy(&r_state.color_array[j], f->color, sizeof(vec3_t));
			r_state.color_array[j + 3] = alpha;
			j += 4;
		}

		// copy texcoord info
		memcpy(&texunit_diffuse.texcoord_array[k], default_texcoords, sizeof(vec2_t) * 4);
		k += sizeof(vec2_t) / sizeof(vec_t) * 4;

		// and lastly copy the 4 verts
		memcpy(&r_state.vertex_array_3d[l], verts, sizeof(vec3_t) * 4);
		l += sizeof(vec3_t) / sizeof(vec_t) * 4;
	}

	glDrawArrays(GL_QUADS, 0, l / 3);

	glEnable(GL_DEPTH_TEST);

	R_EnableColorArray(false);

	R_Color(NULL);
}
