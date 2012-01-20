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
 * R_AddParticle
 */
void R_AddParticle(r_particle_t *p) {

	if (r_view.num_particles >= MAX_PARTICLES)
		return;

	r_view.particles[r_view.num_particles++] = *p;
}

typedef struct particle_state_s {
	vec3_t weather_right;
	vec3_t weather_up;
	vec3_t splash_right[2];
	vec3_t splash_up[2];
} particle_state_t;

static particle_state_t ps;

/*
 * R_ParticleVerts
 */
static void R_ParticleVerts(r_particle_t *p, GLfloat *out) {
	vec3_t v, up, right, upright, downright;
	vec3_t *verts;
	float scale;

	verts = (vec3_t *) out;

	scale = // hack a scale up to keep particles from disappearing
			(p->current_org[0] - r_view.origin[0]) * r_view.forward[0]
					+ (p->current_org[1] - r_view.origin[1])
							* r_view.forward[1] + (p->current_org[2]
					- r_view.origin[2]) * r_view.forward[2];

	if (scale > 20.0) // use it
		p->current_scale += scale * 0.002;

	if (p->type == PARTICLE_BEAM) { // beams are lines with starts and ends
		VectorSubtract(p->current_org, p->current_end, v);
		VectorNormalize(v);
		VectorCopy(v, up);

		VectorSubtract(r_view.origin, p->current_end, v);
		CrossProduct(up, v, right);
		VectorNormalize(right);
		VectorScale(right, p->current_scale, right);

		VectorAdd(p->current_org, right, verts[0]);
		VectorAdd(p->current_end, right, verts[1]);
		VectorSubtract(p->current_end, right, verts[2]);
		VectorSubtract(p->current_org, right, verts[3]);
		return;
	}

	if (p->type == PARTICLE_DECAL) { // decals are aligned with surfaces
		AngleVectors(p->dir, NULL, right, up);

		VectorAdd(up, right, verts[0]);
		VectorSubtract(right, up, verts[1]);
		VectorNegate(verts[0], verts[2]);
		VectorNegate(verts[1], verts[3]);

		VectorMA(p->current_org, p->current_scale, verts[0], verts[0]);
		VectorMA(p->current_org, p->current_scale, verts[1], verts[1]);
		VectorMA(p->current_org, p->current_scale, verts[2], verts[2]);
		VectorMA(p->current_org, p->current_scale, verts[3], verts[3]);
		return;
	}

	// all other particles are aligned with the client's view

	if (p->type == PARTICLE_WEATHER) { // keep it vertical
		VectorScale(ps.weather_right, p->current_scale, right);
		VectorScale(ps.weather_up, p->current_scale, up);
	} else if (p->type == PARTICLE_SPLASH) { // keep it horizontal

		if (p->current_org[2] > r_view.origin[2]) { // it's above us
			VectorScale(ps.splash_right[0], p->current_scale, right);
			VectorScale(ps.splash_up[0], p->current_scale, up);
		} else { // it's below us
			VectorScale(ps.splash_right[1], p->current_scale, right);
			VectorScale(ps.splash_up[1], p->current_scale, up);
		}
	} else if (p->type == PARTICLE_ROLL) { // roll it

		VectorCopy(r_view.angles, p->dir);
		p->dir[2] = p->roll * r_view.time;

		AngleVectors(p->dir, NULL, right, up);

		VectorScale(right, p->current_scale, right);
		VectorScale(up, p->current_scale, up);
	} else { // default particle alignment with view
		VectorScale(r_view.right, p->current_scale, right);
		VectorScale(r_view.up, p->current_scale, up);
	}

	VectorAdd(up, right, upright);
	VectorSubtract(right, up, downright);

	VectorSubtract(p->current_org, downright, verts[0]);
	VectorAdd(p->current_org, upright, verts[1]);
	VectorAdd(p->current_org, downright, verts[2]);
	VectorSubtract(p->current_org, upright, verts[3]);
}

/*
 * R_ParticleTexcoords
 */
static void R_ParticleTexcoords(r_particle_t *p, GLfloat *out) {
	float s, t;

	if (!p->scroll_s && !p->scroll_t) {
		memcpy(out, default_texcoords, sizeof(vec2_t) * 4);
		return;
	}

	s = p->scroll_s * r_view.time;
	t = p->scroll_t * r_view.time;

	out[0] = 0.0 + s;
	out[1] = 0.0 + t;

	out[2] = 1.0 + s;
	out[3] = 0.0 + t;

	out[4] = 1.0 + s;
	out[5] = 1.0 + t;

	out[6] = 0.0 + s;
	out[7] = 1.0 + t;
}

/*
 * R_ParticleColor
 */
static void R_ParticleColor(r_particle_t *p, GLfloat *out) {
	byte color[4];
	int i, j;

	memcpy(&color, &palette[p->color], sizeof(color));
	color[3] = p->current_alpha * 255.0;
	j = 0;

	for (i = 0; i < 4; i++) { // duplicate color data to all 4 verts
		out[j + 0] = color[0] / 255.0;
		out[j + 1] = color[1] / 255.0;
		out[j + 2] = color[2] / 255.0;
		out[j + 3] = color[3] / 255.0;
		j += 4;
	}
}

/*
 * R_DrawParticles_
 */
static void R_DrawParticles_(int mask) {
	r_particle_t *p;
	r_image_t *image;
	int i, j, k, l;

	image = NULL;

	j = k = l = 0;

	for (p = r_view.particles, i = 0; i < r_view.num_particles; i++, p++) {

		if (!(p->type & mask)) // skip it
			continue;

		// bind the particle's texture
		if (p->image != image) {

			if (image) // draw pending particles
				glDrawArrays(GL_QUADS, 0, j / 3);

			j = k = l = 0;

			image = p->image;
			R_BindTexture(image->texnum);

			R_BlendFunc(GL_SRC_ALPHA, p->blend);
		}

		R_ParticleVerts(p, &r_state.vertex_array_3d[j]);
		j += sizeof(vec3_t) / sizeof(vec_t) * 4;

		R_ParticleTexcoords(p, &texunit_diffuse.texcoord_array[k]);
		k += sizeof(vec2_t) / sizeof(vec_t) * 4;

		R_ParticleColor(p, &r_state.color_array[l]);
		l += sizeof(vec4_t) / sizeof(vec_t) * 4;
	}

	if (j)
		glDrawArrays(GL_QUADS, 0, j / 3);
}

#define PARTICLE_MASK (PARTICLE_NORMAL | PARTICLE_ROLL | PARTICLE_BUBBLE | \
					   PARTICLE_BEAM | PARTICLE_WEATHER | PARTICLE_SPLASH)
/*
 * R_DrawParticles
 */
void R_DrawParticles(void) {
	vec3_t v;

	if (!r_view.num_particles)
		return;

	R_EnableColorArray(true);

	R_ResetArrayState();

	VectorCopy(r_view.angles, v);

	v[0] = 0; // keep weather particles vertical by removing pitch
	AngleVectors(v, NULL, ps.weather_right, ps.weather_up);

	v[0] = -90; // and splash particles horizontal by setting it
	AngleVectors(v, NULL, ps.splash_right[0], ps.splash_up[0]);

	v[0] = 90; // even if they are below us
	AngleVectors(v, NULL, ps.splash_right[1], ps.splash_up[1]);

	R_DrawParticles_(PARTICLE_DECAL);

	R_DrawParticles_(PARTICLE_MASK);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	glColor4ubv(color_white);
}

