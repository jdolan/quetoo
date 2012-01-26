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
 * R_CreateSurfaceFlare
 */
void R_CreateSurfaceFlare(r_bsp_surface_t *surf) {
	r_material_t *m;
	const r_stage_t *s;
	vec3_t span;

	m = &surf->texinfo->image->material;

	if (!(m->flags & STAGE_FLARE)) // surface is not flared
		return;

	surf->flare = (r_bsp_flare_t *) R_HunkAlloc(sizeof(*surf->flare));

	// move the flare away from the surface, into the level
	VectorMA(surf->center, 2, surf->normal, surf->flare->origin);

	// calculate the flare radius based on surface size
	VectorSubtract(surf->maxs, surf->mins, span);
	surf->flare->radius = VectorLength(span);

	s = m->stages; // resolve the flare stage
	while (s) {
		if (s->flags & STAGE_FLARE)
			break;
		s = s->next;
	}

	// resolve flare color
	if (s->flags & STAGE_COLOR)
		VectorCopy(s->color, surf->flare->color);
	else
		VectorCopy(surf->color, surf->flare->color);

	// and scaled radius
	if (s->flags & (STAGE_SCALE_S | STAGE_SCALE_T))
		surf->flare->radius *= (s->scale.s ? s->scale.s : s->scale.t);

	// and image
	surf->flare->image = s->image;
}

/*
 * R_DrawFlareSurfaces
 *
 * Flares are batched by their texture.  Usually, this means one draw operation
 * for all flares in view.  Flare visibility is calculated every few millis, and
 * flare alpha is ramped up or down depending on the results of the visibility
 * trace.  Flares are also faded according to the angle of their surface to the
 * view origin.
 */
void R_DrawFlareSurfaces(r_bsp_surfaces_t *surfs) {
	const r_image_t *image;
	unsigned int i, j, k, l, m;
	vec3_t view, verts[4];
	vec3_t right, up, upright, downright;
	float cos, dist, scale, alpha;
	boolean_t visible;

	if (!r_flares->value)
		return;

	if (!surfs->count)
		return;

	R_EnableColorArray(true);

	R_ResetArrayState();

	glDisable(GL_DEPTH_TEST);

	image = r_flare_images[0];
	R_BindTexture(image->texnum);

	j = k = l = 0;
	for (i = 0; i < surfs->count; i++) {

		const r_bsp_surface_t *surf = surfs->surfaces[i];
		r_bsp_flare_t *f;

		if (surf->frame != r_locals.frame)
			continue;

		f = surf->flare;

		// bind the flare's texture
		if (f->image != image) {

			glDrawArrays(GL_QUADS, 0, l / 3);
			j = k = l = 0;

			image = f->image;
			R_BindTexture(image->texnum);
		}

		// periodically test visibility to ramp alpha
		if (r_view.time - f->time > 0.02) {

			if (r_view.time - f->time > 0.5) // reset old flares
				f->alpha = 0;

			R_Trace(r_view.origin, f->origin, 0, MASK_SHOT);
			visible = r_view.trace.fraction == 1.0;

			f->alpha += (visible ? 0.03 : -0.15); // ramp

			if (f->alpha > 1.0) // clamp
				f->alpha = 1.0;
			else if (f->alpha < 0)
				f->alpha = 0.0;

			f->time = r_view.time;
		}

		VectorSubtract(f->origin, r_view.origin, view);
		dist = VectorNormalize(view);

		// fade according to angle
		cos = DotProduct(surf->normal, view);
		if (cos > 0)
			continue;

		alpha = 0.1 + -cos * r_flares->value;

		if (alpha > 1.0)
			alpha = 1.0;

		alpha = f->alpha * alpha;

		// scale according to distance
		scale = f->radius + (f->radius * dist * .0005);

		VectorScale(r_view.right, scale, right);
		VectorScale(r_view.up, scale, up);

		VectorAdd(up, right, upright);
		VectorSubtract(right, up, downright);

		VectorSubtract(f->origin, downright, verts[0]);
		VectorAdd(f->origin, upright, verts[1]);
		VectorAdd(f->origin, downright, verts[2]);
		VectorSubtract(f->origin, upright, verts[3]);

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

	glColor4ubv(color_white);
}
